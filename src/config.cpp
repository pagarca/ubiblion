#include "config.h"

#include <ArduinoJson.h>
#include <EEPROM.h>
#include <LittleFS.h>

namespace Config {
namespace {

const char* const kPath = "/config.json";
Settings g_settings;

// Network credentials are also kept in emulated EEPROM, which lives in a
// flash sector outside LittleFS. That way they survive a `pio run -t
// uploadfs`, which rewrites the whole filesystem and would otherwise wipe
// config.json every time the web UI gets updated.
const uint32_t kCredMagic = 0x434E4255;  // "UBNC"

struct CredBlob {
  uint32_t magic;
  uint8_t version;
  char ssid[33];
  char pass[65];
  char hostname[33];
};

void copyInto(char* dst, size_t size, const String& src) {
  strlcpy(dst, src.c_str(), size);
}

bool readCredentials(CredBlob& out) {
  EEPROM.get(0, out);
  if (out.magic != kCredMagic || out.version != 1) return false;
  out.ssid[sizeof(out.ssid) - 1] = 0;
  out.pass[sizeof(out.pass) - 1] = 0;
  out.hostname[sizeof(out.hostname) - 1] = 0;
  return true;
}

// Only writes if something actually changed: every commit() is a sector erase.
void writeCredentialsIfChanged() {
  CredBlob current{};
  const bool valid = readCredentials(current);
  if (valid && g_settings.ssid == current.ssid && g_settings.pass == current.pass &&
      g_settings.hostname == current.hostname) {
    return;
  }
  CredBlob blob{};
  blob.magic = kCredMagic;
  blob.version = 1;
  copyInto(blob.ssid, sizeof(blob.ssid), g_settings.ssid);
  copyInto(blob.pass, sizeof(blob.pass), g_settings.pass);
  copyInto(blob.hostname, sizeof(blob.hostname), g_settings.hostname);
  EEPROM.put(0, blob);
  EEPROM.commit();
}

}  // namespace

// Recovers credentials from EEPROM when config.json doesn't have them
// (typically right after the filesystem got rewritten).
static void restoreCredentials() {
  if (g_settings.ssid.length()) return;
  CredBlob blob{};
  if (!readCredentials(blob) || !blob.ssid[0]) return;
  g_settings.ssid = blob.ssid;
  g_settings.pass = blob.pass;
  if (blob.hostname[0]) g_settings.hostname = blob.hostname;
  Serial.printf("[cfg] credentials restored from EEPROM (%s)\n", blob.ssid);
  save();  // write them back into config.json
}

void begin() {
  EEPROM.begin(sizeof(CredBlob) + 8);

  File f = LittleFS.open(kPath, "r");
  if (!f) {
    Serial.println(F("[cfg] no config.json, using defaults"));
    restoreCredentials();
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[cfg] config.json is corrupt: %s\n", err.c_str());
    return;
  }
  g_settings.ssid = doc["ssid"] | "";
  g_settings.pass = doc["pass"] | "";
  g_settings.apiKey = doc["apiKey"] | "";
  g_settings.apPass = doc["apPass"] | "ubiblion";
  g_settings.hostname = doc["hostname"] | "ubiblion";
  g_settings.fontId = doc["fontId"] | 1;
  g_settings.fullRefreshEvery = doc["fullRefreshEvery"] | 10;
  g_settings.currentBook = doc["currentBook"] | "";
  g_settings.lang = doc["lang"] | 0;
  if (doc["wifiOnBoot"].is<int>()) {
    g_settings.wifiOnBoot = doc["wifiOnBoot"].as<int>();
  } else if (doc["wifiOnBootIfEmpty"].is<bool>()) {
    // Older (boolean) format.
    g_settings.wifiOnBoot =
        doc["wifiOnBootIfEmpty"].as<bool>() ? WIFI_BOOT_IF_EMPTY : WIFI_BOOT_NEVER;
  }
  if (g_settings.wifiOnBoot > WIFI_BOOT_ALWAYS) g_settings.wifiOnBoot = WIFI_BOOT_IF_EMPTY;

  restoreCredentials();
}

Settings& get() { return g_settings; }

bool save() {
  JsonDocument doc;
  doc["ssid"] = g_settings.ssid;
  doc["pass"] = g_settings.pass;
  doc["apiKey"] = g_settings.apiKey;
  doc["apPass"] = g_settings.apPass;
  doc["hostname"] = g_settings.hostname;
  doc["fontId"] = g_settings.fontId;
  doc["fullRefreshEvery"] = g_settings.fullRefreshEvery;
  doc["currentBook"] = g_settings.currentBook;
  doc["wifiOnBoot"] = g_settings.wifiOnBoot;
  doc["lang"] = g_settings.lang;

  File f = LittleFS.open(kPath, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();

  writeCredentialsIfChanged();
  return ok;
}

}  // namespace Config
