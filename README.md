# Build a Matter controller for a Delta Touch2O faucet

This is a direct, local replacement for the Delta **VoiceIQ** module. A Seeed
Studio XIAO ESP32-C6 connects directly to the Touch2O solenoid's RJ45 jack,
speaks the reverse-engineered local protocol, and exposes the faucet as a
standard Matter Water Valve.

No Delta cloud account. No Alexa or Google account. No Wi-Fi password in source
code. The original capacitive touch controls and manual faucet handle remain in
service; their valve-state changes are reported back through Matter.

I wrote this as the guide to follow before disconnecting the VoiceIQ module.

**Jump to:** [Shopping list](#shopping-list) | [Wiring](#wiring) |
[Build and flash](#build-and-flash) | [Matter pairing](#first-boot-and-matter-pairing) |
[Troubleshooting](#troubleshooting)

## What it does

Your Matter controller gets local Open and Close control as a Water Valve
endpoint. Its reported state comes from the exact Touch2O status frame, not an
optimistic software assumption. Touching the faucet, using the handle, or using
Matter all keep the reported physical valve state aligned.

The published serial protocol supplies binary valve commands and state only.
It does not supply water temperature. The VoiceIQ module contains a separate
Hall-effect **flow** sensor, but using it would require hardware modification
to the removed module and is deliberately outside this first version.

## Compatibility

This project targets the Delta Touch2O solenoid-to-VoiceIQ connection described
by Vitaliy Kholyavenko's reverse engineering work. It is for Touch2O faucets
with the same black, VoiceIQ-compatible solenoid and 8P8C (RJ45-shaped) cable,
not every Delta faucet.

| Situation | Expectation |
| --- | --- |
| Touch2O solenoid using the known VoiceIQ RJ45 interface | Expected target; meter the pins before wiring. |
| Different, older, or non-VoiceIQ solenoid | Do not connect until its pinout is independently verified. |
| Network switch, Ethernet injector, or PoE equipment | Never connect it. The jack is not Ethernet. |

## The idea

```text
┌──────────────────┐    Matter over Wi-Fi     ┌─────────────────┐  3.3 V UART + handshake  ┌──────────────────┐
│ Matter controller │ ◀──────────────────────▶ │ XIAO ESP32-C6   │ ◀────────────────────────▶ │ Touch2O solenoid │
│  (Apple / Google /│  BLE only while pairing  │ ESP-IDF + Matter │        9600 baud           │ touch + valve    │
│   Home Assistant) │                          └────────┬────────┘                           └───────┬──────────┘
└──────────────────┘                                   │                                              │
                                                        │ 5 V                                          │ 9 V on RJ45 pin 1
                                                   ┌────▼─────┐                                        │
                                                   │ 9 V→5 V  │◀───────────────────────────────────────┘
                                                   │   buck   │
                                                   └──────────┘
```

The RJ45-shaped cable carries a proprietary low-voltage interface, **not
Ethernet**. Wi-Fi credentials are supplied by your Matter controller during
commissioning and stored in the XIAO's flash.

## Shopping list

| Part | Use |
| --- | --- |
| Seeed Studio XIAO ESP32-C6 | 4 MB board with 3.3 V GPIO. |
| 9 V-to-5 V buck converter | Set and meter-verify a 5.0 V output before connecting the XIAO. |
| Female 8P8C/RJ45 breakout | For the Touch2O cable; it must never be connected to Ethernet equipment. |
| Enclosure, insulated hookup wire, heat-shrink | Keep exposed low-voltage connections protected under the sink. |
| Multimeter | Required for checking cable orientation and voltage before connection. |

## Safety first

Disconnect the VoiceIQ module before connecting this controller. Work only on
the low-voltage Touch2O cable and the buck converter; keep clear of outlet and
other mains wiring under the sink.

> ## ⚠️ Never connect USB-C and Touch2O power at the same time
>
> **Do not plug the XIAO into a computer over USB-C while the buck converter is
> connected to its `5V/VBUS` pin.** The two 5 V sources can back-feed each
> other. Use USB-C only to flash and record the Matter pairing code, unplug it
> fully, then let the Touch2O RJ45 pin 1 power the controller through the buck.

## Wiring

This table uses the common **T568B** cable colors. With a normal straight
T568B cable, each color below reaches the same-numbered pin at the breakout.
Confirm the breakout's pin numbering and meter the cable before applying power.

| RJ45 pin | T568B color | Measured / documented function | Connect to |
| ---: | --- | --- | --- |
| 1 | White/orange | 9 V for VoiceIQ | Buck `IN+` |
| 2 | Orange | Unknown; measured ~3.3 V | Leave individually insulated |
| 3 | White/green | Touch2O status TX, 3.3 V | XIAO D6 / GPIO16 (`UART1 RX`) |
| 4 | Blue | VoiceIQ Enable, 3.3 V | Leave individually insulated |
| 5 | White/blue | Ground | Buck `IN−`, buck `OUT−`, and XIAO `GND` |
| 6 | Green | Touch2O command RX, 3.3 V | XIAO D7 / GPIO17 (`UART1 TX`) |
| 7 | White/brown | Shared request/acknowledge, 3.3 V | XIAO D3 / GPIO21 (open drain) |
| 8 | Brown | Unknown / unused | Leave individually insulated |

```text
Touch2O RJ45 pin 1 (white/orange, 9 V) ────> buck IN+
Touch2O RJ45 pin 5 (white/blue, GND) ──────> buck IN− ────┬──> buck OUT− ──> XIAO GND
                                                            │
                                                buck OUT+ (5 V) ──────────> XIAO 5V / VBUS

Touch2O RJ45 pin 3 (white/green, TX) ─────────────────────> XIAO D6 / GPIO16, UART1 RX
Touch2O RJ45 pin 6 (green, RX)       <───────────────────── XIAO D7 / GPIO17, UART1 TX
Touch2O RJ45 pin 7 (white/brown, HS) <────────────────────> XIAO D3 / GPIO21, open drain

Pins 2, 4, and 8: no connection. Insulate each wire separately.
```

The serial signals are 3.3 V logic, so do **not** use a 5 V level shifter. Pin
7 is a bidirectional/open-drain signal. The firmware releases it high with an
internal pull-up and only actively drives the required low request pulse.

## Build and flash

Install ESP-IDF 6.x and a compatible ESP-Matter checkout. This project follows
the native ESP-IDF/ESP-Matter structure used by MideaMatter: it does not use
Arduino, PlatformIO, HomeSpan, or a cloud service.

```sh
git clone git@github.com:evantobin/VoiceIQMatter.git
cd VoiceIQMatter
. ./env.sh
idf.py build
idf.py -p /dev/cu.usbmodemXXXX flash
idf.py -p /dev/cu.usbmodemXXXX monitor
```

`env.sh` defaults to ESP-IDF at `~/.espressif/v6.0.2/esp-idf` and ESP-Matter
at `~/esp/esp-matter`. Set `ESP_IDF_EXPORT` and/or `ESP_MATTER_PATH` first if
yours are elsewhere.

## First boot and Matter pairing

1. Keep the Touch2O cable and buck **disconnected**. Power the XIAO from USB-C
   only, flash the project, and open the monitor at 115200 baud.
2. On first boot, record the printed manual pairing code and QR-code URL.
3. Unplug USB-C completely.
4. With power removed, make the RJ45 and buck wiring above. Meter 5.0 V on the
   buck output before connecting it to `5V/VBUS`.
5. Connect the Touch2O cable. It now provides the controller's only power.
6. In your Matter controller, add the accessory with the saved QR code or
   manual pairing code. It supplies Wi-Fi credentials over BLE.
7. Test Matter Open, Matter Close, a faucet touch, and the manual handle one at
   a time. Confirm the Matter state follows all of them.

## Bring-up checklist

1. Verify pin 1 is about 9 V relative to pin 5 and buck output is 5.0 V.
2. Verify pins 2, 4, and 8 have no connection to the XIAO or buck.
3. Pair the Matter device before exposing it to normal automation use.
4. Open the faucet through Matter and confirm the log receives the documented
   open status frame.
5. Close it by touch, then verify Matter reports Closed.
6. Test the manual handle. The handle must be in its normal operating position
   for electronically controlled water flow.

## Local debug log

After Matter commissioning brings Wi-Fi up, open `http://<device-ip>/` on your
local network. It provides a rolling RAM-only log that refreshes every second,
so USB-C is not required for observing commands, heartbeats, and valve state.

The page has no authentication—keep it on your LAN and do not expose it to the
internet.

## Protocol reference

The firmware implements the published 9600 baud, 8N1, 3.3 V protocol. It sends
a 500 µs low request pulse on pin 7, releases the line, waits 200 µs, then
sends a frame. A heartbeat is sent every five seconds.

| Purpose | Frame |
| --- | --- |
| Heartbeat | `AA 06 09 00 00 15 00 00 FF 67` |
| Open valve | `AA 06 82 02 00 15 00 00 FE 34` |
| Close valve | `AA 06 82 00 00 15 00 00 7D 70` |
| Valve open status | `AA 03 82 02 00 42 D5` |
| Valve closed status | `AA 03 82 00 00 20 B3` |

## Troubleshooting

| Symptom | Check |
| --- | --- |
| XIAO does not boot from the faucet | Verify 9 V on pin 1 relative to pin 5, regulated 5 V at `5V/VBUS`, and common ground. |
| Matter command does not move the valve | Confirm pin 6 goes to D7/GPIO17 and pin 7 goes to D3/GPIO21; do not connect pins 2 or 4. |
| State never changes in Matter | Confirm pin 3 reaches D6/GPIO16 and inspect the local debug page for status frames. |
| Faucet touch stops working | Recheck that only the prescribed pins are connected and that USB-C is unplugged while buck power is present. |
| A wire color disagrees with this guide | Stop. Trust pin number and meter results, not cable color. |

## Provenance

The protocol, signal mapping, command frames, and handshake timing follow
Vitaliy Kholyavenko's published reverse-engineering work:

* [Complete pinout and ESP32 sketch (Hubitat post 33)](https://community.hubitat.com/t/delta-voiceiq-integration-killed/158256/33)
* [Original protocol confirmation and local-control update](https://community.hubitat.com/t/delta-voiceiq-integration-v1-killed-v2-still-alive/158256)

Pin 2 remains undocumented and pin 4 is only labeled `VoiceIQ Enable`; neither
is needed by the published working bypass sketch or this implementation.
