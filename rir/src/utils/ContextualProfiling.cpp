#include <iostream>
#include <unordered_map>
#include <vector>
#include "../interpreter/call_context.h"
#include "ContextualProfiling.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <fstream>
#include <chrono>
#include <ctime>

namespace rir {

	namespace {
		using namespace std;

		// Some functions are named, some are anonymous:
		// FunLabel is a base class used to get the name of the function
		// in both cases
		class FunLabel {
			public:
				virtual ~FunLabel() = default;
				virtual string get_name() = 0;
				virtual bool is_anon() = 0;
		};

		class FunLabel_anon : public FunLabel {
		private:
			int const id = 0;
		public:
			FunLabel_anon(int id) : id{id} {};
			string get_name() override {
				stringstream ss;
				ss << "*ANON_FUN_" << id << "*";
				return ss.str();
			}
			bool is_anon() override { return true; }
		};

		class FunLabel_named : public FunLabel {
		private:
			string const name;
		public:
			FunLabel_named(string name) : name{move(name)} {};
			string get_name() override {
				return name;
			}
			bool is_anon() override { return false; }
		};

		unordered_map<size_t, unique_ptr<FunLabel>> names; // names: either a string, or an id for anonymous functions
		string del = ","; // delimier

		set<size_t> lIds; // lIds
		unordered_map<size_t, string> lNames; // lNames
		unordered_map<size_t, set<Context>> lContexts; // lContexts
		unordered_map<size_t, int> lContextCallCount; // lContextCallCount
		unordered_map<size_t, int> lContextCompilationTriggerCount; // lContextCompilationTriggerCount

		unordered_map<size_t, set<Context>> lDispatchedFunctions; // lDispatchedFunctions
		unordered_map<string, int> lDispatchedFunctionsCount; // lDispatchedFunctionsCount

		struct FileLogger {
			ofstream myfile;

			FileLogger() {
				// use ISO 8601 date as log name
				time_t timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				stringstream runId_ss;
				runId_ss << put_time( localtime( &timenow ), "%FT%T%z" );
				string runId = runId_ss.str();

				myfile.open("profile/" + runId + ".csv");
				myfile << "SNO,ID,NAME,PIR_COMPILER_TRIGGERED,CONTEXT_CALLED,CALL TypeFlags,CALL Assumptions,DISPATCHED FUNCTIONS\n";
			}

			static size_t getEntryKey(CallContext& call) {
				/* Identify a function by the SEXP of its BODY. For nested functions, The
				   enclosing CLOSXP changes every time (because the CLOENV also changes):
				       f <- function {
				         g <- function() { 3 }
						 g()
					   }
					Here the BODY of g is always the same SEXP, but a new CLOSXP is used
					every time f is called.
				*/
				return reinterpret_cast<size_t>(BODY(call.callee));
			}

			string getFunctionName(CallContext& call) {
				static const SEXP double_colons = Rf_install("::");
				static const SEXP triple_colons = Rf_install(":::");
				size_t const currentKey = getEntryKey(call);
				SEXP const lhs = CAR(call.ast);

				if (names.count(currentKey) == 0 || names[currentKey]->is_anon() ) {
					if (TYPEOF(lhs) == SYMSXP) {
						// case 1: function call of the form f(x,y,z)
						names[currentKey] = make_unique<FunLabel_named>(CHAR(PRINTNAME(lhs)));
					} else if (TYPEOF(lhs) == LANGSXP && ((CAR(lhs) == double_colons) || (CAR(lhs) == triple_colons))) {
						// case 2: function call of the form pkg::f(x,y,z) or pkg:::f(x,y,z)
						SEXP const fun1 = CAR(lhs);
						SEXP const pkg = CADR(lhs);
						SEXP const fun2 = CADDR(lhs);
						assert(TYPEOF(pkg) == SYMSXP && TYPEOF(fun2) == SYMSXP);
						stringstream ss;
						ss << CHAR(PRINTNAME(pkg)) << CHAR(PRINTNAME(fun1)) << CHAR(PRINTNAME(fun2));
						names[currentKey] = make_unique<FunLabel_named>(ss.str());
					}
				}
				if (names.count(currentKey) == 0) {
					// case 3: function call of the form F()(x, y, z)
					// and this anonymous function has not been seen before
					/*
						TODO: Find a way to recover the name of named functions passed anonymously.
						This mechanism would also handle `::` and `:::`. MWE:

							F <- function() { identity }
							for (i in 1:10) { F()(1) }
					*/
					static int anon_fun_counter = 0;
					names[currentKey] = make_unique<FunLabel_anon>(anon_fun_counter);
					anon_fun_counter++;
				}
				return names[currentKey]->get_name();
			}

