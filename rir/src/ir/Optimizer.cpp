#include "ir/Optimizer.h"
#include "optimizer/cleanup.h"
#include "optimizer/load_elimination.h"
#include "optimizer/stupid_inline.h"

namespace rir {

void Optimizer::optimize(CodeEditor& code, int steam) {
    BCCleanup cleanup(code);
    LoadElimination elim(code);
    for (int i = 0; i < 3; ++i) {
        cleanup.run();
        elim.run();
        code.commit();
    }
}

void Optimizer::inliner(CodeEditor& code) {
    StupidInliner inl(code);
    inl.run();
    code.commit();
}

SEXP Optimizer::reoptimizeFunction(SEXP s) {
    CodeEditor code(s);

    for (int i = 0; i < 2; ++i) {
        Optimizer::inliner(code);
        Optimizer::optimize(code, 1);
    }

    FunctionHandle opt = code.finalize();
    // TODO: just a hack to make sure optimization is not triggered again
    opt.function->invocationCount = 2001;
    CodeVerifier::vefifyFunctionLayout(opt.store, globalContext());
    return opt.store;
}
}
