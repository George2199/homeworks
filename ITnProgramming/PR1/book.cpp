#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <algorithm>

class Book {
private:
    int         pageCount;
    std::string author;
    std::string title;
    std::vector<std::string> quotes; // size == pageCount, each element = 10 random letters

    static std::string generateQuote() {
        std::string q;
        for (int i = 0; i < 10; ++i)
            q += static_cast<char>('a' + rand() % 26);
        return q;
    }

    void initQuotes() {
        quotes.clear();
        for (int i = 0; i < pageCount; ++i)
            quotes.push_back(generateQuote());
    }

public:
    // ───── Constructors / Destructor ─────

    Book() : pageCount(10), author("Unknown"), title("Untitled") {
        std::cout << "[Book::Book()] Default constructor\n";
        initQuotes();
    }

    Book(int pages, const std::string& auth, const std::string& ttl)
        : pageCount(pages > 0 ? pages : 0), author(auth), title(ttl) {
        std::cout << "[Book::Book(int,string,string)] Parameterized constructor\n";
        initQuotes();
    }

    Book(const Book& other)
        : pageCount(other.pageCount), author(other.author),
          title(other.title), quotes(other.quotes) {
        std::cout << "[Book::Book(const Book&)] Copy constructor\n";
    }

    ~Book() {
        std::cout << "[Book::~Book()] Destructor for \"" << title << "\"\n";
    }

    // ───── Getters ─────

    int getPageCount() const {
        std::cout << "[Book::getPageCount()]\n";
        return pageCount;
    }

    std::string getAuthor() const {
        std::cout << "[Book::getAuthor()]\n";
        return author;
    }

    std::string getTitle() const {
        std::cout << "[Book::getTitle()]\n";
        return title;
    }

    const std::vector<std::string>& getQuotes() const {
        std::cout << "[Book::getQuotes()]\n";
        return quotes;
    }

    // ───── Setters ─────

    void setPageCount(int pages) {
        std::cout << "[Book::setPageCount()]\n";
        pageCount = pages > 0 ? pages : 0;
        initQuotes();
    }

    void setAuthor(const std::string& auth) {
        std::cout << "[Book::setAuthor()]\n";
        author = auth;
    }

    void setTitle(const std::string& ttl) {
        std::cout << "[Book::setTitle()]\n";
        title = ttl;
    }

    // ───── Methods ─────

    void read() const {
        std::cout << "[Book::read()] Opening reader for \"" << title << "\" by " << author << "\n";
        if (pageCount == 0) { std::cout << "  The book is empty.\n"; return; }

        int cur = 0; // current page index (0-based)
        bool reading = true;
        while (reading) {
            std::cout << "\n--- Page " << (cur + 1) << " / " << pageCount << " ---\n";
            std::cout << "  " << quotes[cur] << "\n";
            std::cout << "[n] next  [p] prev  [q] close > ";

            char cmd;
            std::cin >> cmd;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            switch (cmd) {
            case 'n': case 'N':
                if (cur + 1 < pageCount) ++cur;
                else std::cout << "  Already on the last page.\n";
                break;
            case 'p': case 'P':
                if (cur > 0) --cur;
                else std::cout << "  Already on the first page.\n";
                break;
            case 'q': case 'Q':
                reading = false;
                std::cout << "  Closed the book.\n";
                break;
            default:
                std::cout << "  Unknown command. Use n / p / q.\n";
            }
        }
    }

    // вырвать страницы
    void tearPages(int n) {
        std::cout << "[Book::tearPages()] Tearing " << n << " page(s)\n";
        if (n <= 0) { std::cout << "  Invalid count.\n"; return; }
        if (n >= pageCount) {
            n = pageCount > 1 ? pageCount - 1 : 0;
            std::cout << "  Limiting tear to " << n << " page(s) (at least 1 must remain).\n";
        }
        for (int i = 0; i < n; ++i) quotes.pop_back();
        pageCount -= n;
        std::cout << "  Pages left: " << pageCount << "\n";
    }

