/* Stockroom frontend. Every action calls the C++ backend REST API; nothing is simulated. */
'use strict';

const LOW_STOCK = 5;
const $ = (sel, root = document) => root.querySelector(sel);
const fmt = new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' });
const money = (n) => fmt.format(n);
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const icon = (name) => `<svg class="ico"><use href="#i-${name}"/></svg>`;
const debounce = (fn, ms) => { let t; return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); }; };

/* ---------- API ---------- */
async function api(method, url, body) {
  let res;
  try {
    res = await fetch(url, {
      method,
      headers: { 'Content-Type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
  } catch (e) {
    setConnection(false);
    throw new Error('Cannot reach the server. Make sure the Store server is still running.');
  }
  setConnection(true);
  let data = null;
  try { data = await res.json(); } catch (e) { /* non-JSON body */ }
  if (!res.ok) throw new Error((data && data.error) || `Request failed (${res.status})`);
  return data;
}
function setConnection(ok) {
  const c = $('#conn');
  c.classList.toggle('offline', !ok);
  $('span', c).textContent = ok ? 'Connected' : 'Server offline';
}

/* ---------- toasts & dialogs ---------- */
function toast(msg, type = 'ok') {
  const el = document.createElement('div');
  el.className = 'toast' + (type === 'error' ? ' error' : '');
  el.textContent = msg;
  $('#toasts').appendChild(el);
  setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 260); }, type === 'error' ? 6000 : 3500);
}
document.addEventListener('click', (e) => {
  const closer = e.target.closest('[data-close]');
  if (closer) closer.closest('dialog').close();
  if (e.target instanceof HTMLDialogElement) { // click on the backdrop (outside the dialog box)
    const r = e.target.getBoundingClientRect();
    if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) e.target.close();
  }
});
function confirmDialog(title, text, okLabel) {
  return new Promise((resolve) => {
    const d = $('#confirmDialog');
    $('#confirmTitle').textContent = title;
    $('#confirmText').textContent = text;
    $('#confirmOk').textContent = okLabel;
    let result = false;
    const ok = () => { result = true; d.close(); };
    $('#confirmOk').addEventListener('click', ok, { once: true });
    d.addEventListener('close', () => { $('#confirmOk').removeEventListener('click', ok); resolve(result); }, { once: true });
    d.showModal();
  });
}

/* ---------- field validation helpers ---------- */
function setFieldError(input, msg) {
  clearFieldError(input);
  input.classList.add('invalid');
  const p = document.createElement('div');
  p.className = 'field-err';
  p.textContent = msg;
  input.insertAdjacentElement('afterend', p);
}
function clearFieldError(input) {
  input.classList.remove('invalid');
  const n = input.nextElementSibling;
  if (n && n.classList.contains('field-err')) n.remove();
}
function clearAllErrors(form) { form.querySelectorAll('input,select').forEach(clearFieldError); }
const isWhole = (v) => /^\d+$/.test(String(v).trim());
function showFormError(box, msg) { box.textContent = msg; box.hidden = !msg; }

