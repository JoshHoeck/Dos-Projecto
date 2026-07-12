#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

enum class OrderType { BUY, SELL };

struct Order {
    string id;
    OrderType type;
    int price;
    int qty;
    long long timestamp;
};

vector<Order> loadData(const string& filename) {
    vector<Order> orders;
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Could not open " << filename << "\n";
        return orders;
    }

    string line;
    getline(file, line);

    while (getline(file, line)) {
        stringstream ss(line);
        string timeStr, temp, priceStr, qtyStr, typeStr;

        getline(ss, timeStr, ',');
        for (int i = 0; i < 4; i++) getline(ss, temp, ',');
        getline(ss, priceStr, ',');
        getline(ss, qtyStr, ',');
        getline(ss, typeStr, ',');

        Order o;
        o.id = "ORD_" + timeStr;
        o.type = (typeStr == "BUY") ? OrderType::BUY : OrderType::SELL;

        double rawPrice = priceStr.empty() ? 0.0 : stod(priceStr);
        o.price = (int)(rawPrice * 10000);

        double rawQty = qtyStr.empty() ? 0.0 : stod(qtyStr);
        o.qty = (int)rawQty;
        o.timestamp = stoll(timeStr);

        if (o.qty > 0) orders.push_back(o);
        if (orders.size() >= 100000) break;
    }

    return orders;
}

class OrderQueue {
    vector<Order> data;
    size_t head = 0;

public:
    bool empty() const { return head >= data.size(); }

    Order& front() { return data[head]; }

    void push(const Order& o) { data.push_back(o); }

    void pop() {
        head++;
        if (head > 64 && head * 2 > data.size()) {
            data.erase(data.begin(), data.begin() + head);
            head = 0;
        }
    }
};

class BuyHeap {
    vector<Order> data;

    bool higherPriority(const Order& a, const Order& b) const {
        if (a.price == b.price) return a.timestamp > b.timestamp;
        return a.price < b.price;
    }

    void heapifyUp(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!higherPriority(data[parent], data[i])) break;
            swap(data[parent], data[i]);
            i = parent;
        }
    }

    void heapifyDown(size_t i) {
        size_t n = data.size();
        while (true) {
            size_t left = 2 * i + 1, right = 2 * i + 2, best = i;
            if (left < n && higherPriority(data[best], data[left])) best = left;
            if (right < n && higherPriority(data[best], data[right])) best = right;
            if (best == i) break;
            swap(data[i], data[best]);
            i = best;
        }
    }

public:
    bool empty() const { return data.empty(); }
    const Order& top() const { return data.front(); }

    void push(const Order& o) {
        data.push_back(o);
        heapifyUp(data.size() - 1);
    }

    void pop() {
        data.front() = data.back();
        data.pop_back();
        if (!data.empty()) heapifyDown(0);
    }
};

class SellHeap {
    vector<Order> data;

    bool higherPriority(const Order& a, const Order& b) const {
        if (a.price == b.price) return a.timestamp > b.timestamp;
        return a.price > b.price;
    }

    void heapifyUp(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!higherPriority(data[parent], data[i])) break;
            swap(data[parent], data[i]);
            i = parent;
        }
    }

    void heapifyDown(size_t i) {
        size_t n = data.size();
        while (true) {
            size_t left = 2 * i + 1, right = 2 * i + 2, best = i;
            if (left < n && higherPriority(data[best], data[left])) best = left;
            if (right < n && higherPriority(data[best], data[right])) best = right;
            if (best == i) break;
            swap(data[i], data[best]);
            i = best;
        }
    }

public:
    bool empty() const { return data.empty(); }
    const Order& top() const { return data.front(); }

    void push(const Order& o) {
        data.push_back(o);
        heapifyUp(data.size() - 1);
    }

    void pop() {
        data.front() = data.back();
        data.pop_back();
        if (!data.empty()) heapifyDown(0);
    }
};

class OrderMap {
    vector<vector<pair<int, OrderQueue>>> buckets;
    size_t entries = 0;

    size_t getBucketIndex(int key) const {
        return (size_t)key % buckets.size();
    }

