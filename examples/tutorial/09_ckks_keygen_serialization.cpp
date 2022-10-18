#include <fstream>

#include <helib/helib.h>

// In this tutorial we consider generating keys, transmitting key
// material, evaluating a function homomorphically, and finally returning for
// decryption.

int main(int argc, char* argv[])
{
  // This scope indicates client side key generation.

  { // we start with building a context.
    helib::Context context = helib::ContextBuilder<helib::CKKS>()
                                 .m(128)
                                 .precision(30)
                                 .bits(70)
                                 .c(3)
                                 .build();

    // NOTE These chosen parameters are for demonstration only. They do not the
    // provide security level that might be required for real use/application
    // scenarios.

    // we create a secret key for this context
    helib::SecKey secretKey(context);
    // and generate
    secretKey.GenSecKey();

    // we now create our first file, secretKey.json, which will contain the
    // context and the secret key material. This files
    std::ofstream outSecretKeyFile;
    outSecretKeyFile.open("secretKey.json", std::ios::out);

    if (outSecretKeyFile.is_open()) {
      // Write the context to a file
      context.writeToJSON(outSecretKeyFile);
      // print the size of the file after printing context
      std::cout << "size of secret key file after printing context: "
                << outSecretKeyFile.tellp() << std::endl;
      // now write only the secret key
      secretKey.writeOnlySecKeyToJSON(outSecretKeyFile);
      std::cout << "size of secret key file after printing secret key: "
                << outSecretKeyFile.tellp() << std::endl;
      // Close the ofstream
      outSecretKeyFile.close();
    } else {
      throw std::runtime_error("Could not open file 'secretKey.json'.");
    }
    // we are going to write the public key twice, once before we add the keyswitching matrices, and once after. Writing before gives a smaller public key which can be used for encryption (called the encPublicKey) and after gives a large public key which can be used for homomorphic function evaluation (called the evalPublicKey).
    helib::PubKey& PublicKey = secretKey;
    // and write to a file
    std::ofstream outEncPublicKeyFile;
    outEncPublicKeyFile.open("encPublicKey.json", std::ios::out);

    if (outEncPublicKeyFile.is_open()) {
      // Again, we write the context to a file
      context.writeToJSON(outEncPublicKeyFile);
      // print the size of the file after printing context
      std::cout << "size of encryption public key file after printing context: "
                << outEncPublicKeyFile.tellp() << std::endl;
      // now write the public key
      PublicKey.writeToJSON(outEncPublicKeyFile);
      std::cout
          << "size of encryption public key file after printing public key: "
          << outEncPublicKeyFile.tellp() << std::endl;
      // Close the ofstream
      outEncPublicKeyFile.close();
    } else {
      throw std::runtime_error("Could not open file 'encPublicKey.json'.");
    }

    // now we generate the evaluation keys
    helib::addSome1DMatrices(secretKey);

    // now the keyswitching matrices are associated with the public key, we can write the larger evaluation public key. 
    std::ofstream outEvalPublicKeyFile;
    outEvalPublicKeyFile.open("evalPublicKey.json", std::ios::out);

    if (outEvalPublicKeyFile.is_open()) {
      // Again, we write the context to a file
      context.writeToJSON(outEvalPublicKeyFile);
      // print the size of the file after printing context
      std::cout << "size of evaluation public key file after printing context: "
                << outEvalPublicKeyFile.tellp() << std::endl;
      // now write the evaluation public key
      PublicKey.writeToJSON(outEvalPublicKeyFile);
      std::cout
          << "size of evaluation public key file after printing public key: "
          << outEvalPublicKeyFile.tellp() << std::endl;
      // Close the ofstream
      outEvalPublicKeyFile.close();
    } else {
      throw std::runtime_error("Could not open file 'evalPublicKey.json'.");
    }
  }

  // Suppose the client now wants to pull the encryption public key from
  // storage, encrypt some data, and write this ciphertext to a file for
  // transmission.

  // we are going to record what the result of computation should be in order to
  // compare.
  std::vector<double> results_vector;
  {
    std::ifstream inPublicKeyFile;
    inPublicKeyFile.open("encPublicKey.json", std::ios::in);
    if (!inPublicKeyFile.is_open()) {
      throw std::runtime_error("Could not open file 'encPublicKey.json'.");
    }

    // First, we read the context from the file
    helib::Context deserialized_context =
        helib::Context::readFromJSON(inPublicKeyFile);
    // then the encryption public key
    helib::PubKey deserialized_pk =
        helib::PubKey::readFromJSON(inPublicKeyFile, deserialized_context);
    // close the ifstream
    inPublicKeyFile.close();

    // we now make some data
    helib::PtxtArray ptxt(deserialized_context);
    ptxt.random();
    // encrypt
    helib::Ctxt ctxt(deserialized_pk);
    ptxt.encrypt(ctxt);

    // and write to a file for transmission
    std::ofstream outCtxtFile;
    outCtxtFile.open("ctxt.json", std::ios::out);

    if (!outCtxtFile.is_open()) {
      throw std::runtime_error("Could not open file 'ctxt.json'.");
    }

    ctxt.writeToJSON(outCtxtFile);
    std::cout << "size of ctxt file: " << outCtxtFile.tellp() << std::endl;

    // close the ofstream
    outCtxtFile.close();

    // to show this works as intended, we are going to compute on this plaintext
    // and store the result. We are computing x^2 + 3x + 4.
    helib::PtxtArray ptxt_result = ptxt;
    ptxt_result *= ptxt;
    ptxt *= 3.0;
    ptxt_result += ptxt;
    ptxt_result += 4.0;
    ptxt_result.store(results_vector);
  }
  // this is now server side, where we want to evaluate x^2 + 3x + 4
  // homomorphically.
  {
    // we first need to read in the public key, which we do the same way as
    // above, but this time using the evalPublicKey.json file.
    std::ifstream inPublicKeyFile;

    inPublicKeyFile.open("evalPublicKey.json", std::ios::in);

    if (!inPublicKeyFile.is_open()) {
      throw std::runtime_error("Could not open file 'evalPublicKey.json'.");
    }

    helib::Context deserialized_context =
        helib::Context::readFromJSON(inPublicKeyFile);
    helib::PubKey deserialized_pk =
        helib::PubKey::readFromJSON(inPublicKeyFile, deserialized_context);
    // close the ifstream
    inPublicKeyFile.close();

    // now we read in the ciphertext, as follows
    std::ifstream inCtxtFile("ctxt.json", std::ios::in);

    if (!inCtxtFile.is_open()) {
      throw std::runtime_error("Could not open file 'ctxt.json'.");
    }
    helib::Ctxt deserialized_ctxt =
        helib::Ctxt::readFromJSON(inCtxtFile, deserialized_pk);
    inCtxtFile.close();

    // compute x^2 + 3x + 4
    helib::Ctxt ctxt_result = deserialized_ctxt;
    ctxt_result *= deserialized_ctxt;
    deserialized_ctxt *= 3.0;
    ctxt_result += deserialized_ctxt;
    ctxt_result += 4.0;
    ctxt_result.cleanUp();

    // and finally write the result to a file
    std::ofstream outCtxtResultFile;
    outCtxtResultFile.open("ctxtResult.json", std::ios::out);
    if (!outCtxtResultFile.is_open()) {
      throw std::runtime_error("Could not open file 'ctxtResult.json'.");
    }
    ctxt_result.writeToJSON(outCtxtResultFile);
    std::cout << "size of results ctxt file: " << outCtxtResultFile.tellp()
              << std::endl;
    // close the ofstream
    outCtxtResultFile.close();
  }

  // back client side, we are ready to decrypt and compare
  {
    // first read the secret key material
    std::ifstream inSecretKeyFile;
    inSecretKeyFile.open("secretKey.json", std::ios::in);
    if (!inSecretKeyFile.is_open()) {
      throw std::runtime_error("Could not open file 'secretKey.json'.");
    }
    helib::Context deserialized_context =
        helib::Context::readFromJSON(inSecretKeyFile);
    helib::SecKey deserialized_sk =
        helib::SecKey::readOnlySecKeyFromJSON(inSecretKeyFile,
                                              deserialized_context);

    // close the ifstream
    inSecretKeyFile.close();

    // read the ciphertext
    std::ifstream inCtxtResultFile;
    inCtxtResultFile.open("ctxtResult.json", std::ios::in);
    if (!inCtxtResultFile.is_open()) {
      throw std::runtime_error("Could not open file 'ctxtResult.json'.");
    }
    helib::Ctxt ctxt_result =
        helib::Ctxt::readFromJSON(inCtxtResultFile, deserialized_sk);
    inCtxtResultFile.close();

    helib::PtxtArray ptxt_result(deserialized_context);
    ptxt_result.decrypt(ctxt_result, deserialized_sk);
    std::cout << "printing client side results: " << std::endl;
    std::vector<double> hom_results_vector;
    ptxt_result.store(hom_results_vector);
    for (std::size_t i = 0; i < 15; i++) {
      std::cout << hom_results_vector[i] << ", ";
    }
    std::cout << "..." << std::endl;
  }
  std::cout << "printing plaintext result:" << std::endl;
  for (std::size_t i = 0; i < 15; i++) {
    std::cout << results_vector[i] << ", ";
  }
  std::cout << "..." << std::endl;
}