    // вклеить страницы
    void gluePages(int n) {
        std::cout << "[Book::gluePages()] Gluing " << n << " page(s)\n";
        if (n <= 0) { std::cout << "  Invalid count.\n"; return; }
        for (int i = 0; i < n; ++i) quotes.push_back(generateQuote());
        pageCount += n;
        std::cout << "  Pages now: " << pageCount << "\n";
    }

    // открыть на случайной странице
    void openRandom() const {
        std::cout << "[Book::openRandom()] Opening on a random page\n";
        if (pageCount == 0) { std::cout << "  The book is empty.\n"; return; }
        int page = rand() % pageCount;
        std::cout << "  Opened page " << (page + 1) << ": " << quotes[page] << "\n";
    }

    // узнать температуру горения бумаги
    // Стандартное значение: ~233 °C (Fahrenheit 451 ≈ 232.8 °C), разброс ±5 °C
    double burningTemp() const {
        std::cout << "[Book::burningTemp()] Paper ignition temperature\n";
        const double temp = 232.8; // Fahrenheit 451 = 232.778 °C
        std::cout << "  Temperature: " << temp << " C\n";
        return temp;
    }

    // ───── Operator overloads ─────

    Book& operator=(const Book& other) {
        std::cout << "[Book::operator=()] Assignment\n";
        if (this != &other) {
            pageCount = other.pageCount;
            author    = other.author;
            title     = other.title;
            quotes    = other.quotes;
        }
        return *this;
    }

    bool operator==(const Book& other) const {
        std::cout << "[Book::operator==()] Comparison\n";
        return pageCount == other.pageCount
            && author    == other.author
            && title     == other.title;
    }

    // Binary >> with int — получить случайную цитату со страницы X (1-based)
    std::string operator>>(int page) const {
        std::cout << "[Book::operator>>(int)] Quote from page " << page << "\n";
        if (page < 1 || page > pageCount) {
            std::cout << "  Page " << page << " does not exist.\n";
            return "";
        }
        std::cout << "  Quote: " << quotes[page - 1] << "\n";
        return quotes[page - 1];
    }

    // Binary [] with char — найти первую страницу, цитата которой начинается с символа c
    int operator[](char c) const {
        std::cout << "[Book::operator[](char)] Searching for first page starting with '" << c << "'\n";
        for (int i = 0; i < pageCount; ++i)
            if (!quotes[i].empty() && quotes[i][0] == c) {
                std::cout << "  Found: page " << (i + 1) << "\n";
                return i + 1;
            }
        std::cout << "  Not found.\n";
        return -1;
    }

    // ───── Friend stream operators ─────

    friend std::ostream& operator<<(std::ostream& os, const Book& b) {
        std::cout << "[operator<<(ostream&, Book&)] Stream output\n";
        os << "==============================\n";
        os << "Title:      " << b.title     << "\n";
        os << "Author:     " << b.author    << "\n";
        os << "Pages:      " << b.pageCount << "\n";
        os << "Quotes (first 5):\n";
        int limit = std::min((int)b.quotes.size(), 5);
        for (int i = 0; i < limit; ++i)
            os << "  [" << (i + 1) << "] " << b.quotes[i] << "\n";
        if (b.pageCount > 5)
            os << "  ... (" << (b.pageCount - 5) << " more pages)\n";
        os << "==============================\n";
        return os;
    }

    friend std::istream& operator>>(std::istream& is, Book& b) {
        std::cout << "[operator>>(istream&, Book&)] Stream input\n";
        std::cout << "  Title:  ";  std::getline(is, b.title);
        std::cout << "  Author: ";  std::getline(is, b.author);
        std::cout << "  Pages:  ";
        is >> b.pageCount;
        is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (b.pageCount < 0) b.pageCount = 0;
        b.initQuotes();
        return is;
    }
};

// ───── Helper ─────

static void pauseInput() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

