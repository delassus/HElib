/* Copyright (C) 2020 IBM Corp.
 * This program is Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. See accompanying LICENSE file.
 */
#include <iostream>

#include <helib/helib.h>
#include <helib/ArgMap.h>
#include <helib/Matrix.h>
#include <helib/partialMatch.h>
#include <helib/timing.h>

#include <psiio.h>

#if (defined(__unix__) || defined(__unix) || defined(unix))
#include <sys/time.h>
#include <sys/resource.h>
#endif

// Struct to hold command line arguments
struct CmdLineOpts
{
  std::string pkFilePath;
  std::string databaseFilePath;
  std::string queryFilePath;
  std::string outFilePath;
  bool isColumn = false;
  long nthreads = 1;
  long offset = 0;
};

int main(int argc, char* argv[])
{
  // PSI STUFF
  // 1. Read in context and pk file
  // 2. Python scripts to gen data (might use utils)
  // 3. Read in numbers (all numbers in a single row) and (all numbers in a
  // single column)
  // 4. Create the query etc. Similar to TestPartial

  CmdLineOpts cmdLineOpts;

  // clang-format off
  helib::ArgMap()
    .required()
    .positional()
    .arg("<pkFile>", cmdLineOpts.pkFilePath, "Public Key file.", nullptr)
    .arg("<databaseFile>", cmdLineOpts.databaseFilePath,
      "Database file.", nullptr)
    .arg("<queryFile>", cmdLineOpts.queryFilePath, "Query file.", nullptr)
    .arg("<outFile>", cmdLineOpts.outFilePath, "Output file.", nullptr)
    .named()
    .arg("-n", cmdLineOpts.nthreads, "Number of threads.")
    .optional()
    .named()
    .arg("--offset", cmdLineOpts.offset, "Offset in bytes when writing to file.")
    .toggle()
    .arg("--column", cmdLineOpts.isColumn,
      "Flag to signify input is in column format.", nullptr)
    .parse(argc, argv);
  // clang-format on
  std::ofstream cout(cmdLineOpts.outFilePath);
  if (cmdLineOpts.nthreads < 1) {
    std::cerr << "Number of threads must be a postive integer. Setting n = 1."
              << std::endl;
    cmdLineOpts.nthreads = 1;
  }

  NTL::SetNumThreads(cmdLineOpts.nthreads);

  HELIB_NTIMER_START(readKey);
  // Load Context and PubKey
  std::shared_ptr<helib::Context> contextp;
  std::unique_ptr<helib::PubKey> pkp;
  std::tie(contextp, pkp) =
      loadContextAndKey<helib::PubKey>(cmdLineOpts.pkFilePath);
  HELIB_NTIMER_STOP(readKey);

  HELIB_NTIMER_START(readDatabase);
  // Read in database
  helib::Database<helib::Ctxt> database =
      readDbFromFile(cmdLineOpts.databaseFilePath, contextp, *pkp);
  HELIB_NTIMER_STOP(readDatabase);

  HELIB_NTIMER_START(readQuery);
  // Read in the query data
  helib::Matrix<helib::Ctxt> queryData =
      readQueryFromFile(cmdLineOpts.queryFilePath, *pkp);
  HELIB_NTIMER_STOP(readQuery);

  HELIB_NTIMER_START(buildQuery);
  const helib::QueryExpr& a = helib::makeQueryExpr(0);
  const helib::QueryExpr& b = helib::makeQueryExpr(1);
  const helib::QueryExpr& c = helib::makeQueryExpr(2);

  helib::QueryBuilder qbDoubleVars(a || !a);
  helib::QueryBuilder qbNotofOr1(!(a || b || c));
  helib::QueryBuilder qbNotofOr2(!(a || b) && c);
  helib::QueryBuilder qbDoubleNot1(!!a);
  helib::QueryBuilder qbDoubleNot2(b || !!a);
  helib::QueryBuilder qbNotofAnd1(!(a && b && c));
  helib::QueryBuilder qbNotofAnd2(!((a || b) && (b || c)));

  qbDoubleVars.removeOr();
  std::cout << qbDoubleVars.getQueryString() << "\n";
  qbNotofOr1.removeOr();
  std::cout << qbNotofOr1.getQueryString() << "\n";
  qbNotofOr2.removeOr();
  std::cout << qbNotofOr2.getQueryString() << "\n";
  qbDoubleNot1.removeOr();
  std::cout << qbDoubleNot1.getQueryString() << "\n";
  qbDoubleNot2.removeOr();
  std::cout << qbDoubleNot2.getQueryString() << "\n";
  qbNotofAnd1.removeOr();
  std::cout << qbNotofAnd2.getQueryString() << "\n";
  qbNotofAnd2.removeOr();
  std::cout << qbNotofAnd2.getQueryString() << "\n";

  helib::QueryType queryDoubleVars = qbDoubleVars.build(database.columns());
  helib::QueryType queryNotofOr1 = qbNotofOr1.build(database.columns());
  helib::QueryType queryNotofOr2 = qbNotofOr2.build(database.columns());
  helib::QueryType queryDoubleNot1 = qbDoubleNot1.build(database.columns());
  helib::QueryType queryDoubleNot2 = qbDoubleNot2.build(database.columns());
  helib::QueryType queryNotofAnd1 = qbNotofAnd1.build(database.columns());
  helib::QueryType queryNotofAnd2 = qbNotofAnd2.build(database.columns());

  auto clean = [](auto& x) { x.cleanUp(); };

  HELIB_NTIMER_START(lookupDoubleVars);
  auto doubleVars = database.contains(qbDoubleVars, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupDoubleVars);  

  HELIB_NTIMER_START(lookupNotofOr1);
  auto NotofOr1 = database.contains(qbNotofOr1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofOr1);

  HELIB_NTIMER_START(lookupNotofOr2);
  auto NotofOr2 = database.contains(qbNotofOr2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofOr2);

  HELIB_NTIMER_START(lookupdoubleNot1);
  auto doubleNot1 = database.contains(qbDoubleNot1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupdoubleNot1);

  HELIB_NTIMER_START(lookupdoubleNot2);
  auto doubleNot2 = database.contains(qbDoubleNot2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupdoubleNot2);

  HELIB_NTIMER_START(lookupNotofAnd1);
  auto NotofAnd1 = database.contains(qbNotofAnd1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofAnd1);

  HELIB_NTIMER_START(lookupNotofAnd2);
  auto NotofAnd2 = database.contains(qbNotofAnd2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofAnd2);

  HELIB_NTIMER_START(writeResults);
  // Write results to file
  writeResultsToFile(cmdLineOpts.outFilePath + "_doubleVars",
                     doubleVars,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_NotofOr1",
                     NotofOr1,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_NotofOr2",
                     NotofOr2,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_doubleNot1",
                     doubleNot1,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_doubleNot2",
                     doubleNot2,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_NotofAnd1",
                     NotofAnd1,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_NotofAnd2",
                     NotofAnd2,cmdLineOpts.offset);
  
  helib::QueryBuilder qb(a);
  helib::QueryBuilder qbAnd(a && b);
  helib::QueryBuilder qbOr(a || b);
  helib::QueryBuilder qbExpand(a || (b && c));
  
  helib::QueryType query = qb.build(database.columns());
  helib::QueryType queryAnd = qbAnd.build(database.columns());
  helib::QueryType queryOr = qbOr.build(database.columns());
  helib::QueryType queryExpand = qbExpand.build(database.columns());
  HELIB_NTIMER_STOP(buildQuery);

  HELIB_NTIMER_START(lookupSame);
  auto clean = [](auto& x) { x.cleanUp(); };
  auto match = database.contains(query, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupSame);
  HELIB_NTIMER_START(lookupAnd);
  auto matchAnd = database.contains(queryAnd, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupAnd);
  HELIB_NTIMER_START(lookupOr);
  auto matchOr = database.contains(queryOr, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupOr);
  HELIB_NTIMER_START(lookupExpand);
  auto matchExpand = database.contains(queryExpand, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupExpand);

  HELIB_NTIMER_START(writeResults);
  // Write results to file
  writeResultsToFile(cmdLineOpts.outFilePath, match, cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_and",
                     matchAnd,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_or",
                     matchOr,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_expand",
                     matchExpand,
                     cmdLineOpts.offset);
  HELIB_NTIMER_STOP(writeResults);

  std::ofstream timers("times.log");
  if (timers.is_open()) {
    helib::printAllTimers(timers);
  }

#if (defined(__unix__) || defined(__unix) || defined(unix))
  std::ofstream usage("usage.log");
  if (usage.is_open()) {
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
    usage << "\n  rusage.ru_utime=" << rusage.ru_utime.tv_sec << '\n';
    usage << "  rusage.ru_stime=" << rusage.ru_utime.tv_sec << '\n';
    usage << "  rusage.ru_maxrss=" << rusage.ru_maxrss << '\n';
    usage << "  rusage.ru_minflt=" << rusage.ru_minflt << '\n';
    usage << "  rusage.ru_majflt=" << rusage.ru_majflt << '\n';
    usage << "  rusage.ru_inblock=" << rusage.ru_inblock << '\n';
    usage << "  rusage.ru_oublock=" << rusage.ru_majflt << '\n';
    usage << "  rusage.ru_nvcsw=" << rusage.ru_nvcsw << '\n';
    usage << "  rusage.ru_nivcsw=" << rusage.ru_minflt << std::endl;
  }
#endif

  return 0;
}
