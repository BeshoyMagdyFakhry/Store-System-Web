// Business logic ported from the original console program (product, Customer, sales, Store).
// Behaviour is the same; console I/O was replaced by exceptions carrying an HTTP status + message.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

struct StoreError : std::runtime_error {
    int status;
    StoreError(int s, const std::string& m) : std::runtime_error(m), status(s) {}
};

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

inline std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

inline std::string nowString() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &lt);
    return buf;
}

class product {
private:
    std::string name;
    double price;
    int quantity;
    int id;

public:
    product() : name(""), price(0), quantity(0), id(0) {}
    product(std::string n, double p, int q, int i) : name(n), price(p), quantity(q), id(i) {}

    std::string getName() const { return name; }
    int getId() const { return id; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }

    void setName(const std::string& n) { name = n; }
    void setPrice(double p) { price = p; }
    void setQuantity(int q) { quantity = q; }
};

class Customer {
private:
    std::string name;
    int id;
    std::string phone;

public:
    Customer() : name(""), id(0), phone("") {}
    Customer(std::string n, int i, std::string p) : name(n), id(i), phone(p) {}

    int getId() const { return id; }
    std::string getName() const { return name; }
    std::string getPhone() const { return phone; }
};

class sales {
private:
    int invoiceNumber;
    int customerId;
    int productId;
    std::string productName;
    std::string customerName;
    int quantity;
    double unitPrice;
    double totalPrice;
    std::string date;

public:
    sales() : invoiceNumber(0), customerId(0), productId(0), quantity(0), unitPrice(0), totalPrice(0) {}
    sales(int invNum, int custId, int prodId, std::string prodName, std::string custName, int qty,
          double unitP, std::string when)
        : invoiceNumber(invNum), customerId(custId), productId(prodId), productName(prodName),
          customerName(custName), quantity(qty), unitPrice(unitP), date(when) {
        totalPrice = std::round(quantity * unitPrice * 100.0) / 100.0;
    }

    int getInvoiceNumber() const { return invoiceNumber; }
    int getCustomerId() const { return customerId; }
    int getProductId() const { return productId; }
    std::string getProductName() const { return productName; }
    std::string getCustomerName() const { return customerName; }
    int getQuantity() const { return quantity; }
    double getUnitPrice() const { return unitPrice; }
    double getTotalPrice() const { return totalPrice; }
    std::string getDate() const { return date; }
};

class Store {
public:
    static constexpr size_t kMax = 1000;           // original capacity was 100
    static constexpr double kMaxPrice = 1000000000.0;
    static constexpr int kMaxQty = 1000000000;

private:
    std::vector<product> products;
    int nextProductId = 10000;
    std::vector<Customer> customers;
    std::vector<sales> salesList;

    static void checkName(const std::string& n, const char* label) {
        if (n.empty()) throw StoreError(400, std::string(label) + " cannot be empty.");
        if (n.size() > 100) throw StoreError(400, std::string(label) + " must be 100 characters or fewer.");
    }
    static double checkPrice(double p) {
        if (!std::isfinite(p) || p < 0 || p > kMaxPrice)
            throw StoreError(400, "Price must be a number between 0 and 1,000,000,000.");
        return std::round(p * 100.0) / 100.0;
    }
    static void checkQty(long long q) {
        if (q < 0 || q > kMaxQty) throw StoreError(400, "Quantity must be a whole number between 0 and 1,000,000,000.");
    }
    bool nameTaken(const std::string& name, int exceptId) const {
        std::string l = lower(name);
        for (const auto& p : products)
            if (p.getId() != exceptId && lower(p.getName()) == l) return true;
        return false;
    }

public:
    // ---- lookups (same as original) ----
    int findProduct(int id) const {
        for (size_t i = 0; i < products.size(); i++)
            if (products[i].getId() == id) return (int)i;
        return -1;
    }
    int findProductByName(const std::string& name) const {
        for (size_t i = 0; i < products.size(); i++)
            if (products[i].getName() == name) return (int)i;
        return -1;
    }
    int findCustomer(int id) const {
        for (size_t i = 0; i < customers.size(); i++)
            if (customers[i].getId() == id) return (int)i;
        return -1;
    }
    int findInvoice(int number) const {
        for (size_t i = 0; i < salesList.size(); i++)
            if (salesList[i].getInvoiceNumber() == number) return (int)i;
        return -1;
    }

    const std::vector<product>& allProducts() const { return products; }
    const std::vector<Customer>& allCustomers() const { return customers; }
    const std::vector<sales>& allSales() const { return salesList; }
    int getNextProductId() const { return nextProductId; }

