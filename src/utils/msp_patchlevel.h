#ifndef MSP_PATCHLEVEL_H
#define MSP_PATCHLEVEL_H
// Local build marker for the mutation_scatter_plot pipeline.
// NOT upstream. Kept on the local/integration branch only, never on the
// branches that back the pull requests. Every entry below is one such branch,
// merged here, and names its pull request against ariloytynoja/pagan2-msa.
#define MSP_PATCHLEVEL \
"  local build: mutation_scatter_plot patch set 2026-10-04, on upstream v1.6\n" \
"    +tunnel-anchors   check_hits_order_conflict() could keep two CROSSING\n" \
"                      exonerate hits (after erasing the earlier hit of a pair\n" \
"                      it never re-compared the survivor with its predecessor),\n" \
"                      so define_tunnel() built a non-monotonic tunnel and\n" \
"                      Tunnel_matrix threw std::out_of_range, aborting the\n" \
"                      batch (4,992 of 200,000 random hit sets on v1.6). The\n" \
"                      pass now repeats until co-linear, and define_tunnel()\n" \
"                      only WIDENS any zigzag left. Replaces the old clamp,\n" \
"                      which raised tunnel starts and could cut anchors out.\n" \
"                      PR ariloytynoja/pagan2-msa#18, open.\n" \
"    +fasttree         the probe no longer inherits stdin (FastTree blocked\n" \
"                      forever on any non-EOF stdin); presence is keyed on the\n" \
"                      shell's 127/126, not on an exit 0 FastTree cannot\n" \
"                      produce (no more 'wasabi2' special case); and\n" \
"                      --fasttree-exec names the executable, passed to the\n" \
"                      shell as one word. PR #15, open.\n" \
"    +fatal-exit-code  four fatal errors exited 0 because Settings::info()\n" \
"                      exits before the exit(1) below it can run. PR #16,\n" \
"                      open (draft).\n" \
"    +per-process-seed srand(time(0)) gave processes started in the same\n" \
"                      second the same temp-file names, so they used and\n" \
"                      deleted each other's files. Seeded from a mix of time\n" \
"                      and pid (not t^pid, which repeats along (t+k,pid+k)).\n" \
"                      PR #17, open.\n" \
"    +exonerate-probe  test_executable() runs per PAIRWISE ALIGNMENT; it now\n" \
"                      looks for exonerate (regular executable file, memoised,\n" \
"                      thread-safe) instead of running it twice. At four\n" \
"                      queries 147 -> 51 execve. PR #19, open.\n" \
"    +exonerate-no-sh  exonerate is posix_spawn()ed with an argument vector\n" \
"                      instead of popen(3)'s /bin/sh per query; close-on-exec\n" \
"                      pipe. /bin/sh execs 100 -> 2 at six queries. PR #20,\n" \
"                      open.\n" \
"    +scoring-model    the read-scoring model at the fixed query-distance is\n" \
"                      built once per run, not per read and candidate node\n" \
"                      (a codon model: ~71 MB, ~10.7 M log() cells). 40-read\n" \
"                      --codons batch 59 s -> 50 s. PR #21, open.\n" \
"    +hit-order        exonerate prints a run's hits in a varying order;\n" \
"                      they are sorted into one, and each target's hits are\n" \
"                      folded per strand pair (highest sum wins), so no print\n" \
"                      order picks the placement. PR #22, open.\n" \
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
