#!/usr/bin/env python3
"""--independent-queries: killing the parent pagan2 kills its query process.

Usage:
    python3 tests/test_independent_queries_die_with_parent.py PAGAN2

With --independent-queries each query is aligned in a child forked from the
parent pagan2. A caller that gives up on a run usually kills only the pid it
started -- a timeout that SIGKILLs the parent, a batch system's kill, an
operator -- and then removes the run's output and --temp-folder. A query
child that outlives its parent goes on computing at full speed for nobody,
into a directory that is being deleted.

THE DATA (deterministic, standard library only, written under TMPDIR)
  Eight codon sequences (ATG + 999 sense codons) on a balanced tree and four
  alike queries, run with --no-anchors and --no-preselection so that a query
  takes about a second: every node of the tree is aligned against it by full
  dynamic programming (peak memory ~370 MB). The run uses a --temp-folder
  whose path is unique to this test; a forked child keeps its parent's
  command line, so every pagan2 process of the run is found by that path in
  /proc/*/cmdline.

WHAT IS CHECKED
  The parent is started, the first query's child is timed from its start to
  its manifest row (T), and as soon as the second query's child exists the
  PARENT pid alone is killed with SIGKILL. Within W = min(T/3, 0.5 s) no
  process of the run may be left (a zombie counts as gone). The second query
  is like the first, so a child that ignored its parent's death would still
  have about 2/3 of T to run when W is over; one killed with its parent is
  gone in milliseconds. W follows the host's measured speed, so the verdict
  does not depend on it.
  PREMISE: W >= 0.2 s (T >= 0.6 s); on a host so fast that a query is over
  sooner, nothing would be told apart, and the test fails saying so.

Linux only (it reads /proc); elsewhere it prints SKIP and exits 0.
Exit 0 and a PASS line, or exit 1 naming what failed.
"""
import os
import random
import shutil
import signal
import subprocess
import sys
import tempfile
import time

STOPS = {"TAA", "TAG", "TGA"}
SENSE = [a + b + c for a in "ACGT" for b in "ACGT" for c in "ACGT"
         if a + b + c not in STOPS]
N_CODONS = 1000
N_TAXA = 8
N_QUERIES = 4
# The window in which the killed parent's child must be gone: a third of
# the measured first query, at most MAX_WINDOW, at least MIN_WINDOW.
WINDOW_FRACTION = 1.0 / 3.0
MAX_WINDOW_SECONDS = 0.5
MIN_WINDOW_SECONDS = 0.2
STARTUP_LIMIT_SECONDS = 300.0


def _mutate(rng, cod, n):
    cod = list(cod)
    for i in rng.sample(range(1, len(cod)), n):
        cod[i] = rng.choice([c for c in SENSE if c != cod[i]])
    return cod


def _tree(names):
    if len(names) == 1:
        return names[0] + ":0.05"
    half = len(names) // 2
    return "(%s,%s):0.02" % (_tree(names[:half]), _tree(names[half:]))


