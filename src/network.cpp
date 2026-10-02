#include <iostream>
#include <vector>

#include "CryptoParams.h"
#include "activation_functions.h"
#include "config.h"
#include "math_op.h"
#include "matrix-mults.h"
#include "openfhe.h"
#include "profiler.h"
#include "utils.h"

using lbcrypto::Ciphertext;
using lbcrypto::CryptoContext;
using lbcrypto::DCRTPoly;
using lbcrypto::KeyPair;
using lbcrypto::Plaintext;

std::tuple<Ciphertext<DCRTPoly>, Ciphertext<DCRTPoly>> inference(const Ciphertext<DCRTPoly>& input, const config& config, PrivateKey<DCRTPoly> sec_key) {
    auto cc = input->GetCryptoContext();
    const auto& file_paths = config.file_paths();
    const auto& model_arch = config.model_arch();
    const auto& mm = config.mat_mult();

    const std::vector<std::vector<double>> weights_input_hidden = read_csv_matrix(file_paths.weights_hidden);
    // Weights from hidden (4) to output (2): 2x4 matrix
    const std::vector<std::vector<double>> weights_hidden_output = read_csv_matrix(file_paths.weights_output);

    auto matvec = [&](const Ciphertext<DCRTPoly>& in, const std::vector<std::vector<double>>& w, uint32_t in_dim, uint32_t out_dim,
                      const std::string& eval_key_map_key) -> Ciphertext<DCRTPoly> {
        switch (mm.method) {
            case MatVecMethod::Diagonal:
                return EvalMatVecDiagonal(in, w, in_dim, out_dim);
            case MatVecMethod::RowNaive:
                return EvalMatVecRowNaive(in, w, in_dim);
            case MatVecMethod::RowImproved:
                return EvalMatVecRowImproved(in, w, in_dim, out_dim, eval_key_map_key);
            case MatVecMethod::RowHaleviShoup:
                return EvalMatVecRowHaleviShoup(in, w, in_dim);
            case MatVecMethod::Col:
                return EvalMatVecCol(in, w, in_dim, out_dim);
            case MatVecMethod::Elementwise:
                return EvalMatVecElementwise(in, w, in_dim, out_dim);
        }
        throw std::logic_error("Unknown MatVecMethod");
    };

    // Layer 1: Input -> Hidden
    Ciphertext<DCRTPoly> hidden;
    {
        Profiler p_mv_mult("first", "diagonal_mult.csv");
        hidden = matvec(input, weights_input_hidden, model_arch.input_dim, model_arch.hidden_dim, "sum_cols");
    }

    auto bias_hidden = read_csv_vector(file_paths.bias_hidden);
    {
        Profiler p_bias("first", "bias.csv");
        auto ptx_bias = cc->MakeCKKSPackedPlaintext(bias_hidden);
        hidden = cc->EvalAdd(hidden, ptx_bias);
    }

    // Evaluate activation function
    {
        Profiler p_relu("constant_conf", config.model_arch().activation_function + ".csv");
        if (config.model_arch().activation_function == "Sigmoid") {
            hidden = EvaluateSigmoid(hidden, config.activation_config());
        } else {
            hidden = EvaluateReLU(hidden, config.activation_config());
        }
    }

    // Layer 2: Hidden -> Output
    Ciphertext<DCRTPoly> output;
    {
        Profiler p_mv_mult("second", "diagonal_mult.csv");
        output = matvec(hidden, weights_hidden_output, model_arch.hidden_dim, model_arch.output_dim, "sum_cols");
    }

    const auto bias_out = read_csv_vector(file_paths.bias_output);
    {
        Profiler p_bias("second", "bias.csv");
        auto ptx_bias = cc->MakeCKKSPackedPlaintext(bias_out);
        output = cc->EvalAdd(output, ptx_bias);
    }

    Ciphertext<DCRTPoly> res;
    {
        Profiler p_output("min_1_bs_1_2", "output_determination.csv");
        // The following line ist required JUST FOR PARKINSONS!
        cc->EvalMultInPlace(output, .1);
        const auto code_green = output;
        const auto code_red = cc->EvalRotate(output, 1);
        const auto& emin = config.eval_min_params();
        const auto minimum = EvalMin(code_green, code_red, emin.iterations, emin.lower, emin.upper);

        // Determine in which slot the minimum lies through the binary step function
        constexpr double green_lamb = 1;
        constexpr double red_lamb = 0;

        const auto& ebs = config.eval_binary_step_params();
        auto sub = cc->EvalSub(minimum, code_green);

        auto bs = EvalBinaryStep(sub, ebs.iterations_sqrt, ebs.gradient_sqrt, ebs.axis_sqrt, ebs.iterations_inv, ebs.gradient_inv, ebs.axis_inv);
        const auto first_summand = cc->EvalMult(bs, green_lamb);

        sub = cc->EvalSub(minimum, code_red);
        bs = EvalBinaryStep(sub, ebs.iterations_sqrt, ebs.gradient_sqrt, ebs.axis_sqrt, ebs.iterations_inv, ebs.gradient_inv, ebs.axis_inv);
        const auto second_summand = cc->EvalMult(bs, red_lamb);

        res = cc->EvalAdd(first_summand, second_summand);
        cc->EvalMultInPlace(res, 2);
    }

    return std::make_tuple(res, output);
}

