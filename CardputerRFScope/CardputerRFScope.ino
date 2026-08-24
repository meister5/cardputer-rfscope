// RFSCOPE -- WiFi and BLE spectrum analyser and RSSI meter for the
// M5Stack Cardputer ADV (and the original Cardputer).
//
// Build:  arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer:PartitionScheme=min_spiffs
//
// See README.md for what this can and cannot measure.

#include "app.h"

static rfscope::App g_app;

void setup()
{
    g_app.begin();
}

void loop()
{
    g_app.loop();
}
