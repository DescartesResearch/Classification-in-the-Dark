#ifndef SARD_NN_PAPER_MATH_OP_H
#define SARD_NN_PAPER_MATH_OP_H

#include <tuple>
#include <vector>

#include "openfhe.h"

using namespace lbcrypto;

Ciphertext<DCRTPoly> EvalSqrt(const Ciphertext<DCRTPoly> &value, unsigned int iterations, double gradient, double axis);
Ciphertext<DCRTPoly> EvalInverse(const Ciphertext<DCRTPoly> &value, unsigned int iterations, double gradient, double axis);
Ciphertext<DCRTPoly> EvalExp(const Ciphertext<DCRTPoly> &value, int terms, int scale);
Ciphertext<DCRTPoly> EvalBinaryStep(const Ciphertext<DCRTPoly> &value, unsigned int iterationsSqrt, double gradientSqrt, double axisSqrt,
                                    unsigned int iterationsInv, double gradientInv, double axisInv);

Ciphertext<DCRTPoly> EvalMax(const Ciphertext<DCRTPoly> &first, const Ciphertext<DCRTPoly> &second, unsigned int iterations, double gradient, double axis);
Ciphertext<DCRTPoly> EvalMin(const Ciphertext<DCRTPoly> &first, const Ciphertext<DCRTPoly> &second, unsigned int iterations, double gradient, double axis);

std::vector<double> NewtonSqrtVec(unsigned int iterations, double gradient, double axis, const std::vector<double> &values);
std::vector<double> NewtonInverseVec(unsigned int iterations, double gradient, double axis, std::vector<double> values);
std::tuple<double, double> findGoodValuesSqrt(double l, double u, unsigned int i_N, unsigned int s, const std::vector<double> &gradRange,
                                              const std::vector<double> &axisRange);
std::tuple<double, double> findGoodValuesInverse(double u, double o, unsigned int i_N, unsigned int s, const std::vector<double> &gradRange,
                                                 const std::vector<double> &axisRange);

#endif  // SARD_NN_PAPER_MATH_OP_H
