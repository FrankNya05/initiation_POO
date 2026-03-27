#pragma once
#include <cstdint>

namespace BLE_UUID {
    constexpr const char* SERVICE  = "12345678-1234-5678-1234-56789abcdef0";
    constexpr const char* CHAR_TX  = "12345678-1234-5678-1234-56789abcdef1";  // ESP32 → RPi (Notify)
    constexpr const char* CHAR_RX  = "12345678-1234-5678-1234-56789abcdef2";  // RPi   → ESP32 (Write)
}