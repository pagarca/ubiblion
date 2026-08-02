#include "library.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace Library {
namespace {

const char* const kDir = "/books";

char g_ids[MAX_BOOKS][ID_LEN];
uint8_t g_count = 0;

void insertSorted(const char* id) {
  if (g_count >= MAX_BOOKS) return;
  uint8_t pos = 0;
  while (pos < g_count && strcmp(g_ids[pos], id) < 0) pos++;
  for (uint8_t i = g_count; i > pos; i--) memcpy(g_ids[i], g_ids[i - 1], ID_LEN);
  strlcpy(g_ids[pos], id, ID_LEN);
  g_count++;
}

}  // namespace

void begin() {
  if (!LittleFS.begin()) {
    Serial.println(F("[fs] formatting LittleFS"));
    LittleFS.format();
    LittleFS.begin();
  }
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  refresh();
}

void refresh() {
  g_count = 0;
  Dir dir = LittleFS.openDir(kDir);
  while (dir.next() && g_count < MAX_BOOKS) {
    String name = dir.fileName();  // e.g. "b0001.json"
    if (!name.endsWith(".json")) continue;
    name.remove(name.length() - 5);
    if (name.length() >= ID_LEN) continue;
    insertSorted(name.c_str());
  }
}

uint8_t count() { return g_count; }

const char* idAt(uint8_t index) { return index < g_count ? g_ids[index] : ""; }

int16_t indexOf(const char* id) {
  for (uint8_t i = 0; i < g_count; i++) {
    if (strcmp(g_ids[i], id) == 0) return i;
  }
  return -1;
}

bool at(uint8_t index, BookMeta& out) {
  if (index >= g_count) return false;
  return load(g_ids[index], out);
}

String textPath(const char* id) { return String(kDir) + "/" + id + ".txt"; }
String idxPath(const char* id) { return String(kDir) + "/" + id + ".idx"; }
String metaPath(const char* id) { return String(kDir) + "/" + id + ".json"; }

bool exists(const char* id) { return LittleFS.exists(metaPath(id)); }

bool load(const char* id, BookMeta& out) {
  File f = LittleFS.open(metaPath(id), "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[lib] %s.json is corrupt: %s\n", id, err.c_str());
    return false;
  }
  strlcpy(out.id, id, ID_LEN);
  out.title = doc["title"] | id;
  out.author = doc["author"] | "";
  out.bytes = doc["bytes"] | 0;
  out.pages = doc["pages"] | 0;
  out.page = doc["page"] | 0;
  out.idxVersion = doc["idxVersion"] | 0;
  out.added = doc["added"] | 0;
  return true;
}

bool save(const BookMeta& meta) {
  JsonDocument doc;
  doc["title"] = meta.title;
  doc["author"] = meta.author;
  doc["bytes"] = meta.bytes;
  doc["pages"] = meta.pages;
  doc["page"] = meta.page;
  doc["idxVersion"] = meta.idxVersion;
  doc["added"] = meta.added;

  File f = LittleFS.open(metaPath(meta.id), "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool remove(const char* id) {
  if (!id || !*id) return false;
  LittleFS.remove(textPath(id));
  LittleFS.remove(idxPath(id));
  const bool ok = LittleFS.remove(metaPath(id));
  refresh();
  return ok;
}

String nextId() {
  for (uint16_t n = 1; n <= 9999; n++) {
    char id[ID_LEN];
    snprintf(id, sizeof(id), "b%04u", n);
    if (indexOf(id) < 0 && !LittleFS.exists(textPath(id))) return String(id);
  }
  return String();
}

uint32_t totalBytes() {
  FSInfo info;
  LittleFS.info(info);
  return info.totalBytes;
}

uint32_t usedBytes() {
  FSInfo info;
  LittleFS.info(info);
  return info.usedBytes;
}

uint32_t freeBytes() {
  FSInfo info;
  LittleFS.info(info);
  // Safety margin: LittleFS needs some free blocks for its own metadata.
  const uint32_t reserved = 16 * 1024;
  if (info.usedBytes + reserved >= info.totalBytes) return 0;
  return info.totalBytes - info.usedBytes - reserved;
}

}  // namespace Library
