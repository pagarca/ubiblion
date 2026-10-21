#include "webui.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "compat.h"
#include "config.h"
#include "display.h"
#include "i18n.h"
#include "input.h"
#include "paginator.h"

namespace WebUI {
namespace {

UBWebServer g_server(80);
bool g_active = false;
bool g_ap = false;
bool g_libraryChanged = false;

char g_pendingIndex[Library::ID_LEN] = {0};

// State of the upload in progress.
struct Upload {
  bool active = false;
  bool failed = false;
  char id[Library::ID_LEN] = {0};
  File file;
  uint32_t written = 0;
  String title;
  String author;
} g_upload;

const uint32_t kConnectTimeoutMs = 15000;

void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  g_server.send(code, F("application/json"), out);
}

void sendError(int code, const char* message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(code, doc);
}

// Returns true if the request is authorized. The key is optional: if
// config.json doesn't set apiKey, the API is open on the local network.
bool authorized() {
  const String& key = Config::get().apiKey;
  if (key.length() == 0) return true;
  if (g_server.header("X-Api-Key") == key) return true;
  sendError(401, "invalid API key");
  return false;
}

String sanitizeTitle(const String& raw) {
  String t = raw;
  t.trim();
  // Strip the extension if we were handed a filename.
  const int dot = t.lastIndexOf('.');
  if (dot > 0 && t.length() - dot <= 5) t.remove(dot);
  t.replace('\n', ' ');
  t.replace('\r', ' ');
  if (t.length() > 60) t = t.substring(0, 60);
  return t;
}

void bookToJson(const Library::BookMeta& m, JsonObject o) {
  // String() forces a copy: with a plain char[], ArduinoJson would keep the
  // pointer, and that buffer is a stack variable reused on every loop
  // iteration, so every book would end up showing the last id.
  o["id"] = String(m.id);
  o["title"] = m.title;
  o["author"] = m.author;
  o["bytes"] = m.bytes;
  o["pages"] = m.pages;
  o["page"] = m.page;
  o["indexed"] = Paginator::indexValid(m);
}

// --- handlers ---

void handleRoot() {
  const char* gz = "/index.html.gz";
  if (LittleFS.exists(gz)) {
    File f = LittleFS.open(gz, "r");
    g_server.sendHeader(F("Content-Encoding"), F("gzip"));
    g_server.streamFile(f, F("text/html"));
    f.close();
    return;
  }
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    g_server.streamFile(f, F("text/html"));
    f.close();
    return;
  }
  g_server.send(200, F("text/plain"),
                F("ubiblion: the web UI is missing. Upload the filesystem "
                  "with 'pio run -t uploadfs'."));
}

void handleStatus() {
  JsonDocument doc;
  doc["version"] = UBIBLION_VERSION;
  doc["dev"] = UBIBLION_DEV != 0;
  doc["uptime"] = millis() / 1000;
  doc["hostname"] = Config::get().hostname;
  doc["heap"] = ESP.getFreeHeap();
  doc["fsTotal"] = Library::totalBytes();
  doc["fsUsed"] = Library::usedBytes();
  doc["fsFree"] = Library::freeBytes();
  doc["books"] = Library::count();
  doc["maxBooks"] = static_cast<uint8_t>(Library::MAX_BOOKS);
  doc["currentBook"] = Config::get().currentBook;
  // Line-wrap metrics: if pagination looks off, this is where to see why.
  JsonObject lay = doc["layout"].to<JsonObject>();
  lay["font"] = static_cast<uint8_t>(Display::font());
  lay["ascent"] = Display::fontAscent();
  lay["lineHeight"] = Display::lineHeight();
  lay["glyphAscent"] = Display::glyphAscent();
  lay["glyphDescent"] = Display::glyphDescent();
  lay["rowHeight"] = Display::rowHeight();
  lay["linesPerPage"] = Display::linesPerPage();
  lay["textWidth"] = Display::textAreaWidth();
  lay["sample"] = Display::textWidth("The quick brown fox");
  JsonObject idx = doc["indexing"].to<JsonObject>();
  idx["active"] = Paginator::indexing();
  idx["id"] = Paginator::indexingId();
  idx["progress"] = Paginator::progress();
  idx["pages"] = Paginator::indexedPages();
  sendJson(200, doc);
}

