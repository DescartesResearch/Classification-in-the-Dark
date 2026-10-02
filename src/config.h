#ifndef SARD_NN_PAPER_CONFIG_H
#define SARD_NN_PAPER_CONFIG_H

#include <string>
#include <variant>
#include <vector>

#include "activation_functions.h"
#include <nlohmann/json.hpp>
#include "matrix-mults.h"
#include "openfhe.h"

struct FilePaths {
    std::string weights_hidden = "weights/energy-efficiency/sigmoid/first_linear_weights.csv";
    std::string weights_output = "weights/energy-efficiency/sigmoid/second_linear_weights.csv";
    std::string bias_hidden = "weights/energy-efficiency/sigmoid/first_linear_bias.csv";
    std::string bias_output = "weights/energy-efficiency/sigmoid/second_linear_bias.csv";
    std::string test_data = "datasets/energy-efficiency/inputs.csv";
    std::string test_labels = "datasets/energy-efficiency/expected.csv";
    std::string results = "results/results.csv";
};

struct ModelArchitecture {
    uint32_t input_dim;
    uint32_t hidden_dim;
    uint32_t output_dim;
    std::string activation_function = "ReLU";
};

struct CryptoParamsConfig {
    uint32_t multiplicative_depth;
    uint32_t scale_mod_size;
    uint32_t batch_size;
    lbcrypto::SecurityLevel security_level = lbcrypto::HEStd_128_classic;
};

struct EvalMinParams {
    uint32_t iterations;
    double lower;
    double upper;
};

struct EvalBinaryStepParams {
    unsigned int iterations_sqrt;
    double gradient_sqrt;
    double axis_sqrt;
    unsigned int iterations_inv;
    double gradient_inv;
    double axis_inv;
};

struct MatrixMultiplicationConfig {
    MatVecMethod method = MatVecMethod::Diagonal;
};

class config {
   public:
    explicit config(const std::string& config_path);

    const FilePaths& file_paths() const { return file_paths_; }
    const ModelArchitecture& model_arch() const { return model_arch_; }
    const CryptoParamsConfig& crypto_params() const { return crypto_params_; }
    const config_option& activation_config() const { return activation_config_; }
    const EvalMinParams& eval_min_params() const { return eval_min_params_; }
    const EvalBinaryStepParams& eval_binary_step_params() const { return eval_binary_step_params_; }
    const MatrixMultiplicationConfig& mat_mult() const { return mat_mult_; }

   private:
    void parse_file_paths(const nlohmann::json& j);
    void parse_model_architecture(const nlohmann::json& j);
    void parse_crypto_params(const nlohmann::json& j);
    void parse_activation_config(const nlohmann::json& j);
    void parse_eval_min_params(const nlohmann::json& j);
    void parse_eval_binary_step_params(const nlohmann::json& j);
    void parse_matrix_multiplication_config(const nlohmann::json& j);
    lbcrypto::SecurityLevel parse_security_level(const std::string& level_str);

    FilePaths file_paths_;
    ModelArchitecture model_arch_;
    CryptoParamsConfig crypto_params_;
    config_option activation_config_;
    EvalMinParams eval_min_params_;
    EvalBinaryStepParams eval_binary_step_params_;
    MatrixMultiplicationConfig mat_mult_;
};

#endif  // SARD_NN_PAPER_CONFIG_H
