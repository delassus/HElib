// Copyright (C) 2022 Intel Corporation

// SPDX-License-Identifier: Apache-2.0

#include <helib/helib.h>
#include <helib/matmul.h> // matmul is not included by default
#include <helib/Matrix.h> // for defining matrices
#include <NTL/BasicThreadPool.h>
#include "utils.h" // helper functions written for this demo
#include <vector>

using namespace std;
using namespace helib;

int main(int argc, char *argv[])
{
    // in this demo we're looking at batched inference for private linear models.

    std::vector<bool> sections(3, 0);
    if (argc == 1)
        fill(sections.begin(), sections.end(), 1);
    for (int i = 1; i < 6; i++)
    {
        if (argc > i)
        {
            string arg = argv[i];
            sections[i - 1] = (arg == "1");
        }
    }

    srand(time(NULL));
    NTL::SetNumThreads(16);

    // HElib setup
    cout << "generating context ...";
    Context context =
        ContextBuilder<CKKS>().m(16 * 1024).bits(119).precision(30).c(2).build();
    SecKey secretKey(context);
    secretKey.GenSecKey();
    addSome1DMatrices(secretKey);
    const PubKey &publicKey = secretKey;
    long n = context.getNSlots();
    cout << " done\n";
    cout << "slot count = " << n << "\n";

    // in this demo, we have a linear model. Let's sample one now
    std::vector<double> model = uniformVector(n, -10, 10);

    if (sections[0])
    {
        cout << "printing model:\n";
        printVector(model, 10);
    }
    // put 1 at top
    // we will be homomorphically classifying up to n samples --
    // we generate these uniformly between -1 and 1
    std::vector<std::vector<double>> samples;
    for (int i = 0; i < n; i++)
        samples.push_back(uniformVector(n, -1, 1));

    if (sections[0])
    {
        cout << "printing first few samples:\n";
        for (int i = 0; i < 5; i++)
        {
            cout << "sample " << i << ": ";
            printVector(samples[i], 10);
        }
    }
    
    // we encrypt the model
    HELIB_NTIMER_START(encryptmodel);
    PtxtArray p(context, model);
    Ctxt c(publicKey);
    p.encrypt(c);
    HELIB_NTIMER_STOP(encryptmodel);
    printNamedTimer(cout, "encryptmodel");

    // and pass the samples to be classified to matmul.
    // as we are multiplying on the right, our samples need to be the
    // *columns* of the matrix. In other words, entry (i,j) in the matrix
    // needs to be the ith coordinate of sample j

    if (sections[1])
    {
        HELIB_NTIMER_START(matenc);
        MatMul_CKKS mat(context,
                        [samples](long i, long j)
                        { return samples[j][i]; });
        EncodedMatMul_CKKS emat(mat);
        emat.upgrade();

        HELIB_NTIMER_STOP(matenc);
        printNamedTimer(cout, "matenc");

        HELIB_NTIMER_START(matmul);
        c *= emat;
        HELIB_NTIMER_STOP(matmul);
        printNamedTimer(cout, "matmul");

        if (sections[2])
        {
            size_t printcount = 10;
            cout << "printing expected predictions:\n";
            for (size_t i = 0; i < printcount; i++)
                cout << dotProduct(samples[i], model) << ",";
            cout << "...\n";

            p.decrypt(c, secretKey);
            std::vector<double> predictions;
            p.store(predictions);
            cout << "printing calculated predictions:\n";
            for (size_t i = 0; i < printcount; i++)
                cout << predictions[i] << ",";
            cout << "...\n";
        }
    }

    return 0;
}
