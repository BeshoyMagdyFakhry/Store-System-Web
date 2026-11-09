#include <iostream>
#include <string>
using namespace std;


class product
{
private:
    string name;
    double price;
    int quantity;
    int id;

public:
    product() : name(""), price(0), quantity(0), id(0) {}

    product(string n, double p, int q, int i)
    {
        name = n;
        price = p;
        quantity = q;
        id = i;
    }

    string getName() {
        return name;
    }
    int getId() {
        return id;
    }
    double getPrice() {
        return price;
    }
    int getQuantity() {
        return quantity;
    }

    void setName(string n) {
        name = n;
    }
    void setPrice(double p) {
        price = p;
    }
    void setQuantity(int q) {
        quantity = q;
    }

    void display() {
        cout << endl << "----------{Product Information}----------" << endl;
        cout << "Product ID: " << id << endl;
        cout << "Product Name: " << name << endl;
        cout << "Price: $" << price << endl;
        cout << "Quantity: " << quantity << endl;
        cout << "-------------------------" << endl << endl;
    }
};

class Customer
{
private:
    string name;
    int id;
    string phone;

public:
    Customer() : name(""), id(0), phone("") {}

    Customer(string n, int i, string p)
    {
        name = n;
        id = i;
        phone = p;
    }

    int getId() {
        return id;
    }

    string getName() {
        return name;
    }

    void display() {
        cout << "Customer Name: " << name << endl;
        cout << "Customer ID: " << id << endl;
        cout << "Phone: " << phone << endl;
    }
};

class sales
{
private:
    int invoiceNumber;
    int customerId;
    string productName;
    string customerName;
    int quantity;
    double unitPrice;
    double totalPrice;

public:
    sales() : invoiceNumber(0), customerId(0), productName(""), customerName(""),
              quantity(0), unitPrice(0), totalPrice(0) {}

    sales(int invNum, int custId, string prodName, string custName, int qty, double unitP)
    {
        invoiceNumber = invNum;
        customerId = custId;
        productName = prodName;
        customerName = custName;
        quantity = qty;
        unitPrice = unitP;
        totalPrice = quantity * unitPrice;
    }

    void display() {
        cout << "----------{Sales Invoice}----------" << endl;
        cout << "Invoice Number: " << invoiceNumber << endl;
        cout << "Customer ID: " << customerId << endl;
        cout << "Product Name: " << productName << endl;
        cout << "Customer Name: " << customerName << endl;
        cout << "Quantity: " << quantity << endl;
        cout << "Unit Price: $" << unitPrice << endl;
        cout << "Total Price: $" << totalPrice << endl;
        cout << "------------------------------------" << endl;
    }
};

class Store
{
private:
    product products[100];
    int productCount = 0;
    int nextProductId = 10000;

    Customer customers[100];
    int customerCount = 0;

    sales salesList[100];
    int salesCount = 0;

public:
    int findProduct(int id)
    {
        for (int i = 0; i < productCount; i++) {
            if (products[i].getId() == id) {
                return i;
            }
        }
        return -1;
    }

    int findProductByName(string name)
    {
        for (int i = 0; i < productCount; i++) {
            if (products[i].getName() == name) {
                return i;
            }
        }
        return -1;
    }

    int findCustomer(int id)
    {
        for (int i = 0; i < customerCount; i++) {
            if (customers[i].getId() == id) {
                return i;
            }
        }
        return -1;
    }

    product& getProduct(int index)
    {
        return products[index];
    }

    void addProduct(string name, double price, int quantity)
    {
        if (productCount < 100)
        {
            products[productCount] = product(name, price, quantity, nextProductId);
            productCount++;
            nextProductId++;
        }
        else
        {
            cout << "Store is full" << endl << endl;
        }
    }

    void displayProducts()
    {
        cout << "---------{Products Info}---------" << endl;

        for (int i = 0; i < productCount; i++)
        {
            cout << "Product ID: " << products[i].getId() << endl;
            cout << "Product Name: " << products[i].getName() << endl;
            cout << "Price: $" << products[i].getPrice() << endl;
            cout << "Quantity: " << products[i].getQuantity() << endl;
            cout << "-------------------------" << endl;
        }
    }

    void searchProduct(string name)
    {
        int index = findProductByName(name);
        if (index == -1) {
            cout << "Product not found" << endl << endl;
        }
        else {
            products[index].display();
        }
    }

    void updateProduct(int index, string name, double price, int quantity)
    {
        products[index].setName(name);
        products[index].setPrice(price);
        products[index].setQuantity(quantity);
        cout << "Product updated" << endl << endl;
    }

    void deleteProduct(int index)
    {
        for (int i = index; i < productCount - 1; i++) {
            products[i] = products[i + 1];
        }
        productCount--;
        cout << "Product deleted" << endl << endl;
    }

    void addCustomer(string name, int id, string phone)
    {
        if (customerCount < 100)
        {
            customers[customerCount] = Customer(name, id, phone);
            customerCount++;
            cout << "Customer added" << endl << endl;
        }
        else
        {
            cout << "Store is full" << endl << endl;
        }
    }

