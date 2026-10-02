#include "matrix-mults.h"

#include <cmath>

using lbcrypto::Ciphertext;
using lbcrypto::CryptoContext;
using lbcrypto::DCRTPoly;
using lbcrypto::Plaintext;

// Required rotation keys: 1,...,input_size
Ciphertext<DCRTPoly> EvalMatVecDiagonal(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights, const unsigned int input_size,
                                        const unsigned int output_size) {
    const auto cc = inputVec->GetCryptoContext();
    Ciphertext<DCRTPoly> accumulator = nullptr;

    Ciphertext<DCRTPoly> input;
    // If we do not replicate in cases smaller than the batch size (assuming there are enough slots for the whole vector), the rotation does no longer work as
    // it rotates unused slots into the calculation
    if (input_size < cc->GetCryptoParameters()->GetBatchSize()) {
        const std::vector mask(input_size, 1.0);
        input = cc->EvalMult(inputVec, cc->MakeCKKSPackedPlaintext(mask));
        const auto rotated_copy = cc->EvalRotate(inputVec, -static_cast<int>(input_size));
        input = cc->EvalAdd(input, rotated_copy);
    } else {
        input = inputVec;
    }

    // For every diagonal
    for (unsigned int d = 0; d < input_size; d++) {
        std::vector<double> diagonal(output_size, 0.0);

        for (unsigned int i = 0; i < output_size; i++) {
            if (unsigned const int j = i + d; j < input_size) {
                diagonal[i] = weights[i][j];
            } else {
                diagonal[i] = weights[i][j - input_size];
            }
        }

        Plaintext diag_plain = cc->MakeCKKSPackedPlaintext(diagonal);

        // rotate input vector by d
        Ciphertext<DCRTPoly> rotated = (d == 0) ? input : cc->EvalRotate(input, d);

        auto product = cc->EvalMult(rotated, diag_plain);

        if (!accumulator) {
            accumulator = product;
        } else {
            cc->EvalAddInPlace(accumulator, product);
        }
    }

    return accumulator;
}

/*
 * Required Rotation Keys
 * Let N = input_size and M = output_size.
 *
 * 1. Replication (Right shifts to duplicate the input vector):
 * -N, -2N, -4N...
 *
 * 2. Summation (Left shifts for the binary tree addition):
 * 1, 2, 4... (up to N)
 *
 * 3. Condensation (Left shifts to pack the scattered results):
 * (N - 1), 2 * (N - 1), 3 * (N - 1)... (up to M - 1)
 */
Ciphertext<DCRTPoly> EvalMatVecRowImproved(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights, const unsigned int input_size,
                                           const unsigned int output_size, const std::string& eval_key_map_key) {
    const auto cc = inputVec->GetCryptoContext();

    unsigned int pow2_size = input_size;

    // Evaluate if input_size is not a power of two and if so, set pow2 accordingly to ensure a correct binary tree multiplication in the for loop
    if ((input_size & (input_size - 1)) != 0) {
        pow2_size = 1;
        while (pow2_size < input_size) {
            pow2_size *= 2;
        }
    }

    // Flatten the rows into a single vector
    std::vector<double> matrix_flattened;
    matrix_flattened.reserve(pow2_size * output_size);
    for (const auto& row : weights) {
        matrix_flattened.insert(matrix_flattened.end(), row.begin(), row.end());
        if (row.size() < pow2_size) {
            unsigned int counter = row.size();
            while (counter < pow2_size) {
                matrix_flattened.push_back(0);
                counter++;
            }
        }
    }

    // Mask to isolate the first inputSize elements
    std::vector mask(pow2_size, 1.0);

    // Initialise the replicated ciphertext correctly
    auto input_replicated = cc->EvalMult(inputVec, cc->MakeCKKSPackedPlaintext(mask));

    // Logarithmic replication
    const unsigned int rotations = std::ceil(std::log2(output_size));
    for (unsigned int i = 0; i < rotations; i++) {
        int shift = -(pow2_size << i);
        auto rotated = cc->EvalRotate(input_replicated, shift);
        cc->EvalAddInPlace(input_replicated, rotated);
    }

    const auto multiplied = cc->EvalMult(input_replicated, cc->MakeCKKSPackedPlaintext(matrix_flattened));

    // Utilise EvalSumCols to sum across each inputsize-element block
    const auto sum = cc->EvalSumCols(multiplied, pow2_size, cc->GetEvalSumKeyMap(eval_key_map_key));

    // Mask the sum to strictly isolate the results at indices 0, inputsize, 2*inputsize, 3*inputsize, ...
    std::vector pack_mask(pow2_size * output_size, 0.0);
    for (unsigned int i = 0; i < output_size; i++) {
        pack_mask[i * pow2_size] = 1.0;
    }
    const auto masked_sum = cc->EvalMult(sum, cc->MakeCKKSPackedPlaintext(pack_mask));

    Ciphertext<DCRTPoly> accumulator =
        masked_sum->Clone();  // Otherwise we also rotate the masked sum every time and get partly wrong summations in the last slot (double the result)

    // Condense the sparse output into a dense vector at the beginning of the slots
    for (unsigned int i = 1; i < output_size; i++) {
        const int shift = (i * pow2_size) - i;
        auto rotated = cc->EvalRotate(masked_sum, shift);
        cc->EvalAddInPlace(accumulator, rotated);
    }

    return accumulator;
}

