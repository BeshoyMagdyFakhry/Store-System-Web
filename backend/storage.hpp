// Converts store objects to/from JSON and persists the whole store to a file.
#pragma once
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "json.hpp"
#include "store.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

inline std::string toJson(const product& p) {
    return "{\"id\":" + std::to_string(p.getId()) + ",\"name\":" + json::quote(p.getName()) +
           ",\"price\":" + json::money(p.getPrice()) + ",\"quantity\":" + std::to_string(p.getQuantity()) + "}";
}

inline std::string toJson(const Customer& c) {
    return "{\"id\":" + std::to_string(c.getId()) + ",\"name\":" + json::quote(c.getName()) +
           ",\"phone\":" + json::quote(c.getPhone()) + "}";
}

inline std::string toJson(const sales& s) {
    return "{\"invoiceNumber\":" + std::to_string(s.getInvoiceNumber()) +
           ",\"customerId\":" + std::to_string(s.getCustomerId()) +
           ",\"productId\":" + std::to_string(s.getProductId()) +
           ",\"customerName\":" + json::quote(s.getCustomerName()) +
           ",\"productName\":" + json::quote(s.getProductName()) +
           ",\"quantity\":" + std::to_string(s.getQuantity()) +
           ",\"unitPrice\":" + json::money(s.getUnitPrice()) +
           ",\"totalPrice\":" + json::money(s.getTotalPrice()) + ",\"date\":" + json::quote(s.getDate()) + "}";
}

template <typename T>
std::string listJson(const std::vector<T>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) out += (i ? "," : "") + toJson(items[i]);
    return out + "]";
}

inline std::string storeToJson(const Store& st) {
    return "{\"nextProductId\":" + std::to_string(st.getNextProductId()) + ",\"products\":" +
           listJson(st.allProducts()) + ",\"customers\":" + listJson(st.allCustomers()) +
           ",\"sales\":" + listJson(st.allSales()) + "}\n";
}

// Writes to a temp file first, then replaces the real file. Returns an empty string on success.
inline std::string saveStore(const Store& st, const std::string& path) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return "cannot write " + tmp;
        f << storeToJson(st);
        f.flush();
        if (!f) return "disk write failed for " + tmp;
    }
#ifdef _WIN32
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return "cannot replace " + path;
#else
    if (std::rename(tmp.c_str(), path.c_str()) != 0) return "cannot replace " + path;
#endif
    return "";
}

namespace detail {
inline const json::Value& field(const json::Value& o, const char* key) {
    const json::Value* v = o.get(key);
    if (!v) throw json::ParseError(std::string("missing field '") + key + "'");
    return *v;
}
inline std::string str(const json::Value& o, const char* k) {
    const auto& v = field(o, k);
    if (v.type != json::Value::String) throw json::ParseError(std::string("field '") + k + "' must be text");
    return v.s;
}
inline double num(const json::Value& o, const char* k) {
    const auto& v = field(o, k);
    if (v.type != json::Value::Number) throw json::ParseError(std::string("field '") + k + "' must be a number");
    return v.n;
}
}  // namespace detail

// Loads a saved store. A missing file is fine (fresh start). Throws on unreadable/corrupt data
// so the caller can refuse to start instead of silently overwriting the file.
inline void loadStore(Store& st, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    if (trim(text).empty()) return;

    using namespace detail;
    json::Value root = json::parse(text);
    if (root.type != json::Value::Object) throw json::ParseError("root must be an object");

    for (const auto& p : field(root, "products").a)
        st.restoreProduct(product(str(p, "name"), num(p, "price"), (int)num(p, "quantity"), (int)num(p, "id")));
    for (const auto& c : field(root, "customers").a)
        st.restoreCustomer(Customer(str(c, "name"), (int)num(c, "id"), str(c, "phone")));
    for (const auto& s : field(root, "sales").a)
        st.restoreSale(sales((int)num(s, "invoiceNumber"), (int)num(s, "customerId"), (int)num(s, "productId"),
                             str(s, "productName"), str(s, "customerName"), (int)num(s, "quantity"),
                             num(s, "unitPrice"), str(s, "date")));
    st.restoreNextProductId((int)num(root, "nextProductId"));
}