static int readInt(const std::string& prompt) {
    int val;
    std::cout << prompt;
    while (!(std::cin >> val)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "  Invalid input. " << prompt;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return val;
}

// ───── Main ─────

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    Book* book = new Book(100, "Ray Bradbury", "Fahrenheit 451");
    Book* clipboard = nullptr;

    bool running = true;
    while (running) {
        std::cout << "\n+================================+\n";
        std::cout <<   "|       BOOK CLASS MENU          |\n";
        std::cout <<   "+================================+\n";
        std::cout <<   "|  1.  Output (operator<<)       |\n";
        std::cout <<   "|  2.  Input  (operator>>)       |\n";
        std::cout <<   "|  3.  Create default book       |\n";
        std::cout <<   "|  4.  Create param. book        |\n";
        std::cout <<   "|  5.  Copy book                 |\n";
        std::cout <<   "|  6.  Read                      |\n";
        std::cout <<   "|  7.  Tear pages                |\n";
        std::cout <<   "|  8.  Glue pages                |\n";
        std::cout <<   "|  9.  Open random page          |\n";
        std::cout <<   "| 10.  Burning temperature       |\n";
        std::cout <<   "| 11.  Quote from page (>>int)   |\n";
        std::cout <<   "| 12.  Find by char ([])         |\n";
        std::cout <<   "| 13.  Assign book (=)           |\n";
        std::cout <<   "| 14.  Compare books (==)        |\n";
        std::cout <<   "| 15.  Getters                   |\n";
        std::cout <<   "| 16.  Setters                   |\n";
        std::cout <<   "|  0.  Exit                      |\n";
        std::cout <<   "+================================+\n";
        std::cout <<   "Choice: ";

        int choice;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "\n";

        switch (choice) {
        case 0:
            running = false;
            break;

        case 1:
            std::cout << *book;
            break;

        case 2:
            std::cin >> *book;
            break;

        case 3:
            delete book;
            book = new Book();
            break;

        case 4: {
            int p = readInt("  Pages: ");
            std::string auth, ttl;
            std::cout << "  Author: "; std::getline(std::cin, auth);
            std::cout << "  Title:  "; std::getline(std::cin, ttl);
            delete book;
            book = new Book(p, auth, ttl);
            break;
        }

        case 5:
            delete clipboard;
            clipboard = new Book(*book);
            std::cout << "  Clipboard book:\n" << *clipboard;
            break;

        case 6:
            book->read();
            break;

        case 7: {
            int n = readInt("  How many pages to tear? ");
            book->tearPages(n);
            break;
        }

        case 8: {
            int n = readInt("  How many pages to glue? ");
            book->gluePages(n);
            break;
        }

        case 9:
            book->openRandom();
            break;

        case 10:
            book->burningTemp();
            break;

        case 11: {
            int p = readInt("  Page number: ");
            (*book) >> p;
            break;
        }

        case 12: {
            char c;
            std::cout << "  Character: ";
            std::cin >> c;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            (*book)[c];
            break;
        }

        case 13:
            if (!clipboard) {
                std::cout << "  No clipboard book. Use option 5 first.\n";
            } else {
                *book = *clipboard;
                std::cout << "  Assigned. Current book:\n" << *book;
            }
            break;

        case 14:
            if (!clipboard) {
                std::cout << "  No clipboard book. Use option 5 first.\n";
            } else {
                bool eq = (*book == *clipboard);
                std::cout << "  Books are " << (eq ? "EQUAL" : "NOT EQUAL") << "\n";
            }
            break;

        case 15:
            std::cout << "  pageCount = " << book->getPageCount() << "\n";
            std::cout << "  author    = " << book->getAuthor()    << "\n";
            std::cout << "  title     = " << book->getTitle()     << "\n";
            std::cout << "  quotes[0] = " << book->getQuotes()[0] << "\n";
            break;

        case 16: {
            int p = readInt("  New page count: ");
            std::string auth, ttl;
            std::cout << "  New author: "; std::getline(std::cin, auth);
            std::cout << "  New title:  "; std::getline(std::cin, ttl);
            book->setPageCount(p);
            book->setAuthor(auth);
            book->setTitle(ttl);
            break;
        }

        default:
            std::cout << "  Unknown option.\n";
        }

        pauseInput();
    }

    delete clipboard;
    delete book;
    return 0;
}
