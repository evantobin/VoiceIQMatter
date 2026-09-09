#include "faucet/touch2o_protocol.h"

#include <algorithm>

namespace faucet::protocol {
namespace {

constexpr CommandFrame kHeartbeatFrame = {0xAA, 0x06, 0x09, 0x00, 0x00,
                                           0x15, 0x00, 0x00, 0xFF, 0x67};
constexpr CommandFrame kOpenFrame = {0xAA, 0x06, 0x82, 0x02, 0x00,
                                      0x15, 0x00, 0x00, 0xFE, 0x34};
constexpr CommandFrame kCloseFrame = {0xAA, 0x06, 0x82, 0x00, 0x00,
                                       0x15, 0x00, 0x00, 0x7D, 0x70};
constexpr std::array<uint8_t, kStatusFrameSize> kOpenStatus = {0xAA, 0x03, 0x82, 0x02,
                                                                 0x00, 0x42, 0xD5};
constexpr std::array<uint8_t, kStatusFrameSize> kClosedStatus = {0xAA, 0x03, 0x82, 0x00,
                                                                   0x00, 0x20, 0xB3};

}  // namespace

const CommandFrame &commandFrame(Command command) {
  switch (command) {
    case Command::Open:
      return kOpenFrame;
    case Command::Close:
      return kCloseFrame;
    case Command::Heartbeat:
    default:
      return kHeartbeatFrame;
  }
}

ValveState parseStatusFrame(const uint8_t *frame, size_t length) {
  if (frame == nullptr || length != kStatusFrameSize) return ValveState::Unknown;
  if (std::equal(kOpenStatus.begin(), kOpenStatus.end(), frame)) return ValveState::Open;
  if (std::equal(kClosedStatus.begin(), kClosedStatus.end(), frame)) return ValveState::Closed;
  return ValveState::Unknown;
}

}  // namespace faucet::protocol
