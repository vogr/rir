#pragma once


#include "Rinternals.h"

#include "runtime/Context.h"

namespace rir {

class FunctionVersion {
    public:
    size_t const function_id;
    Context const context;

    inline bool operator==(FunctionVersion const & other) const {
        return other.context == context && other.function_id == function_id;
    }


    static size_t getFunctionId(SEXP callee) {
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

};

} // namespace rir


namespace std {

template<>
struct hash<rir::FunctionVersion>
{
    inline std::size_t operator()(rir::FunctionVersion const & f) const {
        return hash_combine(hash_combine(0, f.context), f.function_id);
    }
};

} // namespace std