// Rotation Keys: -rows,...,-1 + -inputsize/2,-inputsize/4, ..., -1
Ciphertext<DCRTPoly> EvalMatVecRowNaive(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights, const unsigned int input_size) {
    const auto cc = inputVec->GetCryptoContext();
    Ciphertext<DCRTPoly> accumulator;

    unsigned int pow2_size = input_size;

    // Evaluate if input_size is not a power of two and if so, set pow2 accordingly to ensure a correct binary tree multiplication in the for loop
    if ((input_size & (input_size - 1)) != 0) {
        pow2_size = 1;
        while (pow2_size < input_size) {
            pow2_size *= 2;
        }
    }

    std::vector mask(input_size, 0.0);
    mask[0] = 1.0;
    Plaintext ptx_mask = cc->MakeCKKSPackedPlaintext(mask);

    for (unsigned int row = 0; row < weights.size(); row++) {
        auto multiplied = cc->EvalMult(inputVec, cc->MakeCKKSPackedPlaintext(weights[row]));
        auto sum = multiplied;
        for (int rot = pow2_size / 2; rot >= 1; rot /= 2) {
            sum = cc->EvalAdd(sum, cc->EvalRotate(sum, rot));
        }

        auto multiplied_mask = cc->EvalMult(sum, ptx_mask);

        if (!accumulator) {
            accumulator = multiplied_mask;
        } else {
            multiplied_mask = cc->EvalRotate(multiplied_mask, -static_cast<int>(row));
            cc->EvalAddInPlace(accumulator, multiplied_mask);
        }
    }

    return accumulator;
}

Ciphertext<DCRTPoly> EvalMatVecRowHaleviShoup(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights,
                                              const unsigned int input_size) {
    const auto cc = inputVec->GetCryptoContext();

    // Ensure pow2_size is a power of two for the binary tree
    unsigned int pow2_size = 1;
    while (pow2_size < input_size) {
        pow2_size *= 2;
    }

    // Step 1: Compute p_i = v * r_i for all rows
    std::vector<Ciphertext<DCRTPoly>> p(pow2_size);
    for (unsigned int row = 0; row < weights.size(); ++row) {
        auto pt_row = cc->MakeCKKSPackedPlaintext(weights[row]);
        p[row] = cc->EvalMult(inputVec, pt_row);
    }

    // Pad any remaining rows up to pow2_size with zeros
    std::vector<double> zero_vec(pow2_size, 0.0);
    auto pt_zero = cc->MakeCKKSPackedPlaintext(zero_vec);
    for (unsigned int row = weights.size(); row < pow2_size; ++row) {
        p[row] = cc->EvalMult(inputVec, pt_zero);
    }

    // Step 2: Reverse replication tree (fan-in accumulation)
    for (unsigned int k = 1; (1U << (k - 1)) < pow2_size; ++k) {
        unsigned int shift = 1U << (k - 1);
        unsigned int block_size = 1U << k;
        std::vector<Ciphertext<DCRTPoly>> next_p(pow2_size / block_size);

        // Generate alternating masks for the left and right branches
        std::vector<double> mask_l_vec(pow2_size, 0.0);
        std::vector<double> mask_r_vec(pow2_size, 0.0);
        for (unsigned int i = 0; i < pow2_size; ++i) {
            if ((i % block_size) < shift) {
                mask_l_vec[i] = 1.0;
            } else {
                mask_r_vec[i] = 1.0;
            }
        }
        auto pt_mask_l = cc->MakeCKKSPackedPlaintext(mask_l_vec);
        auto pt_mask_r = cc->MakeCKKSPackedPlaintext(mask_r_vec);

        // Merge adjacent pairs of ciphertexts
        for (unsigned int i = 0; i < pow2_size / block_size; ++i) {
            auto p_l = p[2 * i];
            auto p_r = p[2 * i + 1];

            // Left branch: rotate positive, apply mask
            auto x_l = cc->EvalAdd(p_l, cc->EvalRotate(p_l, shift));
            auto y_l = cc->EvalMult(x_l, pt_mask_l);

            // Right branch: rotate negative, apply mask
            auto x_r = cc->EvalAdd(p_r, cc->EvalRotate(p_r, -static_cast<int>(shift)));
            auto y_r = cc->EvalMult(x_r, pt_mask_r);

            // Fan-in addition
            next_p[i] = cc->EvalAdd(y_l, y_r);
        }

        // Update the working set for the next level of the tree
        p = next_p;
    }

    // The final accumulated ciphertext sits at the root of the tree
    return p[0];
}

