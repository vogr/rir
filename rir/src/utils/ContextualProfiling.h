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
    static void addContextData(
      CallContext&,
      std::string
    );
};

} // namespace rir

#endif
