// Copyright (C) 2022 Intel Corporation

// SPDX-License-Identifier: Apache-2.0

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

int main(int argc, char *argv[])
{
    // command line arguments determine which sections of the demo we run. If no chapters are
    // specified, all sections run.
    std::vector<bool> sections(4, false);
    if (argc == 1)
        fill(sections.begin(), sections.end(), 1);
    for (int i = 1; i < 6; i++)
    {
        if (argc > i)
        {
            std::string arg = argv[i];
            sections[i - 1] = (arg == "1");
        }
    }

    NTL::SetNumThreads(16);
    // HElib setup
    std::cout << "generating context ...";
    Context context =
        ContextBuilder<CKKS>().m(16 * 1024).bits(119).precision(30).c(2).build();
    SecKey secretKey(context);
    secretKey.GenSecKey();
    addSome1DMatrices(secretKey);
    const PubKey &publicKey = secretKey;
    long n = context.getNSlots();
    std::cout << " done\n";

    // generate some data
    helib::Matrix<double> M = uniformMatrix(n, n, -10, 10);
    if (sections[0])
    {
        std::cout << "printing matrix M:\n";
        printMatrix(M, 3, 10);
    }

    // the x sampled in 04_ckks_matmul.cpp
    std::vector<double> x(n);
    for (long j = 0; j < n; j++)
        x[j] = sin(2.0 * PI * j / n);
    if (sections[0])
    {
        std::cout << "printing vector v:\n";
        printVector(x, 10);
    }

    // matmul works for encrypted vectors, plaintext matrices.
    // encrypt v:
    PtxtArray p(context, x);
    Ctxt c(publicKey);
    p.encrypt(c);

    // pass M to matmul:
    MatMul_CKKS mat(context,
                    [M](long i, long j)
                    { return M(i, j); });

    PtxtArray pp(context);
    if (sections[1])
    {
        // compute vM homomorphically
        Ctxt c0 = c;
        HELIB_NTIMER_START(matmul);
        c0 *= mat;
        HELIB_NTIMER_STOP(matmul);
        printNamedTimer(std::cout, "matmul");

        // compute vM on plaintexts
        p *= mat;

        // Let's decrypt and compare:        
        pp.decrypt(c0, secretKey);
        double distance = Distance(p, pp);
        std::cout << "distance = " << distance << "\n";

        printMemoryUsage("matmul");        
    }

    if (sections[2])
    {
        // if we are going to use this matrix more than once,
        // we can speed this up by pre-encoding the plaintext matrix
        HELIB_NTIMER_START(encode);
        EncodedMatMul_CKKS mat1(mat);
        HELIB_NTIMER_STOP(encode);
        printNamedTimer(std::cout, "encode");

        // calculate vM homomorphically again
        Ctxt c1 = c;
        HELIB_NTIMER_START(matmul1);
        c1 *= mat1;
        HELIB_NTIMER_STOP(matmul1);
        printNamedTimer(std::cout, "matmul1");

        // decrypt and compare (we shouldn't see a difference)
        pp.decrypt(c1, secretKey);
        double distance = Distance(p, pp);
        std::cout << "distance = " << distance << "\n";

        printMemoryUsage("matmul1");        
    }

    if (sections[3])
    {
        // if we also have a lot of storage at our disposal,
        // we can get a further speedup using upgrade

        HELIB_NTIMER_START(encandupgrade);
        EncodedMatMul_CKKS mat1(mat);
        mat1.upgrade();
        HELIB_NTIMER_STOP(encandupgrade);
        printNamedTimer(std::cout, "encandupgrade");

        // perform the matrix multiplication a 3rd time
        Ctxt c2 = c;
        HELIB_NTIMER_START(matmul2);
        c2 *= mat1;
        HELIB_NTIMER_STOP(matmul2);
        printNamedTimer(std::cout, "matmul2");

        // decrypt and compare. Again, we shouldn't see a difference
        pp.decrypt(c2, secretKey);
        double distance = Distance(p, pp);
        std::cout << "distance = " << distance << "\n";

        // but the storage requirements of this are very large
        printMemoryUsage("matmul2");        
    }    
    return 0;
}
