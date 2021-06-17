#pragma once

#include "runtime/Context.h"
namespace rir {
namespace CompilationStrategy {

    bool compileFlag(size_t id, Context call_ctxt);
    void markAsCompiled(size_t id, Context call_ctxt);
    void markAsCompiled(SEXP, Context call_ctxt);

}
}
