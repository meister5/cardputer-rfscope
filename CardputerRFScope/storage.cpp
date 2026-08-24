#include "storage.h"

#include <Preferences.h>

#include <cstring>
#include <cstdio>

namespace rfscope {
namespace {

constexpr const char* kSettingsNs = "rfscope";
constexpr const char* kCredsNs    = "rfscope-wifi";

struct CredRecord {
    char ssid[33];
    char pass[65];
};

uint32_t fnv1a(const char* s)
{
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}

void credKey(const char* ssid, char* out, size_t n)
{
    snprintf(out, n, "n%08lx", static_cast<unsigned long>(fnv1a(ssid)));
}

}  // namespace

void Settings::load()
{
    Preferences p;
    if (!p.begin(kSettingsNs, true)) return;
    theme          = static_cast<ThemeId>(p.getUChar("theme", static_cast<uint8_t>(theme)));
    dwellMs        = p.getULong("dwell", dwellMs);
    audioEnabled   = p.getBool("audio", audioEnabled);
    brightness     = p.getUChar("bright", brightness);
    apScanEnabled  = p.getBool("apscan", apScanEnabled);
    localSniff     = p.getBool("lsniff", localSniff);
    p.end();
}

void Settings::save() const
{
    Preferences p;
    if (!p.begin(kSettingsNs, false)) return;
    p.putUChar("theme", static_cast<uint8_t>(theme));
    p.putULong("dwell", dwellMs);
    p.putBool("audio", audioEnabled);
    p.putUChar("bright", brightness);
    p.putBool("apscan", apScanEnabled);
    p.putBool("lsniff", localSniff);
    p.end();
}

bool CredentialStore::load(const char* ssid, char* passOut, size_t passOutLen)
{
    if (!ssid || !*ssid || !passOut || passOutLen == 0) return false;

    char key[16];
    credKey(ssid, key, sizeof(key));

    Preferences p;
    if (!p.begin(kCredsNs, true)) return false;

    CredRecord rec{};
    const size_t got = p.getBytes(key, &rec, sizeof(rec));
    p.end();

    if (got != sizeof(rec)) return false;
    rec.ssid[sizeof(rec.ssid) - 1] = '\0';
    rec.pass[sizeof(rec.pass) - 1] = '\0';
    if (strcmp(rec.ssid, ssid) != 0) return false;  // hash collision

    strncpy(passOut, rec.pass, passOutLen - 1);
    passOut[passOutLen - 1] = '\0';
    return true;
}

bool CredentialStore::save(const char* ssid, const char* pass)
{
    if (!ssid || !*ssid) return false;

    CredRecord rec{};
    strncpy(rec.ssid, ssid, sizeof(rec.ssid) - 1);
    if (pass) strncpy(rec.pass, pass, sizeof(rec.pass) - 1);

    char key[16];
    credKey(ssid, key, sizeof(key));

    Preferences p;
    if (!p.begin(kCredsNs, false)) return false;
    const size_t wrote = p.putBytes(key, &rec, sizeof(rec));
    p.end();
    return wrote == sizeof(rec);
}

bool CredentialStore::has(const char* ssid)
{
    char scratch[65];
    return load(ssid, scratch, sizeof(scratch));
}

bool CredentialStore::forget(const char* ssid)
{
    if (!ssid || !*ssid) return false;
    char key[16];
    credKey(ssid, key, sizeof(key));

    Preferences p;
    if (!p.begin(kCredsNs, false)) return false;
    const bool ok = p.remove(key);
    p.end();
    return ok;
}

void CredentialStore::forgetAll()
{
    Preferences p;
    if (!p.begin(kCredsNs, false)) return;
    p.clear();
    p.end();
}

}  // namespace rfscope
