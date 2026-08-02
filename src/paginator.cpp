#include "paginator.h"

#include <LittleFS.h>

#include "display.h"

namespace Paginator {
namespace {

const uint32_t kMagic = 0x58494255;  // "UBIX" little-endian
const uint8_t kHeaderSize = 8;

// Time budget per step() call: enough to make quick progress without
// starving the watchdog or the network stack of their turn.
const uint32_t kStepBudgetMs = 20;
const uint16_t kBufSize = 128;

// Sequential reader with a buffer that keeps track of the absolute offset.
class Cursor {
 public:
  void attach(File& f, uint32_t start) {
    f_ = &f;
    pos_ = start;
    len_ = 0;
    idx_ = 0;
    f_->seek(start, SeekSet);
  }
  uint32_t pos() const { return pos_; }
  int next() {
    if (idx_ >= len_) {
      int n = f_->read(buf_, kBufSize);
      if (n <= 0) return -1;
      len_ = n;
      idx_ = 0;
    }
    pos_++;
    return buf_[idx_++];
  }

 private:
  File* f_ = nullptr;
  uint32_t pos_ = 0;
  int len_ = 0;
  int idx_ = 0;
  uint8_t buf_[kBufSize];
};

// Longest prefix of `w` that fits within maxW, without splitting a UTF-8
// sequence. Returns at least one character to guarantee the loop advances.
size_t fitPrefix(const String& w, uint16_t maxW) {
  size_t best = 0;
  size_t i = 0;
  while (i < w.length()) {
    size_t j = i + 1;
    while (j < w.length() && (static_cast<uint8_t>(w[j]) & 0xC0) == 0x80) j++;
    if (Display::textWidth(w.substring(0, j).c_str()) > maxW) break;
    best = j;
    i = j;
  }
  if (best == 0) {
    best = 1;
    while (best < w.length() && (static_cast<uint8_t>(w[best]) & 0xC0) == 0x80) best++;
  }
  return best;
}

// Wraps one page starting at the cursor's current position.
//
// This is the ONE implementation of line wrapping, shared by indexing
// (outLines = nullptr) and rendering, so the two can never disagree. Returns
// the offset where the next page starts.
uint32_t layout(Cursor& cur, uint32_t fileSize, String* outLines, uint8_t maxLines,
                uint8_t& outCount) {
  const uint16_t maxW = Display::textAreaWidth();
  String line, word;
  uint32_t wordStart = cur.pos();
  uint8_t lineNo = 0;
  bool started = false;  // the page already has some content

  auto emit = [&](const String& s) -> bool {
    if (outLines && lineNo < maxLines) outLines[lineNo] = s;
    lineNo++;
    started = true;
    return lineNo >= maxLines;
  };
  // Every return path goes through here, to keep outCount consistent.
  auto done = [&](uint32_t nextPageStart) -> uint32_t {
    outCount = lineNo < maxLines ? lineNo : maxLines;
    return nextPageStart;
  };

  while (true) {
    const uint32_t off = cur.pos();
    const int c = cur.next();
    const bool eof = c < 0;

    if (c == '\r') continue;

    if (!eof && c != '\n' && c != ' ' && c != '\t') {
      if (word.length() == 0) wordStart = off;
      word += static_cast<char>(c);
      continue;
    }

    // End of word: try to fit it on the current line.
    if (word.length()) {
      String cand = line.length() ? line + " " + word : word;
      if (Display::textWidth(cand.c_str()) <= maxW) {
        line = cand;
      } else {
        if (line.length()) {
          const bool full = emit(line);
          line = "";
          if (full) return done(wordStart);
        }
        // Word wider than a whole line: hard cut.
        while (Display::textWidth(word.c_str()) > maxW) {
          const size_t cut = fitPrefix(word, maxW);
          const bool full = emit(word.substring(0, cut));
          word = word.substring(cut);
          wordStart += cut;
          if (full) return done(wordStart);
        }
        line = word;
      }
      word = "";
    }

    if (eof) {
      if (line.length()) emit(line);
      return done(fileSize);
    }

    // A space or tab only closes the word; the line stays open and keeps
    // taking more words until they no longer fit.
    if (c != '\n') continue;

    // c == '\n': end of paragraph.
    if (line.length() == 0 && !started) continue;  // blank lines at the top of a page
    const bool full = emit(line);
    line = "";
    if (full) return done(off + 1);
  }
}

struct IndexJob {
  bool active = false;
  char id[Library::ID_LEN] = {0};
  File txt;
  File idx;
  uint32_t size = 0;
  uint32_t pos = 0;
  uint32_t pages = 0;
};

IndexJob g_job;

void writeU32(File& f, uint32_t v) {
  uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
                  static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)};
  f.write(b, 4);
}

bool readU32(File& f, uint32_t& out) {
  uint8_t b[4];
  if (f.read(b, 4) != 4) return false;
  out = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
  return true;
}

