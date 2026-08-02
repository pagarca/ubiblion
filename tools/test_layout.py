"""Faithful port of layout() (src/paginator.cpp) to check its invariants.

Doesn't test the compiled C++ code, tests the ALGORITHM: line wrapping and
the page-end offset arithmetic, which is where a bug turns into text that's
lost, repeated, or wrapped wrong when turning pages.

Rules of the original:
  - measured in UTF-8 bytes, same as the firmware's cursor
  - '\\r' is ignored, '\\n' closes a paragraph, ' ' and '\\t' only close a word
  - a full page returns the offset where the next one starts
"""

CHAR_W = 6          # simulated fixed width per code point
MAX_W = 188         # Display::textAreaWidth()


def width(s):
    return len(s) * CHAR_W


def fit_prefix(word, max_w):
    """Longest prefix that fits, at least one character (mirrors fitPrefix)."""
    best = 0
    for i in range(1, len(word) + 1):
        if width(word[:i]) > max_w:
            break
        best = i
    return best if best else 1


def layout(data, start, max_lines, collect):
    """Returns (lines, paragraph_end_flags, next_page_offset)."""
    lines, flags = [], []
    line = ""
    word = ""
    word_start = start
    line_no = 0
    started = False
    pos = start
    size = len(data)

    def emit(text, para_end=False):
        nonlocal line_no, started
        if collect and line_no < max_lines:
            lines.append(text)
            flags.append(para_end)
        line_no += 1
        started = True
        return line_no >= max_lines

    while True:
        off = pos
        if pos >= size:
            c = None
        else:
            b = data[pos]
            nbytes = 4 if b >= 0xF0 else 3 if b >= 0xE0 else 2 if b >= 0xC0 else 1
            c = data[pos:pos + nbytes].decode("utf-8", "replace")
            pos += nbytes

        eof = c is None
        if c == "\r":
            continue

        if not eof and c not in ("\n", " ", "\t"):
            if not word:
                word_start = off
            word += c
            continue

        if word:
            cand = (line + " " + word) if line else word
            if width(cand) <= MAX_W:
                line = cand
            else:
                if line:
                    full = emit(line)
                    line = ""
                    if full:
                        return lines, flags, word_start
                while width(word) > MAX_W:
                    cut = fit_prefix(word, MAX_W)
                    full = emit(word[:cut])
                    word_start += len(word[:cut].encode("utf-8"))
                    word = word[cut:]
                    if full:
                        return lines, flags, word_start
                line = word
            word = ""

        if eof:
            if line:
                emit(line, True)
            return lines, flags, size

        # A space only closes the word; the line stays open.
        if c != "\n":
            continue

        if not line and not started:
            continue
        full = emit(line, True)
        line = ""
        if full:
            return lines, flags, off + 1


def build_index(data, max_lines):
    """Mirrors Paginator::step(): every page re-seeks to its own offset."""
    offsets = []
    pos = 0
    while pos < len(data):
        offsets.append(pos)
        _, _, nxt = layout(data, pos, max_lines, collect=False)
        assert nxt > pos, "page doesn't advance at offset %d" % pos
        pos = nxt
    return offsets


def reference_wrap(text):
    """Independent oracle: greedy wrap, paragraph by paragraph.

    Deliberately naive and obvious to read. Comparing against it catches
    lines that are wrapped wrong in a way the "no text lost" and "no line
    too wide" invariants don't: splitting a word across every line satisfies
    both of those just fine.
    """
    out = []
    for paragraph in text.split("\n"):
        line = ""
        for word in paragraph.split():
            cand = (line + " " + word) if line else word
            if width(cand) <= MAX_W:
                line = cand
                continue
            if line:
                out.append(line)
            # Word wider than a whole line: hard cut.
            while width(word) > MAX_W:
                cut = fit_prefix(word, MAX_W)
                out.append(word[:cut])
                word = word[cut:]
            line = word
        if line:
            out.append(line)
    return out


def check(name, text, max_lines=10):
    data = text.encode("utf-8")
    offsets = build_index(data, max_lines)

    # 1. offsets are strictly increasing and within the file
    assert offsets == sorted(set(offsets)), "%s: offsets repeated or out of order" % name
    assert all(0 <= o < len(data) for o in offsets), "%s: offset out of range" % name

    rendered = []
    for i, off in enumerate(offsets):
        lines, flags, nxt = layout(data, off, max_lines, collect=True)

        # 2. indexing and rendering agree: a page loaded by its own offset
        #    ends right where the index says the next one begins.
        expected = offsets[i + 1] if i + 1 < len(offsets) else len(data)
        assert nxt == expected, (
            "%s: page %d ends at %d but the index says %d" % (name, i, nxt, expected))
        assert len(lines) <= max_lines, "%s: page %d has %d lines" % (name, i, len(lines))

        for ln in lines:
            assert width(ln) <= MAX_W, "%s: line too wide: %r" % (name, ln)

        rendered.extend(lines)

    # 4. Text is neither lost nor duplicated. Compares characters with
    #    whitespace stripped: order-sensitive, and tolerates a word wider
    #    than the line being split across several, which is correct.
    original = "".join(text.split())
    output = "".join("".join(rendered).split())
    assert output == original, (
        "%s: text isn't preserved (%d characters expected, %d obtained)\n"
        "  they diverge at position %d" % (
            name, len(original), len(output),
            next((i for i, (a, b) in enumerate(zip(original, output)) if a != b),
                 min(len(original), len(output)))))

    # 5. The line breaks match the oracle's. Blank lines are ignored:
    #    layout() skips ones that would fall at the top of a page, and the
    #    oracle doesn't know anything about pages.
    expected = [l for l in reference_wrap(text) if l]
    obtained = [l for l in rendered if l]
    assert obtained == expected, (
        "%s: line wrap differs from expected (%d vs %d lines)\n"
        "  expected:  %r\n  obtained:  %r" % (
            name, len(expected), len(obtained),
            next((e for e, o in zip(expected, obtained) if e != o), None),
            next((o for e, o in zip(expected, obtained) if e != o), None)))

    fill = sum(len(l) for l in rendered) / max(1, len(rendered))
    print("OK  %-28s %4d pages, %4d lines, %4.1f chars/line"
          % (name, len(offsets), len(rendered), fill))


paragraph = ("Call me Ishmael. Some years ago, never mind how long precisely, having "
             "little or no money in my purse, and nothing particular to interest me on "
             "shore, I thought I would sail about a little and see the watery part.")

check("short text", "Hello world.")
check("one long paragraph", paragraph)
check("many paragraphs", "\n".join([paragraph] * 30))
check("blank lines", "One\n\n\n\nTwo\n\n\nThree\n")
check("accents", "\n".join(["Café, naïve, Zürich, façade, résumé, jalapeño, ¿qué? ¡sí!"] * 20))
check("mile-long word", "short " + "a" * 200 + " end")
check("exact-fit words", "\n".join(["x" * 31 + " " + "y" * 31] * 10))
check("no trailing newline", paragraph.replace("\n", " ") + " end")
check("crlf", "One\r\nTwo\r\nThree\r\n")
check("one line per page", "\n".join([paragraph] * 5), max_lines=1)
check("repeated spaces", "Hello     world   with   gaps\n\n   \n  another  line ")

print("\nAll invariants hold.")