    void makeSale(int invoiceNumber, string productName, string customerName, int quantity)
    {
        int customerId = -1;
        bool customerFound = false;

        for (int i = 0; i < customerCount; i++) {
            if (customers[i].getName() == customerName) {
                customerId = customers[i].getId();
                customerFound = true;
                break;
            }
        }

        if (!customerFound) {
            cout << "Customer not found" << endl << endl;
            return;
        }

        int index = findProductByName(productName);
        if (index == -1) {
            cout << "Product not found" << endl << endl;
            return;
        }

        if (quantity <= 0 || quantity > products[index].getQuantity()) {
            cout << "Not enough stock" << endl << endl;
            return;
        }

        if (salesCount < 100)
        {
            double unitPrice = products[index].getPrice();
            products[index].setQuantity(products[index].getQuantity() - quantity);
            salesList[salesCount] = sales(invoiceNumber, customerId, productName, customerName, quantity, unitPrice);
            salesList[salesCount].display();
            salesCount++;
        }
        else
        {
            cout << "Sales record is full" << endl << endl;
        }
    }

    void displaySales()
    {
        cout << "---------{All Sales}---------" << endl;
        for (int i = 0; i < salesCount; i++)
        {
            salesList[i].display();
        }
    }
};

int main() {
    Store store;

    cout << "Welcome to the Store Management System!" << endl;
    while (true) {
        cout << "------------------------------------" << endl;
        cout << "1. Add Product" << endl;
        cout << "2. Display All Products" << endl;
        cout << "3. search for a product" << endl;
        cout << "4. update product" << endl;
        cout << "5. delete product" << endl;
        cout << "6. Add Customer" << endl;
        cout << "7. Make Sale" << endl;
        cout << "8. Display All Sales" << endl;
        cout << "0. Exit" << endl;
        cout << "------------------------------------" << endl << endl;
        cout << "Please select an option: ";

        int choice;
        cin >> choice;

        if (choice == 1) {
            string name;
            double price;
            int quantity;

            cout << "Enter product name: ";
            cin >> name;
            cout << "Enter product price: ";
            cin >> price;
            cout << "Enter product quantity: ";
            cin >> quantity;

            store.addProduct(name, price, quantity);
        }
        else if (choice == 2) {
            store.displayProducts();
        }
        else if (choice == 3) {
            string name;

            cout << "Enter product name to search: ";
            cin >> name;

            store.searchProduct(name);
        }
        else if (choice == 4) {
            string name;

            cout << "Enter product name to update: ";
            cin >> name;

            int index = store.findProductByName(name);
            if (index == -1) {
                cout << "Product not found" << endl << endl;
            }
            else {

                cout << "select what to update:" << endl;
                cout << "1. Name" << endl;
                cout << "2. Price" << endl;
                cout << "3. Quantity" << endl;
                int updateChoice;
                cin >> updateChoice;

                if (updateChoice == 1)
                {
                    string name;

                    cout << "Enter new product name: ";
                    cin >> name;

                    store.updateProduct(index, name, store.getProduct(index).getPrice(), store.getProduct(index).getQuantity());
                }
                else if (updateChoice == 2)
                {
                    double price;

                    cout << "Enter new product price: ";
                    cin >> price;

                    store.updateProduct(index, store.getProduct(index).getName(), price, store.getProduct(index).getQuantity());
                }
                else if (updateChoice == 3)
                {
                    int quantity;

                    cout << "Enter new product quantity: ";
                    cin >> quantity;

                    store.updateProduct(index, store.getProduct(index).getName(), store.getProduct(index).getPrice(), quantity);
                    cout << "Product updated" << endl << endl;
                    cout << "New product information:" << endl;
                    store.getProduct(index).display();
                }
                else
                {
                    cout << "Invalid choice" << endl << endl;
                }
            }
        }
        else if (choice == 5) {
            string name;

            cout << "Enter product name to delete: ";
            cin >> name;

            int index = store.findProductByName(name);
            if (index == -1) {
                cout << "Product not found" << endl << endl;
            }
            else {
                store.deleteProduct(index);
                cout << "Product deleted" << endl << endl;
            }
        }
        else if (choice == 6) {
            string name;
            int id;
            string phone;

            cout << "Enter customer name: ";
            cin >> name;
            cout << "Enter customer ID: ";
            cin >> id;
            cout << "Enter customer phone: ";
            cin >> phone;

            store.addCustomer(name, id, phone);
            cout << "Customer added" << endl << endl;
        }
        else if (choice == 7) {
            int invoiceNumber;
            int customerId;
            string productName;
            string customerName;
            int quantity;

            cout << "Enter invoice number: ";
            cin >> invoiceNumber;
            cout << "Enter customer name: ";
            cin >> customerName;
            cout << "Enter product name: ";
            cin >> productName;
            cout << "Enter quantity: ";
            cin >> quantity;

            store.makeSale(invoiceNumber, productName, customerName, quantity);
            cout << "Sale recorded" << endl << endl;
            cout << "sales invoice" << endl;
            cout << "---------------------------------" << endl;
            cout << "Invoice Number: " << invoiceNumber << endl;
            cout << "Customer Name: " << customerName << endl;
            cout << "Customer ID: " << customerId << endl;
            cout << "Product Name: " << productName << endl;
            cout << "Quantity: " << quantity << endl;
            cout << "Unit Price: $" << store.getProduct(store.findProductByName(productName)).getPrice() << endl;
            cout << "Total Price: $" << store.getProduct(store.findProductByName(productName)).getPrice() * quantity << endl;
            cout << "---------------------------------" << endl << endl;
        }
        else if (choice == 8) {
            store.displaySales();
        }
        else if (choice == 0) {
            break;
        }
        else {
            cout << "Invalid option. Please try again." << endl;
        }
    }

    return 0;
}