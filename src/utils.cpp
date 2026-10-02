#include "utils.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

std::vector<std::vector<double>> read_csv_matrix(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::vector<double>> data;

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::vector<double> row;
        std::stringstream ss(line);
        std::string cell;

        while (std::getline(ss, cell, ',')) {
            try {
                row.push_back(std::stod(cell));
            } catch (const std::invalid_argument&) {
                std::cerr << "Warning: Invalid argument encountered when parsing '" << cell << "'\n";
            } catch (const std::out_of_range&) {
                std::cerr << "Warning: Value out of range encountered when parsing '" << cell << "'\n";
            }
        }

        if (!row.empty()) {
            data.push_back(row);
        }
    }

    return data;
}

std::vector<double> read_csv_vector(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    std::vector<double> data;
    std::string line;
    while (std::getline(file, line)) {
        try {
            data.push_back(std::stod(line));
        } catch (const std::invalid_argument&) {
            std::cerr << "Warning: Invalid argument encountered when parsing '" << line << "'\n";
        } catch (const std::out_of_range&) {
            std::cerr << "Warning: Value out of range encountered when parsing '" << line << "'\n";
        }
    }
    return data;
}

void append_results_to_file(const std::string& filename, int expected, int actual, double before_cast, const std::vector<double>& outputs) {
    // Ensure the vector has the required number of elements to prevent out-of-bounds access
    if (outputs.size() < 2) {
        throw std::invalid_argument("The outputs vector must contain at least two elements.");
    }

    // Ensure the destination directory exists before writing
    const std::filesystem::path output_path(filename);
    if (output_path.has_parent_path()) {
        std::filesystem::create_directories(output_path.parent_path());
    }

    // Determine if the file is empty or does not exist
    bool is_file_empty = true;
    std::ifstream in_file(filename);
    if (in_file.is_open()) {
        // If the file opens and the first character is not EOF, it is not empty
        is_file_empty = (in_file.peek() == std::ifstream::traits_type::eof());
        in_file.close();
    }

    // Open the file in append mode
    std::ofstream out_file(filename, std::ios::app);
    if (!out_file.is_open()) {
        throw std::runtime_error("Failed to open the file for writing.");
    }

    // Write the header if the file is fresh
    if (is_file_empty) {
        out_file << "Expected,Actual,Before_Cast,Output_1,Output_2\n";
    }

    // Configure the stream to output the maximum precise decimal digits for a double
    out_file << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10);

    // Append the standard row of data
    out_file << expected << "," << actual << "," << before_cast << "," << outputs[0] << "," << outputs[1] << "\n";
}
