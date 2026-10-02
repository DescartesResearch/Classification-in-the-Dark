
#include "math_op.h"

#include "float.h"

// Auxiliary function to calculate the straight line
double auxiliary(const double b, const double m, const double y) { return m * b + y; }

double NewtonInv(const double b, const double x_0, const unsigned int i_N) {
    double res = x_0;
    for (unsigned int iter = 0; iter < i_N; iter++) {
        res = res * (2 - res * b);
    }
    return res;
}

std::vector<double> NewtonInvVec(const int iterations, const double gradient, const double axis, const std::vector<double> &values) {
    std::vector<double> res;
    for (const auto val : values) {
        res.push_back(NewtonInv(val, auxiliary(val, gradient, axis), iterations));
    }
    return res;
}

double Error(const double b, const double x_0, const unsigned int i_N) {
    return (1 / b) - NewtonInv(b, x_0, i_N);
}

double ErrorTotal(const double m, const double y, const double o, const double u, const unsigned int s, const unsigned int i_N) {
    double res = 0;
    for (unsigned int sampling_point = 0; sampling_point <= s; sampling_point++) {
        const double b_i = u + (o - u) / s * sampling_point;
        res += std::pow(Error(b_i, auxiliary(b_i, m, y), i_N), 2);
    }
    return res;
}

std::tuple<double, double> findGoodValuesInverse(const double l, const double u, const unsigned int i_N, const unsigned int s,
                                                 const std::vector<double> &gradRange, const std::vector<double> &axisRange) {
    auto current_best_error = DBL_MAX;
    double curr_best_grad = 0, curr_best_axis = 0;

    for (double grad : gradRange) {
        for (double axis : axisRange) {
            double err = ErrorTotal(grad, axis, l, u, s, i_N);

            if (err < current_best_error) {
                current_best_error = err;
                curr_best_grad = grad;
                curr_best_axis = axis;
            }
        }
    }
    std::cout << current_best_error << std::endl;
    return std::make_tuple(curr_best_grad, curr_best_axis);
}

double NewtonSqrt(const double b, const double x_0, const unsigned int i_N) {
    double res = x_0;
    for (unsigned int iter = 0; iter < i_N; iter++) {
        res = res * (1.5 - 0.5 * b * res * res);
    }
    return res * b;
}

std::vector<double> NewtonSqrtVec(const unsigned int iterations, const double gradient, const double axis, const std::vector<double> &values) {
    std::vector<double> res;
    for (auto val : values) {
        res.push_back(NewtonSqrt(val, auxiliary(val, gradient, axis), iterations));
    }
    return res;
}

double ErrorSqrt(const double b, const double x_0, const unsigned int i_N) {
    return std::sqrt(b) - NewtonSqrt(b, x_0, i_N);
}

double ErrorTotalSqrt(const double m, const double y, const double l, const double u, const unsigned int s, const unsigned int i_N) {
    double res = 0;
    for (unsigned int sampling_point = 0; sampling_point <= s; sampling_point++) {
        const double t_i = u + (l - u) / s * sampling_point;
        res += std::pow(ErrorSqrt(t_i, auxiliary(t_i, m, y), i_N), 2);
    }
    return res;
}

std::tuple<double, double> findGoodValuesSqrt(const double l, const double u, const unsigned int i_N, const unsigned int s,
                                              const std::vector<double> &gradRange, const std::vector<double> &axisRange) {
    auto current_best_error = DBL_MAX;
    double curr_best_grad = 0, curr_best_axis = 0;
    for (const double grad : gradRange) {
        for (const double axis : axisRange) {
            if (double err = ErrorTotalSqrt(grad, axis, l, u, s, i_N); err < current_best_error) {
                current_best_error = err;
                curr_best_grad = grad;
                curr_best_axis = axis;
            }
        }
    }
    std::cout << current_best_error << std::endl;
    return std::make_tuple(curr_best_grad, curr_best_axis);
}

Ciphertext<DCRTPoly> EvalSqrt(const Ciphertext<DCRTPoly> &value, const unsigned int iterations, const double gradient, const double axis) {
    const auto context = value->GetCryptoContext();
    Ciphertext<DCRTPoly> xn = context->EvalMult(value, gradient);
    context->EvalAddInPlace(xn, axis);

    const auto half = context->EvalMult(value, 0.5);
    for (unsigned int i = 0; i < iterations; i++) {
        auto second_summand = context->EvalMult(half, context->EvalSquare(xn));
        auto brackets = context->EvalSub(1.5, second_summand);
        xn = context->EvalMult(xn, brackets);
    }
    return context->EvalMult(value, xn);
}

