#pragma once

namespace web_log {

// Rolling, RAM-only log page for safe under-sink debugging without attaching a
// USB-powered computer to the controller.
void begin();

}  // namespace web_log
