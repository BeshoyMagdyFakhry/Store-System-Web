# Stockroom – Store Management (C++ backend + web frontend)

Your console program, turned into a local website. The C++ code is the real backend; the browser talks to it over a JSON API on `localhost`.

## Run it

**Windows:** double-click `run.bat`. It compiles the backend with `g++` (MinGW-w64), starts the server and opens `http://localhost:8080`.
**Linux / macOS:** `./run.sh`, then open `http://localhost:8080`.

Needs only a C++17 compiler with threads (MinGW-w64 "posix" build, e.g. MSYS2 or WinLibs). No Node.js, no internet, no other downloads.
If port 8080 is busy the server uses the next free port (up to 8089) and prints the address. Data is saved to `data/store.json` after every change and loaded on start, so restarts keep everything.

## Layout

```
original/store_original.cpp   your original program, untouched
backend/
  store.hpp        product, Customer, sales, Store  (your logic, with validation)
  json.hpp         small JSON parser/writer
  storage.hpp      JSON <-> objects, saving/loading data/store.json
  http_server.hpp  dependency-free HTTP server (Winsock / POSIX), 127.0.0.1 only
  api.hpp          REST routes + static file serving
  main.cpp         startup, arguments, port selection
frontend/          index.html, css/style.css, js/app.js (no build step)
data/store.json    created automatically
tests/             api_test.py, ui_test.py (Playwright)
run.bat, run.sh    start scripts
```

## API

| Method & path | Purpose |
|---|---|
| `GET /api/products?q=` | list / search (name substring or exact ID) |
| `POST /api/products` | add `{name, price, quantity}` (ID auto-assigned from 10000) |
| `PUT /api/products/{id}` | update `{name, price, quantity}` |
| `DELETE /api/products/{id}` | delete (past invoices are kept) |
| `GET/POST /api/customers` | list / add `{name, id, phone}` |
| `GET/POST /api/sales` | list / record `{invoiceNumber, customerId, productId, quantity}` |

Errors return `{"error": "message"}` with 400 (invalid input), 404 (not found) or 409 (conflict, e.g. not enough stock).

## What changed from the original (and why)

- **Bugs fixed:** uninitialized `customerId` on the invoice; "Sale recorded" shown even when the sale failed; out-of-bounds access for an unknown product; messages printed twice; input that crashed or looped on bad data.
- **Validation added:** no negative price/quantity, no empty names, unique product names (case-insensitive), unique customer IDs and invoice numbers, phone format.
- **Sales use IDs** (chosen from dropdowns) instead of typed names, so duplicate names can't be ambiguous. Names with spaces now work.
- **Search** matches any part of a name (the original needed the exact name).
- **Prices** are rounded to cents. **Capacity** is 1000 per list (was 100).
- **Additions:** customer list view, invoice date, dashboard, invoice printing, filters on customers and sales. Customer edit/delete was not added (the original had none).
- Fonts use your system's (Georgia/Segoe UI/Consolas) so the site works fully offline.

## Tests

`python3 tests/api_test.py` (44 API checks) and `python3 tests/ui_test.py` (browser test; needs `pip install playwright` and its Chromium). Both run against the compiled server using a temporary data file.
