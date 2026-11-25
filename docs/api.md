# REST API

[← Back to the README](../README.md)

Every endpoint returns JSON. If `config.json` sets `apiKey`, it has to be sent in the `X-Api-Key`
header; otherwise the API is open on the local network.

| Method | Route | What it does |
|---|---|---|
| `GET` | `/` | Web UI |
| `GET` | `/api/status` | Version, heap, free space, indexing state |
| `GET` | `/api/library` | List of books |
| `POST` | `/api/books?title=…&author=…` | Uploads a `.txt` (`multipart/form-data`, `file` field) |
| `GET` | `/api/books/{id}` | Downloads the text |
| `DELETE` | `/api/books/{id}` | Deletes the book and its index |
| `POST` | `/api/books/{id}/open?page=N` | Marks it as the current book |
| `POST` | `/api/books/{id}/reindex` | Rebuilds the page index |
| `GET` `POST` | `/api/config` | Reads or saves SSID, password, hostname, API key, `wifiOnBoot`, `lang`… |
| `POST` | `/api/system/reboot` | Restarts |

An upload responds `202` as soon as the file is on flash; indexing continues in the background and its
progress can be checked at `/api/status`.

```bash
curl -F "file=@moby-dick.txt" "http://ubiblion.local/api/books?title=Moby%20Dick"
curl -s http://ubiblion.local/api/library | jq
```

## Console client

`tools/upload_book.py` does the same thing as the web UI (EPUB conversion included) with no
dependencies beyond Python's standard library:

```bash
python tools/upload_book.py book.epub
```

```bash
python tools/upload_book.py --list
```

## Large uploads

Uploading a multi-megabyte book over WiFi to a single-core microcontroller genuinely takes a couple of
minutes end to end (transfer + writing to flash + indexing); the web UI's `fetch()` has no built-in
timeout so it just waits, but `tools/upload_book.py --timeout` defaults to 300s for the same reason --
raise it further for very large files on a slow network.
