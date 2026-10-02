#ifndef SARD_NN_PAPER_ACTIVATION_FUNCTIONS_H
#define SARD_NN_PAPER_ACTIVATION_FUNCTIONS_H

#include <variant>

#include "openfhe.h"

struct PolynomialConfig {
    int lower, upper;
    unsigned int degree;
};

struct ActivationesConfig {
    unsigned int iterations;
    double gradient;
    double axis;
    unsigned int terms;
};

struct ConstantsConfig {};

using config_option = std::variant<PolynomialConfig, ActivationesConfig, ConstantsConfig>;

lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvaluateReLU(lbcrypto::Ciphertext<lbcrypto::DCRTPoly> value, config_option option);
lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvaluateSigmoid(lbcrypto::Ciphertext<lbcrypto::DCRTPoly> value, config_option option);

#endif  // SARD_NN_PAPER_ACTIVATION_FUNCTIONS_H
