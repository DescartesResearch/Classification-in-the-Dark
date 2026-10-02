#include "config.h"

#include <fstream>
#include <stdexcept>

config::config(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    nlohmann::json j;
    file >> j;

    parse_file_paths(j.value("file_paths", nlohmann::json::object()));
    parse_model_architecture(j.value("model_architecture", nlohmann::json::object()));
    parse_crypto_params(j.value("crypto_params", nlohmann::json::object()));
    parse_activation_config(j.value("activation_config", nlohmann::json::object()));
    parse_eval_min_params(j.value("eval_min_params", nlohmann::json::object()));
    parse_eval_binary_step_params(j.value("eval_binary_step_params", nlohmann::json::object()));
    parse_matrix_multiplication_config(j.value("matrix_multiplication", nlohmann::json::object()));
}

void config::parse_file_paths(const nlohmann::json& j) {
    file_paths_.weights_hidden = j.value("weights_hidden", file_paths_.weights_hidden);
    file_paths_.weights_output = j.value("weights_output", file_paths_.weights_output);
    file_paths_.bias_hidden = j.value("bias_hidden", file_paths_.bias_hidden);
    file_paths_.bias_output = j.value("bias_output", file_paths_.bias_output);
    file_paths_.test_data = j.value("test_data", file_paths_.test_data);
    file_paths_.test_labels = j.value("test_labels", file_paths_.test_labels);
    file_paths_.results = j.value("results", file_paths_.results);
}

void config::parse_model_architecture(const nlohmann::json& j) {
    model_arch_.input_dim = j.value("input_dim", model_arch_.input_dim);
    model_arch_.hidden_dim = j.value("hidden_dim", model_arch_.hidden_dim);
    model_arch_.output_dim = j.value("output_dim", model_arch_.output_dim);
    model_arch_.activation_function = j.value("activation_function", model_arch_.activation_function);
}

void config::parse_crypto_params(const nlohmann::json& j) {
    crypto_params_.multiplicative_depth = j.value("multiplicative_depth", crypto_params_.multiplicative_depth);
    crypto_params_.scale_mod_size = j.value("scale_mod_size", crypto_params_.scale_mod_size);
    crypto_params_.batch_size = j.value("batch_size", crypto_params_.batch_size);
    crypto_params_.security_level = parse_security_level(j.value("security_level", "HEStd_128_classic"));
}

lbcrypto::SecurityLevel config::parse_security_level(const std::string& level_str) {
    if (level_str == "HEStd_128_classic") return lbcrypto::HEStd_128_classic;
    if (level_str == "HEStd_192_classic") return lbcrypto::HEStd_192_classic;
    if (level_str == "HEStd_256_classic") return lbcrypto::HEStd_256_classic;
    if (level_str == "HEStd_128_quantum") return lbcrypto::HEStd_128_quantum;
    if (level_str == "HEStd_192_quantum") return lbcrypto::HEStd_192_quantum;
    if (level_str == "HEStd_256_quantum") return lbcrypto::HEStd_256_quantum;
    throw std::invalid_argument("Unknown security level: " + level_str);
}

void config::parse_activation_config(const nlohmann::json& j) {
    std::string type = j.value("type", "Error");

    if (type == "PolynomialConfig") {
        PolynomialConfig pc;
        nlohmann::json poly_obj = j.value("polynomial", nlohmann::json::object());
        pc.lower = poly_obj.value("lower", 0);
        pc.upper = poly_obj.value("upper", 0);
        pc.degree = poly_obj.value("degree", 0);
        activation_config_ = pc;
    } else if (type == "ActivationesConfig") {
        ActivationesConfig ac;
        nlohmann::json act_obj = j.value("activationes", nlohmann::json::object());
        ac.iterations = act_obj.value("iterations", 0);
        ac.gradient = act_obj.value("gradient", 0.0);
        ac.axis = act_obj.value("axis", 0.0);
        ac.terms = act_obj.value("terms", 0);
        activation_config_ = ac;
    } else if (type == "ConstantsConfig") {
        activation_config_ = ConstantsConfig{};
    } else {
        throw std::invalid_argument("Unknown activation type: " + type);
    }
}

void config::parse_eval_min_params(const nlohmann::json& j) {
    eval_min_params_.iterations = j.value("iterations", 0);
    eval_min_params_.lower = j.value("gradient", 0.0);
    eval_min_params_.upper = j.value("axis", 0.0);
}

void config::parse_eval_binary_step_params(const nlohmann::json& j) {
    eval_binary_step_params_.iterations_sqrt = j.value("iterations_sqrt", 0);
    eval_binary_step_params_.gradient_sqrt = j.value("gradient_sqrt", 0.0);
    eval_binary_step_params_.axis_sqrt = j.value("axis_sqrt", 0.0);
    eval_binary_step_params_.iterations_inv = j.value("iterations_inv", 0.0);
    eval_binary_step_params_.gradient_inv = j.value("gradient_inv", 0.0);
    eval_binary_step_params_.axis_inv = j.value("axis_inv", 0.0);
}

void config::parse_matrix_multiplication_config(const nlohmann::json& j) {
    std::string method_str = j.value("method", "Diagonal");
    if (method_str == "Diagonal") {
        mat_mult_.method = MatVecMethod::Diagonal;
    } else if (method_str == "RowNaive") {
        mat_mult_.method = MatVecMethod::RowNaive;
    } else if (method_str == "RowImproved") {
        mat_mult_.method = MatVecMethod::RowImproved;
    } else if (method_str == "RowHaleviShoup") {
        mat_mult_.method = MatVecMethod::RowHaleviShoup;
    } else if (method_str == "Col") {
        mat_mult_.method = MatVecMethod::Col;
    } else if (method_str == "Elementwise") {
        mat_mult_.method = MatVecMethod::Elementwise;
    } else {
        throw std::invalid_argument("Unknown matrix multiplication method: " + method_str);
    }
}
