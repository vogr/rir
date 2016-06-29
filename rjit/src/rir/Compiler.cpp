#include "Compiler.h"

#include "BC.h"
#include "CodeStream.h"

#include "../RIntlns.h"
#include "../RList.h"
#include "../Sexp.h"
#include "../Symbols.h"

#include "OldInterpreter.h"
#include "Optimizer.h"
#include "Pool.h"

#include "CodeVerifier.h"

namespace rjit {
namespace rir {

namespace {

fun_idx_t compilePromise(FunctionHandle& f, SEXP exp);
void compileExpression(FunctionHandle& f, CodeStream& cs, SEXP exp);

// function application
void compileCall(FunctionHandle& parent, CodeStream& cs, SEXP ast, SEXP fun,
                 SEXP args) {
    // application has the form:
    // LHS ( ARGS )

    // LHS can either be an identifier or an expression
    Match(fun) {
        Case(SYMSXP) { cs << BC::getfun(fun); }
        Else({
            compileExpression(parent, cs, fun);
            cs << BC::check_function();
        });
    }

    // Process arguments:
    // Arguments can be optionally named
    std::vector<fun_idx_t> callArgs;
    std::vector<SEXP> names;

    for (auto arg = RList(args).begin(); arg != RList::end(); ++arg) {
        // (1) Arguments are wrapped as Promises:
        //     create a new Code object for the promise
        size_t prom = compilePromise(parent, *arg);
        callArgs.push_back(prom);

        // (2) remember if the argument had a name associated
        names.push_back(arg.hasTag() ? arg.tag() : R_NilValue);
    }
    assert(callArgs.size() < MAX_NUM_ARGS);

    cs << BC::call(callArgs, names);

    cs.addAst(ast);
}

// Lookup
void compileGetvar(CodeStream& cs, SEXP name) { cs << BC::getvar(name); }

// Constant
void compileConst(CodeStream& cs, SEXP constant) {
    SET_NAMED(constant, 2);
    cs << BC::push(constant);
}

void compileExpression(FunctionHandle& function, CodeStream& cs, SEXP exp) {
    // Dispatch on the current type of AST node
    Match(exp) {
        // Function application
        Case(LANGSXP, fun, args) { compileCall(function, cs, exp, fun, args); }
        // Variable lookup
        Case(SYMSXP) { compileGetvar(cs, exp); }
        // Constant
        Else(compileConst(cs, exp));
    }
}

void compileFormals(CodeStream& cs, SEXP formals) {
    size_t narg = 0;
    for (auto arg = RList(formals).begin(); arg != RList::end(); ++arg) {
        // TODO support default args
        assert(*arg == R_MissingArg);

        SEXP name = arg.tag();
        assert(name && name != R_NilValue);

        // TODO
        assert(name != symbol::Ellipsis);

        narg++;
    }
}

fun_idx_t compilePromise(FunctionHandle& function, SEXP exp) {
    CodeStream cs(function, exp);
    compileExpression(function, cs, exp);
    cs << BC::ret();
    return cs.finalize();
}
}

SEXP Compiler::finalize() {
    // Rprintf("****************************************************\n");
    // Rprintf("Compiling function\n");
    FunctionHandle function = FunctionHandle::create();
    CodeStream cs(function, exp);
    if (formals)
        compileFormals(cs, formals);
    compileExpression(function, cs, exp);
    cs << BC::ret();
    cs.finalize();

    CodeVerifier::vefifyFunctionLayout(function.store, globalContext());

    FunctionHandle opt = Optimizer::optimize(function);
    CodeVerifier::vefifyFunctionLayout(opt.store, globalContext());

    return opt.store;
}
}
}
