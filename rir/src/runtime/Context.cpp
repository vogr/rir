#include "Context.h"
#include "R/Serialize.h"
#include "compiler/compiler.h"
#include "compiler/pir/closure.h"
#include "compiler/pir/closure_version.h"

namespace rir {

Context Context::deserialize(SEXP refTable, R_inpstream_t inp) {
    Context as;
    InBytes(inp, &as, sizeof(Context));
    return as;
}

void Context::serialize(SEXP refTable, R_outpstream_t out) const {
    OutBytes(out, this, sizeof(Context));
}

std::ostream& operator<<(std::ostream& out, Assumption a) {
    switch (a) {
    case Assumption::NoExplicitlyMissingArgs:
        out << "!ExpMi";
        break;
    case Assumption::CorrectOrderOfArguments:
        out << "CorrOrd";
        break;
    case Assumption::StaticallyArgmatched:
        out << "Argmatch";
        break;
    case Assumption::NotTooManyArguments:
        out << "!TMany";
        break;
    }
    return out;
};

std::ostream& operator<<(std::ostream& out, TypeAssumption a) {
    switch (a) {
#define TYPE_ASSUMPTIONS(Type, Msg)                                            \
    case TypeAssumption::Arg0Is##Type##_:                                      \
        out << Msg << "0";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg1Is##Type##_:                                      \
        out << Msg << "1";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg2Is##Type##_:                                      \
        out << Msg << "2";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg3Is##Type##_:                                      \
        out << Msg << "3";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg4Is##Type##_:                                      \
        out << Msg << "4";                                                     \
        break;                                                                 \
    case TypeAssumption::Arg5Is##Type##_:                                      \
        out << Msg << "5";                                                     \
        break;                                                                 \

        TYPE_ASSUMPTIONS(Eager, "Eager");
        TYPE_ASSUMPTIONS(NotObj, "!Obj");
        TYPE_ASSUMPTIONS(SimpleInt, "SimpleInt");
        TYPE_ASSUMPTIONS(SimpleReal, "SimpleReal");
        TYPE_ASSUMPTIONS(NonRefl, "NonRefl");
    }
    return out;
};

std::ostream& operator<<(std::ostream& out, const Context& a) {
    for (auto i = a.flags.begin(); i != a.flags.end(); ++i) {
        out << *i;
        if (i + 1 != a.flags.end())
            out << ",";
    }
    if (!a.typeFlags.empty())
        out << ";";
    for (auto i = a.typeFlags.begin(); i != a.typeFlags.end(); ++i) {
        out << *i;
        if (i + 1 != a.typeFlags.end())
            out << ",";
    }
    if (a.missing > 0)
        out << " miss: " << (int)a.missing;
    return out;
}

std::string Context::getShortStringRepr() const {
    std::stringstream contextString;
    contextString << "<";
    static TypeAssumption types[5][6] = {{
                                             TypeAssumption::Arg0IsEager_,
                                             TypeAssumption::Arg1IsEager_,
                                             TypeAssumption::Arg2IsEager_,
                                             TypeAssumption::Arg3IsEager_,
                                             TypeAssumption::Arg4IsEager_,
                                             TypeAssumption::Arg5IsEager_,
                                         },
                                         {
                                             TypeAssumption::Arg0IsNonRefl_,
                                             TypeAssumption::Arg1IsNonRefl_,
                                             TypeAssumption::Arg2IsNonRefl_,
                                             TypeAssumption::Arg3IsNonRefl_,
                                             TypeAssumption::Arg4IsNonRefl_,
                                             TypeAssumption::Arg5IsNonRefl_,
                                         },
                                         {
                                             TypeAssumption::Arg0IsNotObj_,
                                             TypeAssumption::Arg1IsNotObj_,
                                             TypeAssumption::Arg2IsNotObj_,
                                             TypeAssumption::Arg3IsNotObj_,
                                             TypeAssumption::Arg4IsNotObj_,
                                             TypeAssumption::Arg5IsNotObj_,
                                         },
                                         {
                                             TypeAssumption::Arg0IsSimpleInt_,
                                             TypeAssumption::Arg1IsSimpleInt_,
                                             TypeAssumption::Arg2IsSimpleInt_,
                                             TypeAssumption::Arg3IsSimpleInt_,
                                             TypeAssumption::Arg4IsSimpleInt_,
                                             TypeAssumption::Arg5IsSimpleInt_,
                                         },
                                         {
                                             TypeAssumption::Arg0IsSimpleReal_,
                                             TypeAssumption::Arg1IsSimpleReal_,
                                             TypeAssumption::Arg2IsSimpleReal_,
                                             TypeAssumption::Arg3IsSimpleReal_,
                                             TypeAssumption::Arg4IsSimpleReal_,
                                             TypeAssumption::Arg5IsSimpleReal_,
                                         }};

    // assumptions:
    //    Eager
    //    non reflective
    //    non object
    //    simple Integer
    //    simple Real
    std::vector<char> letters = {'E', 'r', 'o', 'I', 'R'};
    for (int i_arg = 0; i_arg < 6; i_arg++) {
        std::vector<char> arg_str;
        for (int i_assum = 0; i_assum < 5; i_assum++) {
            if (this->includes(types[i_assum][i_arg])) {
                arg_str.emplace_back(letters.at(i_assum));
            }
        }
        if (!arg_str.empty()) {
            contextString << i_arg << ":";
            for (auto c : arg_str) {
                contextString << c;
            }
            contextString << " ";
        }
    }

    contextString << "|";

    std::vector<std::string> assum_strings;
    if (this->includes(Assumption::CorrectOrderOfArguments)) {
        assum_strings.emplace_back("O");
    }

    if (this->includes(Assumption::NoExplicitlyMissingArgs)) {
        assum_strings.emplace_back("mi");
    }

    if (this->includes(Assumption::NotTooManyArguments)) {
        assum_strings.emplace_back("ma");
    }

    if (this->includes(Assumption::StaticallyArgmatched)) {
        assum_strings.emplace_back("Stat");
    }

    if (!assum_strings.empty()) {
        contextString << " ";
    }

    for (size_t i = 0; i < assum_strings.size(); i++) {
        contextString << assum_strings[i];
        if (i < assum_strings.size() - 1) {
            contextString << "-";
        }
    }

    contextString << ">";
    return contextString.str();
}

constexpr std::array<TypeAssumption, Context::NUM_TYPED_ARGS>
    Context::EagerContext;
constexpr std::array<TypeAssumption, Context::NUM_TYPED_ARGS>
    Context::NotObjContext;
constexpr std::array<TypeAssumption, Context::NUM_TYPED_ARGS>
    Context::SimpleIntContext;
constexpr std::array<TypeAssumption, Context::NUM_TYPED_ARGS>
    Context::SimpleRealContext;
constexpr std::array<TypeAssumption, Context::NUM_TYPED_ARGS>
    Context::NonReflContext;

void Context::setSpecializationLevel(int level) {
    static Flags preserve =
        pir::Compiler::minimalContext | Assumption::StaticallyArgmatched;

    switch (level) {
    // All Specialization Disabled
    case 0:
        flags = flags & preserve;
        typeFlags.reset();
        missing = 0;
        break;

    // Eager Args
    case 1:
        flags = flags & preserve;
        typeFlags = typeFlags & allEagerArgsFlags();
        missing = 0;
        break;

    // + not Reflective
    case 2:
        flags.reset(Assumption::NoExplicitlyMissingArgs);
        typeFlags = typeFlags & allEagerArgsFlags();
        missing = 0;
        break;

    // + not Object
    case 3:
        flags.reset(Assumption::NoExplicitlyMissingArgs);
        typeFlags = typeFlags & (allEagerArgsFlags() | allNonObjArgsFlags());
        missing = 0;
        break;

    // + arg Types
    case 4:
        flags.reset(Assumption::NoExplicitlyMissingArgs);
        missing = 0;
        break;

    // + nargs
    case 5:
        flags.reset(Assumption::NoExplicitlyMissingArgs);
        break;

    // + no explicitly missing
    // all
    default:
        break;
    }
}

bool Context::isImproving(Function* f) const {
    return isImproving(f->context(), f->signature().hasDotsFormals,
                       f->signature().hasDefaultArgs);
}
bool Context::isImproving(pir::ClosureVersion* f) const {
    return isImproving(f->context(), f->owner()->formals().hasDots(),
                       f->owner()->formals().hasDefaultArgs());
}

bool Context::isImproving(const Context& other, bool hasDotsFormals,
                          bool hasDefaultArgs) const {
    assert(smaller(other));

    if (other == *this)
        return false;
    auto normalized = *this;

    if (!hasDotsFormals)
        normalized.remove(Assumption::StaticallyArgmatched);
    if (!hasDefaultArgs)
        normalized.remove(Assumption::NoExplicitlyMissingArgs);

    // These don't pay of that much...
    normalized.clearObjFlags();

    if (hasDotsFormals || hasDefaultArgs) {
        if (normalized.numMissing() != other.numMissing())
            return true;
    } else {
        normalized.numMissing(other.numMissing());
    }

    normalized = normalized | other;
    return normalized != other;
}

} // namespace rir