// Rotation Keys: -output_size, ..., -1 + 1,...,n
// This is required for the column-wise multiplication and efficiently replicates indices along slots
std::vector<Ciphertext<DCRTPoly>> hybridReplicate(const Ciphertext<DCRTPoly>& inputVec, const unsigned int amount_elements, const unsigned int output_size) {
    const auto cc = inputVec->GetCryptoContext();
    unsigned int giantsteps = std::ceil(std::sqrt(amount_elements));
    std::vector<Ciphertext<DCRTPoly>> output;

    // Precompute baby masks to avoid redundant plaintext encoding in the inner loop later on
    std::vector<Plaintext> precomputed_baby_masks(giantsteps);
    for (unsigned int b = 0; b < giantsteps; b++) {
        std::vector<double> baby_mask(amount_elements, 0.0);
        baby_mask[b] = 1.0;
        precomputed_baby_masks[b] = cc->MakeCKKSPackedPlaintext(baby_mask);
    }

    for (unsigned int step = 0; step < giantsteps; step++) {
        std::vector<double> mask(amount_elements, 0.0);
        unsigned int elements_in_step = 0;

        for (unsigned int m = step * giantsteps; m < (step + 1) * giantsteps && m < amount_elements; m++) {
            mask[m] = 1.0;
            elements_in_step++;
        }

        auto to_babystep = cc->EvalMult(inputVec, cc->MakeCKKSPackedPlaintext(mask));

        if (step > 0) {
            to_babystep = cc->EvalRotate(to_babystep, step * giantsteps);
        }

        for (unsigned int b = 0; b < elements_in_step; b++) {
            auto element = cc->EvalMult(to_babystep, precomputed_baby_masks[b]);

            // Shift the isolated element from index b to index 0
            if (b > 0) {
                element = cc->EvalRotate(element, b);
            }

            // Execute the binary broadcast tree
            for (unsigned int rot = 1; rot < output_size; rot *= 2) {
                auto rotated = cc->EvalRotate(element, -rot);
                cc->EvalAddInPlace(element, rotated);
            }

            output.push_back(element);
        }
    }

    return output;
}

Ciphertext<DCRTPoly> EvalMatVecCol(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights, const unsigned int input_size,
                                   const unsigned int output_size) {
    const auto cc = inputVec->GetCryptoContext();

    // Get the replicated elements of the vector to use them later on
    auto replicated_elements = hybridReplicate(inputVec, input_size, output_size);

    // "Transpose" the matrix to get the columns
    std::vector<std::vector<double>> columns(weights[0].size());
    for (unsigned int row = 0; row < weights.size(); row++) {
        for (unsigned int col = 0; col < weights[row].size(); col++) {
            columns[col].push_back(weights[row][col]);
        }
    }
    // Multiply the replicated elements with the corresponding columns and then sum them up
    Ciphertext<DCRTPoly> output = nullptr;
    for (unsigned int col = 0; col < columns.size(); col++) {
        auto multiplied = cc->EvalMult(replicated_elements[col], cc->MakeCKKSPackedPlaintext(columns[col]));
        if (!output) {
            output = multiplied;
        } else {
            cc->EvalAddInPlace(output, multiplied);
        }
    }
    return output;
}