/* ---------- routing ---------- */
const routes = {
  dashboard: { title: 'Dashboard', sub: 'A snapshot of your stock and sales.', render: renderDashboard },
  products: { title: 'Products', sub: 'Everything on your shelves, with live stock levels.', render: renderProducts },
  customers: { title: 'Customers', sub: 'The people you sell to.', render: renderCustomers },
  sales: { title: 'Sales', sub: 'Record invoices and review past sales.', render: renderSales },
};
let renderToken = 0;
async function router() {
  const key = (location.hash.replace(/^#\/?/, '') || 'dashboard').split('/')[0];
  const route = routes[key];
  if (!route) { location.hash = '#/dashboard'; return; }
  document.querySelectorAll('#nav a').forEach((a) => a.classList.toggle('active', a.dataset.route === key));
  $('#pageTitle').textContent = route.title;
  $('#pageSub').textContent = route.sub;
  $('#pageActions').innerHTML = '';
  document.title = `${route.title} · Stockroom`;
  const view = $('#view');
  view.innerHTML = '<div class="skeleton"></div>';
  const token = ++renderToken;
  try {
    const html = await route.render(token);
    if (token !== renderToken) return;
  } catch (err) {
    if (token !== renderToken) return;
    view.innerHTML = `<div class="card error-box"><b>Something went wrong</b><p>${esc(err.message)}</p><button class="btn ghost" id="retry">Try again</button></div>`;
    $('#retry').onclick = router;
  }
}
window.addEventListener('hashchange', router);
function mount(html) { const v = $('#view'); v.innerHTML = html; v.classList.remove('fade'); void v.offsetWidth; v.classList.add('fade'); return v; }
const stockChip = (q) => q === 0 ? '<span class="chip out">Out of stock</span>' : q <= LOW_STOCK ? `<span class="chip low">Low · ${q}</span>` : `<span class="chip ok">${q}</span>`;

/* ---------- Dashboard ---------- */
async function renderDashboard(token) {
  const [products, customers, sales] = await Promise.all([api('GET', '/api/products'), api('GET', '/api/customers'), api('GET', '/api/sales')]);
  if (token !== renderToken) return;
  const units = products.reduce((a, p) => a + p.quantity, 0);
  const value = products.reduce((a, p) => a + p.price * p.quantity, 0);
  const revenue = sales.reduce((a, s) => a + s.totalPrice, 0);
  const low = products.filter((p) => p.quantity <= LOW_STOCK).sort((a, b) => a.quantity - b.quantity).slice(0, 6);
  const recent = sales.slice(-5).reverse();
  $('#pageActions').innerHTML = `<a class="btn primary" href="#/sales">${icon('receipt')}New sale</a><a class="btn ghost" href="#/products">${icon('box')}Manage products</a>`;
  mount(`
    <div class="stats">
      <div class="card stat"><div class="k">Products</div><div class="v">${products.length}</div></div>
      <div class="card stat"><div class="k">Units in stock</div><div class="v">${units}</div></div>
      <div class="card stat amber"><div class="k">Stock value</div><div class="v">${money(value)}</div></div>
      <div class="card stat"><div class="k">Customers</div><div class="v">${customers.length}</div></div>
      <div class="card stat"><div class="k">Invoices</div><div class="v">${sales.length}</div></div>
      <div class="card stat amber"><div class="k">Revenue</div><div class="v">${money(revenue)}</div></div>
    </div>
    <div class="grid-2">
      <div class="card card-pad">
        <h2>Needs restocking</h2><p class="sub">Products with ${LOW_STOCK} units or fewer.</p>
        ${low.length ? `<ul class="list">${low.map((p) => `<li><div>${esc(p.name)}<small class="mono">#${p.id}</small></div>${stockChip(p.quantity)}</li>`).join('')}</ul>`
          : `<div class="empty"><b>All stocked up</b>${products.length ? 'No product is running low.' : 'Add your first product to get started.'}</div>`}
      </div>
      <div class="card card-pad">
        <h2>Recent sales</h2><p class="sub">The latest five invoices.</p>
        ${recent.length ? `<ul class="list">${recent.map((s) => `<li><div>Invoice <span class="mono">#${s.invoiceNumber}</span> · ${esc(s.customerName)}<small>${esc(s.productName)} × ${s.quantity} · ${esc(s.date)}</small></div><b class="mono">${money(s.totalPrice)}</b></li>`).join('')}</ul>`
          : `<div class="empty"><b>No sales yet</b>Invoices you record will appear here.</div>`}
      </div>
    </div>`);
}

/* ---------- Products ---------- */
let productEditing = null;
let productSearch = '';
let productSearchSeq = 0;

async function renderProducts(token) {
  const list = await api('GET', '/api/products?q=' + encodeURIComponent(productSearch));
  if (token !== renderToken) return;
  $('#pageActions').innerHTML = `<button class="btn primary" id="addProduct">${icon('plus')}Add product</button>`;
  $('#addProduct').onclick = () => openProductDialog(null);
  const v = mount(`
    <div class="card">
      <div class="toolbar">
        <div class="search">${icon('search')}<input id="productSearch" type="search" placeholder="Search by name or ID…" value="${esc(productSearch)}" aria-label="Search products"></div>
        <span class="sub" id="productCount" style="margin:0"></span>
      </div>
      <div class="table-wrap"><table class="stack"><thead><tr><th>ID</th><th>Name</th><th class="num">Price</th><th class="num">Stock</th><th></th></tr></thead><tbody id="productRows"></tbody></table></div>
    </div>`);
  fillProductRows(list);
  $('#productSearch', v).addEventListener('input', debounce(async (e) => {
    productSearch = e.target.value;
    const seq = ++productSearchSeq;
    try {
      const res = await api('GET', '/api/products?q=' + encodeURIComponent(productSearch));
      if (seq === productSearchSeq && $('#productRows')) fillProductRows(res);
    } catch (err) { toast(err.message, 'error'); }
  }, 200));
}
function fillProductRows(list) {
  $('#productCount').textContent = `${list.length} ${list.length === 1 ? 'product' : 'products'}`;
  const tbody = $('#productRows');
  if (!list.length) {
    tbody.innerHTML = `<tr><td colspan="5"><div class="empty"><b>${productSearch ? 'No matching products' : 'No products yet'}</b>${productSearch ? 'Try a different search term.' : 'Use “Add product” to stock your first item.'}</div></td></tr>`;
    return;
  }
  tbody.innerHTML = list.map((p) => `
    <tr>
      <td data-label="ID" class="mono">${p.id}</td>
      <td data-label="Name"><b>${esc(p.name)}</b></td>
      <td data-label="Price" class="num mono">${money(p.price)}</td>
      <td data-label="Stock" class="num">${stockChip(p.quantity)}</td>
      <td class="actions-cell"><div class="actions">
        <button class="icon-btn" data-edit="${p.id}" title="Edit ${esc(p.name)}" aria-label="Edit ${esc(p.name)}">${icon('edit')}</button>
        <button class="icon-btn del" data-del="${p.id}" title="Delete ${esc(p.name)}" aria-label="Delete ${esc(p.name)}">${icon('trash')}</button>
      </div></td>
    </tr>`).join('');
  tbody.querySelectorAll('[data-edit]').forEach((b) => b.onclick = () => openProductDialog(list.find((p) => p.id === +b.dataset.edit)));
  tbody.querySelectorAll('[data-del]').forEach((b) => b.onclick = () => deleteProduct(list.find((p) => p.id === +b.dataset.del)));
}
function openProductDialog(p) {
  productEditing = p;
  const f = $('#productForm');
  clearAllErrors(f);
  showFormError($('#productError'), '');
  $('#productDialogTitle').textContent = p ? 'Edit product' : 'Add product';
  $('#productSubmit').textContent = p ? 'Save changes' : 'Add product';
  $('#pName').value = p ? p.name : '';
  $('#pPrice').value = p ? p.price.toFixed(2) : '';
  $('#pQty').value = p ? p.quantity : '';
  $('#productDialog').showModal();
  $('#pName').focus();
}
$('#productForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const f = e.target, err = $('#productError');
  clearAllErrors(f); showFormError(err, '');
  const name = $('#pName').value.trim(), price = $('#pPrice').value.trim(), qty = $('#pQty').value.trim();
  let bad = false;
  if (!name) { setFieldError($('#pName'), 'Enter a product name.'); bad = true; }
  if (price === '' || isNaN(+price) || +price < 0) { setFieldError($('#pPrice'), 'Enter a price of 0 or more.'); bad = true; }
  if (!isWhole(qty)) { setFieldError($('#pQty'), 'Enter a whole number, 0 or more.'); bad = true; }
  if (bad) return;
  const btn = $('#productSubmit');
  btn.disabled = true;
  try {
    const body = { name, price: +price, quantity: +qty };
    const saved = productEditing ? await api('PUT', '/api/products/' + productEditing.id, body) : await api('POST', '/api/products', body);
    $('#productDialog').close();
    toast(productEditing ? `Updated “${saved.name}”.` : `Added “${saved.name}” (ID ${saved.id}).`);
    router();
  } catch (ex) { showFormError(err, ex.message); }
  finally { btn.disabled = false; }
});
async function deleteProduct(p) {
  const ok = await confirmDialog(`Delete “${p.name}”?`, 'This removes the product from your inventory. Invoices that already include it are kept.', 'Delete product');
  if (!ok) return;
  try { await api('DELETE', '/api/products/' + p.id); toast(`Deleted “${p.name}”.`); router(); }
  catch (ex) { toast(ex.message, 'error'); }
}

/* ---------- Customers ---------- */
async function renderCustomers(token) {
  const customers = await api('GET', '/api/customers');
  if (token !== renderToken) return;
  const nextId = customers.reduce((m, c) => Math.max(m, c.id), 0) + 1;
  const v = mount(`
    <div class="split">
      <form class="card card-pad" id="customerForm" novalidate>
        <h2>Add customer</h2><p class="sub">IDs are yours to choose and must be unique.</p>
        <div class="form-error" id="customerError" role="alert" hidden></div>
        <label>Full name<input id="cName" maxlength="100" autocomplete="off" required></label>
        <label>Customer ID<input id="cId" type="number" min="1" step="1" inputmode="numeric" value="${nextId}" required></label>
        <label>Phone<input id="cPhone" type="tel" maxlength="20" autocomplete="off" placeholder="+20 100 123 4567" required></label>
        <div class="dialog-actions"><button class="btn primary" id="customerSubmit" type="submit">${icon('plus')}Add customer</button></div>
      </form>
      <div class="card">
        <div class="toolbar">
          <div class="search">${icon('search')}<input id="customerFilter" type="search" placeholder="Filter customers…" aria-label="Filter customers"></div>
          <span class="sub" id="customerCount" style="margin:0"></span>
        </div>
        <div class="table-wrap"><table class="stack"><thead><tr><th>ID</th><th>Name</th><th>Phone</th></tr></thead><tbody id="customerRows"></tbody></table></div>
      </div>
    </div>`);
  const fill = () => {
    const q = $('#customerFilter', v).value.trim().toLowerCase();
    const rows = customers.filter((c) => !q || c.name.toLowerCase().includes(q) || String(c.id) === q || c.phone.toLowerCase().includes(q));
    $('#customerCount').textContent = `${rows.length} ${rows.length === 1 ? 'customer' : 'customers'}`;
    $('#customerRows').innerHTML = rows.length ? rows.map((c) => `<tr><td data-label="ID" class="mono">${c.id}</td><td data-label="Name"><b>${esc(c.name)}</b></td><td data-label="Phone" class="mono">${esc(c.phone)}</td></tr>`).join('')
      : `<tr><td colspan="3"><div class="empty"><b>${customers.length ? 'No matches' : 'No customers yet'}</b>${customers.length ? 'Try a different filter.' : 'Add your first customer using the form.'}</div></td></tr>`;
  };
  fill();
  $('#customerFilter', v).addEventListener('input', fill);
  $('#customerForm', v).addEventListener('submit', async (e) => {
    e.preventDefault();
    const f = e.target, err = $('#customerError');
    clearAllErrors(f); showFormError(err, '');
    const name = $('#cName').value.trim(), id = $('#cId').value.trim(), phone = $('#cPhone').value.trim();
    let bad = false;
    if (!name) { setFieldError($('#cName'), 'Enter the customer’s name.'); bad = true; }
    if (!isWhole(id) || +id < 1) { setFieldError($('#cId'), 'Enter a whole number of 1 or more.'); bad = true; }
    if (!/^[0-9+\-() ]{5,20}$/.test(phone)) { setFieldError($('#cPhone'), 'Use 5–20 digits, spaces, + - ( ).'); bad = true; }
    if (bad) return;
    const btn = $('#customerSubmit'); btn.disabled = true;
    try {
      const c = await api('POST', '/api/customers', { name, id: +id, phone });
      toast(`Added customer “${c.name}”.`);
      router();
    } catch (ex) { showFormError(err, ex.message); btn.disabled = false; }
  });
}

/* ---------- Sales ---------- */
function openInvoice(s) {
  $('#invoiceBody').innerHTML = `
    <div class="invoice">
      <div class="inv-head"><div><h2>Sales Invoice</h2><div>Stockroom</div></div><div style="text-align:right"><b>#${s.invoiceNumber}</b><div>${esc(s.date)}</div></div></div>
      <div class="line"><span>Customer</span><span>${esc(s.customerName)}</span></div>
      <div class="line"><span>Customer ID</span><span>${s.customerId}</span></div>
      <div class="line"><span>Product</span><span>${esc(s.productName)}</span></div>
      <div class="line"><span>Quantity</span><span>${s.quantity}</span></div>
      <div class="line"><span>Unit price</span><span>${money(s.unitPrice)}</span></div>
      <div class="line grand"><span>Total</span><span>${money(s.totalPrice)}</span></div>
    </div>`;
  $('#invoiceDialog').showModal();
}
$('#printInvoice').addEventListener('click', () => window.print());

async function renderSales(token) {
  const [products, customers, sales] = await Promise.all([api('GET', '/api/products'), api('GET', '/api/customers'), api('GET', '/api/sales')]);
  if (token !== renderToken) return;
  const nextInv = sales.reduce((m, s) => Math.max(m, s.invoiceNumber), 0) + 1;
  const canSell = customers.length > 0 && products.some((p) => p.quantity > 0);
  const missing = !customers.length ? `Add a <a href="#/customers">customer</a> first.` : !products.some((p) => p.quantity > 0) ? `No product is in stock. Add or restock in <a href="#/products">Products</a>.` : '';
  const v = mount(`
    <div class="split">
      <form class="card card-pad" id="saleForm" novalidate>
        <h2>New sale</h2><p class="sub">Stock is deducted when the invoice is recorded.</p>
        ${missing ? `<div class="notice">${icon('alert')}<div>${missing}</div></div>` : ''}
        <div class="form-error" id="saleError" role="alert" hidden></div>
        <label>Invoice number<input id="sInv" type="number" min="1" step="1" inputmode="numeric" value="${nextInv}" required></label>
        <label>Customer<select id="sCust" required><option value="">Select a customer…</option>${customers.map((c) => `<option value="${c.id}">${esc(c.name)} (ID ${c.id})</option>`).join('')}</select></label>
        <label>Product<select id="sProd" required><option value="">Select a product…</option>${products.map((p) => `<option value="${p.id}" ${p.quantity === 0 ? 'disabled' : ''}>${esc(p.name)} · ${p.quantity} in stock</option>`).join('')}</select></label>
        <label>Quantity<input id="sQty" type="number" min="1" step="1" inputmode="numeric" value="1" required></label>
        <div class="preview" id="salePreview"><div><span>Unit price</span><span class="mono">—</span></div><div class="total"><span>Total</span><span class="mono">—</span></div></div>
        <div class="dialog-actions"><button class="btn primary" id="saleSubmit" type="submit" ${canSell ? '' : 'disabled'}>${icon('receipt')}Record sale</button></div>
      </form>
      <div class="card">
        <div class="toolbar">
          <div class="search">${icon('search')}<input id="saleFilter" type="search" placeholder="Filter by invoice, customer, product…" aria-label="Filter sales"></div>
          <span class="sub" id="saleCount" style="margin:0"></span>
        </div>
        <div class="table-wrap"><table class="stack"><thead><tr><th>Invoice</th><th>Date</th><th>Customer</th><th>Product</th><th class="num">Qty</th><th class="num">Total</th><th></th></tr></thead><tbody id="saleRows"></tbody></table></div>
      </div>
    </div>`);

  const updatePreview = () => {
    const p = products.find((x) => x.id === +$('#sProd').value);
    const q = $('#sQty').value.trim();
    const total = p && isWhole(q) ? money(p.price * +q) : '—';
    $('#salePreview').innerHTML = `<div><span>Unit price</span><span class="mono">${p ? money(p.price) : '—'}</span></div>${p ? `<div><span>Available</span><span class="mono">${p.quantity}</span></div>` : ''}<div class="total"><span>Total</span><span class="mono">${total}</span></div>`;
  };
  ['sProd', 'sQty'].forEach((id) => $('#' + id, v).addEventListener('input', updatePreview));

  const sorted = sales.slice().reverse();
  const fill = () => {
    const q = $('#saleFilter', v).value.trim().toLowerCase();
    const rows = sorted.filter((s) => !q || String(s.invoiceNumber) === q || s.customerName.toLowerCase().includes(q) || s.productName.toLowerCase().includes(q));
    $('#saleCount').textContent = `${rows.length} ${rows.length === 1 ? 'invoice' : 'invoices'}`;
    $('#saleRows').innerHTML = rows.length ? rows.map((s) => `<tr>
        <td data-label="Invoice" class="mono">#${s.invoiceNumber}</td><td data-label="Date" class="mono">${esc(s.date)}</td>
        <td data-label="Customer">${esc(s.customerName)}</td><td data-label="Product">${esc(s.productName)}</td>
        <td data-label="Qty" class="num mono">${s.quantity}</td><td data-label="Total" class="num mono"><b>${money(s.totalPrice)}</b></td>
        <td class="actions-cell"><div class="actions"><button class="icon-btn" data-inv="${s.invoiceNumber}" title="View invoice" aria-label="View invoice ${s.invoiceNumber}">${icon('receipt')}</button></div></td></tr>`).join('')
      : `<tr><td colspan="7"><div class="empty"><b>${sales.length ? 'No matches' : 'No sales yet'}</b>${sales.length ? 'Try a different filter.' : 'Record your first sale using the form.'}</div></td></tr>`;
    $('#saleRows').querySelectorAll('[data-inv]').forEach((b) => b.onclick = () => openInvoice(sales.find((s) => s.invoiceNumber === +b.dataset.inv)));
  };
  fill();
  $('#saleFilter', v).addEventListener('input', fill);

  $('#saleForm', v).addEventListener('submit', async (e) => {
    e.preventDefault();
    const f = e.target, err = $('#saleError');
    clearAllErrors(f); showFormError(err, '');
    const inv = $('#sInv').value.trim(), cust = $('#sCust').value, prod = $('#sProd').value, qty = $('#sQty').value.trim();
    let bad = false;
    if (!isWhole(inv) || +inv < 1) { setFieldError($('#sInv'), 'Enter a whole number of 1 or more.'); bad = true; }
    if (!cust) { setFieldError($('#sCust'), 'Choose a customer.'); bad = true; }
    if (!prod) { setFieldError($('#sProd'), 'Choose a product.'); bad = true; }
    if (!isWhole(qty) || +qty < 1) { setFieldError($('#sQty'), 'Enter a whole number of 1 or more.'); bad = true; }
    else if (prod) {
      const p = products.find((x) => x.id === +prod);
      if (p && +qty > p.quantity) { setFieldError($('#sQty'), `Only ${p.quantity} in stock.`); bad = true; }
    }
    if (bad) return;
    const btn = $('#saleSubmit'); btn.disabled = true;
    try {
      const s = await api('POST', '/api/sales', { invoiceNumber: +inv, customerId: +cust, productId: +prod, quantity: +qty });
      toast(`Invoice #${s.invoiceNumber} recorded — ${money(s.totalPrice)}.`);
      await router();
      openInvoice(s);
    } catch (ex) { showFormError(err, ex.message); btn.disabled = false; }
  });
}

router();