void finish(bool ok) {
  if (g_job.idx) g_job.idx.close();
  if (g_job.txt) g_job.txt.close();
  if (ok) {
    Library::BookMeta meta;
    if (Library::load(g_job.id, meta)) {
      meta.pages = g_job.pages;
      meta.idxVersion = layoutVersion();
      if (meta.page >= meta.pages) meta.page = meta.pages ? meta.pages - 1 : 0;
      Library::save(meta);
    }
    Serial.printf("[idx] %s indexed: %u pages\n", g_job.id, g_job.pages);
  } else {
    LittleFS.remove(Library::idxPath(g_job.id));
    Serial.printf("[idx] %s indexing failed\n", g_job.id);
  }
  g_job.active = false;
}

}  // namespace

uint32_t layoutVersion() {
  return (static_cast<uint32_t>(Display::font()) << 24) |
         (static_cast<uint32_t>(Display::lineHeight()) << 16) |
         (static_cast<uint32_t>(Display::linesPerPage()) << 8) |
         (Display::textAreaWidth() & 0xFF);
}

bool indexValid(const Library::BookMeta& meta) {
  return meta.pages > 0 && meta.idxVersion == layoutVersion() &&
         LittleFS.exists(Library::idxPath(meta.id));
}

bool startIndexing(const char* id) {
  cancel();
  g_job.txt = LittleFS.open(Library::textPath(id), "r");
  if (!g_job.txt) return false;
  g_job.idx = LittleFS.open(Library::idxPath(id), "w");
  if (!g_job.idx) {
    g_job.txt.close();
    return false;
  }
  strlcpy(g_job.id, id, Library::ID_LEN);
  g_job.size = g_job.txt.size();
  g_job.pos = 0;
  g_job.pages = 0;
  g_job.active = true;

  writeU32(g_job.idx, kMagic);
  writeU32(g_job.idx, layoutVersion());
  return true;
}

bool indexing() { return g_job.active; }
const char* indexingId() { return g_job.id; }
uint32_t indexedPages() { return g_job.pages; }

uint8_t progress() {
  if (!g_job.active || g_job.size == 0) return 0;
  uint32_t pct = static_cast<uint64_t>(g_job.pos) * 100 / g_job.size;
  return pct > 100 ? 100 : static_cast<uint8_t>(pct);
}

bool step() {
  if (!g_job.active) return true;
  const uint32_t deadline = millis() + kStepBudgetMs;
  Cursor cur;

  while (g_job.pos < g_job.size) {
    // Every page re-seeks to its exact offset. Reusing the previous page's
    // cursor doesn't work: when a page fills up, layout() returns the start
    // of the pending word, which sits behind wherever the cursor stopped,
    // and that gap would eat a word on every page break.
    cur.attach(g_job.txt, g_job.pos);
    writeU32(g_job.idx, g_job.pos);
    g_job.pages++;

    uint8_t lines = 0;
    const uint32_t next = layout(cur, g_job.size, nullptr, Display::linesPerPage(), lines);
    if (next <= g_job.pos) {  // guard against an infinite loop
      finish(false);
      return true;
    }
    g_job.pos = next;

    if (static_cast<int32_t>(millis() - deadline) >= 0) {
      yield();
      return false;
    }
  }
  finish(true);
  return true;
}

void cancel() {
  if (!g_job.active) return;
  g_job.idx.close();
  g_job.txt.close();
  LittleFS.remove(Library::idxPath(g_job.id));
  g_job.active = false;
}

uint32_t pageCount(const char* id) {
  File f = LittleFS.open(Library::idxPath(id), "r");
  if (!f) return 0;
  uint32_t magic = 0, version = 0;
  if (!readU32(f, magic) || !readU32(f, version) || magic != kMagic ||
      version != layoutVersion()) {
    f.close();
    return 0;
  }
  const uint32_t size = f.size();
  f.close();
  return size > kHeaderSize ? (size - kHeaderSize) / 4 : 0;
}

uint8_t loadPage(const char* id, uint32_t page, String* lines, uint8_t maxLines) {
  for (uint8_t i = 0; i < maxLines; i++) lines[i] = "";

  File idx = LittleFS.open(Library::idxPath(id), "r");
  if (!idx) return 0;
  const uint32_t entries = idx.size() > kHeaderSize ? (idx.size() - kHeaderSize) / 4 : 0;
  if (page >= entries) {
    idx.close();
    return 0;
  }
  idx.seek(kHeaderSize + page * 4, SeekSet);
  uint32_t offset = 0;
  const bool ok = readU32(idx, offset);
  idx.close();
  if (!ok) return 0;

  File txt = LittleFS.open(Library::textPath(id), "r");
  if (!txt) return 0;
  Cursor cur;
  cur.attach(txt, offset);
  uint8_t count = 0;
  layout(cur, txt.size(), lines, maxLines, count);
  txt.close();
  return count;
}

}  // namespace Paginator
