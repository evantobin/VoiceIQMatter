#pragma once

namespace faucet {
class Touch2OController;
struct State;
}

namespace matter_water_valve {

bool begin(faucet::Touch2OController &faucet);
bool start();
void publishValveState(const faucet::State &state);
void printCommissioningInfo();

}  // namespace matter_water_valve
