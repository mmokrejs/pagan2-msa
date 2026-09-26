// Synthetic test for utils/child_pipe.h: helpers are run WITHOUT A SHELL.
//
// What is pinned, each a way the replacement for popen(3) could look right
// and not be:
//
// 1. NO SHELL RE-PARSES THE ARGUMENTS. An argument holding a space, a `;`
//    and a `$VAR` reaches the program as ONE literal argument; under popen it
//    would have been split, and the `;` would have run a second command.
// 2. STDERR IS IN THE STREAM, as the old `2>&1` put it there -- exonerate's
//    error text must still reach pagan2's reader.
// 3. THE EXIT STATUS SURVIVES, readable with WEXITSTATUS as pclose's was.
// 4. A MISSING PROGRAM IS REPORTED BY OPEN FAILING, not by a shell printing
//    "not found" into the output stream and exiting 127 later.
// 5. A bare name is found on PATH, like the shell found it; a name with a
//    '/' is run as given.
// 6. The reader sees EOF: the parent must not keep the pipe's write end.
//
// Build (from src/):
//   g++ -std=c++11 -w -I. -Iutils -o test_child_pipe test_child_pipe.cpp
// Run: ./test_child_pipe  (exit 0 and PASS lines, or FAIL + 1)
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "utils/child_pipe.h"

static int failures = 0;

static void check(bool ok, const std::string &what)
{
    std::cout << (ok ? "PASS " : "FAIL ") << what << std::endl;
    if (!ok)
        failures++;
}

static std::string slurp(const std::vector<std::string> &argv, int *status,
                         bool *opened)
{
    Child_pipe cp;
    *opened = child_pipe_open(argv, &cp);
    std::string out;
    if (!*opened)
        return out;
    char buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, cp.fp)) > 0)
        out.append(buf, n);
    *status = child_pipe_close(&cp);
    return out;
}

int main()
{
    int st = -1;
    bool opened = false;

    std::vector<std::string> a;
    a.push_back("printf");
    a.push_back("[%s]\\n");
    a.push_back("a b; echo INJECTED $HOME");
    std::string out = slurp(a, &st, &opened);
    check(opened && out == "[a b; echo INJECTED $HOME]\n",
          "an argument with spaces, ';' and '$' arrives as one literal argument");
    check(out.find("INJECTED\n") == std::string::npos,
          "no second command was run");

    std::vector<std::string> b;
    b.push_back("/bin/sh");
    b.push_back("-c");
    b.push_back("echo to-stdout; echo to-stderr >&2; exit 3");
    out = slurp(b, &st, &opened);
    check(opened && out.find("to-stdout") != std::string::npos
                 && out.find("to-stderr") != std::string::npos,
          "stderr is merged into the stream, as 2>&1 did");
    check(WIFEXITED(st) && WEXITSTATUS(st) == 3,
          "the exit status is returned like pclose's");

    std::vector<std::string> c;
    c.push_back("no_such_program_qzx7");
    out = slurp(c, &st, &opened);
    check(!opened, "a missing program fails at open, not as output text");

    std::vector<std::string> d;
    d.push_back("/bin/echo");
    d.push_back("absolute");
    out = slurp(d, &st, &opened);
    check(opened && out == "absolute\n" && WEXITSTATUS(st) == 0,
          "a path with '/' is executed as given");

    std::vector<std::string> e;
    child_pipe_append_flags(&e, "  -T dna\t-Q  dna ");
    check(e.size() == 4 && e[0] == "-T" && e[1] == "dna" && e[2] == "-Q"
                        && e[3] == "dna",
          "literal flag text splits into separate arguments");

    // EOF must arrive: `true` prints nothing and exits; if the parent kept
    // the write end the read below would block for ever.
    std::vector<std::string> f;
    f.push_back("true");
    out = slurp(f, &st, &opened);
    check(opened && out.empty() && WEXITSTATUS(st) == 0,
          "the reader sees EOF when the child exits");

    return failures ? 1 : 0;
}
