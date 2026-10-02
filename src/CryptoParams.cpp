
#include "CryptoParams.h"

using namespace lbcrypto;

CryptoParams::CryptoParams(const uint32_t mult_depth, const uint32_t scale_mod_size, const uint32_t batch_size)
    : mult_depth(mult_depth), scale_mod_size(scale_mod_size), batch_size(batch_size), security_level(HEStd_128_classic) {
    // Standard default security level applied.
    // rotation_keys remains an empty vector until initialised.
}

CryptoParams::CryptoParams(const uint32_t mult_depth, const uint32_t scale_mod_size, const uint32_t batch_size, const SecurityLevel security_level)
    : mult_depth(mult_depth), scale_mod_size(scale_mod_size), batch_size(batch_size), security_level(security_level) {}

CryptoParams::CryptoParams(const uint32_t mult_depth, const uint32_t scale_mod_size, const uint32_t batch_size, const SecurityLevel security_level,
                           const std::vector<int> &rotation_keys)
    : mult_depth(mult_depth), scale_mod_size(scale_mod_size), batch_size(batch_size), security_level(security_level), rotation_keys(rotation_keys) {}

uint32_t CryptoParams::getMultDepth() const { return mult_depth; }

uint32_t CryptoParams::getScaleModSize() const { return scale_mod_size; }

uint32_t CryptoParams::getBatchSize() const { return batch_size; }

SecurityLevel CryptoParams::getSecurityLevel() const { return security_level; }

std::vector<int> CryptoParams::getRotationKeys() { return rotation_keys; }

CryptoContext<DCRTPoly> CryptoParams::getCryptoContext() const {
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(mult_depth);
    parameters.SetScalingModSize(scale_mod_size);
    parameters.SetBatchSize(batch_size);
    parameters.SetSecurityLevel(security_level);
    auto cc = GenCryptoContext(parameters);

    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);

    return cc;
}
