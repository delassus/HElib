/* Copyright (C) 2022 Intel Corporation
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

#include <helib/helib.h>
#include <helib/debugging.h>
#include <io.h>
#include <helib/EncryptedArray.h>

#include "test_common.h"
#include "gtest/gtest.h"

struct BGVParametersT
{
  const long m;
  const long p;
  const long r;
  const long bits;

  BGVParametersT(long m,
                long p,
                long r,
                long bits) : m(m), p(p), r(r), bits(bits){};

  friend std::ostream& operator<<(std::ostream& os, const BGVParametersT& params)
  {
    return os << "{"
              << "m = " << params.m << ", "
              << "p = " << params.p << ", "
              << "r = " << params.r << ", "
              << "bits = " << params.bits << "}";
  }
};

struct CKKSParametersT
{
  const long m;
  const long precision;
  const long bits;

  CKKSParametersT(long m, long precision, long bits) :
      m(m), precision(precision), bits(bits){};

  friend std::ostream& operator<<(std::ostream& os,
                                  const CKKSParametersT& params)
  {
    return os << "{"
              << "m = " << params.m << ", "
              << "precision = " << params.precision << ", "
              << "bits = " << params.bits << "}";
  }
};

class TestTotalSums_BGVT : public ::testing::TestWithParam<BGVParametersT>
{
protected:
  const long m;
  const long p;
  const long r;
  const long bits;

  helib::Context context;
  helib::SecKey secretKey;
  helib::PubKey publicKey;
  const helib::EncryptedArray& ea;

  TestTotalSums_BGVT() :
      m(GetParam().m),
      p(GetParam().p),
      r(GetParam().r),
      bits(GetParam().bits),
      context(helib::ContextBuilder<helib::BGV>()
                  .m(m)
                  .p(p)
                  .r(r)
                  .bits(bits)
                  .build()),
      secretKey(context),
      publicKey(
          (secretKey.GenSecKey(), addSome1DMatrices(secretKey), secretKey)),
      ea(context.getEA())
  {}

  virtual void SetUp() override
  {
    helib::setupDebugGlobals(&secretKey, context.shareEA());
  }

  virtual void TearDown() override { helib::cleanupDebugGlobals(); }
};


class TestTotalSums_CKKST : public ::testing::TestWithParam<CKKSParametersT>
{
protected:
  const long m;
  const long precision;
  const long bits;

  helib::Context context;
  helib::SecKey secretKey;
  helib::PubKey publicKey;
  const helib::EncryptedArray& ea;

  TestTotalSums_CKKST() :
      m(GetParam().m),
      precision(GetParam().precision),
      bits(GetParam().bits),
      context(helib::ContextBuilder<helib::CKKS>()
                  .m(m)
                  .precision(precision)
                  .bits(bits)
                  .build()),
      secretKey(context),
      publicKey(
          (secretKey.GenSecKey(), addSome1DMatrices(secretKey), secretKey)),
      ea(context.getEA())
  {}

  virtual void SetUp() override
  {
    helib::setupDebugGlobals(&secretKey, context.shareEA());
  }

  virtual void TearDown() override { helib::cleanupDebugGlobals(); }
};

//tests here

TEST_P(TestTotalSums_BGVT, TSumsWorkCorrForPosValBGV)
{
  std::vector<long> data(context.getEA().size());
  std::iota(data.begin(), data.end(), 1);

  helib::Ptxt<helib::BGV> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::BGV> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);


  for (std::size_t i = 0; i < ptxt.size(); ++i) {
    EXPECT_EQ(post_decryption[i], ptxt[i]);
  }
}

TEST_P(TestTotalSums_BGVT, TSumsWorkCorrForNegValBGV)
{
  std::vector<long> data(context.getEA().size());
  std::iota(data.begin(), data.end(), -context.getEA().size());

  helib::Ptxt<helib::BGV> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::BGV> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);


  for (std::size_t i = 0; i < ptxt.size(); ++i) {
    EXPECT_EQ(post_decryption[i], ptxt[i]);
  }
}

TEST_P(TestTotalSums_BGVT, TSumsWorkCorrForPosNegValBGV)
{
  std::vector<long> data(context.getEA().size());

  for (std::size_t i=0; i<data.size(); i++){
      if (i%2 == 0){
          data[i] = -i;
      }
      else{
          data[i]= i;
      }
  }

  helib::Ptxt<helib::BGV> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::BGV> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  for (std::size_t i = 0; i < ptxt.size(); ++i) {
    EXPECT_EQ(post_decryption[i], ptxt[i]);
  }
}

TEST_P(TestTotalSums_BGVT, TSumsWorkCorrForZeroValBGV)
{
  std::vector<long> data(context.getEA().size(), 0);

  helib::Ptxt<helib::BGV> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::BGV> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  for (std::size_t i = 0; i < ptxt.size(); ++i) {
    EXPECT_EQ(post_decryption[i], ptxt[i]);
  }
}

TEST_P(TestTotalSums_CKKST, TSumsWorkCorrForZeroValCKKS)
{
  std::vector<std::complex<double>> data(context.getEA().size(), 0.0);

  helib::Ptxt<helib::CKKS> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::CKKS> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  COMPARE_CXDOUBLE_VECS(ptxt.getSlotRepr(), post_decryption.getSlotRepr());
}

TEST_P(TestTotalSums_CKKST, TSumsWorkCorrForPosValCKKS)
{
  std::vector<std::complex<double>> data(context.getEA().size());
  for (std::size_t i = 0; i < data.size(); ++i) {
    data[i] = {i / 1.0, (i * i) / 1.0};
  }

  helib::Ptxt<helib::CKKS> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::CKKS> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  COMPARE_CXDOUBLE_VECS(ptxt.getSlotRepr(), post_decryption.getSlotRepr());
}

TEST_P(TestTotalSums_CKKST, TSumsWorkCorrForNegValCKKS)
{
  std::vector<std::complex<double>> data(context.getEA().size());
  for (std::size_t i = 0; i < data.size(); ++i) {
    data[i] = {-i / 1.0, -(i * i) / 1.0};
  }

  helib::Ptxt<helib::CKKS> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::CKKS> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  COMPARE_CXDOUBLE_VECS(ptxt.getSlotRepr(), post_decryption.getSlotRepr());
}

TEST_P(TestTotalSums_CKKST, TSumsWorkCorrForPosNegValCKKS)
{
  std::vector<std::complex<double>> data(context.getEA().size());
  for (std::size_t i = 0; i < data.size(); ++i) {
    if(i%2 == 0){
      data[i] = {-i / 1.0, -(i * i) / 1.0};
    }
    else{
      data[i] = {i / 1.0, (i * i) / 1.0};
    }
  }

  helib::Ptxt<helib::CKKS> ptxt(context, data);
  ptxt.totalSums();

  helib::Ctxt ctxt(publicKey);
  publicKey.Encrypt(ctxt, ptxt);
  totalSums(context.getEA(), ctxt);

  helib::Ptxt<helib::CKKS> post_decryption(context);
  secretKey.Decrypt(post_decryption, ctxt);

  COMPARE_CXDOUBLE_VECS(ptxt.getSlotRepr(), post_decryption.getSlotRepr());
}


// parameters initialization
INSTANTIATE_TEST_SUITE_P(Parameters,
                         TestTotalSums_BGVT,
                         ::testing::Values(BGVParametersT(/*m=*/45,
                                                         /*p=*/317,
                                                         /*r=*/1,
                                                         /*bits=*/500),  BGVParametersT(512, /*fermat_prime=*/257, 1, 500), BGVParametersT(45, 317, 1, 500), BGVParametersT(288, /*fermat_prime=*/17, 1, 500), BGVParametersT(45, 367, 1, 500)));

INSTANTIATE_TEST_SUITE_P(Parameters,
                         TestTotalSums_CKKST,
                         ::testing::Values(CKKSParametersT(/*m=*/64,
                                                          /*precision=*/30,
                                                          /*bits=*/500), CKKSParametersT(128, 35, 500), CKKSParametersT(256, 40, 500), CKKSParametersT(512, 50, 500), CKKSParametersT(1024, 45, 500)));