void handleLibrary() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < Library::count(); i++) {
    Library::BookMeta m;
    if (!Library::at(i, m)) continue;
    bookToJson(m, arr.add<JsonObject>());
  }
  sendJson(200, doc);
}

// Upload body: written straight to LittleFS, no buffering.
void handleUploadData() {
  HTTPUpload& up = g_server.upload();

  if (up.status == UPLOAD_FILE_START) {
    g_upload = Upload();
    if (Config::get().apiKey.length() &&
        g_server.header("X-Api-Key") != Config::get().apiKey) {
      g_upload.failed = true;
      return;
    }
    if (Library::count() >= Library::MAX_BOOKS) {
      g_upload.failed = true;
      return;
    }
    const uint32_t declared = g_server.header("Content-Length").toInt();
    if (declared > 0 && declared > Library::freeBytes()) {
      g_upload.failed = true;
      return;
    }
    const String id = Library::nextId();
    if (id.length() == 0) {
      g_upload.failed = true;
      return;
    }
    strlcpy(g_upload.id, id.c_str(), sizeof(g_upload.id));
    g_upload.file = LittleFS.open(Library::textPath(g_upload.id), "w");
    if (!g_upload.file) {
      g_upload.failed = true;
      return;
    }
    g_upload.title = sanitizeTitle(g_server.hasArg("title") ? g_server.arg("title")
                                                            : up.filename);
    g_upload.author = g_server.hasArg("author") ? g_server.arg("author") : String();
    g_upload.active = true;
    Serial.printf("[web] uploading %s -> %s\n", up.filename.c_str(), g_upload.id);
    return;
  }

  if (up.status == UPLOAD_FILE_WRITE) {
    if (!g_upload.active || g_upload.failed) return;
    if (g_upload.file.write(up.buf, up.currentSize) != up.currentSize) {
      g_upload.failed = true;  // out of space
      g_upload.file.close();
      LittleFS.remove(Library::textPath(g_upload.id));
      return;
    }
    g_upload.written += up.currentSize;
    return;
  }

  if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (g_upload.file) g_upload.file.close();
    if (up.status == UPLOAD_FILE_ABORTED) g_upload.failed = true;
    if (g_upload.failed && g_upload.id[0]) {
      LittleFS.remove(Library::textPath(g_upload.id));
      g_upload.active = false;
    }
  }
}

void handleUploadDone() {
  if (g_upload.failed || !g_upload.active || g_upload.written == 0) {
    if (g_upload.id[0]) LittleFS.remove(Library::textPath(g_upload.id));
    g_upload = Upload();
    sendError(507, "could not save the book (space, limit, or key)");
    return;
  }

  Library::BookMeta meta;
  strlcpy(meta.id, g_upload.id, sizeof(meta.id));
  meta.title = g_upload.title.length() ? g_upload.title : String(g_upload.id);
  meta.author = g_upload.author;
  meta.bytes = g_upload.written;
  meta.pages = 0;
  meta.page = 0;
  meta.idxVersion = 0;
  meta.added = millis();
  Library::save(meta);
  Library::refresh();

  strlcpy(g_pendingIndex, g_upload.id, sizeof(g_pendingIndex));
  g_libraryChanged = true;

  JsonDocument doc;
  doc["id"] = String(meta.id);  // copy, not a pointer (see bookToJson)
  doc["title"] = meta.title;
  doc["bytes"] = meta.bytes;
  doc["indexing"] = true;
  g_upload = Upload();
  sendJson(202, doc);
}

void handleGetBook() {
  if (!authorized()) return;
  const String id = g_server.pathArg(0);
  if (!Library::exists(id.c_str())) {
    sendError(404, "book not found");
    return;
  }
  File f = LittleFS.open(Library::textPath(id.c_str()), "r");
  if (!f) {
    sendError(404, "text not found");
    return;
  }
  g_server.sendHeader(F("Content-Disposition"), "attachment; filename=\"" + id + ".txt\"");
  g_server.streamFile(f, F("text/plain; charset=utf-8"));
  f.close();
}