			string getFunType(int type) {
				switch(type) {
					case SPECIALSXP: return "SPECIALSXP";
					case BUILTINSXP: return "BUILTINSXP";
					case CLOSXP: return "CLOSXP";
					default: return "TYPE_NO_" + type;
				}
			}

			void createEntry(CallContext& call) {
				// TODO CREATE CALL GRAPHS FOR CONTINUING CALL CONTEXTS
			}

			size_t getContextId(
				size_t id,
				Context context
			) {
				return id + context.toI();
			}

			std::string getContextDispatchId(
				size_t id,
				Context contextCaller,
				Context contextDispatched
			) {
				return std::to_string(id) + std::to_string(contextCaller.toI()) + std::to_string(contextDispatched.toI());
			}

			void createRirCallEntry(
				size_t id,
				std::string name,
				Context context,
				bool trigger
			) {
				lIds.insert(id);
				lNames[id] = name;

				if (lContexts.find(id) == lContexts.end()) {
					set<Context> currContext;
					currContext.insert(context);
					lContexts[id] = currContext;
				} else {
					set<Context> mod = lContexts[id];
					mod.insert(context);
					lContexts[id] = mod;
				}

				if (lContextCallCount.find(getContextId(id, context)) == lContextCallCount.end()) {
					lContextCallCount[getContextId(id, context)] = 1;
				} else {
					lContextCallCount[getContextId(id, context)] = lContextCallCount[getContextId(id, context)] + 1;
				}

				if(!trigger) return; // dont increment if no pirCompilationTrigger

				if (lContextCompilationTriggerCount.find(getContextId(id, context)) == lContextCompilationTriggerCount.end()) {
					lContextCompilationTriggerCount[getContextId(id, context)] = 1;
				} else {
					lContextCompilationTriggerCount[getContextId(id, context)] = lContextCompilationTriggerCount[getContextId(id, context)] + 1;
				}


			}

			void addFunctionDispatchInfo(
				size_t id,
				Context context,
				Function f
			) {
				Context funContext = f.context();
				size_t contextId = getContextId(id, context);

				string funContextId = getContextDispatchId(id, context, funContext);

				if (lDispatchedFunctions.find(contextId) == lDispatchedFunctions.end()) {
					set<Context> currFunctions;
					currFunctions.insert(funContext);
					lDispatchedFunctions[contextId] = currFunctions;

					lDispatchedFunctionsCount[funContextId] = 1;
				} else {
					set<Context> mod = lDispatchedFunctions[contextId];
					mod.insert(funContext);
					lDispatchedFunctions[contextId] = mod;
					lDispatchedFunctionsCount[funContextId] = lDispatchedFunctionsCount[funContextId] + 1;
				}
			}

			std::string getContextString(Context& c, bool del) {
				stringstream contextString;
				contextString << "< ";
				TypeAssumption types[] = {
				    TypeAssumption::Arg0IsEager_,
				    TypeAssumption::Arg1IsEager_,
				    TypeAssumption::Arg2IsEager_,
				    TypeAssumption::Arg3IsEager_,
				    TypeAssumption::Arg4IsEager_,
				    TypeAssumption::Arg5IsEager_,
				    TypeAssumption::Arg0IsNonRefl_,
				    TypeAssumption::Arg1IsNonRefl_,
				    TypeAssumption::Arg2IsNonRefl_,
				    TypeAssumption::Arg3IsNonRefl_,
				    TypeAssumption::Arg4IsNonRefl_,
				    TypeAssumption::Arg5IsNonRefl_,
				    TypeAssumption::Arg0IsNotObj_,
				    TypeAssumption::Arg1IsNotObj_,
				    TypeAssumption::Arg2IsNotObj_,
				    TypeAssumption::Arg3IsNotObj_,
				    TypeAssumption::Arg4IsNotObj_,
				    TypeAssumption::Arg5IsNotObj_,
				    TypeAssumption::Arg0IsSimpleInt_,
				    TypeAssumption::Arg1IsSimpleInt_,
				    TypeAssumption::Arg2IsSimpleInt_,
				    TypeAssumption::Arg3IsSimpleInt_,
				    TypeAssumption::Arg4IsSimpleInt_,
				    TypeAssumption::Arg5IsSimpleInt_,
				    TypeAssumption::Arg0IsSimpleReal_,
				    TypeAssumption::Arg1IsSimpleReal_,
				    TypeAssumption::Arg2IsSimpleReal_,
				    TypeAssumption::Arg3IsSimpleReal_,
				    TypeAssumption::Arg4IsSimpleReal_,
				    TypeAssumption::Arg5IsSimpleReal_,
				};

				int iT, jT;
				for(iT = 0; iT < 5; iT++) {
				    std::string currentCheck = "";
				    switch(iT) {
				        case 0:
				            currentCheck = "Eager"; break;
				        case 1:
				            currentCheck = "NonReflective"; break;
				        case 2:
				            currentCheck = "NotObj"; break;
				        case 3:
				            currentCheck = "SimpleInt"; break;
				        default:
				            currentCheck = "SimpleReal";
				    }
				    for(jT = 0; jT < 6; jT++) {
				        if(c.includes(types[iT*6 + jT])) {
							contextString << "Arg" << std::to_string(jT) << currentCheck << " ";
				        }
				    }
				}
				if (del) {
					contextString << " >,< ";
				} else {
					contextString << " >|< ";
				}
				if(c.includes(Assumption::CorrectOrderOfArguments)) {
					contextString << " CorrectOrderOfArguments ";
				}
				if(c.includes(Assumption::NoExplicitlyMissingArgs)) {
					contextString << " NoExplicitlyMissingArgs ";
				}
				if(c.includes(Assumption::NotTooManyArguments)) {
					contextString << " NotTooManyArguments ";
				}
				if(c.includes(Assumption::StaticallyArgmatched)) {
					contextString << " StaticallyArgmatched ";
				}
				contextString << " >";
				return contextString.str();
			}

