#ifndef CONTEXTUAL_PROFILE_H
#define CONTEXTUAL_PROFILE_H
#include "../interpreter/call_context.h"

namespace rir {

class ContextualProfiling {
  public:
    static void createCallEntry(
      CallContext const& // logs [ name, callType ]
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
    static size_t getEntryKey(
      CallContext const&
    );
    static void addFunctionDispatchInfo(
      size_t,
      Context,
      Function const&
    );
    static void countSuccessfulCompilation(
      SEXP,
      Context
    );
    static void countFailedCompilation(
      SEXP,
      Context
    );
};

} // namespace rir

#endif
