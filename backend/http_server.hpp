// Small dependency-free HTTP/1.1 server. Listens on 127.0.0.1 only, one thread per connection.
#pragma once
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
typedef SOCKET sock_t;
static const sock_t kBadSock = INVALID_SOCKET;
inline void closeSock(sock_t s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
static const sock_t kBadSock = -1;
inline void closeSock(sock_t s) { close(s); }
#endif

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> query, headers;
};

struct Response {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

inline const char* statusText(int s) {
    switch (s) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        default: return "Internal Server Error";
    }
}

inline std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2])) {
            out += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

class HttpServer {
    sock_t listener = kBadSock;

    static bool recvSome(sock_t c, std::string& buf) {
        char tmp[4096];
        int n = (int)recv(c, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        buf.append(tmp, (size_t)n);
        return true;
    }

    // Returns 200 when a full request was parsed, 0 to silently drop, or an HTTP error status.
    static int readRequest(sock_t c, Request& req) {
        std::string buf;
        size_t headerEnd;
        while ((headerEnd = buf.find("\r\n\r\n")) == std::string::npos) {
            if (buf.size() > 16384) return 431;
            if (!recvSome(c, buf)) return 0;
        }
        std::string head = buf.substr(0, headerEnd);
        std::string body = buf.substr(headerEnd + 4);

        size_t eol = head.find("\r\n");
        std::string line = head.substr(0, eol);
        size_t s1 = line.find(' '), s2 = line.find(' ', s1 + 1);
        if (s1 == std::string::npos || s2 == std::string::npos) return 400;
        req.method = line.substr(0, s1);
        std::string target = line.substr(s1 + 1, s2 - s1 - 1);

        size_t q = target.find('?');
        req.path = urlDecode(target.substr(0, q));
        if (q != std::string::npos) {
            std::string qs = target.substr(q + 1);
            size_t pos = 0;
            while (pos <= qs.size()) {
                size_t amp = qs.find('&', pos);
                std::string pair = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
                size_t eq = pair.find('=');
                if (!pair.empty())
                    req.query[urlDecode(pair.substr(0, eq))] = eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1));
                if (amp == std::string::npos) break;
                pos = amp + 1;
            }
        }

        size_t pos = eol;
        while (pos != std::string::npos && pos < head.size()) {
            size_t next = head.find("\r\n", pos + 2);
            std::string h = head.substr(pos + 2, next == std::string::npos ? std::string::npos : next - pos - 2);
            size_t colon = h.find(':');
            if (colon != std::string::npos) {
                std::string k = h.substr(0, colon);
                for (auto& ch : k) ch = (char)std::tolower((unsigned char)ch);
                size_t v = h.find_first_not_of(" \t", colon + 1);
                req.headers[k] = v == std::string::npos ? "" : h.substr(v);
            }
            pos = next;
        }

        size_t len = 0;
        auto it = req.headers.find("content-length");
        if (it != req.headers.end()) len = (size_t)std::strtoul(it->second.c_str(), nullptr, 10);
        if (len > 1048576) return 413;
        while (body.size() < len)
            if (!recvSome(c, body)) return 0;
        body.resize(len);
        req.body = body;
        return 200;
    }

    static void sendAll(sock_t c, const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
#ifdef _WIN32
            int n = send(c, data.data() + sent, (int)(data.size() - sent), 0);
#else
            ssize_t n = send(c, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#endif
            if (n <= 0) return;
            sent += (size_t)n;
        }
    }

    static void sendResponse(sock_t c, const Response& r) {
        std::string out = "HTTP/1.1 " + std::to_string(r.status) + " " + statusText(r.status) +
                          "\r\nContent-Type: " + r.contentType + "\r\nContent-Length: " +
                          std::to_string(r.body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" +
                          r.body;
        sendAll(c, out);
    }

    static void handleClient(sock_t c, std::function<Response(const Request&)> handler) {
#ifdef _WIN32
        DWORD ms = 10000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof ms);
#else
        timeval tv{10, 0};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
        Request req;
        int st = readRequest(c, req);
        if (st == 200) {
            Response res;
            try {
                res = handler(req);
            } catch (const std::exception& e) {
                res.status = 500;
                res.body = "{\"error\":\"Internal server error\"}";
            }
            sendResponse(c, res);
        } else if (st != 0) {
            Response res;
            res.status = st;
            res.body = "{\"error\":\"Bad request\"}";
            sendResponse(c, res);
        }
        closeSock(c);
    }

public:
    HttpServer() {
#ifdef _WIN32
        WSADATA w;
        WSAStartup(MAKEWORD(2, 2), &w);
#else
        std::signal(SIGPIPE, SIG_IGN);
#endif
    }

    // Binds 127.0.0.1 on firstPort (or the next free ports). Returns the port used, or 0 on failure.
    int listenLocal(int firstPort, int tries = 10) {
        for (int i = 0; i < tries; i++) {
            int port = firstPort + i;
            sock_t s = socket(AF_INET, SOCK_STREAM, 0);
            if (s == kBadSock) return 0;
#ifndef _WIN32
            int yes = 1;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
#endif
            sockaddr_in a;
            std::memset(&a, 0, sizeof a);
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.sin_port = htons((unsigned short)port);
            if (bind(s, (sockaddr*)&a, sizeof a) == 0 && listen(s, 64) == 0) {
                listener = s;
                return port;
            }
            closeSock(s);
        }
        return 0;
    }

    void serve(std::function<Response(const Request&)> handler) {
        for (;;) {
            sock_t c = accept(listener, nullptr, nullptr);
            if (c == kBadSock) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            std::thread(handleClient, c, handler).detach();
        }
    }
};
