#include "net_manager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>

#include "storage.h"

namespace rfscope {

namespace {
constexpr uint32_t kConnectTimeoutMs = 20000;
}

void NetManager::begin()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
}

void NetManager::connect(const char* ssid, const char* pass, bool save)
{
    // A hidden AP arrives from the scan with no SSID, and there is nothing to
    // associate to. Report it: returning quietly here left the caller on a
    // meter that claimed no network had been picked at all.
    if (!ssid || !*ssid) {
        _ssid[0] = '\0';
        _state   = State::Failed;
        _error   = "hidden network - no SSID";
        return;
    }

    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    _ssid[sizeof(_ssid) - 1] = '\0';
    memset(_pass, 0, sizeof(_pass));
    if (pass) {
        strncpy(_pass, pass, sizeof(_pass) - 1);
    }

    _savePending = save;
    _error       = "";
    _channel     = 0;
    _state       = State::Connecting;
    _startMs     = millis();

    // The sniffer may still own the radio; a connection cannot start while
    // promiscuous mode is on.
    esp_wifi_set_promiscuous(false);

    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    if (_pass[0]) {
        WiFi.begin(_ssid, _pass);
    } else {
        WiFi.begin(_ssid);
    }
}

void NetManager::disconnect()
{
    WiFi.disconnect(false, false);
    _state   = State::Idle;
    _channel = 0;
    _error   = "";
}

void NetManager::loop()
{
    if (_state == State::Connecting) {
        const wl_status_t st = WiFi.status();

        if (st == WL_CONNECTED) {
            _state   = State::Connected;
            _channel = WiFi.channel();
            if (_savePending) {
                CredentialStore::save(_ssid, _pass);
                _savePending = false;
            }
            return;
        }

        if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
            _state = State::Failed;
            _error = (st == WL_NO_SSID_AVAIL) ? "network not found" : "wrong password?";
            return;
        }

        if (millis() - _startMs > kConnectTimeoutMs) {
            WiFi.disconnect(false, false);
            _state = State::Failed;
            _error = "timed out";
        }
        return;
    }

    if (_state == State::Connected && WiFi.status() != WL_CONNECTED) {
        _state = State::Failed;
        _error = "link lost";
    }
}

int NetManager::rssi() const
{
    return WiFi.RSSI();
}

void NetManager::ipString(char* out, size_t n) const
{
    const IPAddress ip = WiFi.localIP();
    snprintf(out, n, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void NetManager::bssidString(char* out, size_t n) const
{
    const uint8_t* b = WiFi.BSSID();
    if (!b) {
        snprintf(out, n, "--:--:--:--:--:--");
        return;
    }
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
}

uint32_t NetManager::connectElapsedMs() const
{
    return millis() - _startMs;
}

const char* authModeName(uint8_t mode)
{
    switch (mode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/2";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/3";
        case WIFI_AUTH_WAPI_PSK:
            return "WAPI";
        default:
            return "?";
    }
}

bool authModeIsOpen(uint8_t mode)
{
    return mode == WIFI_AUTH_OPEN;
}

}  // namespace rfscope
