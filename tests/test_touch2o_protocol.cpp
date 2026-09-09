#include <array>
#include <cassert>
#include <cstdint>

#include "faucet/touch2o_protocol.h"

namespace {

using faucet::protocol::Command;
using faucet::protocol::ValveState;

void testCommandFrames() {
  constexpr std::array<uint8_t, 10> kHeartbeat = {0xAA, 0x06, 0x09, 0x00, 0x00,
                                                   0x15, 0x00, 0x00, 0xFF, 0x67};
  constexpr std::array<uint8_t, 10> kOpen = {0xAA, 0x06, 0x82, 0x02, 0x00,
                                              0x15, 0x00, 0x00, 0xFE, 0x34};
  constexpr std::array<uint8_t, 10> kClose = {0xAA, 0x06, 0x82, 0x00, 0x00,
                                               0x15, 0x00, 0x00, 0x7D, 0x70};

  assert(faucet::protocol::commandFrame(Command::Heartbeat) == kHeartbeat);
  assert(faucet::protocol::commandFrame(Command::Open) == kOpen);
  assert(faucet::protocol::commandFrame(Command::Close) == kClose);
}

void testStatusFrames() {
  std::array<uint8_t, 7> open = {0xAA, 0x03, 0x82, 0x02, 0x00, 0x42, 0xD5};
  std::array<uint8_t, 7> closed = {0xAA, 0x03, 0x82, 0x00, 0x00, 0x20, 0xB3};

  assert(faucet::protocol::parseStatusFrame(open.data(), open.size()) == ValveState::Open);
  assert(faucet::protocol::parseStatusFrame(closed.data(), closed.size()) == ValveState::Closed);
}

void testRejectsInvalidStatusFrames() {
  std::array<uint8_t, 7> bad = {0xAA, 0x03, 0x82, 0x02, 0x00, 0x42, 0xD4};

  assert(faucet::protocol::parseStatusFrame(nullptr, 0) == ValveState::Unknown);
  assert(faucet::protocol::parseStatusFrame(bad.data(), bad.size()) == ValveState::Unknown);
  assert(faucet::protocol::parseStatusFrame(bad.data(), bad.size() - 1) == ValveState::Unknown);
}

}  // namespace

int main() {
  testCommandFrames();
  testStatusFrames();
  testRejectsInvalidStatusFrames();
  return 0;
}
