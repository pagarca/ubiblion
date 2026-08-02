#!/usr/bin/env python3
"""Command-line client for ubiblion.

Does the same thing as the web UI (convert EPUB to plain text and upload it
through the REST API) but from the PC, for bulk uploads or scripting. Uses
only the standard library.

Examples:
    python tools/upload_book.py book.epub
    python tools/upload_book.py *.txt --host 192.168.1.50
    python tools/upload_book.py --list
    python tools/upload_book.py --delete b0003
"""

import argparse
import json
import os
import re
import sys
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from html.parser import HTMLParser
from xml.etree import ElementTree

BLOCK_TAGS = {
    "p", "div", "h1", "h2", "h3", "h4", "h5", "h6", "li", "blockquote",
    "section", "article", "tr", "br", "hr", "pre", "figcaption", "td",
}
SKIP_TAGS = {"script", "style", "head"}
ZERO_WIDTH = dict.fromkeys([0x200B, 0x200C, 0x200D, 0xFEFF], None)


class TextExtractor(HTMLParser):
    """Extracts text from an XHTML file, inserting breaks at block elements."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.skip_depth = 0

    def handle_starttag(self, tag, attrs):
        if tag in SKIP_TAGS:
            self.skip_depth += 1
        elif tag in BLOCK_TAGS:
            self.parts.append("\n")

    def handle_endtag(self, tag):
        if tag in SKIP_TAGS:
            self.skip_depth = max(0, self.skip_depth - 1)
        elif tag in BLOCK_TAGS:
            self.parts.append("\n")

    def handle_data(self, data):
        if not self.skip_depth:
            self.parts.append(re.sub(r"\s+", " ", data))

    def text(self):
        return "".join(self.parts)


def normalize(text):
    """One paragraph per line, no blanks: the format the firmware expects."""
    text = unicodedata.normalize("NFC", text)
    text = text.replace(chr(0xA0), " ").translate(ZERO_WIDTH)
    lines = (re.sub(r"[ \t]+", " ", line).strip() for line in text.split("\n"))
    return "\n".join(line for line in lines if line) + "\n"


def _tag(element):
    """Tag name without the XML namespace."""
    return element.tag.rsplit("}", 1)[-1]


def _find(root, name):
    for element in root.iter():
        if _tag(element) == name:
            return element
    return None


def epub_to_text(path):
    """Returns (text, title, author) from a .epub file."""
    with zipfile.ZipFile(path) as zf:
        container = ElementTree.fromstring(zf.read("META-INF/container.xml"))
        rootfile = _find(container, "rootfile")
        if rootfile is None:
            raise ValueError("the EPUB doesn't declare an OPF file")
        opf_path = rootfile.attrib["full-path"]
        opf_dir = os.path.dirname(opf_path)

        opf = ElementTree.fromstring(zf.read(opf_path))
        title_el = _find(opf, "title")
        creator_el = _find(opf, "creator")
        title = (title_el.text or "").strip() if title_el is not None else ""
        author = (creator_el.text or "").strip() if creator_el is not None else ""

        manifest = {}
        for item in opf.iter():
            if _tag(item) == "item":
                manifest[item.attrib.get("id")] = (
                    item.attrib.get("href", ""),
                    item.attrib.get("media-type", ""),
                )

        chunks = []
        for ref in opf.iter():
            if _tag(ref) != "itemref":
                continue
            href, media = manifest.get(ref.attrib.get("idref"), ("", ""))
            if not href or (media and not re.search(r"html|xml", media)):
                continue
            name = urllib.parse.unquote(os.path.normpath(os.path.join(opf_dir, href)))
            name = name.replace(os.sep, "/")
            try:
                raw = zf.read(name)
            except KeyError:
                continue  # missing chapter: carry on with the rest
            parser = TextExtractor()
            parser.feed(raw.decode("utf-8", "replace"))
            chunks.append(parser.text())

    if not chunks:
        raise ValueError("the EPUB has no content in its spine")
    return normalize("\n\n".join(chunks)), title, author


def request(args, method, path, body=None, content_type=None):
    url = "http://%s%s" % (args.host, path)
    req = urllib.request.Request(url, data=body, method=method)
    if content_type:
        req.add_header("Content-Type", content_type)
    if args.key:
        req.add_header("X-Api-Key", args.key)
    try:
        with urllib.request.urlopen(req, timeout=args.timeout) as res:
            return json.loads(res.read().decode("utf-8") or "{}")
    except urllib.error.HTTPError as err:
        detail = err.read().decode("utf-8", "replace")
        raise SystemExit("device returned error %s: %s" % (err.code, detail))
    except urllib.error.URLError as err:
        raise SystemExit("could not reach %s: %s" % (args.host, err.reason))


def upload(args, text, title, author):
    boundary = "----ubiblion-%d" % os.getpid()
    # The filename travels as a fallback for the URL's `title` parameter.
    filename = re.sub(r'[\\/:*?"<>|]', "_", title)[:60] or "book"
    body = b"".join([
        ("--%s\r\n" % boundary).encode(),
        ('Content-Disposition: form-data; name="file"; filename="%s.txt"\r\n' % filename).encode(),
        b"Content-Type: text/plain; charset=utf-8\r\n\r\n",
        text.encode("utf-8"),
        ("\r\n--%s--\r\n" % boundary).encode(),
    ])
    query = urllib.parse.urlencode({"title": title, "author": author})
    return request(args, "POST", "/api/books?" + query, body,
                   "multipart/form-data; boundary=" + boundary)


def main():
    parser = argparse.ArgumentParser(description="Uploads books to ubiblion.")
    parser.add_argument("files", nargs="*", help=".epub or .txt files")
    parser.add_argument("--host", default="ubiblion.local", help="device host or IP")
    parser.add_argument("--key", default=os.environ.get("UBIBLION_KEY", ""),
                        help="API key (or the UBIBLION_KEY environment variable)")
    parser.add_argument("--title", help="force the title (single file only)")
    parser.add_argument("--author", help="force the author (single file only)")
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="seconds to wait for a response (default: 300; large "
                             "books can take a couple of minutes to upload and write "
                             "to flash over a slow WiFi link)")
    parser.add_argument("--list", action="store_true", help="list the library")
    parser.add_argument("--delete", metavar="ID", help="delete a book by id")
    args = parser.parse_args()

    if args.list:
        for book in request(args, "GET", "/api/library"):
            state = "%s pages" % book["pages"] if book["indexed"] else "not indexed"
            print("%-6s %-40s %8d B  %s" % (book["id"], book["title"][:40], book["bytes"], state))
        return

    if args.delete:
        request(args, "DELETE", "/api/books/" + args.delete)
        print("deleted", args.delete)
        return

    if not args.files:
        parser.error("give at least one file (or use --list / --delete)")

    for path in args.files:
        base = os.path.splitext(os.path.basename(path))[0]
        try:
            if path.lower().endswith(".epub"):
                text, title, author = epub_to_text(path)
            else:
                with open(path, "r", encoding="utf-8", errors="replace") as fh:
                    text, title, author = normalize(fh.read()), base, ""
        except (OSError, ValueError, zipfile.BadZipFile, ElementTree.ParseError) as err:
            print("error with %s: %s" % (path, err), file=sys.stderr)
            continue

        title = args.title or title or base
        author = args.author or author
        print("uploading %s (%s, %d KB)..." % (title, path, len(text.encode("utf-8")) // 1024))
        result = upload(args, text, title, author)
        print("  -> %s, indexing on the device" % result.get("id", "?"))


if __name__ == "__main__":
    main()
