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

		unordered_map<size_t, set<string>> callContexts; // callContexts
		unordered_map<size_t, int> callCount; // callCount
		unordered_map<string, int> contextCallCount; // callCount
		unordered_map<size_t, string> basicData; // function id, type
		unordered_map<size_t, unique_ptr<FunLabel>> names; // names: either a string, or an id for anonymous functions
		set<size_t> entries; // id's of all the methods
		string del = ",";

		struct FileLogger {
			ofstream myfile;

			FileLogger() {
				// use ISO 8601 date as log name
				time_t timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				stringstream runId_ss;
				runId_ss << put_time( localtime( &timenow ), "%FT%T%z" );
				string runId = runId_ss.str();

				myfile.open("profile/" + runId + ".csv");
				myfile << "Sno,id,name,type,callCount,callContexts,PIRCompiled\n";
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

			// void createEntry(CallContext& call, bool tryFastBuiltInCall) {
			// 	string callName = getFunctionName(call);
			// 	cout << "Logging Function:: "
			// 		 << callName << ", "
			// 		 << getFunType(TYPEOF(call.callee));

			// 	if(TYPEOF(call.callee) == BUILTINSXP) {
			// 		cout << ", " << (tryFastBuiltInCall ? "FastBuiltInCallSucceeded" : "FastBuiltInCallFailed");
			// 	}

			// 	cout << "\n";
			// }

			void addContextData(CallContext& call, string data) {
				size_t currentKey = getEntryKey(call);
				// current call context has not been recorded before
				if (callContexts.find(currentKey) == callContexts.end()) {
					set<string> currContext;
					currContext.insert(data);
					callContexts[currentKey] = currContext;
					contextCallCount[data] = 1;
				} else {
					// insert data into the set
					set<string> mod = callContexts[currentKey];
					mod.insert(data);
					callContexts[currentKey] = mod;
					contextCallCount[data] = contextCallCount[data] + 1;
				}

			}

			void createEntry(CallContext& call) {
				size_t currentKey = getEntryKey(call);
				string funName = getFunctionName(call);
				// encounter first call
				if (!(std::count(entries.begin(), entries.end(), currentKey))) {
					entries.insert(currentKey);
					callCount[currentKey] = 1;
					// create basic data entry
					basicData[currentKey] =
						to_string(currentKey) + del +
						getFunType(TYPEOF(call.callee));
				} else {
					callCount[currentKey] = ++callCount[currentKey];
				}
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
				cout << "\n\n\n";
				int i = 1;
				for (auto ir = entries.rbegin(); ir != entries.rend(); ++ir) {
					// *ir -> size_t
					if(callContexts.find(*ir) != callContexts.end()) {
						for (auto itr = callContexts[*ir].begin(); itr != callContexts[*ir].end(); itr++) {
							myfile
								<< i++ // SNO
								<< del
								<< names[*ir]->get_name() // NAME
								<< del
								<< basicData[*ir] // ID, TYPE
								<< del
								<< contextCallCount[*itr] // CONTEXT_CALL_COUNT
								<< del
								<< *itr // i'th context, RIR Compiled
								<< "\n";
						}
					} else {
						myfile
							<< i++ // SNO
							<< del
							<< names[*ir]->get_name() // NAME
							<< del
							<< basicData[*ir] // ID, TYPE
							<< del
							<< callCount[*ir] // CALL_COUNT
							<< del
							// NO context data
							<< del // i'th context
							<< "FALSE" // RIR Compiled
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

void ContextualProfiling::addContextData(
		CallContext& call,
		string data
		) {
	if(fileLogger) {
		fileLogger->addContextData(call, data);
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


} // namespace rir
