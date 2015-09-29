#include "ir.h"

using namespace llvm;

namespace rjit {
namespace ir {


// TODO change CBR to match compare and branch
Type Instruction::match(BasicBlock::iterator & i) {
    llvm::Instruction * ins = i;
    ++i; // move to next instruction
    if (isa<CallInst>(ins)) {
        Type t = Intrinsic::getIRType(ins);
        if (t != Type::unknown) {
            return t;
        }
    } else if (isa<ReturnInst>(ins)) {
        return Type::ret;
    } else if (isa<BranchInst>(ins)) {
        BranchInst * b = cast<BranchInst>(ins);
        return b->isConditional() ? Type::cbr : Type::br;
    } else if (isa<ICmpInst>(ins)) {
        return Type::cmp;
    }
    return Type::unknown;
}





char const * const Intrinsic::MD_NAME = "r_ir_type";

} // namespace ir

} // namespace rjit