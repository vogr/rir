#pragma once

#include "runtime/Context.h"

namespace rir {

class FunctionVersion {
    public:
    size_t const function_id;
    Context const context;

    inline bool operator==(FunctionVersion const & other) const {
        return other.context == context && other.function_id == function_id;
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
