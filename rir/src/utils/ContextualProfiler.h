#pragma once

#include <memory>
#include <fstream>

namespace rir
{

class AbstractContextualProfiler {
    public:
    virtual ~AbstractContextualProfiler() = default;
};

class ConcreteContextualProfiler : public AbstractContextualProfiler {
    private:
    std::ofstream file_call_stats;
    std::ofstream file_compile_stats;

    public:
    ConcreteContextualProfiler();
    ~ConcreteContextualProfiler() override = default;
};

class NullContextualProfiler : public AbstractContextualProfiler {
    public:
    NullContextualProfiler();
    ~NullContextualProfiler() override = default;
};


namespace ContextualProfiler {
    extern std::unique_ptr<AbstractContextualProfiler> profiler;
};

};