def make_data(workdir):
    rng = random.Random(20261014)
    anc = ["ATG"] + [rng.choice(SENSE) for _ in range(N_CODONS - 1)]
    names = ["T%d" % i for i in range(1, N_TAXA + 1)]
    taxa = dict((t, _mutate(rng, anc, N_CODONS // 12)) for t in names)
    with open(os.path.join(workdir, "ref.aln.fas"), "w") as fh:
        for t in names:
            fh.write(">%s\n%s\n" % (t, "".join(taxa[t])))
    with open(os.path.join(workdir, "ref.tre"), "w") as fh:
        fh.write(_tree(names)[:-len(":0.02")] + ";\n")
    with open(os.path.join(workdir, "queries.fas"), "w") as fh:
        for k in range(1, N_QUERIES + 1):
            q = _mutate(rng, taxa[names[k % N_TAXA]], N_CODONS // 20)
            fh.write(">query_%d\n%s\n" % (k, "".join(q)))


def _children(pid):
    """Direct children of *pid*, from /proc (empty once it is gone)."""
    kids = set()
    try:
        for tid in os.listdir("/proc/%d/task" % pid):
            with open("/proc/%d/task/%s/children" % (pid, tid)) as fh:
                kids.update(int(x) for x in fh.read().split())
    except (FileNotFoundError, ProcessLookupError):
        pass
    return kids


def _alive(pid):
    """True for a process that exists and is not a zombie."""
    try:
        with open("/proc/%d/stat" % pid) as fh:
            stat = fh.read()
    except (FileNotFoundError, ProcessLookupError):
        return False
    return stat[stat.rindex(")") + 2] != "Z"


def _processes_of_run(marker):
    """Live pids whose command line holds *marker* (this run's temp folder)."""
    found = []
    needle = marker.encode()
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open("/proc/%s/cmdline" % entry, "rb") as fh:
                if needle in fh.read() and _alive(int(entry)):
                    found.append(int(entry))
        except (FileNotFoundError, ProcessLookupError, PermissionError):
            continue
    return found


def _manifest_rows(path):
    try:
        with open(path) as fh:
            return [l for l in fh.read().split("\n")[1:] if l]
    except FileNotFoundError:
        return []


def main(argv):
    if len(argv) != 2:
        sys.stderr.write(__doc__)
        return 2
    if not sys.platform.startswith("linux"):
        print("SKIP: reads /proc, Linux only")
        return 0
    binary = os.path.abspath(argv[1])
    workdir = tempfile.mkdtemp(prefix="pagan2_die_with_parent.")
    make_data(workdir)
    marker = os.path.join(workdir, "tmp_%d_%d" % (os.getpid(), int(time.time())))
    os.makedirs(marker)
    manifest = os.path.join(workdir, "out.queries.tsv")
    cmd = [binary, "-a", os.path.join(workdir, "ref.aln.fas"),
           "-t", os.path.join(workdir, "ref.tre"),
           "-q", os.path.join(workdir, "queries.fas"),
           "-o", os.path.join(workdir, "out"), "--codons",
           "--no-anchors", "--no-preselection", "--independent-queries",
           "--temp-folder", marker]
    failures = []
    with open(os.path.join(workdir, "log"), "w") as log:
        parent = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                                  stdin=subprocess.DEVNULL, cwd=workdir)
    try:
        deadline = time.monotonic() + STARTUP_LIMIT_SECONDS
        first_child = None
        first_started = None
        while first_child is None and time.monotonic() < deadline:
            if parent.poll() is not None:
                break
            kids = _children(parent.pid)
            if kids:
                first_child = min(kids)
                first_started = time.monotonic()
            else:
                time.sleep(0.01)
        query_seconds = None
        while first_child is not None and time.monotonic() < deadline:
            if parent.poll() is not None:
                break
            if _manifest_rows(manifest):
                query_seconds = time.monotonic() - first_started
                break
            time.sleep(0.01)
        second_child = None
        while query_seconds is not None and time.monotonic() < deadline:
            if parent.poll() is not None:
                break
            kids = [k for k in _children(parent.pid) if k != first_child and _alive(k)]
            if kids:
                second_child = kids[0]
                break
            time.sleep(0.005)
        if second_child is None:
            failures.append("never saw the second query's process start (exit "
                            "status %r; see %s/log)" % (parent.poll(), workdir))
        else:
            window = min(query_seconds * WINDOW_FRACTION, MAX_WINDOW_SECONDS)
            os.kill(parent.pid, signal.SIGKILL)
            parent.wait()
            killed_at = time.monotonic()
            left = _processes_of_run(marker)
            while left and time.monotonic() - killed_at < window:
                time.sleep(0.01)
                left = _processes_of_run(marker)
            if window < MIN_WINDOW_SECONDS:
                failures.append("PREMISE: a query took only %.2f s, so the window "
                                "(%.2f s) is too short to tell a child killed with "
                                "its parent from one that finished its query; "
                                "nothing was tested" % (query_seconds, window))
            elif left:
                failures.append("%d pagan2 process(es) of the run still running "
                                "%.2f s after the parent was killed (a query takes "
                                "%.2f s here): %s"
                                % (len(left), window, query_seconds, left))
    finally:
        if parent.poll() is None:
            parent.kill()
            parent.wait()
        # Leave nothing burning CPU: every process still carrying this run's
        # unique temp folder in its command line is one of ours.
        for pid in _processes_of_run(marker):
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass

    if failures:
        for f in failures:
            print("FAIL: %s" % f)
        print("(data and log kept in %s)" % workdir)
        return 1
    print("PASS: the parent killed while query 2 ran (query 1 took %.2f s); "
          "no process of the run left within %.2f s"
          % (query_seconds, min(query_seconds * WINDOW_FRACTION, MAX_WINDOW_SECONDS)))
    shutil.rmtree(workdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