Ciphertext<DCRTPoly> EvalMatVecElementwise(const Ciphertext<DCRTPoly>& inputVec, const std::vector<std::vector<double>>& weights, const unsigned int input_size,
                                           const unsigned int output_size) {
    const auto cc = inputVec->GetCryptoContext();

    // Precompute masks: for each j, a vector with 1.0 at position j and 0 elsewhere
    std::vector<Plaintext> isolation_masks(input_size);
    for (unsigned int j = 0; j < input_size; j++) {
        std::vector<double> mask(input_size, 0.0);
        mask[j] = 1.0;
        isolation_masks[j] = cc->MakeCKKSPackedPlaintext(mask);
    }

    Ciphertext<DCRTPoly> accumulator = nullptr;

    for (unsigned int i = 0; i < output_size; i++) {
        Ciphertext<DCRTPoly> row_sum = nullptr;

        for (unsigned int j = 0; j < input_size; j++) {
            // Isolate element j of the input vector
            auto isolated = cc->EvalMult(inputVec, isolation_masks[j]);

            // Multiply by weights[i][j]
            std::vector<double> weight_vec(input_size, weights[i][j]);
            auto weighted = cc->EvalMult(isolated, cc->MakeCKKSPackedPlaintext(weight_vec));

            // Rotate element j to slot 0
            if (j > 0) {
                weighted = cc->EvalRotate(weighted, -static_cast<int>(j));
            }

            // Accumulate into row sum (slot 0 holds the running dot product)
            if (!row_sum) {
                row_sum = weighted;
            } else {
                cc->EvalAddInPlace(row_sum, weighted);
            }
        }

        // Move row result from slot 0 to slot i
        if (i > 0) {
            row_sum = cc->EvalRotate(row_sum, static_cast<int>(i));
        }

        // Add to the final accumulator
        if (!accumulator) {
            accumulator = row_sum;
        } else {
            cc->EvalAddInPlace(accumulator, row_sum);
        }
    }

    return accumulator;
}

RotationKeyRequirements GetRotationKeyRequirements(MatVecMethod method, uint32_t input_size, uint32_t output_size) {
    RotationKeyRequirements req;
    switch (method) {
        case MatVecMethod::Diagonal: {
            for (uint32_t d = 1; d < input_size; d++) {
                req.rotation_indices.insert(static_cast<int>(d));
            }
            req.rotation_indices.insert(-static_cast<int>(input_size));
            break;
        }
        case MatVecMethod::RowNaive: {
            unsigned int pow2_size = input_size;
            if ((input_size & (input_size - 1)) != 0) {
                pow2_size = 1;
                while (pow2_size < input_size) {
                    pow2_size *= 2;
                }
            }
            for (int rot = static_cast<int>(pow2_size) / 2; rot >= 1; rot /= 2) {
                req.rotation_indices.insert(rot);
            }
            for (uint32_t r = 1; r < output_size; r++) {
                req.rotation_indices.insert(-static_cast<int>(r));
            }
            break;
        }
        case MatVecMethod::RowImproved: {
            unsigned int pow2_size = input_size;

            // Evaluate if input_size is not a power of two and if so, set pow2 accordingly to ensure a correct binary tree multiplication in the for loop
            if ((input_size & (input_size - 1)) != 0) {
                pow2_size = 1;
                while (pow2_size < input_size) {
                    pow2_size *= 2;
                }
            }
            // Replication: -N, -2N, -4N, ... up to -N * 2^(ceil(log2(M))-1)
            unsigned int rotations = std::ceil(std::log2(output_size));
            for (unsigned int i = 0; i < rotations; i++) {
                req.rotation_indices.insert(-static_cast<int>(pow2_size << i));
            }
            // Condensation: (N-1), 2*(N-1), 3*(N-1), ... up to (M-1)*(N-1)
            for (unsigned int i = 1; i < output_size; i++) {
                req.rotation_indices.insert(static_cast<int>(i * pow2_size - i));
            }
            req.needs_eval_sum_cols = true;
            break;
        }
        case MatVecMethod::RowHaleviShoup: {
            unsigned int pow2_size = 1;
            while (pow2_size < input_size) {
                pow2_size *= 2;
            }
            for (unsigned int k = 1; (1U << (k - 1)) < pow2_size; ++k) {
                int s = static_cast<int>(1U << (k - 1));
                req.rotation_indices.insert(s);
                req.rotation_indices.insert(-s);
            }
            break;
        }
        case MatVecMethod::Elementwise: {
            for (unsigned int j = 1; j < input_size; j++) {
                req.rotation_indices.insert(-static_cast<int>(j));
            }
            for (unsigned int i = 1; i < output_size; i++) {
                req.rotation_indices.insert(static_cast<int>(i));
            }
            break;
        }
        case MatVecMethod::Col: {
            // Output shifts: -output_size, ..., -1
            for (uint32_t r = 1; r <= output_size; r++) {
                req.rotation_indices.insert(-static_cast<int>(r));
            }
            // hybridReplicate internal rotations
            uint32_t giantsteps = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(input_size))));
            for (uint32_t step = 1; step < giantsteps; step++) {
                req.rotation_indices.insert(static_cast<int>(step * giantsteps));
            }
            for (uint32_t b = 1; b < giantsteps; b++) {
                req.rotation_indices.insert(static_cast<int>(b));
            }
            for (uint32_t rot = 1; rot < output_size; rot *= 2) {
                req.rotation_indices.insert(-static_cast<int>(rot));
            }
            break;
        }
    }
    return req;
}
