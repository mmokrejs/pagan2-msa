// Minimal stub so the test harness doesn't need to compile check_version.cpp
// (which #includes <curl/curl.h> unconditionally on upstream/master -- a
// pre-existing, unrelated build issue documented in
// docs/issues/pagan2_quirks.md's "Upstream cannot be built without the NCBI
// toolkit" section). Only exists for this test binary; not part of the fix.
#include "utils/check_version.h"

namespace ppa {
Check_version::Check_version(float) {}
}
