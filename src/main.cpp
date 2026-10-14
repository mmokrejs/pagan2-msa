/***************************************************************************
 *   Copyright (C) 2010-2019 by Ari Loytynoja                              *
 *   ari.loytynoja@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <string>
#include <vector>
#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils/process_seed.h"
#include "utils/settings.h"
#include "utils/settings_handle.h"
#include "utils/log_output.h"
#include "utils/input_output_parser.h"
#include "utils/fasta_reader.h"
#include "main/node.h"
#include "utils/model_factory.h"
#include "main/reads_aligner.h"

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

using namespace std;

using namespace ppa;


/*****************************************************************************/
/*  --independent-queries                                                    */
/*****************************************************************************/
//
// By default the queries of --queryfile are placed one after another into
// ONE growing tree and alignment, so where a query goes and how it is aligned
// depends on the queries before it in the same file. Placing a query
//
//   - inserts a new internal node "#n#" (the target node and the query)
//     into the tree, and re-points the parent's alignment sites to it
//     (Reads_aligner::correct_sites_index()), so the next query is aligned
//     against a grown alignment;
//   - re-reconstructs the parent's ancestral sequence
//     (reconstruct_one_parsimony_ancestor()), which later queries are
//     scored and aligned against;
//   - with target preselection, adds the query and the new node to the
//     candidate targets of every later query (target_sequences);
//   - consumes an internal node number, so later nodes are named differently.
//
// With --independent-queries the reference alignment, the tree and the model
// are set up ONCE, and each query is then placed and aligned in a child
// process forked from that state: the child aligns that query alone, exactly
// as a run given only that query would, and writes what such a run writes,
// under the output name "<outfile>.<n>" (n = the query's 1-based position
// among the queries read from --queryfile; a repeated name is renamed
// "<name>.1", ... as in a default run). The fork's copy of the process is
// what gives every query the pristine reference: nothing a child changes
// reaches the parent or the next child. The parent lists every query's
// outcome in "<outfile>.queries.tsv":
//
//   query  name  status  detail  alignment
//
// with status "placed" (the alignment file is named), "unplaced" (the query
// matched no node or failed the overlap/identity thresholds; no file, as a
// single-query run writes none) or "failed" (the child exited non-zero or was
// killed; detail says how). A failed query loses that query only: the rest
// are still aligned, and the run exits 1 at the end.
//
// The parent must be single-threaded when it forks, and is: with a reference
// alignment nothing before query placement starts a thread.

