// Minimal JSON support: a recursive-descent parser and string helpers for writing.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace json {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;

    const Value* get(const std::string& key) const {
        if (type != Object) return nullptr;
        for (const auto& kv : o)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

class Parser {
    const std::string& t;
    size_t p = 0;
    int depth = 0;

    [[noreturn]] void fail(const std::string& m) const {
        throw ParseError(m + " (at position " + std::to_string(p) + ")");
    }
    void ws() {
        while (p < t.size() && (t[p] == ' ' || t[p] == '\t' || t[p] == '\n' || t[p] == '\r')) p++;
    }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    unsigned hex4() {
        if (p + 4 > t.size()) fail("Bad \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            char c = t[p++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else fail("Bad \\u escape");
        }
        return v;
    }
    std::string str() {
        if (t[p] != '"') fail("Expected string");
        p++;
        std::string out;
        while (true) {
            if (p >= t.size()) fail("Unterminated string");
            unsigned char c = (unsigned char)t[p++];
            if (c == '"') break;
            if (c < 0x20) fail("Control character in string");
            if (c != '\\') { out += (char)c; continue; }
            if (p >= t.size()) fail("Unterminated escape");
            char e = t[p++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && p + 1 < t.size() && t[p] == '\\' && t[p + 1] == 'u') {
                        p += 2;
                        unsigned lo = hex4();
                        if (lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else fail("Bad surrogate pair");
                    }
                    utf8(out, cp);
                    break;
                }
                default: fail("Bad escape");
            }
        }
        return out;
    }
    Value value() {
        ws();
        if (p >= t.size()) fail("Unexpected end of input");
        if (++depth > 64) fail("Nesting too deep");
        Value v;
        char c = t[p];
        if (c == '{') {
            p++;
            v.type = Value::Object;
            ws();
            if (p < t.size() && t[p] == '}') p++;
            else while (true) {
                ws();
                if (p >= t.size()) fail("Unterminated object");
                std::string k = str();
                ws();
                if (p >= t.size() || t[p] != ':') fail("Expected ':'");
                p++;
                v.o.emplace_back(k, value());
                ws();
                if (p < t.size() && t[p] == ',') { p++; continue; }
                if (p < t.size() && t[p] == '}') { p++; break; }
                fail("Expected ',' or '}'");
            }
        } else if (c == '[') {
            p++;
            v.type = Value::Array;
            ws();
            if (p < t.size() && t[p] == ']') p++;
            else while (true) {
                v.a.push_back(value());
                ws();
                if (p < t.size() && t[p] == ',') { p++; continue; }
                if (p < t.size() && t[p] == ']') { p++; break; }
                fail("Expected ',' or ']'");
            }
        } else if (c == '"') {
            v.type = Value::String;
            v.s = str();
        } else if (t.compare(p, 4, "true") == 0) { p += 4; v.type = Value::Bool; v.b = true; }
        else if (t.compare(p, 5, "false") == 0) { p += 5; v.type = Value::Bool; v.b = false; }
        else if (t.compare(p, 4, "null") == 0) { p += 4; }
        else {
            size_t start = p;
            while (p < t.size() && std::string("0123456789+-.eE").find(t[p]) != std::string::npos) p++;
            if (start == p) fail("Unexpected character");
            std::string num = t.substr(start, p - start);
            char* end = nullptr;
            double d = std::strtod(num.c_str(), &end);
            if (*end != '\0') fail("Bad number");
            v.type = Value::Number;
            v.n = d;
        }
        depth--;
        return v;
    }

public:
    explicit Parser(const std::string& text) : t(text) {}
    Value parse() {
        Value v = value();
        ws();
        if (p != t.size()) fail("Unexpected trailing characters");
        return v;
    }
};

inline Value parse(const std::string& text) { return Parser(text).parse(); }

// ---- writing helpers ----
inline std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += (char)c;
        }
    }
    return out + "\"";
}

inline std::string money(double d) {
    char b[48];
    std::snprintf(b, sizeof b, "%.2f", d);
    return b;
}

}  // namespace json
