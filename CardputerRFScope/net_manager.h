// Association lifecycle for the RSSI meter: pick a network, supply a
// password if it needs one, connect, and report link state.
#pragma once

#include <cstddef>
#include <cstdint>

namespace rfscope {

class NetManager {
public:
    enum class State : uint8_t { Idle, Connecting, Connected, Failed };

    void begin();

    // `save` stores the password in NVS on a successful connection.
    void connect(const char* ssid, const char* pass, bool save);
    void disconnect();
    void loop();

    State state() const
    {
        return _state;
    }
    const char* ssid() const
    {
        return _ssid;
    }
    const char* errorText() const
    {
        return _error;
    }

    // Valid while Connected.
    int rssi() const;
    int channel() const
    {
        return _channel;
    }
    void ipString(char* out, size_t n) const;
    void bssidString(char* out, size_t n) const;

    uint32_t connectElapsedMs() const;

private:
    State _state       = State::Idle;
    char _ssid[33]     = {0};
    char _pass[65]     = {0};
    bool _savePending  = false;
    int _channel       = 0;
    uint32_t _startMs  = 0;
    const char* _error = "";
};

// Human-readable auth mode from a wifi_auth_mode_t value.
const char* authModeName(uint8_t mode);
bool authModeIsOpen(uint8_t mode);

}  // namespace rfscope
