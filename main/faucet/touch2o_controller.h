#pragma once

#include <functional>

namespace faucet {

// The Touch2O status packet is authoritative: it is reported for both Matter
// commands and touch/manual changes made at the faucet.
struct State {
  bool valveOpen = false;
};

class Touch2OController {
 public:
  using StateCallback = std::function<void(const State &)>;

  void begin();
  void poll();
  // Starts outbound handshakes, heartbeats, and commands after Matter has a
  // commissioned fabric. UART status reception remains active before this.
  void enableProtocolTraffic();
  void requestValve(bool open);
  void setStateCallback(StateCallback callback);
  // Called by the serial-frame parser after an exact physical status frame.
  void publishState(bool open);

 private:
  StateCallback stateCallback_;
};

}  // namespace faucet
