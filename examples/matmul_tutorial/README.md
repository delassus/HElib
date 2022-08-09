## Building the repo
This repo uses the HElib library, and is configured with CMake.

To build, run `cmake -S . -B build -Dhelib_DIR=<helib install prefix>/share/cmake/helib/>` from the project folder. For my directory structure, this is `cmake -S . -B build -Dhelib_DIR=~/helib_install/helib_pack/share/cmake/helib/`. You can then compile via moving into the build directory and running `make`. You can optionally specify the number of threads via e.g. `make -j16`. Compilation should produce five executable demos in the `build` directory.

## Running the demos
To run all sections of demo k, type ./demo`k` from the build directory. For easy presentation, there's also the option to only run certain
sections -- e.g. ./demo3 1 0 1 0 0 will only run sections 1 and 3 of demo3. If only some arguments are specified, the rest will be assumed to be 0.

# demo 0
demo0 covers the basics of using matmul, and closely resembles CKKS tutorial 4.
- section 1: print matrix and vector
- section 2: standard vector matrix multiplication 
- section 3: vector matrix multiplication with encoded matrix
- section 4: vector matrix multiplication with upgraded matrix

# demo 1
demo1 computes batched inference for an encrypted linear model. Here, the model is encrypted while the data passed through the model is unencrypted.
- section 1: print model and samples
- section 2: linear inference
- section 3: print expected and calculated predictions

# demo 2
demo2 trains a linear model on encrypted labels. The resulting model is encrypted.

# demo 3
demo3 computes a standard convolutional layer on encrypted data. Here, the convolutional layer is known in the clear, while the data being processed is encrypted.
- section 1: print data
- section 2: central pixels
- section 3: surrounding pixels
- section 4: convolution of 4x4 image
- section 5: convolution of 64x64 image

# demo 4 -- not covered in presentation
demo4 is very similar to demo3, but does not using padding: in other words, the filter is only applied to "inner pixels". Look up CNN padding for further information.
- section 1: print data
- section 2: isolating inner pixels
- section 3: surrounding pixels
- section 4: convolution of 4x4 image
- section 5: convolution of 64x64 image

# Extensions
If you want to have a go at programming with matmul, or extending the functionality here, but can't think of any new applications, here are some ideas to play around with. Not a test! google, skip, ignore, etc. etc.
- demo0:
    - Try changing the HE parameters
    - Try changing the matrix/vector dimension
    - Try generating massive data/really small data/integer data/all positive data/.... and seeing what happens
    - **Can you use matmul to give encrypted matrix x plaintext matrix multiplication?
- demo1:
    - Try not completely packing the matrix and vector
    - Try with a real dataset
    - *How would you measure the accuracy of predictions for a real dataset?
    - ***How would you adapt this demo to perform batched inference for an encrypted logistic regression model? 
    - ***How does "accuracy of predictions" relate to the "distance" computed by HELib?
- demo2:
    - Try with a real dataset 
    - *Can you adapt this code to perform ridge regression training? 
- demo3/demo4:
    - Try with a real image 
    - I've gone for an easy to implement 3x3 filter. Can you instead implement e.g. a 3x3 sharpen filter, a 3x3 vertical line detector
    - How would the code change if the image was not square?
    - Try a 5x5 filter? How does this change demo4?
    - Try modifying the stride
    - **Can you write code which takes a dimension, a filter, a stride, and a padding, and outputs the required MatMul?
    - *For an unpacked convolutional layer, can you get the non zero entries to be contiguous in the output vector? In other words, can you get rid of the zero rows and columns?

- What effect does the matrix in chapter 4 of the HElib tutorial have on the original vector?
- ****Can you use these demos to evaluate the approximate output of a real neural network? [HINT: combining demo0, the second last exercise of demo1, and demos 3 and 4. You will almost certainly need to modify the parameters. You can find secure parameters for HElib in examples/tutorial/01_ckks_basics.cpp]
    






