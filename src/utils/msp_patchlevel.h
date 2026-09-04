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
"  local build: mutation_scatter_plot patch set 2026-09-04, on upstream v1.6\n" \
"    +tunnel-monotonic Find_anchors::define_tunnel() clamps non-monotonic\n" \
"                      tunnel bounds instead of letting Tunnel_matrix throw\n" \
"                      std::out_of_range and SIGABRT the whole batch.\n" \
"                      NOT submitted upstream; v1.6 does not carry it.\n" \
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
