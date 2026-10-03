#ifndef MSP_PATCHLEVEL_H
#define MSP_PATCHLEVEL_H
// Local build marker for the mutation_scatter_plot pipeline.
// NOT upstream. Kept on the local/integration branch only, never on the
// branches that back the pull requests.
//
// 2026-09-04: upstream merged PRs #5-#13 and released v1.6, so the nine
// patches this banner used to enumerate are now UPSTREAM CODE and are listed
// here no longer -- claiming them as local would misreport what this binary
// is. What remains is the one fix that was never submitted.
#define MSP_PATCHLEVEL \
"  local build: mutation_scatter_plot patch set 2026-10-04, on upstream v1.6\n" \
"    +tunnel-monotonic Find_anchors::define_tunnel() clamps non-monotonic\n" \
"                      tunnel bounds instead of letting Tunnel_matrix throw\n" \
"                      std::out_of_range and SIGABRT the whole batch.\n" \
"                      NOT submitted upstream; v1.6 does not carry it.\n" \
"    +fasttree         three fixes that each alone made FastTree unusable:\n" \
"                      the probe no longer inherits stdin (FastTree blocked\n" \
"                      forever on any non-EOF stdin); presence is keyed on the\n" \
"                      shell's 127/126 rather than on an exit 0 FastTree\n" \
"                      cannot produce, so detection no longer needs the host\n" \
"                      to be named wasabi2; and --fasttree-exec names the\n" \
"                      executable, since upstream ships FastTree/FastTreeMP/\n" \
"                      FastTreeUPGMA and none of them is called \"fasttree\".\n" \
"                      Submitted as PR ariloytynoja/pagan2-msa#15, still open.\n" \
"    +fatal-exit-code  four fatal errors exited 0 because Settings::info()\n" \
"                      exits before the exit(1) below it can run.\n" \
"                      Submitted as PR ariloytynoja/pagan2-msa#16, still open.\n" \
"    +per-process-seed srand(time(0)) gave two processes started in the same\n" \
"                      SECOND the same seed, so both walked the same sequence\n" \
"                      of candidate temp names (q<r>.fas/t<r>.fas) and each\n" \
"                      overwrote and deleted the other's -- silently, with the\n" \
"                      alignment simply wrong or empty. Seeded with the pid\n" \
"                      mixed in, which is what the existing retry loops assume.\n" \
"                      Submitted as PR ariloytynoja/pagan2-msa#17, still open.\n" \
"    +exonerate-probe  test_executable() is called from INSIDE the alignment\n" \
"                      path, so a probe that runs exonerate twice was paid per\n" \
"                      PAIRWISE ALIGNMENT. Replaced by a memoised PATH walk:\n" \
"                      at four queries 147 -> 51 execve, 71 -> 23 exonerate\n" \
"                      (the 23 are the real anchoring runs), out.fas\n" \
"                      byte-identical.\n" \
"    +exonerate-no-sh  exonerate ran through popen(3), i.e. a /bin/sh per\n" \
"                      QUERY; it is now posix_spawn()ed with an argument\n" \
"                      vector (utils/child_pipe.h). At six queries /bin/sh\n" \
"                      execs 100 -> 2, out.fas and out.nhx_tree byte-identical.\n" \
"    +scoring-model    the model a read is scored with at the fixed\n" \
"                      query-distance was rebuilt per read and per candidate\n" \
"                      node (a codon model: ~71 MB, ~10.7 M log() cells); it\n" \
"                      is built once per run. 40-read --codons guide-tree\n" \
"                      batch: 59 s -> 50 s, +70 MB peak; the same rows as the\n" \
"                      unpatched build. NOT submitted upstream.\n" \
"    +hit-order        exonerate prints a run's hits in an order that\n" \
"                      changes run to run, and the readers were order-\n" \
"                      sensitive, so one batch was placed differently from\n" \
"                      run to run (v1.6: 4 different out.fas in 6 runs of a\n" \
"                      40-read batch). Hits are sorted into one order before\n" \
"                      any is folded: 6 of 6 runs byte-identical.\n" \
"                      NOT submitted upstream.\n" \
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
