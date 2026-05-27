#include <iostream>
#include <vector>
#include <initializer_list>
#include <string>
#include <limits>

// ═══════════════════════════════════════════════════════════════════
// Part 1. Function template
// Find the maximum absolute value among all positive elements.
// For positive x: |x| = x, so this equals max(positive elements).
// Returns T(0) if no positive elements exist.
// Types tested: char (signed 1-byte int) and double.
// ═══════════════════════════════════════════════════════════════════

template<typename T>
T findMaxAbsPositive(const T* arr, int n) {
    T result = T(0);
    bool found = false;
    for (int i = 0; i < n; ++i) {
        if (arr[i] > T(0)) {
            if (!found || arr[i] > result) {
                result = arr[i];
                found = true;
            }
        }
    }
    return result;
}

// Unary + promotes char to int for printing, leaves other types unchanged
template<typename T>
void printArr(const T* arr, int n, const std::string& label) {
    std::cout << label << ": [";
    for (int i = 0; i < n; ++i) {
        std::cout << +arr[i];
        if (i < n - 1) std::cout << ", ";
    }
    std::cout << "]\n";
}

// ═══════════════════════════════════════════════════════════════════
// Part 2. Class template
// ADT: Input-Restricted Deque (insertions at back only,
//      deletions from both ends). Storage: std::vector<T>.
// ═══════════════════════════════════════════════════════════════════

template<typename T>
class InputRestrictedDeque {
private:
    std::vector<T> data;

public:
    // Constructor with parameters
    InputRestrictedDeque(std::initializer_list<T> list) : data(list) {}

    // Copy constructor
    InputRestrictedDeque(const InputRestrictedDeque& o) : data(o.data) {}

    // Move constructor
    InputRestrictedDeque(InputRestrictedDeque&& o) noexcept
        : data(std::move(o.data)) {}

    // Destructor
    ~InputRestrictedDeque() {}

    // Copy assignment
    InputRestrictedDeque& operator=(const InputRestrictedDeque& o) {
        if (this != &o) data = o.data;
        return *this;
    }

    // Move assignment
    InputRestrictedDeque& operator=(InputRestrictedDeque&& o) noexcept {
        if (this != &o) data = std::move(o.data);
        return *this;
    }

    // Insert at back (only input end)
    void pushBack(const T& val) {
        data.push_back(val);
    }

    // Remove from front
    void popFront() {
        if (data.empty()) { std::cout << "  Error: deque is empty.\n"; return; }
        data.erase(data.begin());
    }

    // Remove from back
    void popBack() {
        if (data.empty()) { std::cout << "  Error: deque is empty.\n"; return; }
        data.pop_back();
    }

    // View front element
    T front() const { return data.front(); }

    // View back element
    T back() const { return data.back(); }

    bool isEmpty() const { return data.empty(); }
    int  size()    const { return static_cast<int>(data.size()); }

    void print() const {
        if (data.empty()) { std::cout << "  [empty]\n"; return; }
        std::cout << "  [front] ";
        for (const auto& v : data) std::cout << +v << " ";
        std::cout << "[back]\n";
    }

    void clear() { data.clear(); }
};

// ═══════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════

static int readInt(const std::string& prompt) {
    int v;
    std::cout << prompt;
    while (!(std::cin >> v)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid. " << prompt;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return v;
}

template<typename T>
T readVal(const std::string& prompt) {
    T v;
    std::cout << prompt;
    while (!(std::cin >> v)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid. " << prompt;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return v;
}

template<typename T>
void dequeMenu(InputRestrictedDeque<T>& dq, const std::string& typeName) {
    bool running = true;
    while (running) {
        std::cout << "\n--- Deque<" << typeName << "> ---\n";
        dq.print();
        std::cout << "+----------------------------+\n"
                  << "| 1. pushBack (back only)    |\n"
                  << "| 2. popFront                |\n"
                  << "| 3. popBack                 |\n"
                  << "| 4. front                   |\n"
                  << "| 5. back                    |\n"
                  << "| 6. isEmpty                 |\n"
                  << "| 7. size                    |\n"
                  << "| 8. clear                   |\n"
                  << "| 0. Back                    |\n"
                  << "+----------------------------+\n";
        int choice = readInt("Choice: ");
        switch (choice) {
        case 0: running = false; break;
        case 1: { T val = readVal<T>("  Value: "); dq.pushBack(val); break; }
        case 2: dq.popFront(); break;
        case 3: dq.popBack();  break;
        case 4:
            if (!dq.isEmpty()) std::cout << "  front = " << +dq.front() << "\n";
            else               std::cout << "  Deque is empty.\n";
            break;
        case 5:
            if (!dq.isEmpty()) std::cout << "  back = " << +dq.back() << "\n";
            else               std::cout << "  Deque is empty.\n";
            break;
        case 6:
            std::cout << "  isEmpty = " << (dq.isEmpty() ? "true" : "false") << "\n";
            break;
        case 7:
            std::cout << "  size = " << dq.size() << "\n";
            break;
        case 8:
            dq.clear();
            std::cout << "  Cleared.\n";
            break;
        default:
            std::cout << "  Unknown option.\n";
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════

int main() {
    // ── Part 1 ──────────────────────────────────────────────────────
    std::cout << "=== Part 1: findMaxAbsPositive<T> ===\n\n";

    // Test with char (signed 1-byte integer)
    char ca1[] = {(char)-5, (char)3, (char)-1, (char)7, (char)-8, (char)2};
    printArr(ca1, 6, "char");
    std::cout << "Result (char): " << +findMaxAbsPositive(ca1, 6) << "\n\n";

    char ca2[] = {(char)-5, (char)-3, (char)-8};
    printArr(ca2, 3, "char (no positives)");
    std::cout << "Result (char): " << +findMaxAbsPositive(ca2, 3) << "\n\n";

    // Test with double
    double da1[] = {-1.5, 3.7, -0.5, 2.1, -9.9, 5.5};
    printArr(da1, 6, "double");
    std::cout << "Result (double): " << findMaxAbsPositive(da1, 6) << "\n\n";

    double da2[] = {-1.5, -3.7, -0.5};
    printArr(da2, 3, "double (no positives)");
    std::cout << "Result (double): " << findMaxAbsPositive(da2, 3) << "\n\n";

    std::cout << "Press Enter to continue to Part 2...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // ── Part 2 ──────────────────────────────────────────────────────
    // Two instances of the template class with different types
    InputRestrictedDeque<int>    dqInt    = {1, 2, 3, 4, 5};
    InputRestrictedDeque<double> dqDouble = {1.1, 2.2, 3.3};

    bool running = true;
    while (running) {
        std::cout << "\n+================================+\n";
        std::cout <<   "| Part 2: Deque Template Menu   |\n";
        std::cout <<   "+================================+\n";
        std::cout <<   "| 1. Deque<int>                 |\n";
        std::cout <<   "| 2. Deque<double>              |\n";
        std::cout <<   "| 0. Exit                       |\n";
        std::cout <<   "+================================+\n";
        int choice = readInt("Choice: ");
        switch (choice) {
        case 0: running = false; break;
        case 1: dequeMenu(dqInt,    "int");    break;
        case 2: dequeMenu(dqDouble, "double"); break;
        default: std::cout << "Unknown option.\n";
        }
    }

    return 0;
}
