// Copyright (C) 2022 Intel Corporation

// SPDX-License-Identifier: Apache-2.0

#include <helib/helib.h>
#include <helib/matmul.h> // matmul is not included by default
#include <helib/Matrix.h> // for defining matrices
#include <NTL/BasicThreadPool.h>
#include "utils.h" // helper functions written for this demo
#include <vector>
#include <functional> // for passing functions to matmul
#include <sys/time.h> // for recording timings

using namespace helib;

int main(int argc, char *argv[])
{
    // in this demo we are looking at performing convolutions on encrypted data.

    // we are going to pass the filter
    // -1 -1 -1
    // -1  8 -1
    // -1 -1 -1
    // over a square image with 1 row of padding and stride equal to 1.
    // we will start with a 4x4 input image to explain the technique, and then
    // proceed to a 64x64 image.

    // command line arguments determine which sections of the demo we run. If no chapters are
    // specified, all sections run.
    std::vector<bool> sections(5, 0);
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

    // for clarity, we're going to change the way cout displays decimals
    std::cout << std::fixed << std::showpoint;
    // Throughout the code, we will periodically use "cout << setprecision(0)"
    // to make the program not print after the decimal point.

    NTL::SetNumThreads(16);
    srand(time(NULL));

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

    // we are going to first look at a smaller dxd matrix
    int d = 4;
    std::vector<int> data = discreteUniformVector(d * d, -10, 10);
    // and encrypt it
    PtxtArray p(context, data);
    Ctxt c(publicKey);
    p.encrypt(c);

    if (sections[0])
    {
        std::cout << "Printing data as matrix:\n";
        printVectorAsMatrix(data, d, d);
    }

    // Now we need to pass the convolution we want to matmul.
    // So far we've been looking at cases where we have the matrix
    // we need in matrix form already: this makes things really simple!
    // Sometimes (like now) however, we'll need to build our matrix
    // using C++ lambda functions. This is the second argument in the matmul_CKKS
    // constructor.

    // In matmul, these must take the form
    // [<any parameters you need>](long i, long j){return <what you want in entry (i,j)>;}

    // A good rule of thumb is that entry (i,j) dictates how entry i of the input
    // relates to entry j of the output. If entry (i,j) = k, entry j of the output
    // will have k*input[i].

    // Let's think about our filter again. The central value of 8 tells us that
    // in cell j of the output value we want 8 copies of cell j of the input.
    // We could of course just take 8x the input vector, but lets build a matrix.

    if (sections[1])
    {
        // Let's make a filter which outputs 8x each pixel: call this "central pixels"
        HELIB_NTIMER_START(cpixelencode);

        MatMul_CKKS cpixelmat(context, [](long i, long j)
                              { return 8 * (i == j); });

        EncodedMatMul_CKKS ecpixelmat(cpixelmat);
        ecpixelmat.upgrade();
        HELIB_NTIMER_STOP(cpixelencode);
        printNamedTimer(std::cout, "cpixelencode");

        Ctxt c0 = c;

        // and apply it to our 4x4 matrix
        HELIB_NTIMER_START(cpixelmatmul);
        c0 *= ecpixelmat;
        HELIB_NTIMER_STOP(cpixelmatmul);
        printNamedTimer(std::cout, "cpixelmatmul");

        p.decrypt(c0, secretKey);
        std::vector<double> cpixels;
        p.store(cpixels);
        std::cout << "printing 8x central pixels:\n";
        std::cout << std::setprecision(0);
        printVectorAsMatrix(cpixels, 4, 4);
        std::cout << std::setprecision(6);
    }

    // For the rest of the filter, for each j we want to pick out -1 copies
    // of each i which is a surrounding pixel of j.

    // This is a little fiddly: essentially, i is a surrounding pixel of j
    // if and only if i != j AND the column difference and row difference between
    // i and j is one of {-1,0,1}.

    // This function returns TRUE if and only if i is a surrounding pixel of j
    // in a dxd matrix

    std::function<bool(long, long, long)> surroundingpixels = [](long i, long j, int d)
    {
        long coldiff = ((i - j) % d + d) % d; // C++ returns negative remainders for negative numbers
        long rowdiff = i / d - j / d;
        return (j != i) // don't want to pick out the central pixel
        && ((coldiff == 1) || (coldiff == 0) || (coldiff == (d - 1))) 
        && ((rowdiff == 1) || (rowdiff == 0) || (rowdiff == -1));
    };

    if (sections[2])
    {
        // let's use this function to pick out only the pixels surrounding (1,1).
        // This is entry 5 of the vector for a 4x4 matrix
        HELIB_NTIMER_START(spixelencode);
        MatMul_CKKS spixelmat(context, [d, surroundingpixels](long i, long j)
                              { return (i == j) && surroundingpixels(i, 5, d); });
        EncodedMatMul_CKKS espixelmat(spixelmat);
        espixelmat.upgrade();
        HELIB_NTIMER_STOP(spixelencode);
        printNamedTimer(std::cout, "spixelencode");

        Ctxt c1 = c;

        HELIB_NTIMER_START(spixelmatmul);
        c1 *= espixelmat;
        HELIB_NTIMER_STOP(spixelmatmul);
        printNamedTimer(std::cout, "spixelmatmul");

        p.decrypt(c1, secretKey);
        std::vector<double> spixel;
        p.store(spixel);
        std::cout << "printing surrounding pixels- of (1,1):\n";
        std::cout << std::setprecision(0);
        printVectorAsMatrix(spixel, 4, 4);
        std::cout << std::setprecision(6);
    }

    // now we're ready to combine to make our filter.
    // For output[j], we want to add 8 copies of input[j], and -1 copy
    // of each pixel that surrounds j.

    if (sections[3])
    {
        // we define our convolution matrix
        HELIB_NTIMER_START(convencode);
        MatMul_CKKS convmat(context, [d, surroundingpixels](long i, long j)
                            { return (8 * (j == i) - surroundingpixels(i, j, d)); });

        EncodedMatMul_CKKS econvmat(convmat);
        econvmat.upgrade();
        HELIB_NTIMER_STOP(convencode);
        printNamedTimer(std::cout, "convencode");

        HELIB_NTIMER_START(convmatmul);
        c *= econvmat;
        HELIB_NTIMER_STOP(convmatmul);
        printNamedTimer(std::cout, "convmatmul");

        p.decrypt(c, secretKey);
        std::vector<double> conv;
        p.store(conv);
        std::cout << "printing output of convolution:\n";
        std::cout << std::setprecision(0);
        printVectorAsMatrix(conv, 4, 4);
        std::cout << std::setprecision(6);
    }

    // so far, we've been working with a 4x4 matrix or image.
    // Our parameters however allow us to pack up to 4096 pixels,
    // which gives a 64x64 image. So let's run the same filter over
    // a fully packed image.

    if (sections[4])
    {
        int D = sqrt(n);
        std::vector<int> bigdata = discreteUniformVector(D * D, -10, 10);
        std::cout << "printing " << D << " by " << D << " image:\n";
        // we're only going to print the top left 16x16 block as 64 integers
        // don't fit on my terminal
        for (size_t i = 0; i < 16; i++)
        {
            for (size_t j = 0; j < 16; j++)
                std::cout << bigdata[i * D + j] << "\t";
            std::cout << ". . . \n";
        }
        for (size_t i = 0; i < 3; i++)
        {
            for (size_t j = 0; j < 16; j++)
                std::cout << ". \t";
            std::cout << "\n";
        }
        PtxtArray P(context, bigdata);
        Ctxt C(publicKey);
        P.encrypt(C);
        // as we've changed the dimension, we need a new matrix
        HELIB_NTIMER_START(bigconvencode);
        MatMul_CKKS bigconvmat(context, [D, surroundingpixels](long i, long j)
                               { return (8 * (i == j) - surroundingpixels(i, j, D)); });

        EncodedMatMul_CKKS ebigconvmat(bigconvmat);
        ebigconvmat.upgrade();
        HELIB_NTIMER_STOP(bigconvencode);
        printNamedTimer(std::cout, "bigconvencode");

        HELIB_NTIMER_START(bigconvmatmul);
        C *= ebigconvmat;
        HELIB_NTIMER_STOP(bigconvmatmul);
        printNamedTimer(std::cout, "bigconvmatmul");

        P.decrypt(C, secretKey);
        std::vector<double> bigconv;
        P.store(bigconv);
        std::cout << "printing output of convolution:\n";
        std::cout << std::setprecision(0);
        for (size_t i = 0; i < 16; i++)
        {
            for (size_t j = 0; j < 16; j++)
                std::cout << bigconv[i * D + j] << "\t";
            std::cout << ". . . \n";
        }
        for (size_t i = 0; i < 3; i++)
        {
            for (size_t j = 0; j < 16; j++)
                std::cout << ". \t";
            std::cout << "\n";
        }
    }
    
    return 0;
}