			void createCodePointEntry(
				int line,
				std::string function,
				std::string name
			) {
				cout << "Line: " << line << ", ";
				cout << "[ " << function << " ]";
				cout << " : " << name << " ";
				cout << "\n";
			}

			~FileLogger() {
				int i=0;
				for (auto ir = lIds.rbegin(); ir != lIds.rend(); ++ir) {
					size_t id = *ir; // function __id
					string name = lNames[id]; // function __name
					// iterate over contexts
					for (auto itr = lContexts[id].begin(); itr != lContexts[id].end(); itr++) {
						// *itr -> Context
						Context currContext = *itr; // current context
						size_t currContextId = getContextId(id, currContext); // current context __id
						string currContextString = getContextString(currContext, true); // current context __string
						int currContextCallCount = lContextCallCount[currContextId]; // current context __count

						stringstream contextsDispatched;

						int currContextPIRTriggerCount = 0;
						if (lContextCompilationTriggerCount.find(currContextId) != lContextCompilationTriggerCount.end()) {
							currContextPIRTriggerCount = lContextCompilationTriggerCount[currContextId];
						}

						// iterate over dispatched functions under this context
						for (auto itr1 = lDispatchedFunctions[currContextId].begin(); itr1 != lDispatchedFunctions[currContextId].end(); itr1++) {
							 // *itr1 -> Context
							 Context currFunctionContext = *itr1; // current function context
							 string currContextString = getContextString(currFunctionContext, false); // current function context __string
							 string funContextId = getContextDispatchId(id, currContext, currFunctionContext); // id to get function context call count for given call id
							 int functionContextCallCount = lDispatchedFunctionsCount[funContextId]; // current function context __call count

							 contextsDispatched << "[" << functionContextCallCount << "]" << currContextString << " ";
						}
						// print row
						myfile
							<< i++ // SNO
							<< del
							<< id // id
							<< del
							<< name // name
							<< del
							<< currContextPIRTriggerCount // call context PIR trigger count
							<< del
							<< currContextCallCount // call context count
							<< del
							<< currContextString // call context
							<< del
							<< contextsDispatched.str() // functions dispatched under this context
							<< "\n";
					}
				}
				myfile.close();
			}

			public:
		};

	} // namespace

std::unique_ptr<FileLogger> fileLogger = std::unique_ptr<FileLogger>(
	getenv("CONTEXT_LOGS") ? new FileLogger : nullptr);

void ContextualProfiling::createCallEntry(
		CallContext& call) {
	if(fileLogger) {
		fileLogger->createEntry(call);
	}
}

void ContextualProfiling::recordCodePoint(
		int line,
		std::string function,
		std::string name
		) {
	if(fileLogger) {
		fileLogger->createCodePointEntry(line, function, name);
	}
}

std::string ContextualProfiling::getFunctionName(CallContext& cc) {
	if(fileLogger) {
		return fileLogger->getFunctionName(cc);
	} else {
		return "ERR <ContextualProfiler>";
	}
}

size_t ContextualProfiling::getEntryKey(CallContext& cc) {
	if(fileLogger) {
		return fileLogger->getEntryKey(cc);
	} else {
		return 0;
	}
}

void ContextualProfiling::addRirCallData(
	size_t id,
	std::string name,
	Context context,
	bool trigger
) {
	if(fileLogger) {
		return fileLogger->createRirCallEntry(id, name, context, trigger);
	}
}

void ContextualProfiling::addFunctionDispatchInfo(
	size_t id,
	Context contextCaller,
	Function f
) {
	if(fileLogger) {
		return fileLogger->addFunctionDispatchInfo(id, contextCaller, f);
	}
}


} // namespace rir
