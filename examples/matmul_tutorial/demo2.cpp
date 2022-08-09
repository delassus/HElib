// Copyright (C) 2022 Intel Corporation

// SPDX-License-Identifier: Apache-2.0

#include <vector>
#include <helib/helib.h>
#include <helib/matmul.h> // matmul is not included by default
#include <helib/Matrix.h> // for defining matrices
#include <NTL/BasicThreadPool.h>
#include "utils.h" // helper functions written for this demo

using namespace helib;

#if defined(__unix__) || defined(__unix) || defined(unix)
#include <sys/time.h>
#include <sys/resource.h>

// the function for recording memory usage from 04_ckks_matmul.cpp
void printMemoryUsage(const std::string &s)
{
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    std::cout << "  " << s;
    std::cout << "  ru_maxrss=" << r.ru_maxrss << std::endl;
}
#else
void printMemoryUsage()
{
}
#endif

int main()
{
    // in this example, we are going to consider training a linear regression model with encrypted labels.
    // we're going to hardcode a model, generate some synthetic data with it, and then rederive our model
    // using the generated samples.

    // Our model is going to have 21 weights (1 intercept, 20 coefficients).

    srand(time(NULL));
    NTL::SetNumThreads(16);

    // HElib setup
    std::cout << "generating context ...";
    Context context =
        ContextBuilder<CKKS>().m(16 * 1024).bits(119).precision(35).c(2).build();
    SecKey secretKey(context);
    secretKey.GenSecKey();
    addSome1DMatrices(secretKey);
    const PubKey &publicKey = secretKey;
    long n = context.getNSlots();
    std::cout << " done\n";

    // first we're going to make some synthetic data. This has three stages: define the model, generate n
    // vectors of variable values (x1,...,x20), and generate a label (or response) y for each (x1,...,x20)
    // using the model.

    int d = 20;                     // this will be the dimension of the x vectors
    double sigma = 1.0;             // the standard deviation of the error
    std::vector<double> model(d + 1, 0); // we need one extra weight for the intercept, which goes in the 0 slot
    iota(model.begin(), model.end(), -10);

    std::cout << "printing model:\n";
    printVector(model, d + 1);
    std::vector<std::vector<double>> X; // this is the data matrix (with left most column all 1s to make life easier later)
    std::vector<double> y;         // this is the vector of labels
    HELIB_NTIMER_START(makesamples);
    for (int i = 0; i < n; i++)
    {
        std::vector<double> sample = uniformVector(d, -10, 10);
        sample.insert(sample.begin(), 1);
        X.push_back(sample);
        y.push_back(dotProduct(sample, model) + sampleGaussian(0, sigma));
    }
    HELIB_NTIMER_STOP(makesamples);
    printNamedTimer(std::cout, "makesamples");
    // in this example, we are encrypting the vector of labels only
    PtxtArray p(context, y);
    Ctxt c(publicKey);
    p.encrypt(c);
    // Now for the matrix. The model which minimises the squared loss is given by yZ, where Z is the matrix
    // (X)(X^T X)^-1. Since we have X in the clear, we can just calculate Z:
    HELIB_NTIMER_START(createZ);
    std::vector<std::vector<double>> Z = createZ(X);
    HELIB_NTIMER_STOP(createZ);
    printNamedTimer(std::cout, "createZ");

    // If you want to look at training in the clear, uncomment this code
    /**std::vector<std::vector<double>> Y = {y};
    std::vector<std::vector<double>> betahat = matrixProduct(Y,Z);
    std::cout << "printing betahat:\n";
    printMatrix(betahat, 1, d + 1);**/ 

    // And pass directly to matmul:
    HELIB_NTIMER_START(createmat);
    MatMul_CKKS mat(context, [Z, d, n](long i, long j)
                     {if((i < n) && (j < (d + 1))){return Z[i][j];}
                        else{ return 0.0;} });
    EncodedMatMul_CKKS emat(mat);
    emat.upgrade();
    HELIB_NTIMER_STOP(createmat);
    printNamedTimer(std::cout, "createmat");
    HELIB_NTIMER_START(matmul);
    c *= emat;
    HELIB_NTIMER_STOP(matmul);
    printNamedTimer(std::cout, "matmul");

    p.decrypt(c, secretKey);
    std::vector<double> beta;
    p.store(beta);
    std::cout << "Printing calculated model:\n";
    printVector(beta, d + 1);
    return 0;
}
