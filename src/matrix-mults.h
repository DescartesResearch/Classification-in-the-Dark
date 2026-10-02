#ifndef SARD_NN_PAPER_MATRIX_MULTS_H
#define SARD_NN_PAPER_MATRIX_MULTS_H

#include <string>
#include <vector>

#include "openfhe.h"

enum class MatVecMethod { Diagonal, RowNaive, RowImproved, RowHaleviShoup, Col, Elementwise };

struct RotationKeyRequirements {
    std::set<int> rotation_indices;
    bool needs_eval_sum_key = false;
    bool needs_eval_sum_cols = false;
    bool needs_hybrid_replicate = false;
    std::vector<int> hybrid_replicate_indices;
};

lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecDiagonal(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                            const std::vector<std::vector<double>>& weights, unsigned int input_size, unsigned int output_size);

lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecRowNaive(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                            const std::vector<std::vector<double>>& weights, unsigned int input_size);
lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecRowImproved(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                               const std::vector<std::vector<double>>& weights, unsigned int inputSize, unsigned int outputSize,
                                                               const std::string& eval_key_map_key);
lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecRowHaleviShoup(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                                  const std::vector<std::vector<double>>& weights, unsigned int input_size);

lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecCol(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                       const std::vector<std::vector<double>>& weights, unsigned int input_size, unsigned int output_size);

lbcrypto::Ciphertext<lbcrypto::DCRTPoly> EvalMatVecElementwise(const lbcrypto::Ciphertext<lbcrypto::DCRTPoly>& inputVec,
                                                               const std::vector<std::vector<double>>& weights, unsigned int input_size,
                                                               unsigned int output_size);

RotationKeyRequirements GetRotationKeyRequirements(MatVecMethod method, uint32_t input_size, uint32_t output_size);

#endif  // SARD_NN_PAPER_MATRIX_MULTS_H
