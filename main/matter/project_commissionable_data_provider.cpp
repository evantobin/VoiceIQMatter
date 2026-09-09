#include "matter/project_commissionable_data_provider.h"

#include <cstring>

#include <crypto/CHIPCryptoPAL.h>
#include <esp_matter_providers.h>
#include <lib/support/CodeUtils.h>
#include <platform/CommissionableDataProvider.h>

#include "project_config.h"

namespace matter_water_valve {
namespace {

class ProjectCommissionableDataProvider final : public chip::DeviceLayer::CommissionableDataProvider {
 public:
  CHIP_ERROR GetSetupDiscriminator(uint16_t &discriminator) override {
    discriminator = project_config::kMatterDiscriminator;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR SetSetupDiscriminator(uint16_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }

  CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterations) override {
    iterations = project_config::kMatterSpake2pIterations;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &saltBuffer) override {
    constexpr size_t kSaltLength = sizeof(project_config::kMatterSpake2pSalt) - 1;
    VerifyOrReturnError(saltBuffer.size() >= kSaltLength, CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(saltBuffer.data(), project_config::kMatterSpake2pSalt, kSaltLength);
    saltBuffer.reduce_size(kSaltLength);
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifierBuffer, size_t &verifierLength) override {
    uint8_t salt[chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length] = {};
    chip::MutableByteSpan saltSpan(salt);
    ReturnErrorOnFailure(GetSpake2pSalt(saltSpan));

    chip::Crypto::Spake2pVerifier verifier;
    ReturnErrorOnFailure(verifier.Generate(
        project_config::kMatterSpake2pIterations, saltSpan, project_config::kMatterPasscode));
    ReturnErrorOnFailure(verifier.Serialize(verifierBuffer));
    verifierLength = verifierBuffer.size();
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR GetSetupPasscode(uint32_t &passcode) override {
    passcode = project_config::kMatterPasscode;
    return CHIP_NO_ERROR;
  }

  CHIP_ERROR SetSetupPasscode(uint32_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
};

ProjectCommissionableDataProvider sCommissionableDataProvider;

}  // namespace

void installProjectCommissionableDataProvider() {
  esp_matter::set_custom_commissionable_data_provider(&sCommissionableDataProvider);
}

}  // namespace matter_water_valve