void handleDeleteBook() {
  if (!authorized()) return;
  const String id = g_server.pathArg(0);
  if (!Library::exists(id.c_str())) {
    sendError(404, "book not found");
    return;
  }
  if (Paginator::indexing() && id == Paginator::indexingId()) Paginator::cancel();
  Library::remove(id.c_str());
  if (Config::get().currentBook == id) {
    Config::get().currentBook = "";
    Config::save();
  }
  g_libraryChanged = true;
  JsonDocument doc;
  doc["deleted"] = id;
  sendJson(200, doc);
}

void handleOpenBook() {
  if (!authorized()) return;
  const String id = g_server.pathArg(0);
  Library::BookMeta meta;
  if (!Library::load(id.c_str(), meta)) {
    sendError(404, "book not found");
    return;
  }
  if (g_server.hasArg("page")) {
    const long page = g_server.arg("page").toInt();
    meta.page = page > 0 ? static_cast<uint32_t>(page) : 0;
    if (meta.pages && meta.page >= meta.pages) meta.page = meta.pages - 1;
    Library::save(meta);
  }
  Config::get().currentBook = id;
  Config::save();
  g_libraryChanged = true;
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  bookToJson(meta, o);
  sendJson(200, doc);
}

void handleReindex() {
  if (!authorized()) return;
  const String id = g_server.pathArg(0);
  if (!Library::exists(id.c_str())) {
    sendError(404, "book not found");
    return;
  }
  strlcpy(g_pendingIndex, id.c_str(), sizeof(g_pendingIndex));
  JsonDocument doc;
  doc["reindexing"] = id;
  sendJson(202, doc);
}

// Returns the configuration without exposing the stored secrets.
void handleGetConfig() {
  if (!authorized()) return;
  Config::Settings& s = Config::get();
  JsonDocument doc;
  doc["ssid"] = s.ssid;
  doc["hostname"] = s.hostname;
  doc["hasPassword"] = s.pass.length() > 0;
  doc["hasApiKey"] = s.apiKey.length() > 0;
  doc["fontId"] = s.fontId;
  doc["fullRefreshEvery"] = s.fullRefreshEvery;
  doc["wifiOnBoot"] = s.wifiOnBoot;
  doc["lang"] = I18n::currentLanguage();
  doc["mode"] = g_ap ? "ap" : "sta";
  JsonArray langs = doc["languages"].to<JsonArray>();
  for (uint8_t i = 0; i < I18n::languageCount(); i++) {
    JsonObject o = langs.add<JsonObject>();
    o["code"] = I18n::languageCode(i);
    o["name"] = I18n::languageName(i);
  }
  sendJson(200, doc);
}

// JSON body. Fields that are absent are left untouched, so the SSID alone
// can be changed without resending the password.
void handleSetConfig() {
  if (!authorized()) return;
  JsonDocument doc;
  if (deserializeJson(doc, g_server.arg("plain"))) {
    sendError(400, "invalid JSON");
    return;
  }
  Config::Settings& s = Config::get();
  if (doc["ssid"].is<const char*>()) s.ssid = doc["ssid"].as<const char*>();
  if (doc["pass"].is<const char*>()) s.pass = doc["pass"].as<const char*>();
  if (doc["apiKey"].is<const char*>()) s.apiKey = doc["apiKey"].as<const char*>();
  if (doc["hostname"].is<const char*>()) {
    String h = doc["hostname"].as<const char*>();
    h.trim();
    if (h.length()) s.hostname = h;
  }
  if (doc["wifiOnBoot"].is<int>()) {
    const int v = doc["wifiOnBoot"].as<int>();
    if (v >= Config::WIFI_BOOT_NEVER && v <= Config::WIFI_BOOT_ALWAYS) s.wifiOnBoot = v;
  }
  if (doc["fullRefreshEvery"].is<int>()) {
    s.fullRefreshEvery = doc["fullRefreshEvery"].as<int>();
    Display::setFullRefreshEvery(s.fullRefreshEvery);
  }
  if (doc["fontId"].is<int>()) {
    const int id = doc["fontId"].as<int>();
    if (id >= 0 && id < Display::FONT_COUNT) {
      s.fontId = id;
      Display::setFont(static_cast<Display::FontId>(id));
    }
  }
  if (doc["lang"].is<int>()) {
    const int lang = doc["lang"].as<int>();
    if (lang >= 0 && lang < I18n::languageCount()) I18n::setLanguage(lang);
  }
  if (!Config::save()) {
    sendError(500, "could not save the configuration");
    return;
  }
  JsonDocument out;
  out["saved"] = true;
  out["note"] = "WiFi changes apply the next time it's turned on";
  sendJson(200, out);
}