std::tuple<int, double, std::vector<double>> run(CryptoContext<DCRTPoly> cc, KeyPair<DCRTPoly> keys, const std::vector<double>& input, const config& config) {
    Plaintext input_plain = cc->MakeCKKSPackedPlaintext(input);
    Ciphertext<DCRTPoly> input_enc = cc->Encrypt(keys.publicKey, input_plain);

    std::tuple<Ciphertext<DCRTPoly>, Ciphertext<DCRTPoly>> inference_res;
    {
        Profiler p_infer("full", "inference.csv");
        inference_res = inference(input_enc, config, keys.secretKey);
    }
    auto result = std::get<0>(inference_res);
    auto output_nodes = std::get<1>(inference_res);

    try {
        Plaintext decrypted;
        cc->Decrypt(keys.secretKey, result, &decrypted);
        decrypted->SetLength(1);

        Plaintext ptx_output;
        cc->Decrypt(keys.secretKey, output_nodes, &ptx_output);
        ptx_output->SetLength(2);

        // Classification: decrypted value cutoff at the first decimal place, because
        // through encrypted computation the result is not exactly 0 or 1
        double before_cast = decrypted->GetRealPackedValue()[0];
        const unsigned int classification = static_cast<int>(before_cast);
        std::cout << "Classification: " << classification << std::endl;

        return std::make_tuple(classification, before_cast, ptx_output->GetRealPackedValue());

    } catch (const OpenFHEException& e) {
        std::cerr << "Warning: OpenFHE exception caught - " << e.what() << std::endl;
        return std::make_tuple(-1, -1.0, std::vector<double>{-1.0, -1.0});

    } catch (...) {
        std::cerr << "Warning: Unknown exception caught during decryption process." << std::endl;
        return std::make_tuple(-1, -1.0, std::vector<double>{-1.0, -1.0});
    }
}

int main() {
    const std::string config_path = "config.json";
    config config(config_path);

    int error_count = 0;

    const auto& cp = config.crypto_params();
    std::cout << "Cryptography parameters:" << cp.multiplicative_depth << "," << cp.scale_mod_size << "," << cp.batch_size << "," << cp.security_level
              << std::endl;
    auto params = CryptoParams(cp.multiplicative_depth, cp.scale_mod_size, cp.batch_size, cp.security_level);
    const CryptoContext<DCRTPoly> cc = params.getCryptoContext();

    // Keys Generation
    KeyPair<DCRTPoly> keys;
    {
        Profiler p_keys("basic", "keys.csv");
        keys = cc->KeyGen();
    }
    {
        Profiler p_eval("eval", "keys.csv");
        cc->EvalMultKeyGen(keys.secretKey);
    }

    // Rotation Keys Generation for matrix-vector multiplication
    {
        Profiler p_rot("rotation", "keys.csv");
        const auto& model_arch = config.model_arch();
        auto req = GetRotationKeyRequirements(config.mat_mult().method, model_arch.input_dim, model_arch.hidden_dim);
        auto indices = req.rotation_indices;
        if (config.mat_mult().method == MatVecMethod::RowImproved || config.mat_mult().method == MatVecMethod::Diagonal ||
            config.mat_mult().method == MatVecMethod::Col || config.mat_mult().method == MatVecMethod::RowHaleviShoup ||
            config.mat_mult().method == MatVecMethod::Elementwise) {  // Both need different indices for the second multiplication, therefore we need
            // to generate keys for them as well; Diagonal only needs the other input size, improved needs many more
            auto second_indices = GetRotationKeyRequirements(config.mat_mult().method, model_arch.hidden_dim, model_arch.output_dim).rotation_indices;
            indices.insert(second_indices.begin(), second_indices.end());
        }
        std::vector rotation_indices(indices.begin(), indices.end());
        rotation_indices.push_back(1);  // Always need a 1 for the output determination
        cc->EvalRotateKeyGen(keys.secretKey, rotation_indices);
        if (req.needs_eval_sum_key) {
            cc->EvalSumKeyGen(keys.secretKey);
        }
        if (req.needs_eval_sum_cols) {
            auto sum_row_keys = cc->EvalSumColsKeyGen(keys.secretKey);
            cc->InsertEvalSumKey(sum_row_keys, "sum_cols");
        }
    }

    const std::vector<std::vector<double>> inputs = read_csv_matrix(config.file_paths().test_data);

    auto tmp_vec = read_csv_vector(config.file_paths().test_labels);
    const std::vector<int> expected(tmp_vec.begin(), tmp_vec.end());

    for (size_t i = 0; i < inputs.size(); i++) {
        auto result = run(cc, keys, inputs[i], config);
        const int classification = std::get<0>(result);
        if (classification != expected[i]) {
            std::cout << "Test case " << (i + 1) << " failed: expected " << expected[i] << ", got " << classification << std::endl;
            error_count++;
        } else {
            std::cout << "Test case " << (i + 1) << " passed." << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;
        append_results_to_file(config.file_paths().results, expected[i], classification, std::get<1>(result), std::get<2>(result));
    }

    if (error_count == 0) {
        std::cout << "\nAll test cases passed." << std::endl;
    } else {
        std::cout << "\nNumber of failed test cases: " << error_count << std::endl;
    }
    return error_count;
}
