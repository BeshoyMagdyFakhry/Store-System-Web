"""End-to-end API test. Starts the compiled server on a temp data file and exercises every endpoint."""
import json, os, subprocess, sys, tempfile, time, urllib.request, urllib.error

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe = os.path.join(ROOT, "build", "store_server" + (".exe" if os.name == "nt" else ""))
tmp = tempfile.mkdtemp()
data = os.path.join(tmp, "store.json")
PORT = 18765
fails = 0

def start():
    p = subprocess.Popen([exe, "--port", str(PORT), "--data", data, "--frontend", os.path.join(ROOT, "frontend")],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=ROOT)
    time.sleep(0.6)
    return p

def call(method, path, body=None, raw=None):
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", method=method,
                                 data=raw if raw is not None else (json.dumps(body).encode() if body is not None else None),
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req) as r:
            t = r.read().decode()
            return r.status, (json.loads(t) if r.headers.get_content_type() == "application/json" else t)
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode())

def check(name, cond, info=""):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {info}"))
    if not cond: fails += 1

srv = start()
try:
    s, b = call("POST", "/api/products", {"name": "  Laptop ", "price": 999.999, "quantity": 5})
    check("add product", s == 201 and b["id"] == 10000 and b["name"] == "Laptop" and b["price"] == 1000.0, (s, b))
    s, b = call("POST", "/api/products", {"name": "Mouse", "price": 19.5, "quantity": 2})
    check("second product id 10001", s == 201 and b["id"] == 10001, (s, b))
    s, b = call("POST", "/api/products", {"name": "laptop", "price": 1, "quantity": 1})
    check("duplicate name rejected (case-insens.)", s == 409, (s, b))
    for bad, label in [({"name": "", "price": 1, "quantity": 1}, "empty name"),
                       ({"name": "X", "price": -1, "quantity": 1}, "negative price"),
                       ({"name": "X", "price": 1, "quantity": -3}, "negative quantity"),
                       ({"name": "X", "price": 1, "quantity": 1.5}, "fractional quantity"),
                       ({"name": "X", "price": "abc", "quantity": 1}, "text price"),
                       ({"name": "X"}, "missing fields")]:
        s, b = call("POST", "/api/products", bad)
        check("reject " + label, s == 400 and "error" in b, (s, b))
    s, b = call("POST", "/api/products", raw=b"{not json")
    check("reject malformed JSON", s == 400, (s, b))
    s, b = call("GET", "/api/products")
    check("list products", s == 200 and len(b) == 2, (s, b))
    s, b = call("GET", "/api/products?q=lap")
    check("search substring", s == 200 and len(b) == 1 and b[0]["name"] == "Laptop", (s, b))
    s, b = call("GET", "/api/products?q=zzz")
    check("search no match", s == 200 and b == [], (s, b))
    s, b = call("PUT", "/api/products/10001", {"name": "Mouse Pro", "price": 25, "quantity": 10})
    check("update product", s == 200 and b["name"] == "Mouse Pro" and b["quantity"] == 10, (s, b))
    s, b = call("PUT", "/api/products/10001", {"name": "Laptop", "price": 25, "quantity": 10})
    check("update to duplicate name rejected", s == 409, (s, b))
    s, b = call("PUT", "/api/products/99999", {"name": "Q", "price": 1, "quantity": 1})
    check("update missing product 404", s == 404, (s, b))

    s, b = call("POST", "/api/customers", {"name": "Ali Hassan", "id": 1, "phone": "+20 100 123 4567"})
    check("add customer (name with space)", s == 201 and b["name"] == "Ali Hassan", (s, b))
    s, b = call("POST", "/api/customers", {"name": "Dup", "id": 1, "phone": "12345"})
    check("duplicate customer id rejected", s == 409, (s, b))
    s, b = call("POST", "/api/customers", {"name": "Bad", "id": 2, "phone": "abc"})
    check("bad phone rejected", s == 400, (s, b))
    s, b = call("POST", "/api/customers", {"name": "Bad", "id": 0, "phone": "12345"})
    check("bad customer id rejected", s == 400, (s, b))
    s, b = call("GET", "/api/customers")
    check("list customers", s == 200 and len(b) == 1, (s, b))

    s, b = call("POST", "/api/sales", {"invoiceNumber": 1, "customerId": 1, "productId": 10000, "quantity": 2})
    check("make sale", s == 201 and b["totalPrice"] == 2000.0 and b["customerName"] == "Ali Hassan" and b["date"], (s, b))
    s, b = call("GET", "/api/products/10000")
    check("stock deducted", s == 200 and b["quantity"] == 3, (s, b))
    s, b = call("POST", "/api/sales", {"invoiceNumber": 1, "customerId": 1, "productId": 10000, "quantity": 1})
    check("duplicate invoice rejected", s == 409, (s, b))
    s, b = call("POST", "/api/sales", {"invoiceNumber": 2, "customerId": 1, "productId": 10000, "quantity": 4})
    check("insufficient stock rejected", s == 409 and "3" in b["error"], (s, b))
    s, b = call("POST", "/api/sales", {"invoiceNumber": 2, "customerId": 1, "productId": 10000, "quantity": 0})
    check("zero quantity rejected", s == 400, (s, b))
    s, b = call("POST", "/api/sales", {"invoiceNumber": 2, "customerId": 77, "productId": 10000, "quantity": 1})
    check("unknown customer 404", s == 404, (s, b))
    s, b = call("POST", "/api/sales", {"invoiceNumber": 2, "customerId": 1, "productId": 55555, "quantity": 1})
    check("unknown product 404", s == 404, (s, b))
    s, b = call("GET", "/api/products/10000")
    check("failed sales did not change stock", b["quantity"] == 3, b)
    s, b = call("GET", "/api/sales")
    check("list sales", s == 200 and len(b) == 1, (s, b))

    # persistence: restart and verify
    srv.terminate(); srv.wait(); time.sleep(0.3)
    srv = start()
    s, b = call("GET", "/api/products")
    check("persistence: products after restart", len(b) == 2 and {p["quantity"] for p in b} == {3, 10}, b)
    s, b = call("GET", "/api/customers"); check("persistence: customers", len(b) == 1, b)
    s, b = call("GET", "/api/sales"); check("persistence: sales", len(b) == 1 and b[0]["invoiceNumber"] == 1, b)
    s, b = call("POST", "/api/products", {"name": "Keyboard", "price": 40, "quantity": 1})
    check("persistence: next id continues", b.get("id") == 10002, b)

    s, b = call("DELETE", "/api/products/10002")
    check("delete product", s == 200, (s, b))
    s, b = call("DELETE", "/api/products/10002")
    check("delete again 404", s == 404, (s, b))
    s, b = call("GET", "/api/sales")
    check("sales history kept after product delete", len(b) == 1, b)
    s, b = call("POST", "/api/products", {"name": "Cable", "price": 5, "quantity": 1})
    check("deleted ids are not reused", b.get("id") == 10003, b)

    s, b = call("GET", "/api/nothing"); check("unknown endpoint 404", s == 404, (s, b))
    s, b = call("DELETE", "/api/customers"); check("method not allowed 405", s == 405, (s, b))
    s, b = call("GET", "/api/products/abc"); check("bad id 400", s == 400, (s, b))
    s, b = call("GET", "/")
    check("serves index.html", s == 200 and "<html" in b, (s, str(b)[:80]))
    s, b = call("GET", "/../backend/main.cpp"); check("path traversal blocked", s == 404, (s, b))
    s, b = call("GET", "/%2e%2e/backend/main.cpp"); check("encoded traversal blocked", s == 404, (s, b))
    s, b = call("GET", "/css/missing.css"); check("missing static 404", s == 404, (s, b))
finally:
    srv.terminate(); srv.wait()

print(f"\n{'ALL PASSED' if fails == 0 else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
