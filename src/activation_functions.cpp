#include "activation_functions.h"

#include "math_op.h"

using lbcrypto::Ciphertext;
using lbcrypto::DCRTPoly;

Ciphertext<DCRTPoly> EvaluateReLU(Ciphertext<DCRTPoly> value, config_option option) {
    auto cc = value->GetCryptoContext();
    return std::visit(
        [cc, value](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, PolynomialConfig>) {
                std::function<double(double)> relu = [](double x) { return std::max(0.0, x); };
                return cc->EvalChebyshevFunction(relu, value, arg.lower, arg.upper, arg.degree);
            } else if constexpr (std::is_same_v<T, ActivationesConfig>) {
                // Formula: 0.5x - sqrt((0.5x)^2)
                const auto first_summand = cc->EvalMult(value, .5);

                const auto beneath_square = cc->EvalSquare(first_summand);  // Reuse the first summand
                const auto second_summand = EvalSqrt(beneath_square, arg.iterations, arg.gradient, arg.axis);

                return cc->EvalAdd(first_summand, second_summand);
            } else if constexpr (std::is_same_v<T, ConstantsConfig>) {
                // max(0, x) = 0.5 * (0 + x) + 0.5 * sqrt((0 - x)^2) = 0.5 * x + 0.5 * |x| =
                // 0.5x + 0.159155x^2 - 0.002135x^4 + 0.003922 (approx) Using 4th degree
                // polynomial approximation for ReLU
                auto x2 = cc->EvalMult(value, value);
                x2 = cc->Rescale(x2);

                auto x4 = cc->EvalMult(x2, x2);
                x4 = cc->Rescale(x4);

                const auto term1 = cc->EvalMult(value, 0.5);

                const auto term2 = cc->EvalMult(x2, 0.159155);

                const auto term3 = cc->EvalMult(x4, -0.002135);

                auto result = cc->EvalAdd(cc->EvalAdd(cc->EvalAdd(term1, term2), term3), 0.003922);

                return result;
            }
        },
        option);
}

Ciphertext<DCRTPoly> EvaluateSigmoid(Ciphertext<DCRTPoly> value, config_option option) {
    auto cc = value->GetCryptoContext();
    return std::visit(
        [cc, value](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, PolynomialConfig>) {
                std::function<double(double)> sigm = [](double x) { return 1.0 / (1 + std::exp(-x)); };
                return cc->EvalChebyshevFunction(sigm, value, arg.lower, arg.upper, arg.degree);
            } else if constexpr (std::is_same_v<T, ActivationesConfig>) {
                // Formula: 1- 1/(exp(x)+1)
                auto denominator = EvalExp(value, arg.terms, 32);
                denominator = cc->EvalAdd(denominator, 1);
                auto inverse = EvalInverse(denominator, arg.iterations, arg.gradient, arg.axis);

                return cc->EvalSub(1, inverse);
            } else if constexpr (std::is_same_v<T, ConstantsConfig>) {
                // No implementation for this case here
                return value;
            }
        },
        option);
}
