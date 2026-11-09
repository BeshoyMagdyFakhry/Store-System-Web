"""Browser test (Playwright + Chromium): drives the real UI against the real C++ server."""
import os, subprocess, sys, tempfile, time, json
from playwright.sync_api import sync_playwright, expect

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe = os.path.join(ROOT, "build", "store_server" + (".exe" if os.name == "nt" else ""))
data = os.path.join(tempfile.mkdtemp(), "store.json")
PORT = 18766
URL = f"http://127.0.0.1:{PORT}"
SHOTS = os.path.join(ROOT, "tests", "screenshots"); os.makedirs(SHOTS, exist_ok=True)
fails = 0

def check(name, cond, info=""):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  -> {info}"))
    if not cond: fails += 1

srv = subprocess.Popen([exe, "--port", str(PORT), "--data", data, "--frontend", os.path.join(ROOT, "frontend")], cwd=ROOT,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.6)
try:
    with sync_playwright() as pw:
        browser = pw.chromium.launch()
        page = browser.new_page(viewport={"width": 1366, "height": 850})
        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type in ("error", "warning") else None)
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.on("requestfailed", lambda r: errors.append("requestfailed " + r.url))
        page.on("response", lambda r: errors.append(f"{r.status} {r.url}") if r.status >= 400 and "/api/" not in r.url else None)

        page.goto(URL)
        expect(page.locator("#pageTitle")).to_have_text("Dashboard")
        check("dashboard loads (empty)", page.locator(".stat").count() == 6)
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/dashboard-empty.png")

        # nav links
        for route, title in [("products", "Products"), ("customers", "Customers"), ("sales", "Sales"), ("dashboard", "Dashboard")]:
            page.click(f'#nav a[data-route="{route}"]')
            expect(page.locator("#pageTitle")).to_have_text(title)
            check(f"nav -> {title}", page.locator(f'#nav a[data-route="{route}"].active').count() == 1)

        # ---- products ----
        page.click('#nav a[data-route="products"]')
        check("empty products state", "No products yet" in page.locator("#productRows").inner_text())
        page.click("#addProduct")
        page.click("#productSubmit")
        check("product form validates empty", page.locator("#productForm .field-err").count() == 3)
        page.fill("#pName", "Laptop"); page.fill("#pPrice", "999.99"); page.fill("#pQty", "10")
        page.click("#productSubmit")
        expect(page.locator("#productRows")).to_contain_text("Laptop")
        check("product added to table", "Laptop" in page.locator("#productRows").inner_text() and "$999.99" in page.locator("#productRows").inner_text())
        check("success toast shown", "Added" in page.locator(".toast").first.inner_text())
        for n, p, q in [("Mouse", "19.5", "3"), ("Keyboard", "45", "0")]:
            page.click("#addProduct"); page.fill("#pName", n); page.fill("#pPrice", p); page.fill("#pQty", q); page.click("#productSubmit")
        expect(page.locator("#productRows tr")).to_have_count(3)
        page.click("#addProduct"); page.fill("#pName", "laptop"); page.fill("#pPrice", "1"); page.fill("#pQty", "1"); page.click("#productSubmit")
        expect(page.locator("#productError")).to_be_visible()
        check("server error shown in dialog (duplicate)", "already exists" in page.locator("#productError").inner_text())
        page.click("#productDialog [data-close]")
        check("low/out of stock chips", page.locator(".chip.low").count() == 1 and page.locator(".chip.out").count() == 1)
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/products.png")

        page.fill("#productSearch", "mou")
        expect(page.locator("#productRows tr")).to_have_count(1)
        check("search filters via backend", "Mouse" in page.locator("#productRows").inner_text())
        page.fill("#productSearch", "zzz")
        expect(page.locator("#productRows .empty")).to_be_visible()
        page.fill("#productSearch", "")
        expect(page.locator("#productRows tr")).to_have_count(3)

        page.click('[data-edit="10001"]')
        check("edit dialog prefilled", page.input_value("#pName") == "Mouse" and page.input_value("#pQty") == "3")
        page.fill("#pPrice", "21"); page.fill("#pQty", "8"); page.click("#productSubmit")
        expect(page.locator("#productRows tr").nth(1)).to_contain_text("$21.00")
        check("edit saved", "8" in page.locator("#productRows tr").nth(1).inner_text())

        page.click('[data-del="10002"]')
        expect(page.locator("#confirmDialog")).to_be_visible()
        page.click("#confirmDialog [data-close]")
        expect(page.locator("#productRows tr")).to_have_count(3)
        check("delete cancel keeps product", True)
        page.click('[data-del="10002"]'); page.click("#confirmOk")
        expect(page.locator("#productRows tr")).to_have_count(2)
        check("delete removes product", "Keyboard" not in page.locator("#productRows").inner_text())

        # ---- customers ----
        page.click('#nav a[data-route="customers"]')
        page.click("#customerSubmit")
        check("customer form validates", page.locator("#customerForm .field-err").count() == 2)
        page.fill("#cName", "Ali Hassan"); page.fill("#cPhone", "+20 100 123 4567"); page.click("#customerSubmit")
        expect(page.locator("#customerRows")).to_contain_text("Ali Hassan")
        check("customer added with suggested id 1", "Ali Hassan" in page.locator("#customerRows").inner_text() and page.input_value("#cId") == "2")
        page.fill("#cName", "Sara"); page.fill("#cId", "1"); page.fill("#cPhone", "12345"); page.click("#customerSubmit")
        expect(page.locator("#customerError")).to_be_visible()
        check("duplicate customer id error shown", "already in use" in page.locator("#customerError").inner_text())
        page.fill("#cId", "7"); page.click("#customerSubmit")
        expect(page.locator("#customerRows tr")).to_have_count(2)
        page.fill("#customerFilter", "sar")
        check("customer filter", page.locator("#customerRows tr").count() == 1)
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/customers.png")

        # ---- sales ----
        page.click('#nav a[data-route="sales"]')
        check("sales empty state", "No sales yet" in page.locator("#saleRows").inner_text())
        page.select_option("#sCust", "1"); page.select_option("#sProd", "10000"); page.fill("#sQty", "3")
        check("live total preview", "$2,999.97" in page.locator("#salePreview").inner_text())
        page.fill("#sQty", "99"); page.click("#saleSubmit")
        check("client stock validation", "Only 10 in stock" in page.locator("#saleForm .field-err").inner_text())
        page.fill("#sInv", "5"); page.fill("#sQty", "3"); page.click("#saleSubmit")
        expect(page.locator("#invoiceDialog")).to_be_visible()
        check("invoice dialog shows real data", "$2,999.97" in page.locator("#invoiceBody").inner_text() and "Ali Hassan" in page.locator("#invoiceBody").inner_text())
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/invoice.png")
        page.click("#invoiceDialog [data-close]")
        expect(page.locator("#saleRows")).to_contain_text("#5")
        check("sale in list, next invoice suggested 6", page.input_value("#sInv") == "6")
        check("stock deducted in dropdown", "7 in stock" in page.locator("#sProd option", has_text="Laptop").inner_text())
        page.select_option("#sCust", "1"); page.select_option("#sProd", "10000"); page.fill("#sInv", "5"); page.fill("#sQty", "1"); page.click("#saleSubmit")
        expect(page.locator("#saleError")).to_be_visible()
        check("duplicate invoice error shown", "already exists" in page.locator("#saleError").inner_text())
        page.click('[data-inv="5"]'); expect(page.locator("#invoiceDialog")).to_be_visible()
        page.keyboard.press("Escape")
        page.fill("#saleFilter", "nobody")
        expect(page.locator("#saleRows .empty")).to_be_visible()
        page.fill("#saleFilter", "")
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/sales.png")

        # ---- dashboard with data ----
        page.click('#nav a[data-route="dashboard"]')
        expect(page.locator(".stat .v").nth(5)).to_contain_text("$2,999.97")
        check("dashboard revenue + recent sale", "#5" in page.locator(".grid-2").inner_text())
        page.wait_for_timeout(700); page.screenshot(path=f"{SHOTS}/dashboard.png")

        # ---- persistence across restart (UI level) ----
        srv.terminate(); srv.wait()
        page.click('#nav a[data-route="products"]')
        expect(page.locator(".error-box")).to_be_visible()
        check("offline state handled", "Cannot reach the server" in page.locator(".error-box").inner_text() and page.locator("#conn.offline").count() == 1)
        srv = subprocess.Popen([exe, "--port", str(PORT), "--data", data, "--frontend", os.path.join(ROOT, "frontend")], cwd=ROOT,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.6)
        page.click("#retry")
        expect(page.locator("#productRows tr")).to_have_count(2)
        check("data survived restart", "Laptop" in page.locator("#productRows").inner_text())

        # ---- mobile & tablet ----
        for name, (w, h) in {"mobile": (390, 800), "tablet": (820, 1000)}.items():
            ctx = browser.new_context(viewport={"width": w, "height": h}); mp = ctx.new_page()
            mp.on("pageerror", lambda e: errors.append(str(e)))
            mp.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
            for r in ["dashboard", "products", "customers", "sales"]:
                mp.goto(f"{URL}/#/{r}"); mp.wait_for_selector(".fade")
                overflow = mp.evaluate("document.documentElement.scrollWidth > document.documentElement.clientWidth")
                check(f"{name}: no horizontal overflow on {r}", not overflow)
            mp.goto(f"{URL}/#/products"); mp.wait_for_selector("#productRows tr")
            mp.wait_for_timeout(700); mp.screenshot(path=f"{SHOTS}/{name}-products.png")
            mp.click('[data-edit="10000"]'); check(f"{name}: dialog opens", mp.locator("#productDialog").is_visible())
            ctx.close()

        # The only console noise allowed: the 3 deliberate duplicate submissions (409) and the deliberate server stop.
        expected = lambda e: "409 (Conflict)" in e or "ERR_CONNECTION_REFUSED" in e or e.startswith("requestfailed") and "/api/products" in e
        unexpected = [e for e in errors if not expected(e)]
        check("no unexpected browser console errors / failed requests", not unexpected, unexpected)
        check("exactly the 3 expected 409 responses", sum("409 (Conflict)" in e for e in errors) == 3, errors)
        browser.close()
finally:
    srv.terminate(); srv.wait()
print(f"\n{'ALL PASSED' if fails == 0 else str(fails) + ' FAILED'}")
sys.exit(1 if fails else 0)
