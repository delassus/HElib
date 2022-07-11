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
// use assert rather than EXPECT_EQ which is from gtest
#include <cassert>

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

int main()
{
  NTL::SetNumThreads(8);
  // the parameters from the test suite
  helib::Context context = helib::ContextBuilder<helib::BGV>()
                               .m(1024)
                               .p(1087)
                               .r(1)
                               .bits(700)
                               .build();
  const helib::EncryptedArray& ea = context.getEA();
  helib::SecKey secretKey(context);
  secretKey.GenSecKey();
  helib::PubKey publicKey((secretKey.GenSecKey(),
                           addSome1DMatrices(secretKey),
                           addFrbMatrices(secretKey),
                           secretKey));

  // query
  const helib::QueryExpr& a = helib::makeQueryExpr(0);
  const helib::QueryExpr& b = helib::makeQueryExpr(1);
  const helib::QueryExpr& c = helib::makeQueryExpr(2);
  helib::QueryBuilder qbNotOfOr2(!(a || b) && c);
  helib::Query_t queryNotOfOr2 = qbNotOfOr2.build(5);
  qbNotOfOr2.removeOr();
  std::cout << qbNotOfOr2.getQueryString() << "\n";

  helib::Matrix<helib::Ptxt<helib::BGV>> plaintext_database_data(2l, 5l);
  // columns/features
  std::vector<std::vector<long>> plaintext_database_numbers = {
      {2, 1, 3, 2, 2, 1, 4, 2, 3, 4, 1, 2},
      {2, 1, 3, 2, 2, 1, 4, 2, 3, 4, 1, 2},
      {5, 2, 1, 4, 7, 1, 7, 9, 5, 2, 3, 4},
      {9, 3, 7, 3, 1, 4, 9, 5, 1, 0, 1, 1},
      {1, 9, 3, 4, 5, 7, 5, 4, 5, 1, 8, 4}};
  for (int i = 0; i < 5; ++i) {
    plaintext_database_data(0, i) =
        helib::Ptxt<helib::BGV>(context, plaintext_database_numbers[i]);
    plaintext_database_data(1, i) =
        helib::Ptxt<helib::BGV>(context, plaintext_database_numbers[i]);
  }

  helib::Matrix<helib::Ctxt> encrypted_database_data(helib::Ctxt(publicKey),
                                                     2l,
                                                     5l);
  for (std::size_t i = 0; i < plaintext_database_data.dims(0); ++i)
    for (std::size_t j = 0; j < plaintext_database_data.dims(1); ++j)
      publicKey.Encrypt(encrypted_database_data(i, j),
                        plaintext_database_data(i, j));

  helib::Database<helib::Ptxt<helib::BGV>> plaintext_database(
      plaintext_database_data,
      context);
  helib::Database<helib::Ctxt> encrypted_database(encrypted_database_data,
                                                  context);

  // columns/features
  helib::Matrix<helib::Ptxt<helib::BGV>> plaintext_query(1l, 5l);
  std::vector<std::vector<long>> plaintext_query_numbers = {
      {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
      {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},
      {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}};
  for (int i = 0; i < 5; ++i)
    plaintext_query(0, i) =
        helib::Ptxt<helib::BGV>(context, plaintext_query_numbers[i]);

  helib::Matrix<helib::Ctxt> encrypted_query(helib::Ctxt(publicKey), 1l, 5l);
  for (std::size_t i = 0; i < plaintext_query.dims(0); ++i)
    for (std::size_t j = 0; j < plaintext_query.dims(1); ++j)
      publicKey.Encrypt(encrypted_query(i, j), plaintext_query(i, j));

  // at this point, have two databases (plaintext_database and
  // encrypted_database) and two queries (plaintext_query and encrypted_query).
  // Run contains() with all four combinations and then check

  auto clean = [](auto& x) { x.cleanUp(); };
  helib::Ptxt<helib::BGV> expected_result(context);
  expected_result[0] = 0;
  expected_result[1] = 1;
  expected_result[2] = 0;
  expected_result[3] = 0;
  expected_result[4] = 0;
  expected_result[5] = 0;
  expected_result[6] = 0;
  expected_result[7] = 0;
  expected_result[8] = 0;
  expected_result[9] = 1;
  expected_result[10] = 0;
  expected_result[11] = 0;
  //ciphertext database, ciphertext query
  auto NotOfOr2CC =
      encrypted_database.contains(qbNotOfOr2, encrypted_query).apply(clean);
  
  // decrypt results
  helib::Matrix<helib::Ptxt<helib::BGV>> resultsCC(
      helib::Ptxt<helib::BGV>(context),
      NotOfOr2CC.dims(0),
      NotOfOr2CC.dims(1));
  resultsCC.entrywiseOperation<helib::Ctxt>(
      NotOfOr2CC,
      [&](auto& ptxt, const auto& ctxt) -> decltype(auto) {
        secretKey.Decrypt(ptxt, ctxt);
        return ptxt;
      });
  

  assert(resultsCC.dims(1) == 1l);
  std::cout << "resultsCC dim(1) = " << resultsCC.dims(1)
            << ", resultsCC dim(0) = " << resultsCC.dims(0) << "\n";
  for (size_t i = 0; i < resultsCC.dims(0); ++i) {
    for (size_t j = 0; j < resultsCC.dims(1); ++j) {
      assert(resultsCC(i, j) == expected_result);
    }
  }
  std::cout << "CC check passed\n";
  
  // ciphertext database, plaintext query
  auto NotOfOr2CP = encrypted_database.contains(qbNotOfOr2,plaintext_query).apply(clean);
  // decrypt results
  helib::Matrix<helib::Ptxt<helib::BGV>> resultsCP(
      helib::Ptxt<helib::BGV>(context),
      NotOfOr2CP.dims(0),
      NotOfOr2CP.dims(1));
  resultsCP.entrywiseOperation<helib::Ctxt>(
      NotOfOr2CP,
      [&](auto& ptxt, const auto& ctxt) -> decltype(auto) {
        secretKey.Decrypt(ptxt, ctxt);
        return ptxt;
      });
  

  assert(resultsCP.dims(1) == 1l);
  std::cout << "resultsCP dim(1) = " << resultsCP.dims(1)
            << ", resultsCP dim(0) = " << resultsCP.dims(0) << "\n";
  for (size_t i = 0; i < resultsCP.dims(0); ++i) {
    for (size_t j = 0; j < resultsCP.dims(1); ++j) {
      assert(resultsCP(i, j) == expected_result);
    }
  }
  std::cout << "CP check passed\n";
  
  //plaintext database, ciphertext query
  auto NotOfOr2PC = plaintext_database.contains(qbNotOfOr2, encrypted_query).apply(clean);
  // decrypt results
  helib::Matrix<helib::Ptxt<helib::BGV>> resultsPC(
      helib::Ptxt<helib::BGV>(context),
      NotOfOr2PC.dims(0),
      NotOfOr2PC.dims(1));
  resultsPC.entrywiseOperation<helib::Ctxt>(
      NotOfOr2PC,
      [&](auto& ptxt, const auto& ctxt) -> decltype(auto) {
        secretKey.Decrypt(ptxt, ctxt);
        return ptxt;
      });
  

  assert(resultsPC.dims(1) == 1l);
  std::cout << "resultsPC dim(1) = " << resultsPC.dims(1)
            << ", resultsPC dim(0) = " << resultsPC.dims(0) << "\n";
  for (size_t i = 0; i < resultsPC.dims(0); ++i) {
    for (size_t j = 0; j < resultsPC.dims(1); ++j) {
      assert(resultsPC(i, j) == expected_result);
    }
  }
  std::cout << "PC check passed\n";

  //plaintext database, plaintext query
  auto NotOfOr2PP = plaintext_database.contains(qbNotOfOr2, plaintext_query).apply(clean);

  assert(NotOfOr2PP.dims(1) == 1l);
  std::cout << "NotOfOr2PP dim(1) = " << NotOfOr2PP.dims(1)
            << ", NotOfOr2PP dim(0) = " << NotOfOr2PP.dims(0) << "\n";
  for (size_t i = 0; i < NotOfOr2PP.dims(0); ++i) {
    for (size_t j = 0; j < NotOfOr2PP.dims(1); ++j) {
      assert(NotOfOr2PP(i, j) == expected_result);
    }
  }
  std::cout << "PP check passed\n";
  return 0;
}
