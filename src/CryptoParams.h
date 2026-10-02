#ifndef SARD_NN_PAPER_CRYPTO_PARAMS_H
#define SARD_NN_PAPER_CRYPTO_PARAMS_H

#include "openfhe.h"

class CryptoParams {
   private:
    uint32_t mult_depth;
    uint32_t scale_mod_size;
    uint32_t batch_size;
    lbcrypto::SecurityLevel security_level;
    std::vector<int> rotation_keys;

   public:
    CryptoParams(uint32_t mult_depth, uint32_t scale_mod_size, uint32_t batch_size);
    CryptoParams(uint32_t mult_depth, uint32_t scale_mod_size, uint32_t batch_size, lbcrypto::SecurityLevel security_level);
    CryptoParams(uint32_t mult_depth, uint32_t scale_mod_size, uint32_t batch_size, lbcrypto::SecurityLevel security_level,
                 const std::vector<int> &rotation_keys);

    uint32_t getMultDepth() const;
    uint32_t getScaleModSize() const;
    uint32_t getBatchSize() const;
    lbcrypto::SecurityLevel getSecurityLevel() const;
    std::vector<int> getRotationKeys();

    lbcrypto::CryptoContext<lbcrypto::DCRTPoly> getCryptoContext() const;
};

#endif  // SARD_NN_PAPER_CRYPTO_PARAMS_H
