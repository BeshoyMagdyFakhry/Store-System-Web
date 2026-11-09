// REST API (/api/...) plus static file serving for the frontend.
#pragma once
#include <cctype>
#include <cmath>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "http_server.hpp"
#include "json.hpp"
#include "storage.hpp"
#include "store.hpp"

class Api {
    Store& store;
    std::string dataFile;
    std::string webRoot;
    std::mutex mu;

    static Response reply(int status, const std::string& body) {
        Response r;
        r.status = status;
        r.body = body;
        return r;
    }
    static Response fail(int status, const std::string& msg) {
        return reply(status, "{\"error\":" + json::quote(msg) + "}");
    }

    // ---- request-body readers: check types only; value rules live in Store ----
    static const json::Value& bodyObject(const Request& r, json::Value& holder) {
        try {
            holder = json::parse(r.body);
        } catch (const json::ParseError& e) {
            throw StoreError(400, std::string("Request body is not valid JSON: ") + e.what());
        }
        if (holder.type != json::Value::Object) throw StoreError(400, "Request body must be a JSON object.");
        return holder;
    }
    static std::string readString(const json::Value& b, const char* key, const char* label) {
        const json::Value* v = b.get(key);
        if (!v || v->type != json::Value::String) throw StoreError(400, std::string(label) + " is required.");
        return v->s;
    }
    static double readNumber(const json::Value& b, const char* key, const char* label) {
        const json::Value* v = b.get(key);
        if (!v || v->type != json::Value::Number || !std::isfinite(v->n))
            throw StoreError(400, std::string(label) + " must be a number.");
        return v->n;
    }
    static long long readInt(const json::Value& b, const char* key, const char* label) {
        double d = readNumber(b, key, label);
        if (d != std::floor(d) || std::fabs(d) > 1e10)
            throw StoreError(400, std::string(label) + " must be a whole number.");
        return (long long)d;
    }
    static int pathId(const std::string& s) {
        if (s.empty() || s.size() > 9) throw StoreError(400, "Invalid ID in URL.");
        for (char c : s)
            if (!std::isdigit((unsigned char)c)) throw StoreError(400, "Invalid ID in URL.");
        return std::atoi(s.c_str());
    }

    void persist() {
        std::string err = saveStore(store, dataFile);
        if (!err.empty()) throw StoreError(500, "The change was made but could not be saved to disk (" + err + ").");
    }

    Response route(const Request& r) {
        std::vector<std::string> seg;
        std::stringstream ss(r.path.size() > 5 ? r.path.substr(5) : "");  // after "/api/"
        std::string part;
        while (std::getline(ss, part, '/'))
            if (!part.empty()) seg.push_back(part);
        if (seg.empty()) return fail(404, "Unknown API endpoint.");

        std::lock_guard<std::mutex> lock(mu);
        const std::string& m = r.method;

        if (seg[0] == "products") {
            if (seg.size() == 1) {
                if (m == "GET") {
                    auto it = r.query.find("q");
                    auto found = store.searchProducts(it == r.query.end() ? "" : it->second);
                    std::string out = "[";
                    for (size_t i = 0; i < found.size(); i++) out += (i ? "," : "") + toJson(*found[i]);
                    return reply(200, out + "]");
                }
                if (m == "POST") {
                    json::Value h;
                    const auto& b = bodyObject(r, h);
                    const product& p = store.addProduct(readString(b, "name", "Product name"),
                                                        readNumber(b, "price", "Price"),
                                                        readInt(b, "quantity", "Quantity"));
                    std::string out = toJson(p);
                    persist();
                    return reply(201, out);
                }
                return fail(405, "Method not allowed.");
            }
            if (seg.size() == 2) {
                int id = pathId(seg[1]);
                if (m == "GET") {
                    int i = store.findProduct(id);
                    if (i == -1) return fail(404, "Product not found.");
                    return reply(200, toJson(store.allProducts()[i]));
                }
                if (m == "PUT") {
                    json::Value h;
                    const auto& b = bodyObject(r, h);
                    const product& p = store.updateProduct(id, readString(b, "name", "Product name"),
                                                           readNumber(b, "price", "Price"),
                                                           readInt(b, "quantity", "Quantity"));
                    std::string out = toJson(p);
                    persist();
                    return reply(200, out);
                }
                if (m == "DELETE") {
                    store.deleteProduct(id);
                    persist();
                    return reply(200, "{\"deleted\":" + std::to_string(id) + "}");
                }
                return fail(405, "Method not allowed.");
            }
        } else if (seg[0] == "customers" && seg.size() == 1) {
            if (m == "GET") return reply(200, listJson(store.allCustomers()));
            if (m == "POST") {
                json::Value h;
                const auto& b = bodyObject(r, h);
                const Customer& c = store.addCustomer(readString(b, "name", "Customer name"),
                                                      readInt(b, "id", "Customer ID"),
                                                      readString(b, "phone", "Phone"));
                std::string out = toJson(c);
                persist();
                return reply(201, out);
            }
            return fail(405, "Method not allowed.");
        } else if (seg[0] == "sales" && seg.size() == 1) {
            if (m == "GET") return reply(200, listJson(store.allSales()));
            if (m == "POST") {
                json::Value h;
                const auto& b = bodyObject(r, h);
                const sales& s = store.makeSale(readInt(b, "invoiceNumber", "Invoice number"),
                                                readInt(b, "customerId", "Customer"),
                                                readInt(b, "productId", "Product"),
                                                readInt(b, "quantity", "Quantity"));
                std::string out = toJson(s);
                persist();
                return reply(201, out);
            }
            return fail(405, "Method not allowed.");
        }
        return fail(404, "Unknown API endpoint.");
    }

    static std::string mimeFor(const std::string& path) {
        size_t dot = path.rfind('.');
        std::string ext = dot == std::string::npos ? "" : lower(path.substr(dot));
        if (ext == ".html") return "text/html; charset=utf-8";
        if (ext == ".css") return "text/css; charset=utf-8";
        if (ext == ".js") return "text/javascript; charset=utf-8";
        if (ext == ".json") return "application/json; charset=utf-8";
        if (ext == ".svg") return "image/svg+xml";
        if (ext == ".png") return "image/png";
        if (ext == ".ico") return "image/x-icon";
        return "application/octet-stream";
    }

    Response serveStatic(std::string path) {
        if (path == "/") path = "/index.html";
        if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos ||
            path.find(':') != std::string::npos || path.find('\0') != std::string::npos)
            return fail(404, "Not found.");
        std::ifstream f(webRoot + path, std::ios::binary);
        if (!f) return fail(404, "Not found.");
        std::stringstream buf;
        buf << f.rdbuf();
        Response r;
        r.contentType = mimeFor(path);
        r.body = buf.str();
        return r;
    }

public:
    Api(Store& s, std::string data, std::string web) : store(s), dataFile(std::move(data)), webRoot(std::move(web)) {}

    Response handle(const Request& r) {
        try {
            if (r.path == "/api" || r.path.rfind("/api/", 0) == 0) return route(r);
            if (r.method != "GET") return fail(405, "Method not allowed.");
            return serveStatic(r.path);
        } catch (const StoreError& e) {
            return fail(e.status, e.what());
        } catch (const std::exception& e) {
            return fail(500, "Internal server error.");
        }
    }
};
