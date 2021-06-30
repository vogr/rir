#include "ContextualProfiler.h"

#include <string>

extern "C" {
#include "sys/stat.h"
}

namespace rir {

namespace {

std::unique_ptr<AbstractContextualProfiler> get_contextual_profiler() {
    std::string const context_profile_env = getenv("CONTEXT_PROFILE");
    if (context_profile_env == "true") {
        return std::make_unique<ConcreteContextualProfiler>();
    } else {
        return std::make_unique<NullContextualProfiler>();
    }
}

};

std::unique_ptr<AbstractContextualProfiler> ContextualProfiler::profiler = get_contextual_profiler();



ConcreteContextualProfiler::ConcreteContextualProfiler() {
    std::string out_dir = "profile";
    mkdir(out_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    std::string call_stats = out_dir + "/call_stats.csv";
    std::string compile_stats = out_dir + "/compile_stats.csv";

    file_call_stats.open(call_stats);
    file_call_stats << "CALL_ID,FUN_ID,NAME,CALL_CONTEXT,VERSION,CALL_COUNT\n";

    file_compile_stats.open(compile_stats);
    file_compile_stats << "ID,NAME,VERSION,ID_CMP,SUCCESS,CMP_TIME\n";
}

}; // namespace rir
