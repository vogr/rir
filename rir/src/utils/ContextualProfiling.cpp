#include <iostream>
#include <unordered_map>
#include <vector>
#include "../interpreter/call_context.h"
#include "ContextualProfiling.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <fstream>
#include <functional>
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


		class ContextDispatchData {
			public:
			int call_count_in_ctxt = 0;
			int successful_compilation_count = 0;
			int failed_compilation_count = 0;
			// Count the number of time the version for the context C
			// has been called in this context in
			//     version_called_count[C]
			unordered_map<Context, int> version_called_count;
		};

		class Entry {
			public:
			int total_call_count = 0;
			// per context call and dispatch data
			unordered_map<Context, ContextDispatchData> dispatch_data;
		};

		// Map from a function (identified by its body) to the data about the
		// different contexts it has been called in
		unordered_map<size_t, Entry> entries;

		struct FileLogger {
			ofstream myfile;

			FileLogger() {
				// use ISO 8601 date as log name
				time_t timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				stringstream runId_ss;
				runId_ss << put_time( localtime( &timenow ), "%FT%T%z" );
				string runId = runId_ss.str();

				myfile.open("profile/" + runId + ".csv");
				myfile << "SNO,ID,NAME,CMP_SUCCESS,CMP_FAIL,CONTEXT_CALLED,CALL TypeFlags,CALL Assumptions,DISPATCHED FUNCTIONS\n";
			}

			static size_t getEntryKey(SEXP callee) {
				/* Identify a function by the SEXP of its BODY. For nested functions, The
				   enclosing CLOSXP changes every time (because the CLOENV also changes):
				       f <- function {
				         g <- function() { 3 }
						 g()
					   }
					Here the BODY of g is always the same SEXP, but a new CLOSXP is used
					every time f is called.
				*/
				return reinterpret_cast<size_t>(BODY(callee));
			}

			void registerFunctionName(CallContext const& call) {
				static const SEXP double_colons = Rf_install("::");
				static const SEXP triple_colons = Rf_install(":::");
				size_t const currentKey = getEntryKey(call.callee);
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
			}

			string getFunType(int type) {
				switch(type) {
					case SPECIALSXP: return "SPECIALSXP";
					case BUILTINSXP: return "BUILTINSXP";
					case CLOSXP: return "CLOSXP";
					default:
						stringstream ss;
						ss << "TYPE_NO_" << type;
						return ss.str();
					}
			}

			void createEntry(CallContext const& call) {
				registerFunctionName(call);

				auto fun_id = getEntryKey(call.callee);
				// create or get entry
				auto & entry = entries[fun_id];
				entry.total_call_count++;

				// /!\ do not rely on call.givenContext here, it will
				// be inferred just before the compilation happens

				// TODO CREATE CALL GRAPHS FOR CONTINUING CALL CONTEXTS
			}

			void addFunctionDispatchInfo(
				size_t id,
				Context call_context,
				Function const & f
			) {
				Context version_context = f.context();

				// find entry for this function
				// entry must have been previously created by a call to createEntry
				auto & entry = entries.at(id);

				// create or get call context data
				auto & ctxt_data = entry.dispatch_data[call_context];
				ctxt_data.call_count_in_ctxt++;

				// count one call in the context callContextId  to version compiled for funContextId
				ctxt_data.version_called_count[version_context]++;
			}

			// For the two functions below: function entry must have been previously
			// created by createEntry, context entry may not exist yet
			void countSuccessfulCompilation(
				SEXP callee,
				Context call_ctxt
			) {
				size_t entry_key = getEntryKey(callee);

				auto & entry = entries.at(entry_key);
				auto & dispatch_data = entry.dispatch_data[call_ctxt];

				dispatch_data.successful_compilation_count++;
			}

			void countFailedCompilation(
				SEXP callee,
				Context call_ctxt
			) {
				size_t entry_key = getEntryKey(callee);

				auto & entry = entries.at(entry_key);
				auto & dispatch_data = entry.dispatch_data[call_ctxt];

				dispatch_data.failed_compilation_count++;
			}


			std::string getContextString(Context c, bool del) {
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
				for (auto ir = entries.begin(); ir != entries.end(); ++ir) {
					auto fun_id = ir->first;
					auto & entry = ir->second;
					string name = names.at(fun_id)->get_name(); // function name

					// iterate over contexts
					for (auto itr = entry.dispatch_data.begin(); itr != entry.dispatch_data.end(); itr++) {
						// *itr -> Context
						auto call_ctxt = itr->first; // current context __id
						auto & dispatch_data = itr->second; // current context

						string currContextString = getContextString(call_ctxt, true); // current context __string

						stringstream contextsDispatched;


						// iterate over dispatched functions under this context
						for (auto itr1 = dispatch_data.version_called_count.begin(); itr1 != dispatch_data.version_called_count.end(); itr1++) {
							// *itr1 -> Context
							Context version_context = itr1->first; // current function context
							int functionContextCallCount = itr1->second; // current function context __call count
							string currContextString = getContextString(version_context, false); // current function context __string

							contextsDispatched << "[" << functionContextCallCount << "]" << currContextString << " ";
						}
						// print row
						myfile
							<< i++ // SNO
							<< del
							<< fun_id // id
							<< del
							<< name // name
							<< del
							<< dispatch_data.successful_compilation_count // number of successful compilations in this context
							<< del
							<< dispatch_data.failed_compilation_count // number of failed compilations in this context
							<< del
							<< dispatch_data.call_count_in_ctxt // call context count
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

auto fileLogger = getenv("CONTEXT_LOGS") ? std::make_unique<FileLogger>() : nullptr;

void ContextualProfiling::createCallEntry(
		CallContext const& call) {
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

size_t ContextualProfiling::getEntryKey(CallContext const& cc) {
	if(fileLogger) {
		return fileLogger->getEntryKey(cc.callee);
	} else {
		return 0;
	}
}

void ContextualProfiling::addFunctionDispatchInfo(
	size_t id,
	Context contextCaller,
	Function const &f
) {
	if(fileLogger) {
		return fileLogger->addFunctionDispatchInfo(id, contextCaller, f);
	}
}


void ContextualProfiling::countSuccessfulCompilation(
	SEXP callee,
	Context assumptions
) {
	if (fileLogger) {
		fileLogger->countSuccessfulCompilation(callee, assumptions);
	}
}

void ContextualProfiling::countFailedCompilation(
	SEXP callee,
	Context assumptions
) {
	if (fileLogger) {
		fileLogger->countFailedCompilation(callee, assumptions);
	}
}


} // namespace rir
