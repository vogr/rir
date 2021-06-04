#ifndef CONTEXTUAL_PROFILE_H
#define CONTEXTUAL_PROFILE_H
#include "../interpreter/call_context.h"

namespace rir {

class ContextualProfiling {
  public:
    static void createCallEntry(
      CallContext& // logs [ name, callType ]
      );
    static void recordCodePoint(
      int,
      std::string,
      std::string
    );
    // static void addContextData(
    //   CallContext&,
    //   std::string
    // );
    static std::string getFunctionName(
      CallContext&
    );
    static size_t getEntryKey(
      CallContext&
    );
    static void addRirCallData(
      size_t,
      std::string,
      Context
    );
    static void addFunctionDispatchInfo(
      size_t,
      Context,
      Function
    );
    static void countSuccessfulCompilation(
      SEXP,
      Context const&
    );
    static void countFailedCompilation(
      SEXP,
      Context const&
    );
};

} // namespace rir

#endif
