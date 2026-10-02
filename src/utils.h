#ifndef SARD_NN_PAPER_UTILS_H
#define SARD_NN_PAPER_UTILS_H
#include <string>
#include <vector>

std::vector<std::vector<double>> read_csv_matrix(const std::string& filename);
std::vector<double> read_csv_vector(const std::string& filename);
void append_results_to_file(const std::string& filename, int expected, int actual, double before_cast,
                            const std::vector<double>& outputs);  // For final results

#endif  // SARD_NN_PAPER_UTILS_H
