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

// BGV without bootstrapping key generation file. Writes secret key and
// evaluation key to different files.

#include <fstream>
#include <ctime>

#include <helib/helib.h>
#include <helib/ArgMap.h>
#include <helib/debugging.h>

#include <NTL/BasicThreadPool.h>

#include "common.h"

struct CmdLineOpts
{
  std::string paramFileName;
  std::string outputPrefixPath;
};

// Captures parameters for BGV
struct ParamsFileOpts
{
  long m = 0;
  long p = 0;
  long r = 0;
  long c = 0;
  long Qbits = 0;
};

// Write context.printout to file.out
void printoutToStream(const helib::Context& context, std::ostream& out)
{
  // write the algebra info
  context.printout(out);
}

// sk is child of pk in HElib.
void writeKeyToFile(std::string& pathPrefix,
                    helib::Context& context,
                    helib::SecKey& secretKey,
                    bool pkNotSk)
{
  std::string path = pathPrefix + (pkNotSk ? ".pk" : ".sk");
  std::ofstream keysFile(path, std::ios::binary);
  if (!keysFile.is_open()) {
    std::runtime_error("Cannot write keys to file at '" + path);
  }

  // write the context
  context.writeTo(keysFile);

  // write the keys
  if (pkNotSk) {
    const helib::PubKey& pk = secretKey;
    pk.writeTo(keysFile);
  } else {
    secretKey.writeTo(keysFile);
  }
  std::cout << path << " size = "<<keysFile.tellp() << "\n";
}

int main(int argc, char* argv[])
{
  CmdLineOpts cmdLineOpts;

  // clang-format off
  helib::ArgMap()
        .toggle()
        .separator(helib::ArgMap::Separator::WHITESPACE)
        .named()
          .arg("-o", cmdLineOpts.outputPrefixPath,
               "choose an output prefix path.", nullptr)
        .required()
        .positional()
          .arg("<params-file>", cmdLineOpts.paramFileName,
               "the parameters file.", nullptr)
        .parse(argc, argv);
  // clang-format on

  ParamsFileOpts paramsOpts;

  try {
    // clang-format off
    helib::ArgMap()
          .arg("p", paramsOpts.p, "require p.", "")
          .arg("m", paramsOpts.m, "require m.", "")
          .arg("r", paramsOpts.r, "require r.", "")
          .arg("c", paramsOpts.c, "require c.", "")
          .arg("Qbits", paramsOpts.Qbits, "require Q bits.", "")
          .parse(cmdLineOpts.paramFileName);
    // clang-format on
  } catch (const helib::RuntimeError& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << paramsOpts.p << ", " << paramsOpts.m << ", " << paramsOpts.r << ", " << paramsOpts.c << ", " << paramsOpts.Qbits << "\n";

  // Create the FHE context
  long p;

  if (paramsOpts.p < 2) {
    std::cerr << "BGV invalid plaintext modulus. "
                 "In BGV it must be a prime number greater than 1."
              << std::endl;
    return EXIT_FAILURE;
  }
  p = paramsOpts.p;

  try {
    helib::Context* contextp = helib::ContextBuilder<helib::BGV>()
                                   .m(paramsOpts.m)
                                   .p(p)
                                   .r(paramsOpts.r)
                                   .bits(paramsOpts.Qbits)
                                   .c(paramsOpts.c)
                                   .buildPtr();

    // and a new secret/public key
    helib::SecKey secretKey(*contextp);
    secretKey.GenSecKey(); // A +-1/0 secret key

    // If not set by user, returns params file name with truncated UTC
    if (cmdLineOpts.outputPrefixPath.empty()) {
      cmdLineOpts.outputPrefixPath =
          stripExtension(cmdLineOpts.paramFileName) +
          std::to_string(std::time(nullptr) % 100000);
      std::cout << "File prefix: " << cmdLineOpts.outputPrefixPath << std::endl;
    }

    // write only secret key to file
    std::string path1 = cmdLineOpts.outputPrefixPath + "_beforeKSM";
    writeKeyToFile(path1, *contextp, secretKey, 0);

    // compute key-switching matrices
    helib::addSome1DMatrices(secretKey);
    helib::addFrbMatrices(secretKey);    
    std::string path2 = cmdLineOpts.outputPrefixPath + "_afterKSM";
    writeKeyToFile(path2, *contextp, secretKey, 0);

  } catch (const std::invalid_argument& e) {
    std::cerr << "Exit due to invalid argument thrown:\n"
              << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const helib::IOError& e) {
    std::cerr << "Exit due to IOError thrown:\n" << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::runtime_error& e) {
    std::cerr << "Exit due to runtime error thrown:\n" << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::logic_error& e) {
    std::cerr << "Exit due to logic error thrown:\n" << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception& e) {
    std::cerr << "Exit due to unknown exception thrown:\n"
              << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
