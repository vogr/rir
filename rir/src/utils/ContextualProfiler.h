#pragma once

#include <chrono>
#include <memory>
#include <fstream>

#include "runtime/Context.h"
#include "runtime/Function.h"

namespace rir
{

namespace ContextualProfiler {

void logCall(SEXP call_ast, Function const & called_fun, Context call_context, std::chrono::nanoseconds exec_time_nanos);
void logCompilation(SEXP callee_ast, Context call_context, bool success, std::chrono::nanoseconds cmp_time_nanos);

}; //namespace ContextualProfiler
}; //namespace rir
