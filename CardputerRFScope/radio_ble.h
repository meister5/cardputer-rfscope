// BLE observer.
//
// The ESP32-S3 has no Bluetooth Classic radio, and a BLE scanner only ever
// hears the three advertising channels (37/38/39, at 2402/2426/2480 MHz). So
// there is no 40-channel BLE sweep to be had: what we can report honestly is
// per-device RSSI and how busy the advertising channels are.
#pragma once

#include <cstdint>
#include <vector>

#include "rfscope_config.h"
#include "signal_stats.h"

namespace rfscope {

struct BleDevice {
    uint8_t addr[6]   = {0};
    uint8_t addrType  = 0;
    char name[24]     = {0};
    int8_t rssi       = RSSI_INVALID;
    int8_t bestRssi   = RSSI_INVALID;
    uint32_t packets  = 0;
    uint32_t lastSeen = 0;
    bool connectable  = false;
    Ewma smooth{0.35f};

    bool hasName() const
    {
        return name[0] != '\0';
    }
    void addrToString(char* out, size_t n) const;
};

class BleScanner {
public:
    // Brings up the NimBLE stack. Costs heap, so it is only called when a BLE
    // screen is first opened.
    bool begin();
    bool available() const
    {
        return _inited;
    }

    void start();
    void stop();
    bool running() const
    {
        return _running;
    }

    // Merges staged advertisements and ages out devices. Call from the UI loop.
    void loop();

    const std::vector<BleDevice>& devices() const
    {
        return _devices;
    }
    void clear();

    // Advertising-channel packet rate, averaged over the last second.
    uint32_t advRate() const
    {
        return _advRate;
    }

    // Advertisements the UI task was too slow to collect. Non-zero here means
    // the frame rate is starving the BLE staging ring.
    uint32_t dropped() const;

    // Index of a device by address, or -1.
    int findDevice(const uint8_t addr[6]) const;

private:
    std::vector<BleDevice> _devices;
    bool _inited        = false;
    bool _running       = false;
    uint32_t _advRate   = 0;
    uint32_t _rateMark  = 0;
    uint32_t _rateCount = 0;
};

}  // namespace rfscope