namespace {

// Everything buffered is written now, or a child would write it again.
void flush_all_output()
{
    Log_output::flush();
    cout.flush();
    cerr.flush();
    fflush(NULL);
}

bool set_close_on_exec(int fd)
{
    int flags = fcntl(fd, F_GETFD);
    return flags != -1 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != -1;
}

// The child's whole life: align queries->at(i) alone, write its output under
// query_outfile, tell the parent "placed" or "unplaced" through status_fd.
void align_one_query_alone(Node *root, Model_factory *mf, int count,
                           Input_output_parser *iop, Fasta_reader *fr,
                           vector<Fasta_entry> *sequences,
                           vector<Fasta_entry> *queries, int i,
                           const string &query_outfile, int status_fd)
{
    // As main() seeds a process: from the clock and THIS process's pid, so
    // a child's temp-file names are not its siblings' (see process_seed.h).
    srand(process_seed(static_cast<uint64_t>(time(0)), static_cast<uint64_t>(getpid())));

    Settings_handle::st.set_string("outfile", query_outfile);

    vector<Fasta_entry> query(1, queries->at(i));

    Reads_aligner ra;
    ra.align(root,mf,count,&query);
    Node *query_root = ra.get_global_root();

    bool placed = query_root->get_number_of_query_leaves() > 0;
    iop->output_aligned_sequences(fr,sequences,query_root);

    flush_all_output();

    string status = placed ? "placed" : "unplaced";
    const char *p = status.c_str();
    size_t left = status.size();
    while(left > 0)
    {
        ssize_t n = write(status_fd, p, left);
        if(n < 0 && errno == EINTR)
            continue;
        if(n <= 0)
            _exit(1);
        p += n;
        left -= n;
    }
    _exit(0);
}

// Runs one query in a child; returns "placed", "unplaced" or "failed", with
// *detail saying why for "failed".
string run_query_in_child(Node *root, Model_factory *mf, int count,
                          Input_output_parser *iop, Fasta_reader *fr,
                          vector<Fasta_entry> *sequences,
                          vector<Fasta_entry> *queries, int i,
                          const string &query_outfile, string *detail)
{
    int fds[2];
    if(pipe(fds) != 0)
    {
        *detail = string("pipe: ")+strerror(errno);
        return "failed";
    }
    // Not inherited by the helpers (Exonerate, MAFFT, ...) the child starts,
    // so the parent sees end-of-file when the child itself is gone.
    if(!set_close_on_exec(fds[0]) || !set_close_on_exec(fds[1]))
    {
        *detail = string("fcntl: ")+strerror(errno);
        close(fds[0]);
        close(fds[1]);
        return "failed";
    }

    flush_all_output();

    pid_t pid = fork();
    if(pid < 0)
    {
        *detail = string("fork: ")+strerror(errno);
        close(fds[0]);
        close(fds[1]);
        return "failed";
    }
    if(pid == 0)
    {
        close(fds[0]);
        align_one_query_alone(root,mf,count,iop,fr,sequences,queries,i,query_outfile,fds[1]);
        _exit(1);  // not reached
    }

    close(fds[1]);

    string message;
    char buf[64];
    while(true)
    {
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if(n < 0 && errno == EINTR)
            continue;
        if(n <= 0)
            break;
        message.append(buf, n);
    }
    close(fds[0]);

    int wstatus = 0;
    while(waitpid(pid, &wstatus, 0) < 0)
    {
        if(errno != EINTR)
        {
            *detail = string("waitpid: ")+strerror(errno);
            return "failed";
        }
    }

    stringstream ds;
    if(WIFSIGNALED(wstatus))
    {
        ds << "killed by signal " << WTERMSIG(wstatus) << " (" << strsignal(WTERMSIG(wstatus)) << ")";
        *detail = ds.str();
        return "failed";
    }
    if(!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
    {
        ds << "exit status " << WEXITSTATUS(wstatus);
        *detail = ds.str();
        return "failed";
    }
    if(message != "placed" && message != "unplaced")
    {
        *detail = "exited 0 without reporting an outcome";
        return "failed";
    }
    return message;
}

// The parent: one child per query, in file order; the outcomes go to
// "<outfile>.queries.tsv". Returns main()'s exit status.
int align_queries_independently(Node *root, Model_factory *mf, int count,
                                Input_output_parser *iop, Fasta_reader *fr,
                                vector<Fasta_entry> *sequences)
{
    string outfile = "outfile";
    if(Settings_handle::st.is("outfile"))
        outfile = Settings_handle::st.get("outfile").as<string>();

    string format = "fasta";
    if(Settings_handle::st.is("outformat"))
        format = Settings_handle::st.get("outformat").as<string>();

    vector<Fasta_entry> queries;
    Reads_aligner::read_queries(&queries);

    string manifest_file = outfile+".queries.tsv";
    ofstream manifest(manifest_file.c_str());
    if(!manifest)
    {
        Log_output::write_out("Error: cannot write '"+manifest_file+"'.\nExiting.\n\n",0);
        exit(1);
    }
    manifest << "query\tname\tstatus\tdetail\talignment\n";
    manifest.flush();

    int n_placed = 0, n_unplaced = 0, n_failed = 0;

    for(int i=0; i<(int)queries.size(); i++)
    {
        int query_1based = i+1;

        stringstream qs;
        qs << outfile << "." << query_1based;
        string query_outfile = qs.str();

        string detail;
        string status = run_query_in_child(root,mf,count,iop,fr,sequences,&queries,i,query_outfile,&detail);

        string alignment_file = "";
        if(status == "placed")
        {
            alignment_file = query_outfile+fr->get_format_suffix(format);
            n_placed++;
        }
        else if(status == "unplaced")
            n_unplaced++;
        else
            n_failed++;

        manifest << query_1based << "\t" << queries.at(i).name << "\t" << status << "\t"
                 << detail << "\t" << alignment_file << "\n";
        manifest.flush();

        stringstream msg;
        msg << "\nQuery " << query_1based << "/" << queries.size() << " '" << queries.at(i).name << "': " << status;
        if(detail != "")
            msg << ", " << detail;
        if(alignment_file != "")
            msg << ", " << alignment_file;
        msg << "\n";
        Log_output::write_out(msg.str(),0);
    }

    manifest.close();
    if(!manifest)
    {
        Log_output::write_out("Error: writing '"+manifest_file+"' failed.\nExiting.\n\n",0);
        exit(1);
    }

    struct rusage children;
    double children_cpu = 0;
    if(getrusage(RUSAGE_CHILDREN, &children) == 0)
        children_cpu = children.ru_utime.tv_sec + children.ru_utime.tv_usec/1e6
                     + children.ru_stime.tv_sec + children.ru_stime.tv_usec/1e6;

    stringstream ss;
    ss << "\nIndependent queries: " << queries.size() << " aligned one at a time; "
       << n_placed << " placed, " << n_unplaced << " unplaced, " << n_failed << " failed; "
       << "the query processes used " << children_cpu << " cpu sec.\n"
       << "Outcome per query: " << manifest_file << "\n";
    Log_output::write_out(ss.str(),0);

    return n_failed > 0 ? 1 : 0;
}

}

int main(int argc, char *argv[])
{


    /***********************************************************************/
    /*  Start the clock: requires somes OSX specific hacks                 */
    /***********************************************************************/

    clock_t t_start=clock();
    struct timespec tcpu_start, tcpu_finish;

    #ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    tcpu_start.tv_sec = mts.tv_sec;
    tcpu_start.tv_nsec = mts.tv_nsec;
    #else
    clock_gettime(CLOCK_MONOTONIC, &tcpu_start);
    #endif



    /*****************************************************************************/
    /*  Read the command-line parameters; start the log system; reset the clock  */
    /*****************************************************************************/

    try
    {
        int rv = Settings_handle::st.read_command_line_arguments(argc, argv);
    }
    catch ( const boost::program_options::error& e ) {
            Settings_handle::st.info_noexit();
            Log_output::write_out("Error in command line arguments: "+string(e.what())+".\n\n",0);
            exit(1);
    }

    Log_output::open_stream();


    if(!Settings_handle::st.is("silent"))
    {
        Settings_handle::st.print_msg();
        time_t s_time;
        time( &s_time );
        Log_output::write_out("\nThe analysis started: " +string( asctime( localtime( &s_time ) ) )+"\n",0);
    }

    // Seed per PROCESS, not per second.
    //
    // The temp files that Exonerate_queries, Mafft_alignment, BppAncestors,
    // Bppdist_tree, Raxml_tree and FastTree_tree write are named from rand():
    // "q<r>.fas", "t<r>.fas", "m<r>.fas" and so on, all in the one directory
    // returned by get_temp_dir() (i.e. /tmp, or --temp-folder).  Each of those
    // sites already guards against reusing a name with a check-then-create
    // retry loop -- for example Exonerate_queries::write_exonerate_input():
    //
    //     while(true) { ... if(!q_file && !t_file) { open; break; } *r = rand(); }
    //
    // With srand(time(0)) that loop CANNOT diverge between two pagan2
    // processes started in the same second: identical seed means identical
    // rand() sequence, so both walk the same candidate names in the same
    // order, both find the name free, and both create it.  Each then runs its
    // helper on the other's data and delete_files() removes the other's
    // files.  Nothing crashes; the alignment is simply of the wrong
    // sequences, or comes back empty.
    //
    // process_seed() mixes the pid in (see utils/process_seed.h for why not
    // with a plain XOR).  getpid() adds no new portability constraint:
    // <unistd.h> is already used in the tree (see utils/bppancestors.cpp)
    // and there are no Windows guards anywhere in it.
    //
    // This does not make a run reproducible and is not meant to -- rand() is
    // also used for stochastic backtracking in viterbi_alignment.cpp, which
    // was never reproducible under a clock-derived seed either.
    srand(process_seed(static_cast<uint64_t>(time(0)), static_cast<uint64_t>(getpid())));
    clock_t analysis_start_time=clock();


    /***********************************************************************/
    /*  Threaded alignment: using maximum number by defualt                */
    /***********************************************************************/

    int n_threads = 1;
    int max_threads = boost::thread::hardware_concurrency();
    if(Settings_handle::st.is("threads"))
    {
        int nt = Settings_handle::st.get("threads").as<int > ();
        if(nt>0 && nt<=max_threads) { n_threads = nt; }
    }

    /***********************************************************************/
    stringstream ss;
    ss << "Running with "<<n_threads<<" threads.\n";
    Log_output::write_out(ss.str(),1);
    /***********************************************************************/





    /***********************************************************************/
    /*  Read the sequence file                                             */
    /***********************************************************************/

    Fasta_reader fr;
    vector<Fasta_entry> sequences;
    bool reference_alignment = false;

    Input_output_parser iop;
    iop.parse_input_sequences(&fr,&sequences,&reference_alignment);



    /***********************************************************************/
    /*  Read the guidetree file                                            */
    /***********************************************************************/

    Node *root = iop.parse_input_tree(&fr,&sequences,reference_alignment,n_threads);



    /***********************************************************************/
    /*  Check that input is fine and place the sequences to nodes          */
    /***********************************************************************/

    int data_type = -1;
    iop.match_sequences_and_tree(&fr,&sequences,root,reference_alignment,&data_type);


    /***********************************************************************/
    ss.str(string());
    ss << "Time main::input: "<<double(clock()-t_start)/CLOCKS_PER_SEC<<"\n";
    Log_output::write_out(ss.str(),"time");
    /***********************************************************************/



    /***********************************************************************/
    /*  Define the alignment model                                         */
    /***********************************************************************/

    Model_factory mf(data_type);
    iop.define_alignment_model(&fr,&mf,data_type);


    /***********************************************************************/
    ss.str(string());
    ss << "Time main::model: "<< double(clock()-t_start)/CLOCKS_PER_SEC <<"\n";
    Log_output::write_out(ss.str(),"time");
    /***********************************************************************/




    /***********************************************************************/
    /*  Read or align the sequence data.                                   */
    /***********************************************************************/

    int count = 1;
    root->name_internal_nodes(&count);

    if(reference_alignment)
    {
        root->read_reference_alignment(&mf);
    }
    else
    {
        // some features only work with 1 thread
        if(n_threads==1)
            root->start_alignment(&mf);
        else if(Settings_handle::st.is("boost"))
            root->start_threaded_alignment(&mf,n_threads);
        else
            root->start_openmp_alignment(&mf,n_threads);
    }

    /***********************************************************************/
    ss.str(string());
    ss << "Time main::align: "<< double(clock()-t_start)/CLOCKS_PER_SEC <<"\n";
    Log_output::write_out(ss.str(),"time");
    /***********************************************************************/



    /***********************************************************************/
    /*  Or align each query alone against it: writes the output per query  */
    /***********************************************************************/

    int exit_code = 0;
    bool independent_queries = Settings_handle::st.is("independent-queries");

    if( independent_queries )
    {
        exit_code = align_queries_independently(root,&mf,count,&iop,&fr,&sequences);
    }


    /***********************************************************************/
    /*  If query sequences, add them to the alignment.                     */
    /***********************************************************************/

    if( Settings_handle::st.is("queryfile") && !independent_queries )
    {

        Reads_aligner ra;
        ra.align(root,&mf,count);

        root = ra.get_global_root();

        /***********************************************************************/
        ss.str(string());
        ss << "Time main::reads_align: "<< double(clock()-t_start)/CLOCKS_PER_SEC <<"\n";
        Log_output::write_out(ss.str(),"time");
        /***********************************************************************/
    }



    /***********************************************************************/
    /*  Collect the results and output them                                */
    /***********************************************************************/

    if( !independent_queries )
        iop.output_aligned_sequences(&fr,&sequences,root);


    /***********************************************************************/
    ss.str(string());
    ss << "Time main::main_exit: "<< double(clock()-t_start)/CLOCKS_PER_SEC <<"\n";
    Log_output::write_out(ss.str(),"time");
    /***********************************************************************/


    /***********************************************************************/
    /*  Stop the clock: requires somes OSX specific hacks                  */
    /***********************************************************************/

    #ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    tcpu_finish.tv_sec = mts.tv_sec;
    tcpu_finish.tv_nsec = mts.tv_nsec;
    #else
    clock_gettime(CLOCK_MONOTONIC, &tcpu_finish);
    #endif

    /***********************************************************************/

    double elapsed;
    elapsed = (tcpu_finish.tv_sec - tcpu_start.tv_sec);
    elapsed += (tcpu_finish.tv_nsec - tcpu_start.tv_nsec) / 1000000000.0;

    time_t s_time;
    time( &s_time );

    /***********************************************************************/
    ss.str(string());
    ss << "\nThe analysis finished: " << asctime( localtime( &s_time ) );
    ss<<"Total time used by PAGAN: "<<elapsed<<" wall sec, " <<double_t(clock()-analysis_start_time)/CLOCKS_PER_SEC<<" cpu sec.\n\n";
    Log_output::write_out(ss.str(),0);
    /***********************************************************************/

    delete root;

    return exit_code;
}
