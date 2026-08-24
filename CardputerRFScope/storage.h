// Persisted settings and saved WiFi credentials (NVS via Preferences).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ui_theme.h"

namespace rfscope {

struct Settings {
    ThemeId theme       = ThemeId::Retro;
    uint32_t dwellMs    = 120;
    bool audioEnabled   = true;
    uint8_t brightness  = 110;  // 0..255
    bool apScanEnabled  = true;
    // Sniff the associated channel while the meter is connected. Gives a
    // local occupancy read-out at the cost of putting the radio in
    // promiscuous mode during a live connection.
    bool localSniff = true;

    void load();
    void save() const;
};

// SSID/password store. NVS keys are limited to 15 characters, so entries are
// keyed by a hash of the SSID and the SSID is stored in the record so a hash
// collision can be detected rather than silently returning a wrong password.
class CredentialStore {
public:
    static bool load(const char* ssid, char* passOut, size_t passOutLen);
    static bool save(const char* ssid, const char* pass);
    static bool forget(const char* ssid);
    static bool has(const char* ssid);
    static void forgetAll();
};

}  // namespace rfscope
