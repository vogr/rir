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
#include <sys/stat.h>

#include "FunctionVersion.h"

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
			// Count the number of time the version for the context C
			// has been called in this context in
			//     version_called_count[C]
			unordered_map<Context, int> version_called_count;
		};

		class CallEntry {
			public:
			int total_call_count = 0;
			// per context call and dispatch data
			unordered_map<Context, ContextDispatchData> dispatch_data;
		};

		// Map from a function (identified by its body) to the data about the
		// different contexts it has been called in
		unordered_map<size_t, CallEntry> call_entries;


		class CompilationData {
			public:
			bool success;
			double cmp_time_ms;
		};

		using CompilationEntry =
			unordered_map<Context, std::vector<CompilationData>>;

		std::unordered_map<size_t, CompilationEntry> compilation_entries;

		class FileLogger {
			private:
			ofstream file_call_stats;
			ofstream file_compile_stats;

			public:
			FileLogger() {
				// use ISO 8601 date as log name
				time_t timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				stringstream runId_ss;
				runId_ss << put_time( localtime( &timenow ), "%FT%T%z" );
				string runId = runId_ss.str();

				string out_dir = "profile/" + runId;
				mkdir(out_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

				string call_stats = out_dir + "/call_stats.csv";
				string compile_stats = out_dir + "/compile_stats.csv";

				file_call_stats.open(call_stats);
				file_call_stats << "ID,NAME,CALL_CONTEXT,VERSION,CALL_COUNT\n";

				file_compile_stats.open(compile_stats);
				file_compile_stats << "ID,NAME,VERSION,ID_CMP,SUCCESS,CMP_TIME\n";
			}

			void registerFunctionName(CallContext const& call) {
				size_t const currentKey = FunctionVersion::getFunctionId(call.callee);

				if (names.count(currentKey) == 0 || names[currentKey]->is_anon() ) {
                                    std::string name = ContextualProfiling::
                                        extractFunctionName(call.ast);
                                    if (name.length() > 0) {
                                        names[currentKey] =
                                            make_unique<FunLabel_named>(name);
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

				auto fun_id = FunctionVersion::getFunctionId(call.callee);
				// create or get entry
				auto & entry = call_entries[fun_id];
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
				auto & entry = call_entries.at(id);

				// create or get call context data
				auto & ctxt_data = entry.dispatch_data[call_context];
				ctxt_data.call_count_in_ctxt++;

				// count one call in the context callContextId  to version compiled for funContextId
				ctxt_data.version_called_count[version_context]++;
			}

			// For the two functions below: function entry must have been previously
			// created by createEntry, context entry may not exist yet
			void countCompilation(
				SEXP callee,
				Context call_ctxt,
				bool success,
				double cmp_time_ms
			) {
				size_t entry_key = FunctionVersion::getFunctionId(callee);

				CompilationData d {success, cmp_time_ms};

				auto & cmp_entry = compilation_entries[entry_key];
				cmp_entry[call_ctxt].push_back(d);
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
				for (auto const & ir : call_entries) {
					auto fun_id = ir.first;
					auto & entry = ir.second;
					string name = names.at(fun_id)->get_name(); // function name

					// iterate over contexts
					for (auto const & itr : entry.dispatch_data) {
						// *itr -> Context
						auto call_ctxt = itr.first; // current context __id
						auto & dispatch_data = itr.second; // current context
						string callContextString = call_ctxt.getShortStringRepr(); // current context __string

						// iterate over dispatched functions under this context
						for (auto const & itr1 : dispatch_data.version_called_count) {
							// *itr1 -> Context
							Context version_context = itr1.first; // current function context
							int functionContextCallCount = itr1.second; // current function context __call count
							string versionContextString = version_context.getShortStringRepr(); // current function context __string
							file_call_stats
								<< fun_id // id
								<< del
								<< name // name
								<< del
								<< callContextString // call context
								<< del
								<< versionContextString
								<< del
								<< functionContextCallCount // number of time this version is called in this context
								<< "\n";
						}
						// print row

					}
				}
				file_call_stats.close();

				for (auto const & it_e : compilation_entries) {
					auto fun_id = it_e.first;
					auto & cmp_entry = it_e.second;
					string fun_name = names.at(fun_id)->get_name(); // function name

					for (auto & it_d : cmp_entry) {
						auto version = it_d.first;
						auto & data = it_d.second;

						int cmp_id = 0;
						for (auto & d : data) {
							file_compile_stats
								<< fun_id
								<< del
								<< fun_name
								<< del
								<< version.getShortStringRepr()
								<< del
								<< cmp_id
								<< del
								<< d.success
								<< del
								<< d.cmp_time_ms
								<< "\n";
							cmp_id++;
						}
					}
				}
				file_compile_stats.close();
			}
		};

	} // namespace

auto fileLogger = getenv("CONTEXT_LOGS") ? std::make_unique<FileLogger>() : nullptr;

std::string ContextualProfiling::extractFunctionName(SEXP call) {
    static const SEXP double_colons = Rf_install("::");
    static const SEXP triple_colons = Rf_install(":::");
    SEXP const lhs = CAR(call);
    if (TYPEOF(lhs) == SYMSXP) {
        // case 1: function call of the form f(x,y,z)
        return CHAR(PRINTNAME(lhs));
    } else if (TYPEOF(lhs) == LANGSXP &&
               ((CAR(lhs) == double_colons) || (CAR(lhs) == triple_colons))) {
        // case 2: function call of the form pkg::f(x,y,z) or pkg:::f(x,y,z)
        SEXP const fun1 = CAR(lhs);
        SEXP const pkg = CADR(lhs);
        SEXP const fun2 = CADDR(lhs);
        assert(TYPEOF(pkg) == SYMSXP && TYPEOF(fun2) == SYMSXP);
        stringstream ss;
        ss << CHAR(PRINTNAME(pkg)) << CHAR(PRINTNAME(fun1))
           << CHAR(PRINTNAME(fun2));
        return ss.str();
    }
    return "";
}

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

void ContextualProfiling::addFunctionDispatchInfo(
	size_t id,
	Context contextCaller,
	Function const &f
) {
	if(fileLogger) {
		return fileLogger->addFunctionDispatchInfo(id, contextCaller, f);
	}
}


void ContextualProfiling::countCompilation(
	SEXP callee,
	Context assumptions,
	bool success,
	double cmp_time_ms
) {
	if (fileLogger) {
		fileLogger->countCompilation(callee, assumptions, success, cmp_time_ms);
	}
}


} // namespace rir