    // ---- products ----
    const product& addProduct(std::string name, double price, long long quantity) {
        name = trim(name);
        checkName(name, "Product name");
        price = checkPrice(price);
        checkQty(quantity);
        if (products.size() >= kMax) throw StoreError(409, "Store is full.");
        if (nameTaken(name, -1)) throw StoreError(409, "A product named \"" + name + "\" already exists.");
        products.push_back(product(name, price, (int)quantity, nextProductId));
        nextProductId++;
        return products.back();
    }

    // Case-insensitive substring search (the original did an exact-name search; exact matches are included).
    std::vector<const product*> searchProducts(const std::string& query) const {
        std::string q = lower(trim(query));
        std::vector<const product*> out;
        for (const auto& p : products)
            if (q.empty() || lower(p.getName()).find(q) != std::string::npos || std::to_string(p.getId()) == q)
                out.push_back(&p);
        return out;
    }

    const product& updateProduct(int id, std::string name, double price, long long quantity) {
        int idx = findProduct(id);
        if (idx == -1) throw StoreError(404, "Product not found.");
        name = trim(name);
        checkName(name, "Product name");
        price = checkPrice(price);
        checkQty(quantity);
        if (nameTaken(name, id)) throw StoreError(409, "A product named \"" + name + "\" already exists.");
        products[idx].setName(name);
        products[idx].setPrice(price);
        products[idx].setQuantity((int)quantity);
        return products[idx];
    }

    void deleteProduct(int id) {
        int idx = findProduct(id);
        if (idx == -1) throw StoreError(404, "Product not found.");
        products.erase(products.begin() + idx);
    }

    // ---- customers ----
    const Customer& addCustomer(std::string name, long long id, std::string phone) {
        name = trim(name);
        phone = trim(phone);
        checkName(name, "Customer name");
        if (id < 1 || id > 2000000000LL) throw StoreError(400, "Customer ID must be a positive whole number.");
        if (phone.size() < 5 || phone.size() > 20) throw StoreError(400, "Phone must be 5 to 20 characters.");
        for (char c : phone)
            if (!std::isdigit((unsigned char)c) && c != '+' && c != '-' && c != ' ' && c != '(' && c != ')')
                throw StoreError(400, "Phone may only contain digits, spaces, + - ( ).");
        if (customers.size() >= kMax) throw StoreError(409, "Store is full.");
        if (findCustomer((int)id) != -1) throw StoreError(409, "Customer ID " + std::to_string(id) + " is already in use.");
        customers.push_back(Customer(name, (int)id, phone));
        return customers.back();
    }

    // ---- sales ----
    const sales& makeSale(long long invoiceNumber, long long customerId, long long productId, long long quantity) {
        if (invoiceNumber < 1 || invoiceNumber > 2000000000LL)
            throw StoreError(400, "Invoice number must be a positive whole number.");
        if (findInvoice((int)invoiceNumber) != -1)
            throw StoreError(409, "Invoice number " + std::to_string(invoiceNumber) + " already exists.");
        int ci = (customerId < 1 || customerId > 2000000000LL) ? -1 : findCustomer((int)customerId);
        if (ci == -1) throw StoreError(404, "Customer not found.");
        int pi = (productId < 1 || productId > 2000000000LL) ? -1 : findProduct((int)productId);
        if (pi == -1) throw StoreError(404, "Product not found.");
        if (quantity <= 0) throw StoreError(400, "Quantity must be at least 1.");
        if (quantity > products[pi].getQuantity())
            throw StoreError(409, "Not enough stock: only " + std::to_string(products[pi].getQuantity()) + " available.");
        if (salesList.size() >= kMax) throw StoreError(409, "Sales record is full.");

        double unitPrice = products[pi].getPrice();
        products[pi].setQuantity(products[pi].getQuantity() - (int)quantity);
        salesList.push_back(sales((int)invoiceNumber, customers[ci].getId(), products[pi].getId(),
                                  products[pi].getName(), customers[ci].getName(), (int)quantity,
                                  unitPrice, nowString()));
        return salesList.back();
    }

    // ---- used only when loading saved data ----
    void restoreProduct(const product& p) { products.push_back(p); }
    void restoreCustomer(const Customer& c) { customers.push_back(c); }
    void restoreSale(const sales& s) { salesList.push_back(s); }
    void restoreNextProductId(int next) {
        int minNext = 10000;
        for (const auto& p : products) minNext = std::max(minNext, p.getId() + 1);
        nextProductId = std::max(next, minNext);
    }
};