void handleReboot() {
  if (!authorized()) return;
  g_server.send(200, F("application/json"), F("{\"rebooting\":true}"));
  delay(200);
  ESP.restart();
}

void handleNotFound() { sendError(404, "unknown route"); }

#if UBIBLION_DEV
// --- development endpoints ---
// Only exist in the `dev` build. Let you drive and inspect the device from
// the browser without having it in front of you or its buttons wired up.

// Text mirror of what's currently on screen.
void handleDebugScreen() {
  JsonDocument doc;
  doc["refreshes"] = Display::refreshCount();
  doc["lineHeight"] = Display::lineHeight();
  doc["linesPerPage"] = Display::linesPerPage();
  JsonArray arr = doc["lines"].to<JsonArray>();
  for (uint8_t i = 0; i < Display::mirrorCount(); i++) {
    JsonObject o = arr.add<JsonObject>();
    o["x"] = Display::mirrorX(i);
    o["y"] = Display::mirrorY(i);
    o["t"] = Display::mirrorText(i);
    if (Display::mirrorInverted(i)) o["inv"] = true;
  }
  sendJson(200, doc);
}

// Injects a button press: /api/debug/press?button=OK&long=1
void handleDebugPress() {
  const String name = g_server.arg("button");
  Input::Button b = Input::BTN_NONE;
  if (name == "PREV" || name == "0") b = Input::BTN_PREV;
  else if (name == "NEXT" || name == "1") b = Input::BTN_NEXT;
  else if (name == "OK" || name == "2") b = Input::BTN_OK;
  else if (name == "BACK" || name == "3") b = Input::BTN_BACK;
  if (b == Input::BTN_NONE) {
    sendError(400, "unknown button (PREV|NEXT|OK|BACK)");
    return;
  }
  const bool longPress = g_server.arg("long") == "1";
  Input::inject(b, longPress);
  JsonDocument doc;
  doc["pressed"] = Input::name(b);
  doc["long"] = longPress;
  sendJson(200, doc);
}

// Latest button events, to check the wiring from the web.
void handleDebugInput() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  const uint32_t now = millis();
  for (uint8_t i = 0; i < Input::logCount(); i++) {
    const Input::LogEntry e = Input::logAt(i);
    JsonObject o = arr.add<JsonObject>();
    o["ago"] = now - e.ms;
    o["button"] = Input::name(e.button);
    o["long"] = e.longPress;
    o["injected"] = e.injected;
  }
  sendJson(200, doc);
}

// Line wrap of one specific page, exactly as the reader would produce it.
void handleDebugPage() {
  const String id = g_server.arg("id");
  const uint32_t page = g_server.arg("page").toInt();
  if (!Library::exists(id.c_str())) {
    sendError(404, "book not found");
    return;
  }
  String lines[Paginator::MAX_LINES];
  uint8_t maxLines = Display::linesPerPage();
  if (maxLines > Paginator::MAX_LINES) maxLines = Paginator::MAX_LINES;
  const uint8_t n = Paginator::loadPage(id.c_str(), page, lines, maxLines);

  JsonDocument doc;
  doc["id"] = id;
  doc["page"] = page;
  doc["pages"] = Paginator::pageCount(id.c_str());
  doc["maxLines"] = maxLines;
  JsonArray arr = doc["lines"].to<JsonArray>();
  for (uint8_t i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["t"] = lines[i];
    o["w"] = Display::textWidth(lines[i].c_str());
  }
  sendJson(200, doc);
}

// Diagnostic pattern for the white-on-black text bug.
void handleDebugPattern() {
  Display::testPattern();
  JsonDocument doc;
  doc["pattern"] = true;
  sendJson(200, doc);
}

