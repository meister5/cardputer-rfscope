#include "radio_ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <cstdio>
#include <cstring>

namespace rfscope {
namespace {

// Advertisements arrive on the NimBLE host task. Rather than take a lock on
// the device vector there (and allocate in a callback), each report is copied
// into a fixed POD ring and merged later on the UI task.
struct Staged {
    uint8_t addr[6];
    uint8_t addrType;
    int8_t rssi;
    bool connectable;
    char name[24];
};

constexpr size_t kStageSize = 48;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Staged g_stage[kStageSize];
volatile size_t g_stageHead = 0;
volatile size_t g_stageTail = 0;
volatile uint32_t g_advSeen = 0;
volatile uint32_t g_dropped = 0;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override
    {
        if (dev == nullptr) return;

        Staged s;
        const uint8_t* v = dev->getAddress().getVal();
        memcpy(s.addr, v, 6);
        s.addrType    = dev->getAddress().getType();
        s.rssi        = dev->getRSSI();
        s.connectable = dev->isConnectable();

        const std::string n = dev->getName();
        const size_t len    = n.size() < sizeof(s.name) - 1 ? n.size() : sizeof(s.name) - 1;
        memcpy(s.name, n.c_str(), len);
        s.name[len] = '\0';

        portENTER_CRITICAL(&g_mux);
        g_advSeen++;
        const size_t next = (g_stageHead + 1) % kStageSize;
        if (next == g_stageTail) {
            g_dropped++;  // UI task is behind; the rate counter still counts it
        } else {
            g_stage[g_stageHead] = s;
            g_stageHead          = next;
        }
        portEXIT_CRITICAL(&g_mux);
    }
};

ScanCallbacks g_callbacks;

bool popStaged(Staged& out)
{
    bool got = false;
    portENTER_CRITICAL(&g_mux);
    if (g_stageTail != g_stageHead) {
        out         = g_stage[g_stageTail];
        g_stageTail = (g_stageTail + 1) % kStageSize;
        got         = true;
    }
    portEXIT_CRITICAL(&g_mux);
    return got;
}

uint32_t takeAdvCount()
{
    portENTER_CRITICAL(&g_mux);
    const uint32_t n = g_advSeen;
    g_advSeen        = 0;
    portEXIT_CRITICAL(&g_mux);
    return n;
}

}  // namespace

void BleDevice::addrToString(char* out, size_t n) const
{
    // NimBLE stores addresses little-endian; print them the usual way round.
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", addr[5], addr[4], addr[3], addr[2], addr[1],
             addr[0]);
}

bool BleScanner::begin()
{
    if (_inited) return true;
    if (!NimBLEDevice::init("")) return false;
    _inited = true;
    _devices.reserve(MAX_BLE_DEVICES);
    return true;
}

void BleScanner::start()
{
    if (!_inited || _running) return;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_callbacks, /*wantDuplicates=*/true);
    scan->setActiveScan(true);  // ask for scan responses, which carry names
    scan->setInterval(80);
    scan->setWindow(60);
    scan->setDuplicateFilter(false);  // we want every packet, for the rate
    scan->setMaxResults(0);           // callback only; do not retain results
    _running   = scan->start(0, false, true);
    _rateMark  = millis();
    _rateCount = 0;
}

void BleScanner::stop()
{
    if (!_inited || !_running) return;
    NimBLEDevice::getScan()->stop();
    _running = false;
}

uint32_t BleScanner::dropped() const
{
    portENTER_CRITICAL(&g_mux);
    const uint32_t n = g_dropped;
    portEXIT_CRITICAL(&g_mux);
    return n;
}

void BleScanner::clear()
{
    _devices.clear();
}

int BleScanner::findDevice(const uint8_t addr[6]) const
{
    for (size_t i = 0; i < _devices.size(); i++) {
        if (memcmp(_devices[i].addr, addr, 6) == 0) return static_cast<int>(i);
    }
    return -1;
}

void BleScanner::loop()
{
    const uint32_t now = millis();

    Staged s;
    while (popStaged(s)) {
        int idx = findDevice(s.addr);

        if (idx < 0) {
            if (_devices.size() >= MAX_BLE_DEVICES) {
                // Table full: evict the stalest entry so new devices can appear.
                size_t oldest = 0;
                for (size_t i = 1; i < _devices.size(); i++) {
                    if (_devices[i].lastSeen < _devices[oldest].lastSeen) oldest = i;
                }
                _devices.erase(_devices.begin() + static_cast<long>(oldest));
            }
            _devices.push_back(BleDevice{});
            idx = static_cast<int>(_devices.size()) - 1;
            memcpy(_devices[idx].addr, s.addr, 6);
            _devices[idx].addrType = s.addrType;
            _devices[idx].bestRssi = s.rssi;
        }

        BleDevice& d = _devices[idx];
        if (s.name[0] != '\0') memcpy(d.name, s.name, sizeof(d.name));
        d.rssi        = s.rssi;
        d.connectable = s.connectable;
        d.lastSeen    = now;
        d.packets++;
        if (d.bestRssi == RSSI_INVALID || s.rssi > d.bestRssi) d.bestRssi = s.rssi;
        d.smooth.push(static_cast<float>(s.rssi));
    }

    _rateCount += takeAdvCount();
    if (now - _rateMark >= 1000) {
        _advRate   = _rateCount * 1000 / (now - _rateMark);
        _rateCount = 0;
        _rateMark  = now;
    }

    for (size_t i = _devices.size(); i-- > 0;) {
        if (now - _devices[i].lastSeen > BLE_DEVICE_TTL_MS) {
            _devices.erase(_devices.begin() + static_cast<long>(i));
        }
    }
}

}  // namespace rfscope
