// Standalone, deterministic reproducer for the check_hits_order_conflict()
// out_of_range abort (see docs/issues/pagan2_quirks.md in the
// mutation_scatter_plot repo for the production incident this fixes).
//
// Bypasses Exonerate entirely and calls the crashing function directly with
// a hand-built Substring_hit whose start_site lands exactly at len1/len2 --
// the same boundary a real Exonerate hit landed on in production (confirmed
// from a core dump: len1=836 at the throw, and a q_end of 2508 for the same
// read elsewhere in the same call chain; 2508/3 == 836 exactly, so the hit's
// reported end coincided with the sequence's own length after codon
// translation -- consistent with Exonerate's own coordinate being one past
// the last valid 0-based index).
//
// We cannot ship the real GISAID read that triggered this in production, so
// this constructs the same boundary condition directly instead of trying to
// coax Exonerate into reproducing it from a synthetic FASTA -- an approach
// that was tried first (a hand-built ~1274-codon synthetic reference/query
// pair, then a 60-read randomized stress batch) and did not reproduce it:
// the trigger is a specific Exonerate coordinate edge case, not a general
// property of divergent/gappy input.
//
// Build (from src/, after a normal build has produced the listed .o files;
// test_check_version_stub.cpp is only needed if check_version.o was not
// built, e.g. libcurl-dev is unavailable -- see check_version.cpp's own
// #include <curl/curl.h>, unrelated to this fix):
//   g++ -std=c++11 -w -Iutils -Imain -I. -o test_check_hits_order_conflict \
//     test_check_hits_order_conflict.cpp test_check_version_stub.cpp \
//     find_anchors.o settings.o settings_handle.o log_output.o text_utils.o \
//     -lboost_program_options -lboost_regex -lboost_thread -lboost_system \
//     -lgomp -lm -lz -lpthread -ldl
// Run: ./test_check_hits_order_conflict
//   Exit 0 + two PASS lines on the fixed code; SIGABRT (std::out_of_range)
//   on the pre-fix code, from the [boundary] scenario alone.
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include "utils/find_anchors.h"
#include "utils/settings_handle.h"

using namespace ppa;
using namespace std;

static void die(const string &msg)
{
    cerr << "FAIL: " << msg << endl;
    exit(1);
}

int main(int argc, char *argv[])
{
    // Populate Settings_handle::st with defaults (exonerate-hit-trim=5 etc.)
    // the same way main() does, without requiring any of the normally
    // mandatory alignment-input flags.
    const char *fake_argv[] = {"test_check_hits_order_conflict"};
    Settings_handle::st.read_command_line_arguments(1, const_cast<char**>(fake_argv));
    int trim = Settings_handle::st.get("exonerate-hit-trim").as<int>();

    string seq1(40, 'A');
    string seq2(40, 'A');
    int len1 = (int)seq1.length();
    int len2 = (int)seq2.length();

    // ---- Scenario 1: boundary hit -- this is what crashed in production ----
    // start_site_1 == len1 (one past the last valid 0-based index).  length
    // must survive "length -= trim*2" and still be positive, or the inner
    // loop's own bound (start_site_1+length) never exceeds start_site_1 and
    // the loop body -- where the .at() call that actually throws lives --
    // never runs even with the bug live.  (A short length was the mistake in
    // the first draft of this reproducer: it silently passed against BOTH
    // the buggy and the fixed binary, which would have shipped a test that
    // proves nothing.)
    {
        vector<Substring_hit> hits;
        Substring_hit h;
        h.start_site_1 = len1;
        h.start_site_2 = 5;
        h.length = 20;
        h.score = 100;
        hits.push_back(h);

        cout << "[boundary] start_site_1=" << h.start_site_1 << " len1=" << len1 << endl;
        Find_anchors fa;
        fa.check_hits_order_conflict(&seq1, &seq2, &hits);
        cout << "PASS: boundary hit handled without throwing." << endl;
    }

    // ---- Scenario 2: a normal, comfortably in-bounds hit is preserved ----
    // Confirms the fix does not over-clamp: a hit that already fits inside
    // [0, len) after trimming must survive with its trimmed coordinates
    // intact, not be zeroed out by the new length-clamp.
    {
        vector<Substring_hit> hits;
        Substring_hit h;
        h.start_site_1 = 10;
        h.start_site_2 = 10;
        h.length = 30;
        h.score = 100;
        hits.push_back(h);

        Find_anchors fa;
        fa.check_hits_order_conflict(&seq1, &seq2, &hits);

        if (hits.size() != 1)
            die("normal in-bounds hit was dropped, expected 1 survivor");
        const Substring_hit &out = hits.front();
        int expect_start = 10 + trim;
        int expect_length = 30 - trim*2;
        if (out.start_site_1 != expect_start || out.start_site_2 != expect_start)
            die("normal hit's start was altered beyond the documented trim");
        if (out.length != expect_length)
            die("normal hit's length was altered beyond the documented trim");
        cout << "PASS: normal in-bounds hit preserved (start=" << out.start_site_1
             << " length=" << out.length << ")." << endl;
    }

    return 0;
}