    void resizeTable() {
        vector<vector<pair<int, OrderQueue>>> old = move(buckets);
        buckets.assign(old.size() * 2, {});
        for (auto& b : old) {
            for (auto& kv : b) {
                buckets[getBucketIndex(kv.first)].push_back(move(kv));
            }
        }
    }

public:
    OrderMap(size_t startBuckets = 16) : buckets(startBuckets) {}

    bool count(int key) const {
        for (auto& kv : buckets[getBucketIndex(key)]) {
            if (kv.first == key) return true;
        }
        return false;
    }

    OrderQueue& operator[](int key) {
        if (entries + 1 > buckets.size() * 2) resizeTable();
        auto& bucket = buckets[getBucketIndex(key)];
        for (auto& kv : bucket) {
            if (kv.first == key) return kv.second;
        }
        bucket.push_back({key, OrderQueue()});
        entries++;
        return bucket.back().second;
    }
};

class HeapEngine {
    BuyHeap buys;
    SellHeap sells;
    int trades = 0;

    bool hasMatch() const {
        return !buys.empty() && !sells.empty() && buys.top().price >= sells.top().price;
    }

    void processTrade() {
        Order b = buys.top(); buys.pop();
        Order s = sells.top(); sells.pop();

        int matched = min(b.qty, s.qty);
        trades++;
        b.qty -= matched;
        s.qty -= matched;

        if (b.qty > 0) buys.push(b);
        if (s.qty > 0) sells.push(s);
    }

    void reset() { trades = 0; }

public:
    void run(const vector<Order>& orders) {
        reset();
        for (const auto& o : orders) {
            if (o.type == OrderType::BUY) buys.push(o);
            else sells.push(o);

            while (hasMatch()) processTrade();
        }
    }

    int getTrades() const { return trades; }
};

class HashEngine {
    OrderMap buyBook;
    OrderMap sellBook;
    int trades = 0;

    void matchAtPrice(int p) {
        auto& bq = buyBook[p];
        auto& sq = sellBook[p];

        while (!bq.empty() && !sq.empty()) {
            Order& b = bq.front();
            Order& s = sq.front();

            int matched = min(b.qty, s.qty);
            trades++;
            b.qty -= matched;
            s.qty -= matched;

            if (b.qty == 0) bq.pop();
            if (s.qty == 0) sq.pop();
        }
    }

    void reset() { trades = 0; }

public:
    void run(const vector<Order>& orders) {
        reset();
        for (const auto& o : orders) {
            int p = o.price;
            if (o.type == OrderType::BUY) buyBook[p].push(o);
            else sellBook[p].push(o);

            if (buyBook.count(p) && sellBook.count(p)) matchAtPrice(p);
        }
    }

    int getTrades() const { return trades; }
};

int main() {
    vector<Order> orders;
    HeapEngine heapSim;
    HashEngine hashSim;
    long long heapTime = 0, hashTime = 0;
    int choice = 0;

    while (choice != 5) {
        cout << "\n1. Load Data\n2. Run Heap Engine\n3. Run Hash Engine\n4. Print Metrics\n5. Exit\nChoice: ";
        cin >> choice;

        switch (choice) {
            case 1: {
                orders = loadData("sample_data.csv");
                if (!orders.empty()) cout << "Loaded " << orders.size() << " orders.\n";
                break;
            }
            case 2: {
                if (orders.empty()) { cout << "Load data first!\n"; break; }
                auto start = chrono::high_resolution_clock::now();
                heapSim.run(orders);
                auto stop = chrono::high_resolution_clock::now();
                heapTime = chrono::duration_cast<chrono::microseconds>(stop - start).count();
                cout << "Heap engine finished.\n";
                break;
            }
            case 3: {
                if (orders.empty()) { cout << "Load data first!\n"; break; }
                auto start = chrono::high_resolution_clock::now();
                hashSim.run(orders);
                auto stop = chrono::high_resolution_clock::now();
                hashTime = chrono::duration_cast<chrono::microseconds>(stop - start).count();
                cout << "Hash engine finished.\n";
                break;
            }
            case 4:
                cout << "\nHeap Time:   " << heapTime << " us\n";
                cout << "Hash Time:   " << hashTime << " us\n";
                cout << "Heap Trades: " << heapSim.getTrades() << "\n";
                cout << "Hash Trades: " << hashSim.getTrades() << "\n";
                break;
        }
    }

    return 0;
}