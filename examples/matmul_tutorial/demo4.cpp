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
    // over a square image with no padding and stride equal to 1.
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
    // In general, column j of the plaintext matrix needs to
    // select the elements i of the input vector we want in entry
    // j of the output vector.

    // So for example, if we wanted to reverse the entries of our
    // vector, column j should have a 1 in position i if and only
    // if i + j = n.

    // The difference between this demo and demo3 is that we only
    // want output in the cells that correspond to inner pixels.

    // In our 4x4 image, which we've packed as 16 dimensional vector,
    // the inner pixels are at entries 5,6, 9, and 10
    // (or (1,1), (1,2), (2,1), and (2,2)).

    // Lets make a std::function on long j and long d which returns
    // TRUE if j is an inner pixel when the vector holds of dxd matrix
    // We want to only select pixels which AREN'T in the first or
    // last row or column. For the small 4x4 example, this means
    // we only want to select vector entries with  4 <= j <= 11
    // (the row number) and  0 < j % 4 < 3 (the column number).
    // In general this gives:
    std::function<bool(long, long)> innerpixels = [](long j, int d)
    {
        return (d <= j) && (j < d * (d - 1)) && (0 < (j % d)) && ((j % d) < d - 1);
    };

    if (sections[1])
    {
        // Let's make a filter which only lets through the inner pixels
        HELIB_NTIMER_START(ipixelencode);
        MatMul_CKKS ipixelmat(context, [d, innerpixels](long i, long j)
                              { return innerpixels(j, d) && (i == j); });
        EncodedMatMul_CKKS eipixelmat(ipixelmat);
        eipixelmat.upgrade();
        HELIB_NTIMER_STOP(ipixelencode);
        printNamedTimer(std::cout, "ipixelencode");

        Ctxt c0 = c;

        // and apply it to our 4x4 matrix
        HELIB_NTIMER_START(ipixelmatmul);
        c0 *= eipixelmat;
        HELIB_NTIMER_STOP(ipixelmatmul);
        printNamedTimer(std::cout, "ipixelmatmul");

        p.decrypt(c0, secretKey);
        std::vector<double> ipixels;
        p.store(ipixels);
        std::cout << "printing inner pixels:\n";
        std::cout << std::setprecision(0);
        printVectorAsMatrix(ipixels, 4, 4);
        std::cout << std::setprecision(6);
    }

    // now let's replicate the function from demo3 which returns TRUE
    // if i is a surrounding pixel of j in a dxd matrix
    std::function<bool(long, long, long)> surroundingpixels = [](long i, long j, int d)
    {
        long coldiff = ((i - j) % d + d) % d;
        long rowdiff = i / d - j / d;
        return (j != i) && ((coldiff == 1) || (coldiff == 0) || (coldiff == (d - 1))) && ((rowdiff == 1) || (rowdiff == 0) || (rowdiff == -1));
    };

    if (sections[2])
    {
        // and select only the elements which are surrounding pixels of
        // entry 5 of the vector = coordinate (1,1) of the matrix
        HELIB_NTIMER_START(opixelencode);
        MatMul_CKKS opixelmat(context, [d, surroundingpixels](long i, long j)
                              { return (i == j) && surroundingpixels(i, 5, d); });
        EncodedMatMul_CKKS eopixelmat(opixelmat);
        eopixelmat.upgrade();
        HELIB_NTIMER_STOP(opixelencode);
        printNamedTimer(std::cout, "opixelencode");

        Ctxt c1 = c;

        HELIB_NTIMER_START(opixelmatmul);
        c1 *= eopixelmat;
        HELIB_NTIMER_STOP(opixelmatmul);
        printNamedTimer(std::cout, "opixelmatmul");

        p.decrypt(c1, secretKey);
        std::vector<double> opixel;
        p.store(opixel);
        std::cout << "printing outer pixels:\n";
        std::cout << std::setprecision(0);
        printVectorAsMatrix(opixel, 4, 4);
        std::cout << std::setprecision(6);
    }

    // now we're ready to combine to make our filter.
    // if j corresponds to an "inner pixel", we want to
    // select 8x the jth entry, and -1 of each surrounding pixel.

    if (sections[3])
    {
        // we define our convolution matrix
        HELIB_NTIMER_START(convencode);
        MatMul_CKKS convmat(context, [d, innerpixels, surroundingpixels](long i, long j)
                            { return innerpixels(j, d) * (8 * (j == i) - surroundingpixels(i, j, d)); });

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
        MatMul_CKKS bigconvmat(context, [D, innerpixels, surroundingpixels](long i, long j)
                               { return innerpixels(j, D) * (8 * (i == j) - surroundingpixels(i, j, D)); });

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
