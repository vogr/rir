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
		// bool error = false;
		// string errorMessage = "";

		unordered_map<size_t, set<string>> callContexts; // callContexts
		unordered_map<size_t, int> callCount; // callCount
		unordered_map<string, int> contextCallCount; // callCount
		unordered_map<size_t, string> basicData; // function name, type
		set<size_t> entries; // id's of all the methods
		string del = ",";

		struct FileLogger {
			ofstream myfile;

			FileLogger() {
				auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				string runId = (string)ctime(&timenow);

				myfile.open("profile/"+runId+".csv");
				myfile << "Sno,id,name,type,callCount,callContexts,PIRCompiled\n";

			}

			string getFunctionName(CallContext& call) {
				SEXP lhs = CAR(call.ast);
				SEXP name = R_NilValue;
				if (TYPEOF(lhs) == SYMSXP)
					name = lhs;
				string nameStr = CHAR(PRINTNAME(name));
				return nameStr;
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
				size_t currentKey = (size_t) call.callee;
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
				size_t currentKey = (size_t) call.callee;
				string funName = getFunctionName(call);
				// encounter first call
				if (!(std::count(entries.begin(), entries.end(), currentKey))) {
					entries.insert(currentKey);
					callCount[currentKey] = 1;
					// create basic data entry
					basicData[currentKey] =
						to_string(currentKey) + del +
						funName + del +
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
								<< basicData[*ir] // ID, NAME, TYPE
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
							<< basicData[*ir] // ID, NAME, TYPE
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
