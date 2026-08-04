#ifndef MSP_PATCHLEVEL_H
#define MSP_PATCHLEVEL_H
// Local build marker for the mutation_scatter_plot pipeline.
// NOT upstream. Kept on the local/integration branch only, never on the
// branches that back the pull requests.
#define MSP_PATCHLEVEL \
"  local build: mutation_scatter_plot patch set 2026-08-04\n" \
"    +anchor-file      external anchors from a file\n" \
"                      PR ariloytynoja/pagan2-msa#5\n" \
"    +build-no-ncbi    builds without the NCBI toolkit / libcurl\n" \
"                      PR ariloytynoja/pagan2-msa#5\n" \
"    +parsimony-state  fixes std::out_of_range abort on an invalid parsimony\n" \
"                      state (lost the whole batch); PR ariloytynoja/pagan2-msa#6\n" \
"  NOT an upstream release; see docs/issues/pagan2_quirks.md\n"
#endif
