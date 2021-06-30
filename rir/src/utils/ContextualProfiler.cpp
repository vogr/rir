#include "ContextualProfiler.h"

#include <sstream>
#include <string>

extern "C" {
#include "sys/stat.h"
}

#include "interpreter/instance.h"

namespace rir {
namespace ContextualProfiler {

namespace {

char const * context_profile_env = getenv("CONTEXT_PROFILE");

// Will get flushed on program termination
std::ofstream file_call_stats;
std::ofstream file_compile_stats;

// Initialize the logfile
// (global variable definition are run before main())
bool profiling_enabled = []{
    if (!context_profile_env) {
        return false;
    }
    std::string context_profile_opt = context_profile_env;
    if (context_profile_opt == "false" ) {
        return false;
    } else if (context_profile_opt == "true") {
        std::string const out_dir = "profile";
        mkdir(out_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        std::string const call_stats = out_dir + "/call_stats.csv";
        std::string const compile_stats = out_dir + "/compile_stats.csv";

        file_call_stats.open(call_stats);
        file_call_stats << "FUN_ID,NAME,CALL_CONTEXT,VERSION,EXEC_TIME\n";

        file_compile_stats.open(compile_stats);
        file_compile_stats << "ID,VERSION,SUCCESS,CMP_TIME\n";

        return true;
    } else {
        std::cerr << "Error: correct values for CONTEXT_PROFILE are \"true\" and \"false\"\n";
        exit(1);
    }
}();

size_t getClosureID(SEXP callee) {
    /* Identify a function by the SEXP of its BODY. For nested functions, The
        enclosing CLOSXP changes every time (because the CLOENV also changes):
            f <- function {
                g <- function() { 3 }
                g()
            }
        Here the BODY of g is always the same SEXP, but a new CLOSXP is used
        every time f is called.
    */
    return reinterpret_cast<size_t>(BODY(callee));
}

std::string getFunctionName(SEXP call) {
    static const SEXP double_colons = Rf_install("::");
    static const SEXP triple_colons = Rf_install(":::");
    SEXP const lhs = CAR(call);
    if (TYPEOF(lhs) == SYMSXP) {
        // case 1: function call of the form f(x,y,z)
        return CHAR(PRINTNAME(lhs));
    } else if (TYPEOF(lhs) == LANGSXP &&
               ((CAR(lhs) == double_colons) || (CAR(lhs) == triple_colons))) {
        // case 2: function call of the form pkg::f(x,y,z) or pkg:::f(x,y,z)
        SEXP const fun1 = CAR(lhs);
        SEXP const pkg = CADR(lhs);
        SEXP const fun2 = CADDR(lhs);
        assert(TYPEOF(pkg) == SYMSXP && TYPEOF(fun2) == SYMSXP);
        std::stringstream ss;
        ss << CHAR(PRINTNAME(pkg)) << CHAR(PRINTNAME(fun1))
           << CHAR(PRINTNAME(fun2));
        return ss.str();
    } else {
        // Function is called anonymously
        return "*ANON_FUN*";
    }
}


} // namespace


void logCall(SEXP call_ast, Function const & called_fun, Context call_context, std::chrono::nanoseconds exec_time_nanos)
{
    if (!profiling_enabled) {
        return;
    }
    auto const callee_code = called_fun.body();
    SEXP callee = src_pool_at(globalContext(), callee_code->src);

    size_t id =  getClosureID(callee);
    std::string name = getFunctionName(call_ast);

    // FUN_ID,NAME,CALL_CONTEXT,VERSION,EXEC_TIME
    file_call_stats
        << id << ","
        << name << ","
        << call_context.toI() << ","
        << called_fun.context().toI() << ","
        << exec_time_nanos.count()
        << "\n";
}

void logCompilation(SEXP callee, Context call_context, bool success, std::chrono::nanoseconds cmp_time_nanos)
{
    if (!profiling_enabled) {
        return;
    }

    size_t id = getClosureID(callee);

    // ID,VERSION,SUCCESS,CMP_TIME
    file_compile_stats
        << id << ","
        << call_context.toI() << ","
        << (success ? "TRUE" : "FALSE") << ","
        << cmp_time_nanos.count()
        << "\n";
}

}; // namespace ContextualProfiler
}; // namespace rir
