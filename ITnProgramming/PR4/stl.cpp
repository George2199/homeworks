#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <random>
#include <iterator>
#include <iomanip>
#include <stdexcept>

// ═══════════════════════════════════════════════════════════════════
// Data structure
// ═══════════════════════════════════════════════════════════════════

struct Item {
    std::string name;
    long long   mainParam;
    long long   secondParam;
};

// ═══════════════════════════════════════════════════════════════════
// File I/O helpers
// ═══════════════════════════════════════════════════════════════════

std::vector<std::string> parseWords(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) throw std::runtime_error("Cannot open file: " + filename);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    std::vector<std::string> words;
    std::istringstream ss(content);
    std::string token;
    while (std::getline(ss, token, '$')) {
        size_t s = token.find_first_not_of(" \t\r\n");
        size_t e = token.find_last_not_of(" \t\r\n");
        if (s != std::string::npos)
            words.push_back(token.substr(s, e - s + 1));
    }
    return words;
}

// ── JSON write ─────────────────────────────────────────────────────

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else                out += c;
    }
    return out;
}

void writeJson(const std::list<Item>& items, const std::string& filename) {
    std::ofstream f(filename);
    if (!f) throw std::runtime_error("Cannot write: " + filename);
    f << "[\n";
    bool first = true;
    for (const auto& item : items) {
        if (!first) f << ",\n";
        first = false;
        f << "  {"
          << "\"name\": \""  << jsonEscape(item.name) << "\", "
          << "\"mainParam\": "   << item.mainParam   << ", "
          << "\"secondParam\": " << item.secondParam
          << "}";
    }
    f << "\n]\n";
    std::cout << "  Saved to: " << filename << "\n";
}

// ── JSON read ──────────────────────────────────────────────────────

std::list<Item> readJson(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) throw std::runtime_error("Cannot open: " + filename);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    std::list<Item> items;
    size_t pos = 0;
    while (true) {
        size_t open  = content.find('{', pos);
        if (open == std::string::npos) break;
        size_t close = content.find('}', open);
        if (close == std::string::npos) break;
        std::string obj = content.substr(open + 1, close - open - 1);
        pos = close + 1;

        Item item;
        // name
        size_t np = obj.find("\"name\"");
        if (np != std::string::npos) {
            size_t colon = obj.find(':', np + 6);
            size_t q1    = obj.find('"', colon + 1);
            size_t q2    = obj.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos)
                item.name = obj.substr(q1 + 1, q2 - q1 - 1);
        }
        // mainParam
        size_t mp = obj.find("\"mainParam\"");
        if (mp != std::string::npos) {
            size_t colon    = obj.find(':', mp + 11);
            size_t numStart = obj.find_first_of("-0123456789", colon);
            if (numStart != std::string::npos)
                item.mainParam = std::stoll(obj.substr(numStart));
        }
        // secondParam
        size_t sp = obj.find("\"secondParam\"");
        if (sp != std::string::npos) {
            size_t colon    = obj.find(':', sp + 13);
            size_t numStart = obj.find_first_of("-0123456789", colon);
            if (numStart != std::string::npos)
                item.secondParam = std::stoll(obj.substr(numStart));
        }
        if (!item.name.empty())
            items.push_back(item);
    }
    return items;
}

// ═══════════════════════════════════════════════════════════════════
// Display
// ═══════════════════════════════════════════════════════════════════

void displayItems(const std::list<Item>& items) {
    std::cout << std::left
              << std::setw(18) << "Name"
              << std::setw(14) << "MainParam"
              << "SecondParam\n"
              << std::string(46, '-') << "\n";
    for (const auto& item : items)
        std::cout << std::setw(18) << item.name
                  << std::setw(14) << item.mainParam
                  << item.secondParam << "\n";
    std::cout << "(" << items.size() << " items)\n";
}

// ═══════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════

