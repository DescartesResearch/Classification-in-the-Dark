# Classification in the Dark: Performance Insights and Novel Methods for Homomorphic Binary Networks 🐟
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23091266.svg)](https://doi.org/10.5281/zenodo.23091266)

Reference implementation and evaluation configurations for homomorphic binary neural-network inference, accompanying the paper:

> **Classification in the Dark: Performance Insights and Novel Methods for Homomorphic Binary Networks**
> Lukas Horn, Lennart Svoboda, Simon Engel, Thomas Prantl, Samuel Kounev
> WAHC 2026 (to appear)

This repository contains the homomorphic implementations of the networks, the matrix-vector multiplication strategies evaluated in this work (the WAHC paper and its SSP companion), the activation-function configurations, and the pinned configuration for every evaluation run across all three datasets.

This repository is the reference implementation for the **WAHC 2026 paper, which is the main paper for this work**. It reports the full study — all three datasets, the activation-function configurations, the classification results, and the end-to-end inference timings. A companion paper, *"Cooking in the Dark: Performance Evaluation of Matrix-Vector Operations in Neural Networks under Homomorphic Encryption"* (SSP 2026), is restricted to matrix-vector multiplication micro-benchmarks on the `energy-efficiency` dataset using the `RowNaive`, `Diagonal`, `Col`, and `Elementwise` methods; it does not cover the other datasets, classification results, or overall timings. `Elementwise` appears only in the SSP paper and is not part of the WAHC paper.

## Overview

The code evaluates homomorphic inference of small binary neural networks (one hidden layer) across three public datasets and several implementation choices:

- **Matrix-vector multiplication methods** (`matrix_multiplication.method`):
  `Diagonal`, `RowNaive`, `RowImproved`, `RowHaleviShoup`, `Col`, and `Elementwise`.
- **Activation-function configurations** (`activation_config.type`):
  - `PolynomialConfig` — Chebyshev/polynomial approximation of the activation.
  - `ActivationesConfig` — iterative "activationes" approximation.
  - `ConstantsConfig` — piecewise constant approximation.
- **Datasets**: `energy-efficiency`, `ionosphere`, and `parkinsons`.
- **Security**: CKKS with `HEStd_128_classic` by default.

Each inference is profiled with the RAII `Profiler` in [`src/profiler.h`](src/profiler.h), which records wall time, CPU time, system time, page faults, context switches, and per-process resident memory. The network emits the per-test classification results to `results/results.csv` and the profiler output to `results/profiler_results/`.

## Prerequisites

- [OpenFHE](https://github.com/openfheorg/openfhe-development) installed and discoverable by CMake (with a `OpenFHEConfig.cmake`)
- A C++20 compiler
- CMake >= 3.16.3
- Network access at configure time: CMake fetches [nlohmann/json](https://github.com/nlohmann/json) v3.11.2 via `FetchContent`. To build offline, pre-populate the FetchContent cache (`FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON`) or vendor the header locally.

## Build

```bash
mkdir -p build && cd build
cmake ..
make -j"$(nproc)"
```

The first `cmake ..` downloads [nlohmann/json](https://github.com/nlohmann/json) v3.11.2 into `build/_deps/` via `FetchContent`, so configure time requires network access. The `classify_dark` executable is produced at `build/classify_dark`.

## Configuration

All run parameters are read from `config.json` in the working directory (the repository root holds a default template). Fields:

| Field | Description |
| --- | --- |
| `file_paths.weights_hidden` / `weights_output` | CSV files with the first/second layer weight matrices |
| `file_paths.bias_hidden` / `bias_output` | CSV files with the first/second layer bias vectors |
| `file_paths.test_data` / `test_labels` | CSV inputs and expected labels |
| `file_paths.results` | Destination CSV for per-test classification output |
| `model_architecture.input_dim` / `hidden_dim` / `output_dim` | Layer dimensions |
| `model_architecture.activation_function` | `ReLU` or `Sigmoid` |
| `crypto_params.multiplicative_depth` | CKKS multiplicative depth |
| `crypto_params.scale_mod_size` | CKKS scaling modulus size |
| `crypto_params.batch_size` | CKKS slot count |
| `crypto_params.security_level` | e.g. `HEStd_128_classic` |
| `activation_config` | Approximation configuration (`PolynomialConfig`, `ActivationesConfig`, or `ConstantsConfig`) |
| `eval_min_params` | Newton-iteration parameters for the min operation |
| `eval_binary_step_params` | Newton-iteration parameters for the binary step / square-root / inverse |
| `matrix_multiplication.method` | Matrix-vector method (see above) |

Every evaluation configuration under `evaluation_configs/` carries its own `config.json` that reproduces that specific run.

## Running

The output directory is created on demand, so no preparation is needed:

```bash
./build/classify_dark
```

The executable reads `./config.json`, runs the full inference over the test set, writes `results/results.csv` to the configured path, and appends profiler rows to `results/profiler_results/`.

### Batch evaluation

[`run_evaluations.sh`](run_evaluations.sh) runs the full evaluation sweep: it walks every `evaluation_configs/**/config.json`, runs `./build/classify_dark` with that configuration, and writes the generated `results.csv` and `profiler_results/` into the corresponding configuration directory. It is self-contained and derives the repository root from its own location. Note that evaluation outputs are not shipped with this repository; they are regenerated by running the sweep.

```bash
./run_evaluations.sh
```

## Datasets

The `datasets/` and `weights/` directories cover three public datasets from the UCI Machine Learning Repository, each licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

**Energy Efficiency**

> Tsanas, A. & Xifara, A. (2012). *Energy Efficiency* [Dataset]. UCI Machine Learning Repository. https://doi.org/10.24432/C51307

Original study: Tsanas, A. & Xifara, A. (2012). Accurate quantitative estimation of energy performance of residential buildings using statistical machine learning tools. *Energy and Buildings*, vol. 49.

**Ionosphere**

> Sigillito, V., Wing, S., Hutton, L., & Baker, K. (1989). *Ionosphere* [Dataset]. UCI Machine Learning Repository. https://doi.org/10.24432/C5W01B

Original study: Sigillito, V. G., Wing, S. P., Hutton, L. V., & Baker, K. B. (1989). Classification of radar returns from the ionosphere using neural networks. *Johns Hopkins APL Technical Digest*, 10, 262–266.

**Parkinsons**

> Little, M. (2007). *Parkinsons* [Dataset]. UCI Machine Learning Repository. https://doi.org/10.24432/C59C74

Original study: Little, M. A., McSharry, P. E., Roberts, S. J., Costello, D. A. E., & Moroz, I. M. (2007). Exploiting nonlinear recurrence and fractal scaling properties for voice disorder detection. *BioMedical Engineering OnLine*, 6:23.

### Modifications

The copies in `datasets/` are prepared versions of the original features; `expected.csv` holds the labels used in this work. Relative to the original datasets:

- **Energy Efficiency**: adapted to a binary classification problem for house energy efficiency. The class is positive (1) if the combined load exceeds the median; otherwise, it is negative (0).
- **Ionosphere** and **Parkinsons**: the labels stem from the original data.

These derived files are redistributed under CC BY 4.0 with the attribution above.

## Repository layout

```
.
├── CMakeLists.txt              # Build definition for the `classify_dark` target
├── config.json                 # Default run configuration (repository root)
├── run_evaluations.sh          # Full evaluation sweep over evaluation_configs/
├── src/                        # All C++ sources and headers
│   ├── config.{h,cpp}              # Configuration loader/parser
│   ├── CryptoParams.{h,cpp}        # CKKS crypto-context construction
│   ├── activation_functions.{h,cpp} # Activation approximations and configuration variants
│   ├── math_op.{h,cpp}             # Min / binary-step / basic HE operations
│   ├── matrix-mults.{h,cpp}        # The six matrix-vector multiplication methods
│   ├── network.cpp                 # Entry point: key generation + full inference loop
│   ├── utils.{h,cpp}               # CSV I/O and result logging helpers
│   └── profiler.h                  # RAII profiler for time/memory/resource metrics
├── weights/                    # Trained weights per dataset and activation
├── datasets/                   # Inputs and expected labels per dataset
├── evaluation_configs/         # Pinned configuration per evaluation run, organised by
│                               #   <dataset>/<method>/<activation_config>/
└── results/                    # Generated run output (created on demand, gitignored)
```

The `evaluation_configs/` directory follows the convention `evaluation_configs/<dataset>/<method>/<activation_config>/`, where each leaf contains the `config.json` used for that run. The dataset directory `ee` abbreviates `energy-efficiency`. Running the sweep regenerates the outputs (`results.csv` and `profiler_results/*.csv`) in place.

Evaluation directory aliases map to configuration values: `diag_mult`/`diagonal` = `Diagonal`, `column`/`column_mult` = `Col`, `row_halevi-shoup` = `RowHaleviShoup`, `row_naive` = `RowNaive`, `row_improved` = `RowImproved`, `elementwise` = `Elementwise`.

## License and attribution

This project is released under the [Apache License 2.0](LICENSE). Redistribution and derivative works must retain the copyright, attribution, and license notices. Source code and weights are Apache-2.0; the derived datasets under `datasets/` are redistributed under CC BY 4.0 (see [Datasets](#datasets)). See [`NOTICE`](NOTICE) for details.

Third-party components:

- [OpenFHE](https://github.com/openfheorg/openfhe-development) is used for the homomorphic encryption backend and is distributed under the BSD 2-Clause License.
- [nlohmann/json](https://github.com/nlohmann/json) (v3.11.2) is fetched at configure time via CMake `FetchContent` and is distributed under the MIT License.

## Authors

| Name    | ORCID iD             |
| ------- | -------------------- |
| Lukas Horn    | [<img src="https://orcid.org/assets/vectors/orcid.logo.icon.svg" alt="ORCID iD" width="16" height="16">](https://orcid.org/0009-0004-4959-1371) [0009-0004-4959-1371](https://orcid.org/0009-0004-4959-1371) |
| Lennart Svoboda | [<img src="https://orcid.org/assets/vectors/orcid.logo.icon.svg" alt="ORCID iD" width="16" height="16">](https://orcid.org/0009-0009-9487-3104) [0009-0009-9487-3104](https://orcid.org/0009-0009-9487-3104) |
| Simon Engel   | [<img src="https://orcid.org/assets/vectors/orcid.logo.icon.svg" alt="ORCID iD" width="16" height="16">](https://orcid.org/0009-0005-3354-4746) [0009-0005-3354-4746](https://orcid.org/0009-0005-3354-4746) |
| Thomas Prantl | [<img src="https://orcid.org/assets/vectors/orcid.logo.icon.svg" alt="ORCID iD" width="16" height="16">](https://orcid.org/0000-0003-4044-8494) [0000-0003-4044-8494](https://orcid.org/0000-0003-4044-8494) |
| Samuel Kounev | [<img src="https://orcid.org/assets/vectors/orcid.logo.icon.svg" alt="ORCID iD" width="16" height="16">](https://orcid.org/0000-0001-9742-2063) [0000-0001-9742-2063](https://orcid.org/0000-0001-9742-2063) |

## Citation

If you use this software or these configurations, please cite the paper and this repository. The WAHC 2026 paper (`HoSvEnPrKo-ClassifyingDark2026`) is the main paper for this work and is the **preferred citation**; the SSP 2026 companion (`HoSvEnPrKo-CookingDark2026`) should be cited in addition only when referring to the matrix-vector multiplication micro-benchmarks or the `Elementwise` results.

```bibtex
@inproceedings{HoSvEnPrKo-ClassifyingDark2026,
  author = {Horn, Lukas and Svoboda, Lennart and Engel, Simon and Prantl, Thomas and Kounev, Samuel},
  booktitle = {WAHC 2026},
  title = {Classification in the Dark: Performance Insights and Novel Methods for Homomorphic Binary Networks},
  year = 2026
}

@inproceedings{HoSvEnPrKo-CookingDark2026,
  author = {Horn, Lukas and Svoboda, Lennart and Engel, Simon and Prantl, Thomas and Kounev, Samuel},
  booktitle = {SSP 2026},
  title = {Cooking in the Dark: Performance Evaluation of Matrix-Vector Operations in Neural Networks under Homomorphic Encryption},
  year = 2026
}
```

<!-- 🐟 a manatee and calf rest here:
     <img src="https://commons.wikimedia.org/wiki/Special:FilePath/Mother_manatee_and_calf.jpg?width=20" alt="mother manatee and calf" width="20" height="13">
     source: https://commons.wikimedia.org/wiki/File:Mother_manatee_and_calf.jpg
     credit: Sam Farkas / NOAA Photo Library, CC BY 2.0, https://creativecommons.org/licenses/by/2.0/ -->
