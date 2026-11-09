// Store Management System - local web server entry point.
// Usage: store_server [--port N] [--frontend DIR] [--data FILE] [--open]
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "api.hpp"
#include "http_server.hpp"
#include "storage.hpp"
#include "store.hpp"

int main(int argc, char** argv) {
    int port = 8080;
    std::string frontend = "frontend";
    std::string dataFile = "data/store.json";
    bool openBrowser = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (a == "--frontend" && i + 1 < argc) frontend = argv[++i];
        else if (a == "--data" && i + 1 < argc) dataFile = argv[++i];
        else if (a == "--open") openBrowser = true;
        else {
            std::cerr << "Unknown or incomplete argument: " << a << "\n"
                      << "Usage: store_server [--port N] [--frontend DIR] [--data FILE] [--open]\n";
            return 2;
        }
    }
    if (port < 1 || port > 65500) {
        std::cerr << "Invalid port.\n";
        return 2;
    }

    namespace fs = std::filesystem;
    if (!fs::exists(fs::path(frontend) / "index.html")) {
        std::cerr << "Cannot find " << frontend << "/index.html - run the server from the project folder.\n";
        return 1;
    }
    try {
        fs::path parent = fs::path(dataFile).parent_path();
        if (!parent.empty()) fs::create_directories(parent);
    } catch (const std::exception& e) {
        std::cerr << "Cannot create data folder: " << e.what() << "\n";
        return 1;
    }

    Store store;
    try {
        loadStore(store, dataFile);
    } catch (const std::exception& e) {
        std::cerr << "Could not read " << dataFile << ": " << e.what() << "\n"
                  << "The file was left untouched. Fix or move it, then start again.\n";
        return 1;
    }

    HttpServer server;
    int bound = server.listenLocal(port);
    if (!bound) {
        std::cerr << "Could not open a port between " << port << " and " << port + 9 << ".\n";
        return 1;
    }

    std::string url = "http://localhost:" + std::to_string(bound);
    std::cout << "Store Management System running at " << url << "\n"
              << "Loaded " << store.allProducts().size() << " products, " << store.allCustomers().size()
              << " customers, " << store.allSales().size() << " sales from " << dataFile << "\n"
              << "Press Ctrl+C to stop.\n" << std::flush;

#ifdef _WIN32
    if (openBrowser) std::system(("start \"\" \"" + url + "\"").c_str());
#else
    (void)openBrowser;
#endif

    Api api(store, dataFile, frontend);
    server.serve([&api](const Request& r) { return api.handle(r); });
    return 0;
}
