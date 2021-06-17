#include "CompilationStrategy.h"

#include "FunctionVersion.h"

#include <unordered_set>

namespace rir {
namespace CompilationStrategy {

    static std::unordered_set<FunctionVersion> compiled_versions;

    static bool compileEachVersionOnlyOnce(size_t entry_key, Context call_ctxt) {
        return compiled_versions.count({entry_key, call_ctxt});
    }


    void markAsCompiled(size_t id, Context call_ctxt) {
        compiled_versions.insert({id, call_ctxt});
    }

    void markAsCompiled(SEXP callee, Context call_ctxt) {
        return markAsCompiled(FunctionVersion::getFunctionId(callee), call_ctxt);
    }

    bool compileFlag(size_t id, Context call_ctxt) {
        if (getenv("SKIP_ALL_COMPILATION")) {
            return false;
        }
        if (getenv("COMPILE_ONLY_ONCE")) {
            return compileEachVersionOnlyOnce(id, call_ctxt);
        }
        return true;
    }
}
}
