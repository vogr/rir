#ifndef PIR_REFERENCE_COUNT_H
#define PIR_REFERENCE_COUNT_H

#include "../pir/pir_impl.h"
#include "generic_static_analysis.h"

namespace rir {
namespace pir {

struct AUses {
    std::unordered_map<Instruction*, size_t> uses;
    AbstractResult mergeExit(const AUses& other) { return merge(other); }
    AbstractResult merge(const AUses& other) {
        AbstractResult res;
        for (auto u : other.uses) {
            if (!uses.count(u.first)) {
                uses.emplace(u);
                res.update();
            } else if (uses.at(u.first) < u.second) {
                // used in both branches, use count is the maximum
                uses.at(u.first) = u.second;
                res.update();
            }
        }
        return res;
    }
    void print(std::ostream&, bool) const {
        // TODO
    }
};

class StaticReferenceCount : public StaticAnalysis<AUses> {
  public:
    StaticReferenceCount(ClosureVersion* cls, LogStream& log)
        : StaticAnalysis("StaticReferenceCountAnalysis", cls, cls, log) {
        Visitor::run(code->entry, [&](Instruction* i) {
            if (Phi::Cast(i)) {
                i->eachArg([&](Value* v) {
                    if (auto a = Instruction::Cast(v))
                        alias[i].insert(a);
                });
            }
        });
    }

  protected:
    std::unordered_map<Instruction*, SmallSet<Instruction*>> alias;

    AbstractResult apply(AUses& state, Instruction* i) const override {
        AbstractResult res;
        if (Phi::Cast(i))
            return res;

        if (i->needsReferenceCount()) {
            // This value was used only once in a loop, we can thus
            // reset the count on redefinition.
            if (state.uses.count(i) && state.uses.at(i) == 1) {
                state.uses[i] = 0;
                res.update();
            }
        }

        auto count = [&](Instruction* i) {
            auto& use = state.uses[i];
            if (use < 2) {
                use++;
                res.update();
            }
        };

        auto apply = [&](Value* v) {
            if (auto j = Instruction::Cast(v)) {
                if (!j->needsReferenceCount())
                    return;
                if (alias.count(j))
                    for (auto a : alias.at(j))
                        count(a);
                else
                    count(j);
            }
        };

        i->eachArg(apply);
        return res;
    }
};
}
}

#endif