int main() {
    const std::string wordsFile = "words.txt";
    const std::string jsonFile  = "output.json";

    // ── Step 1 ─────────────────────────────────────────────────────
    std::cout << "=== Step 1: Build set of unique words ===\n";
    std::vector<std::string> words = parseWords(wordsFile);
    std::cout << "  Total word tokens: " << words.size() << "\n";

    std::set<std::string> wordSet(words.begin(), words.end());
    std::cout << "  Unique words (set size): " << wordSet.size() << "\n";
    std::cout << "  Words: ";
    for (const auto& w : wordSet) std::cout << w << " ";
    std::cout << "\n\n";

    // ── Step 2 ─────────────────────────────────────────────────────
    std::cout << "=== Step 2: Build word frequency map ===\n";
    std::map<std::string, int> wordMap;
    for (const auto& w : words) wordMap[w]++;
    for (const auto& [word, cnt] : wordMap)
        std::cout << "  " << std::setw(14) << word << ": " << cnt << "\n";
    std::cout << "\n";

    // ── Step 3 ─────────────────────────────────────────────────────
    std::cout << "=== Step 3: Generate 100 items ===\n";
    std::mt19937 rng(std::random_device{}());

    auto pickRandomWord = [&]() -> std::string {
        std::uniform_int_distribution<size_t> dist(0, wordSet.size() - 1);
        auto it = wordSet.begin();
        std::advance(it, dist(rng));
        return *it;
    };

    auto pickRandomMapVal = [&]() -> long long {
        std::uniform_int_distribution<size_t> dist(0, wordMap.size() - 1);
        auto it = wordMap.begin();
        std::advance(it, dist(rng));
        return it->second;
    };

    std::list<Item> items;
    for (int i = 0; i < 100; ++i) {
        Item item;
        item.name      = pickRandomWord();
        item.mainParam = pickRandomMapVal() * pickRandomMapVal() * pickRandomMapVal();
        long long val  = wordMap[item.name];
        item.secondParam = val * val;
        items.push_back(item);
    }
    std::cout << "  Generated: " << items.size() << " items\n\n";

    // ── Step 4 ─────────────────────────────────────────────────────
    std::cout << "=== Step 4: Sort by mainParam descending ===\n";
    items.sort([](const Item& a, const Item& b) {
        return a.mainParam > b.mainParam;
    });
    std::cout << "  First 5 after sort:\n";
    auto it4 = items.begin();
    for (int i = 0; i < 5 && it4 != items.end(); ++i, ++it4)
        std::cout << "    " << std::setw(14) << it4->name
                  << " mainParam=" << it4->mainParam << "\n";
    std::cout << "\n";

    // ── Step 5 ─────────────────────────────────────────────────────
    std::cout << "=== Step 5: Filter — odd letter count in name ===\n";
    std::list<Item> filtered;
    std::copy_if(items.begin(), items.end(), std::back_inserter(filtered),
        [](const Item& item) { return item.name.size() % 2 == 1; });
    std::cout << "  After filter: " << filtered.size() << " items\n";
    std::cout << "  Odd-length words kept: ";
    std::set<std::string> shownNames;
    for (const auto& item : filtered)
        if (shownNames.insert(item.name).second)
            std::cout << item.name << "(" << item.name.size() << ") ";
    std::cout << "\n\n";

    // ── Step 6 ─────────────────────────────────────────────────────
    std::cout << "=== Step 6: Write JSON ===\n";
    writeJson(filtered, jsonFile);
    std::cout << "\n";

    // ── Step 7 ─────────────────────────────────────────────────────
    std::cout << "=== Step 7: Load JSON, shuffle, display ===\n";
    std::cout << "  Enter JSON filename [" << jsonFile << "]: ";
    std::string loadFile;
    std::getline(std::cin, loadFile);
    if (loadFile.empty()) loadFile = jsonFile;
    std::list<Item> loaded = readJson(loadFile);
    std::cout << "  Loaded: " << loaded.size() << " items\n\n";
    std::cout << "  Before shuffle:\n\n";
    displayItems(loaded);

    // std::shuffle requires random-access iterators; copy to vector and back
    std::vector<Item> tmp(loaded.begin(), loaded.end());
    std::shuffle(tmp.begin(), tmp.end(), rng);
    loaded.assign(tmp.begin(), tmp.end());
    std::cout << "\n  After shuffle:\n\n";
    displayItems(loaded);

    return 0;
}
