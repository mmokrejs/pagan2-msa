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
"  local build: mutation_scatter_plot patch set 2026-09-08, on upstream v1.6\n" \
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
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