void devRoutes() {
  g_server.on("/api/debug/pattern", HTTP_POST, handleDebugPattern);
  g_server.on("/api/debug/screen", HTTP_GET, handleDebugScreen);
  g_server.on("/api/debug/press", HTTP_POST, handleDebugPress);
  g_server.on("/api/debug/input", HTTP_GET, handleDebugInput);
  g_server.on("/api/debug/page", HTTP_GET, handleDebugPage);
}
#endif  // UBIBLION_DEV

void routes() {
  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/api/status", HTTP_GET, handleStatus);
  g_server.on("/api/library", HTTP_GET, handleLibrary);
  g_server.on("/api/books", HTTP_POST, handleUploadDone, handleUploadData);
  g_server.on(UriBraces("/api/books/{}"), HTTP_GET, handleGetBook);
  g_server.on(UriBraces("/api/books/{}"), HTTP_DELETE, handleDeleteBook);
  g_server.on(UriBraces("/api/books/{}/open"), HTTP_POST, handleOpenBook);
  g_server.on(UriBraces("/api/books/{}/reindex"), HTTP_POST, handleReindex);
  g_server.on("/api/config", HTTP_GET, handleGetConfig);
  g_server.on("/api/config", HTTP_POST, handleSetConfig);
  g_server.on("/api/system/reboot", HTTP_POST, handleReboot);
#if UBIBLION_DEV
  devRoutes();
#endif
  g_server.onNotFound(handleNotFound);

  // Two collected headers via the shim: the classic array+size overload is
  // ambiguous on the ESP8266 core, and the ESP32 one doesn't take variadics
  // with F() strings the same way. NodeMCU/i18n: none of this touches RAM
  // beyond the two tiny String copies compat.h makes.
  ubCollectHeaders(g_server, "Content-Length", "X-Api-Key");
}

bool connectStation() {
  Config::Settings& s = Config::get();
  if (s.ssid.length() == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.hostname(s.hostname);
  WiFi.begin(s.ssid.c_str(), s.pass.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) {
    delay(100);
    yield();
  }
  return WiFi.status() == WL_CONNECTED;
}

void startAccessPoint() {
  Config::Settings& s = Config::get();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(s.hostname.c_str(), s.apPass.c_str());
  g_ap = true;
}

}  // namespace

void begin() {
  if (g_active) return;
  ubWiFiRadioWake();
  WiFi.persistent(false);
  g_ap = false;

  // A busy router occasionally doesn't hand out an association within one
  // 15 s window (seen once on real hardware, which dumped the reader into
  // AP mode with no obvious way back). Retry once before giving up.
  if (!connectStation()) {
#if UBIBLION_DEV
    Serial.printf("[web] STA not up after %u ms, retrying once\n", kConnectTimeoutMs);
#endif
    WiFi.disconnect();
    delay(100);
    if (!connectStation()) startAccessPoint();
  }

  if (MDNS.begin(Config::get().hostname)) MDNS.addService("http", "tcp", 80);
  routes();
  g_server.begin();
  g_active = true;
  Serial.printf("[web] up at %s (%s)\n", url().c_str(), g_ap ? "AP" : "STA");
}

void stop() {
  if (!g_active) return;
  g_server.stop();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  ubWiFiRadioOff();
  g_active = false;
  g_ap = false;
  Serial.println(F("[web] stopped"));
}

bool isActive() { return g_active; }

void loop() {
  if (!g_active) return;
  g_server.handleClient();
  ubMDNSUpdate();
}

bool isAP() { return g_ap; }

IPAddress ip() { return g_ap ? WiFi.softAPIP() : WiFi.localIP(); }

String ssid() { return g_ap ? Config::get().hostname : WiFi.SSID(); }

String url() { return String("http://") + ip().toString(); }

bool takePendingIndex(char* out, size_t len) {
  if (!g_pendingIndex[0]) return false;
  strlcpy(out, g_pendingIndex, len);
  g_pendingIndex[0] = 0;
  return true;
}

bool takeLibraryChanged() {
  if (!g_libraryChanged) return false;
  g_libraryChanged = false;
  return true;
}

}  // namespace WebUI