Ciphertext<DCRTPoly> EvalInverse(const Ciphertext<DCRTPoly> &value, const unsigned int iterations, const double gradient, const double axis) {
    const auto context = value->GetCryptoContext();
    Ciphertext<DCRTPoly> xn = context->EvalMult(value, gradient);
    context->EvalAddInPlace(xn, axis);

    for (unsigned int i = 0; i < iterations; i++) {
        auto right = context->EvalMult(value, context->EvalSquare(xn));
        auto left = context->EvalAdd(xn, xn);  // Could be a multiplication with two as well, but addition
                                               // does not require a level
        xn = context->EvalSub(left, right);
    }
    return xn;
}

Ciphertext<DCRTPoly> EvalBinaryStep(const Ciphertext<DCRTPoly> &value, const unsigned int iterationsSqrt, const double gradientSqrt, const double axisSqrt,
                                    const unsigned int iterationsInv, const double gradientInv, const double axisInv) {
    // 0.5 * (x+1/sqrt(x^2) +1)
    const auto cc = value->GetCryptoContext();
    const auto squared = cc->EvalSquare(value);  // Square
    const auto root = EvalSqrt(squared, iterationsSqrt, gradientSqrt, axisSqrt);
    const auto inv = EvalInverse(root, iterationsInv, gradientInv, axisInv);

    auto brackets = cc->EvalMult(value, inv);
    cc->EvalAddInPlace(brackets, 1);
    return cc->EvalMult(brackets, .5);
}

Ciphertext<DCRTPoly> EvalMax(const Ciphertext<DCRTPoly> &first, const Ciphertext<DCRTPoly> &second, const unsigned int iterations, const double gradient,
                             const double axis) {
    const auto cc = first->GetCryptoContext();
    const auto sum = cc->EvalAdd(first, second);
    const auto firstSummand = cc->EvalMult(sum, .5);  // (a + b) / 2

    auto sub = cc->EvalSub(first, second);
    cc->EvalSquareInPlace(sub);  // (a - b) ^ 2
    const auto sqrt = EvalSqrt(sub, iterations, gradient, axis);
    const auto secondSummand = cc->EvalMult(sqrt, .5);  // sqrt((a - b) ^ 2) / 2
    return cc->EvalAdd(firstSummand, secondSummand);
}

Ciphertext<DCRTPoly> EvalMin(const Ciphertext<DCRTPoly> &first, const Ciphertext<DCRTPoly> &second, const unsigned int iterations, const double gradient,
                             const double axis) {
    // Formula: a + b - max(a,b)
    const auto cc = first->GetCryptoContext();
    const auto sum = cc->EvalAdd(first, second);
    return cc->EvalSub(sum, EvalMax(first, second, iterations, gradient, axis));
}


Ciphertext<DCRTPoly> eval_pow(Ciphertext<DCRTPoly> x, int exponent) {
    auto cc = x->GetCryptoContext();
    if (exponent < 1) {
        throw std::invalid_argument("EvalPow can only be used with exponents >= 1");
    }

    if (exponent == 1) {
        return x;
    }

    int n = exponent;

    Ciphertext<DCRTPoly> y;

    bool y_is_one = true;  // This is done such that we do not need to encode 1 as a Ciphertext sole for multiplying it with x
    while (n > 1) {
        if (n % 2 == 0)  // Even
        {
            cc->EvalSquareInPlace(x);
            n /= 2;
        } else  // Odd
        {
            if (y_is_one) {
                y_is_one = false;
                y = x;  // Same as y = x * 1
            } else {
                y = cc->EvalMult(y, x);
            }

            cc->EvalSquareInPlace(x);
            n = (n - 1) / 2;
        }
    }

    // This is done such that we do not need to encode 1 as a Ciphertext.
    if (y_is_one) {
        return x;
    } else {
        return cc->EvalMult(x, y);
    }
}


Ciphertext<DCRTPoly> EvalExp(const Ciphertext<DCRTPoly> &value, int terms, int scale) {
    auto cc = value->GetCryptoContext();
    if (terms == 1) {
        std::cout
            << "Evaluating Exp with one term is not supported. This is a limitation of the encrypted evaluation. See code annotations for more information. "
            << std::endl;
    }
    if (terms < 3 && scale > 1) {
        std::cout << "Scaling may not make sense in this configuration. Terms: " << terms << " Scale: " << scale << std::endl;
    }

    auto x = value;  // Copy input
    if (scale > 1) {
        x = cc->EvalMult(value, static_cast<double>(1) / scale);
    }

    // Second term
    Ciphertext<DCRTPoly> xn = cc->EvalAdd(x, 1.0);  // 1 + x ...

    // Third term and onward: i is the exponent which is (term - 1)
    for (int i = 2; i < terms; i++) {
        double factor = 1 / std::pow(std::tgamma(i + 1), 1.0 / i);
        auto inner = cc->EvalMult(x, factor);  // + 1
        auto term = eval_pow(inner, i);        // + depth_pow
        cc->EvalAddInPlace(xn, term);
    }

    // Backscaling step (If no scaling is applied this step has no overhead)
    auto result = eval_pow(xn, scale);
    return result;
}
