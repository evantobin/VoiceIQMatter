#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace faucet::protocol {

enum class Command : uint8_t { Heartbeat, Open, Close };
enum class ValveState : uint8_t { Unknown, Open, Closed };

using CommandFrame = std::array<uint8_t, 10>;

constexpr uint8_t kFrameHeader = 0xAA;
constexpr uint8_t kStatusFrameId = 0x03;
constexpr uint8_t kCommandFrameId = 0x06;
constexpr size_t kStatusFrameSize = 7;
constexpr size_t kCommandFrameSize = 10;

// Returns the exact wire frame used by the Touch2O solenoid.
const CommandFrame &commandFrame(Command command);

// Accepts only complete, exact status frames. Any other input is Unknown.
ValveState parseStatusFrame(const uint8_t *frame, size_t length);

}  // namespace faucet::protocol
