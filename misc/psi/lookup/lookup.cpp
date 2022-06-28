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

  helib::QueryBuilder qba(a);
  helib::QueryBuilder qbNota(!a);
  helib::QueryBuilder qbb(b);
  helib::QueryBuilder qbc(c);
  helib::QueryBuilder qbAnd(a && b);
  helib::QueryBuilder qbOr(a || b);
  helib::QueryBuilder qbExpand1(a || (b && c));
  helib::QueryBuilder qbExpand2((a || b) && c);
  helib::QueryBuilder qbExpand3((a && b) || (a && b));
  helib::QueryBuilder qbComplex1(a || (!b && c));
  helib::QueryBuilder qbComplex2(((!b && c) || (!a)));
  helib::QueryBuilder qbComplex3((a && !b));
  helib::QueryBuilder qbdoublevars(a || !a);
  helib::QueryBuilder qbNotofOr1(!(a||b||c));
  helib::QueryBuilder qbNotofOr2(!(a||b) && c);
  helib::QueryBuilder qbdoubleNot1(!!a);
  helib::QueryBuilder qbdoubleNot2(b || !!a);
  helib::QueryBuilder qbNotofAnd1(!(a && b && c));
  helib::QueryBuilder qbNotofAnd2(!((a || b)&&(b || c)));

  /**std::cout << "query a:\n";
  helib::Query_t querya = qba.build(database.columns());
  std::cout << "query not a:\n";
  helib::Query_t querynota = qbNota.build(database.columns());
  std::cout << "query b:\n";
  helib::Query_t queryb = qbb.build(database.columns());
  std::cout << "query c:\n";
  helib::Query_t queryc = qbc.build(database.columns());
  std::cout << "query a and b:\n";
  helib::Query_t queryAnd = qbAnd.build(database.columns());
  std::cout << "query a or b:\n";
  helib::Query_t queryOr = qbOr.build(database.columns());
  std::cout << "query a or (b and c):\n";
  helib::Query_t queryExpand1 = qbExpand1.build(database.columns());
  std::cout << "query (a or b) and c:\n";
  helib::Query_t queryExpand2 = qbExpand2.build(database.columns());
  std::cout << "query (a and b) or (a and b):\n";
  helib::Query_t queryExpand3 = qbExpand3.build(database.columns());
  std::cout << "query a or (!b and c):\n";
  helib::Query_t queryComplex1 = qbComplex1.build(database.columns());
  std::cout << "query (!b and c) or !a:\n";
  helib::Query_t queryComplex2 = qbComplex2.build(database.columns());
  std::cout << "query a and !b:\n";
  helib::Query_t queryComplex3 = qbComplex3.build(database.columns());**/
  std:: cout << "query nota or a:\n";
  helib::Query_t querydoublevars = qbdoublevars.build(database.columns());
  std:: cout << "query not(a or b or c):\n";
  helib::Query_t queryNotofOr1 = qbNotofOr1.build(database.columns());
  std:: cout << "query not(a or b) and c:\n";
  helib::Query_t queryNotofOr2 = qbNotofOr2.build(database.columns());
  std:: cout << "query not not a:\n";
  helib::Query_t querydoubleNot1 = qbdoubleNot1.build(database.columns());
  std:: cout << "query b or not not a:\n";
  helib::Query_t querydoubleNot2 = qbdoubleNot2.build(database.columns());
  std:: cout << "query not (a and b and c):\n";
  helib::Query_t queryNotofAnd1 = qbNotofAnd1.build(database.columns());
  std:: cout << "query not ((a or b) and (b or c)):\n";
  //bug with duplicates in CNF still get added to offset
  helib::Query_t queryNotofAnd2 = qbNotofAnd2.build(database.columns());
  HELIB_NTIMER_STOP(buildQuery);

  
  auto clean = [](auto& x) { x.cleanUp(); };

  /*HELIB_NTIMER_START(lookupSamea);
  auto matcha = database.contains(querya, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupSamea);

  HELIB_NTIMER_START(lookupNot);
  auto matchnota = database.contains(querynota, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNot);

  HELIB_NTIMER_START(lookupSameb);
  auto matchb = database.contains(queryb, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupSameb);

  HELIB_NTIMER_START(lookupSamec);
  auto matchc = database.contains(queryc, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupSamec);

  HELIB_NTIMER_START(lookupAnd);
  auto matchAnd = database.contains(queryAnd, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupAnd);

  HELIB_NTIMER_START(lookupOr);
  auto matchOr = database.contains(queryOr, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupOr);

  HELIB_NTIMER_START(lookupExpand1);
  auto matchExpand1 = database.contains(queryExpand1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupExpand1);

  HELIB_NTIMER_START(lookupExpand2);
  auto matchExpand2 = database.contains(queryExpand2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupExpand2);

  HELIB_NTIMER_START(lookupExpand3);
  auto matchExpand3 = database.contains(queryExpand2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupExpand3);

  HELIB_NTIMER_START(lookupComplex1);
  auto matchcomplex1 = database.contains(queryComplex1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupComplex1);

  HELIB_NTIMER_START(lookupComplex2);
  auto matchcomplex2 = database.contains(queryComplex1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupComplex2);

  HELIB_NTIMER_START(lookupComplex3);
  auto matchcomplex3 = database.contains(queryComplex3, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupComplex3);*/

  HELIB_NTIMER_START(lookupdoublevars);
  auto doublevars = database.contains(querydoublevars, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupdoublevars);
  
  HELIB_NTIMER_START(lookupNotofOr1);
  auto NotofOr1 = database.contains(queryNotofOr1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofOr1);

  HELIB_NTIMER_START(lookupNotofOr2);
  auto NotofOr2 = database.contains(queryNotofOr2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofOr2);

  HELIB_NTIMER_START(lookupdoubleNot1);
  auto doubleNot1 = database.contains(querydoubleNot1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupdoubleNot1);

  HELIB_NTIMER_START(lookupdoubleNot2);
  auto doubleNot2 = database.contains(querydoubleNot2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupdoubleNot2);

  HELIB_NTIMER_START(lookupNotofAnd1);
  auto NotofAnd1 = database.contains(queryNotofAnd1, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofAnd1);

  HELIB_NTIMER_START(lookupNotofAnd2);
  auto NotofAnd2 = database.contains(queryNotofAnd2, queryData).apply(clean);
  HELIB_NTIMER_STOP(lookupNotofAnd2);


  HELIB_NTIMER_START(writeResults);
  // Write results to file
  /**writeResultsToFile(cmdLineOpts.outFilePath + "_a",
                     matcha,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_!a",
                     matchnota,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_b",
                     matchb,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_c",
                     matchc,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_and",
                     matchAnd,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_or",
                     matchOr,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_expand1",
                     matchExpand1,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_expand2",
                     matchExpand2,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_expand3",
                     matchExpand3,
                     cmdLineOpts.offset);                   
  writeResultsToFile(cmdLineOpts.outFilePath + "_aOr_!bAndc",
                     matchcomplex1,
                     cmdLineOpts.offset);
  writeResultsToFile(cmdLineOpts.outFilePath + "_!bAndc_Or_!a",
                     matchcomplex2,
                     cmdLineOpts.offset);                   
  writeResultsToFile(cmdLineOpts.outFilePath + "_aAnd!b",
                     matchcomplex3,
                     cmdLineOpts.offset);
  **/
  writeResultsToFile(cmdLineOpts.outFilePath + "_doublevars",
                     doublevars,
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
                     NotofAnd2,
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
