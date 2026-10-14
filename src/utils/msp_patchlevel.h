#ifndef MSP_PATCHLEVEL_H
#define MSP_PATCHLEVEL_H
// Local build marker for the mutation_scatter_plot pipeline.
// NOT upstream. Kept on the local/integration branch only, never on the
// branches that back the pull requests. Every entry below is one such branch,
// merged here, and names its pull request against ariloytynoja/pagan2-msa,
// or says that it has none.
#define MSP_PATCHLEVEL \
"  local build: mutation_scatter_plot patch set 2026-10-14, on upstream v1.6\n" \
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
"    +indep-queries    --independent-queries: the reference is set up once,\n" \
"                      then each query is placed and aligned ALONE in a child\n" \
"                      forked from that state, to <outfile>.<n>.fas, and\n" \
"                      <outfile>.queries.tsv lists every outcome; a crashed\n" \
"                      child loses its query only. A query's result no longer\n" \
"                      depends on the file it is in: 291 reads in 40- and\n" \
"                      150-read files, run twice, 1116 of 1116 placements\n" \
"                      byte-identical to the read run alone. 0.73-0.78 s per\n" \
"                      read, against 1.30 s per one-read process and 1.16 /\n" \
"                      2.35 s per read in a 40 / 145-read batch. Branch\n" \
"                      feature/independent-queries; NOT submitted upstream.\n" \
"    +sites-index      correct_sites_index() summed a parent's index shift\n" \
"                      only at the child sites the parent points at, missing\n" \
"                      new columns beside a node's own insertions -- so a\n" \
"                      query placed onto a node made for an earlier query\n" \
"                      shifted that clade and cut its 3' end off, reference\n" \
"                      rows included. 19+37-read batches: 41 of 55 rows short\n" \
"                      by 125-459 nt -> 55 of 55 whole; single-query runs\n" \
"                      unchanged. Branch fix/correct-sites-index-shift; NOT\n" \
"                      submitted upstream.\n" \
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
