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
    static void addFunctionDispatchInfo(
      size_t,
      Context,
      Function const&
    );
    static void countCompilation(
      SEXP callee,
      Context assumptions,
      bool success,
      double cmp_time_ms
    );
    static std::string extractFunctionName(SEXP call);
};

} // namespace rir

#endif
