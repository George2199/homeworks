#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <typeinfo>
#include <sstream>
#include <limits>

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::string randomStr() {
    std::string s;
    for (int i = 0; i < 10; ++i)
        s += static_cast<char>('a' + rand() % 26);
    return s;
}

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

static std::string readStr(const std::string& prompt) {
    std::string s;
    std::cout << prompt;
    std::getline(std::cin, s);
    return s;
}

// ─── Base: PrintedMedia (abstract) ───────────────────────────────────────────

class PrintedMedia {
protected:
    int         pageCount;
    std::string topic;
    std::string author;
    std::string title;

    // Shared helper for getInfo() in subclasses
    std::string baseInfo() const {
        std::ostringstream ss;
        ss << "  pageCount: " << pageCount << "\n"
           << "  topic:     " << topic     << "\n"
           << "  author:    " << author    << "\n"
           << "  title:     " << title     << "\n";
        return ss.str();
    }

public:
    PrintedMedia()
        : pageCount(1), topic("Unknown"), author("Unknown"), title("Unknown") {}

    PrintedMedia(int pc, const std::string& top,
                 const std::string& auth, const std::string& ttl)
        : pageCount(pc), topic(top), author(auth), title(ttl) {}

    PrintedMedia(const PrintedMedia& o)
        : pageCount(o.pageCount), topic(o.topic),
          author(o.author), title(o.title) {}

    virtual ~PrintedMedia() {}

    // Getters
    int         getPageCount() const { return pageCount; }
    std::string getTopic()     const { return topic;     }
    std::string getAuthor()    const { return author;    }
    std::string getTitle()     const { return title;     }

    // Setters
    void setPageCount(int v)              { pageCount = v; }
    void setTopic    (const std::string& v) { topic   = v; }
    void setAuthor   (const std::string& v) { author  = v; }
    void setTitle    (const std::string& v) { title   = v; }

    // Pure virtual methods
    virtual void        read()              = 0;
    virtual std::string getRandomQuote()    = 0;
    virtual void        openRandomPage()    = 0;
    virtual std::string getClassName() const = 0;
    virtual std::string getInfo()      const = 0;
};

// ─── Book ─────────────────────────────────────────────────────────────────────
// Unique field:  edition (int)
// Unique method: makeBookmark()

class Book : public PrintedMedia {
private:
    int edition;

public:
    Book() : PrintedMedia(), edition(1) {}

    Book(int pc, const std::string& top, const std::string& auth,
         const std::string& ttl, int ed)
        : PrintedMedia(pc, top, auth, ttl), edition(ed) {}

    Book(const Book& o) : PrintedMedia(o), edition(o.edition) {}

    ~Book() {}

    int  getEdition() const    { return edition; }
    void setEdition(int v)     { edition = v;    }

    std::string getClassName() const override { return "Book"; }

    std::string getInfo() const override {
        std::ostringstream ss;
        ss << "Type: Book\n" << baseInfo()
           << "  edition:   " << edition << "\n";
        return ss.str();
    }

    void read() override {
        std::cout << "[Book::read()] \"" << title << "\", ed." << edition
                  << ", reading from page 1:\n  " << randomStr() << "\n";
    }

    std::string getRandomQuote() override {
        std::string q = randomStr();
        std::cout << "[Book::getRandomQuote()] Excerpt from \"" << title << "\": " << q << "\n";
        return q;
    }

    void openRandomPage() override {
        int p = pageCount > 0 ? rand() % pageCount + 1 : 1;
        std::cout << "[Book::openRandomPage()] Page " << p << " of \""
                  << title << "\":\n  " << randomStr() << "\n";
    }

    // Unique
    void makeBookmark() {
        std::cout << "[Book::makeBookmark()] Bookmark placed in \"" << title << "\".\n";
    }
};

// ─── Comic ────────────────────────────────────────────────────────────────────
// Unique field:  artist (string)
// Unique method: lookAtPictures()

class Comic : public PrintedMedia {
private:
    std::string artist;

public:
    Comic() : PrintedMedia(), artist("Unknown") {}

    Comic(int pc, const std::string& top, const std::string& auth,
          const std::string& ttl, const std::string& art)
        : PrintedMedia(pc, top, auth, ttl), artist(art) {}

    Comic(const Comic& o) : PrintedMedia(o), artist(o.artist) {}

    ~Comic() {}

    std::string getArtist() const               { return artist; }
    void        setArtist(const std::string& v) { artist = v;    }

    std::string getClassName() const override { return "Comic"; }

    std::string getInfo() const override {
        std::ostringstream ss;
        ss << "Type: Comic\n" << baseInfo()
           << "  artist:    " << artist << "\n";
        return ss.str();
    }

    void read() override {
        std::cout << "[Comic::read()] \"" << title << "\", art by " << artist
                  << ". *flip* *flip*\n  Bubble: \"" << randomStr() << "\"\n";
    }

    std::string getRandomQuote() override {
        std::string q = randomStr();
        std::cout << "[Comic::getRandomQuote()] Panel caption: *" << q << "*\n";
        return q;
    }

    void openRandomPage() override {
        int p = pageCount > 0 ? rand() % pageCount + 1 : 1;
        std::cout << "[Comic::openRandomPage()] Panel page " << p << " of \""
                  << title << "\":\n  Art: " << randomStr() << "\n";
    }

    // Unique
    void lookAtPictures() {
        std::cout << "[Comic::lookAtPictures()] Admiring artwork in \""
                  << title << "\" by " << artist << ". Wow!\n";
    }
};

// ─── Newspaper ────────────────────────────────────────────────────────────────
// Unique field:  issueDate (string)
// Unique method: readHeadlines()

class Newspaper : public PrintedMedia {
private:
    std::string issueDate;

public:
    Newspaper() : PrintedMedia(), issueDate("01.01.2000") {}

    Newspaper(int pc, const std::string& top, const std::string& auth,
              const std::string& ttl, const std::string& date)
        : PrintedMedia(pc, top, auth, ttl), issueDate(date) {}

    Newspaper(const Newspaper& o) : PrintedMedia(o), issueDate(o.issueDate) {}

    ~Newspaper() {}

    std::string getIssueDate() const               { return issueDate; }
    void        setIssueDate(const std::string& v) { issueDate = v;    }

    std::string getClassName() const override { return "Newspaper"; }

    std::string getInfo() const override {
        std::ostringstream ss;
        ss << "Type: Newspaper\n" << baseInfo()
           << "  issueDate: " << issueDate << "\n";
        return ss.str();
    }

    void read() override {
        std::cout << "[Newspaper::read()] Unfolding \"" << title
                  << "\" (" << issueDate << "):\n  " << randomStr() << "\n";
    }

    std::string getRandomQuote() override {
        std::string q = randomStr();
        std::cout << "[Newspaper::getRandomQuote()] Headline: " << q << "\n";
        return q;
    }

    void openRandomPage() override {
        int p = pageCount > 0 ? rand() % pageCount + 1 : 1;
        std::cout << "[Newspaper::openRandomPage()] Page " << p << " of \""
                  << title << "\":\n  Article snippet: " << randomStr() << "\n";
    }

    // Unique
    void readHeadlines() {
        std::cout << "[Newspaper::readHeadlines()] Headlines of \""
                  << title << "\" (" << issueDate << "):\n";
        for (int i = 0; i < 3; ++i)
            std::cout << "  - " << randomStr() << "\n";
    }
};

// ─── Brochure ─────────────────────────────────────────────────────────────────
// Unique field:  organization (string)
// Unique method: fold()

class Brochure : public PrintedMedia {
private:
    std::string organization;

public:
    Brochure() : PrintedMedia(), organization("Unknown") {}

    Brochure(int pc, const std::string& top, const std::string& auth,
             const std::string& ttl, const std::string& org)
        : PrintedMedia(pc, top, auth, ttl), organization(org) {}

    Brochure(const Brochure& o) : PrintedMedia(o), organization(o.organization) {}

    ~Brochure() {}

    std::string getOrganization() const               { return organization; }
    void        setOrganization(const std::string& v) { organization = v;   }

    std::string getClassName() const override { return "Brochure"; }

    std::string getInfo() const override {
        std::ostringstream ss;
        ss << "Type: Brochure\n" << baseInfo()
           << "  organization: " << organization << "\n";
        return ss.str();
    }

    void read() override {
        std::cout << "[Brochure::read()] Skimming \"" << title
                  << "\" from " << organization << ":\n  " << randomStr() << "\n";
    }

    std::string getRandomQuote() override {
        std::string q = randomStr();
        std::cout << "[Brochure::getRandomQuote()] Key fact: " << q << "\n";
        return q;
    }

    void openRandomPage() override {
        int p = pageCount > 0 ? rand() % pageCount + 1 : 1;
        std::cout << "[Brochure::openRandomPage()] Section " << p << " of \""
                  << title << "\":\n  " << randomStr() << "\n";
    }

    // Unique
    void fold() {
        std::cout << "[Brochure::fold()] Folding \"" << title
                  << "\" from " << organization << " into three parts.\n";
    }
};

// ─── Unique method dispatch (typeid + dynamic_cast) ───────────────────────────

static void callUniqueMethod(PrintedMedia* p) {
    const std::type_info& ti = typeid(*p);
    std::cout << "typeid: " << ti.name() << "\n";

    if      (ti == typeid(Book))      dynamic_cast<Book*>     (p)->makeBookmark();
    else if (ti == typeid(Comic))     dynamic_cast<Comic*>    (p)->lookAtPictures();
    else if (ti == typeid(Newspaper)) dynamic_cast<Newspaper*>(p)->readHeadlines();
    else if (ti == typeid(Brochure))  dynamic_cast<Brochure*> (p)->fold();
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    std::vector<PrintedMedia*> shelf;

    // Pre-fill with 6 objects (min required)
    shelf.push_back(new Book      (300, "Fantasy",  "Tolkien",   "LOTR",        1));
    shelf.push_back(new Book      (200, "Sci-Fi",   "Asimov",    "Foundation",  2));
    shelf.push_back(new Comic     ( 24, "Action",   "Lee",       "Spider-Man",  "Ditko"));
    shelf.push_back(new Comic     ( 32, "Drama",    "Moore",     "Watchmen",    "Gibbons"));
    shelf.push_back(new Newspaper ( 12, "Politics", "Editorial", "Daily News",  "01.01.2024"));
    shelf.push_back(new Brochure  (  8, "Tourism",  "City Hall", "Visit Us!",   "City Tourism Dept."));

    bool running = true;
    while (running) {
        std::cout << "\n+================================+\n";
        std::cout <<   "| PRINTED MEDIA HIERARCHY MENU  |\n";
        std::cout <<   "+================================+\n";
        std::cout <<   "| 1. Add object to shelf        |\n";
        std::cout <<   "| 2. Call base method for all   |\n";
        std::cout <<   "| 3. Print all info             |\n";
        std::cout <<   "| 4. Call unique method         |\n";
        std::cout <<   "| 0. Exit                       |\n";
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

        case 1: {
            std::cout << "Choose class:\n"
                      << "  1. Book\n  2. Comic\n  3. Newspaper\n  4. Brochure\n";
            int cls  = readInt("Class:  ");
            int pc   = readInt("Pages:  ");
            std::string top  = readStr("Topic:  ");
            std::string auth = readStr("Author: ");
            std::string ttl  = readStr("Title:  ");

            PrintedMedia* obj = nullptr;
            switch (cls) {
            case 1: { int ed          = readInt("Edition:      ");
                      obj = new Book(pc, top, auth, ttl, ed); break; }
            case 2: { std::string art  = readStr("Artist:       ");
                      obj = new Comic(pc, top, auth, ttl, art); break; }
            case 3: { std::string date = readStr("Issue date:   ");
                      obj = new Newspaper(pc, top, auth, ttl, date); break; }
            case 4: { std::string org  = readStr("Organization: ");
                      obj = new Brochure(pc, top, auth, ttl, org); break; }
            default: std::cout << "Unknown class.\n";
            }
            if (obj) {
                shelf.push_back(obj);
                std::cout << "Added. Shelf size: " << shelf.size() << "\n";
            }
            break;
        }

        case 2: {
            std::cout << "Choose method:\n"
                      << "  1. read()\n"
                      << "  2. getRandomQuote()\n"
                      << "  3. openRandomPage()\n";
            int m = readInt("Method: ");
            for (auto* p : shelf) {
                std::cout << "--- " << p->getClassName() << ": " << p->getTitle() << " ---\n";
                if      (m == 1) p->read();
                else if (m == 2) p->getRandomQuote();
                else if (m == 3) p->openRandomPage();
                else { std::cout << "Unknown method.\n"; break; }
            }
            break;
        }

        case 3: {
            std::cout << "Shelf (" << shelf.size() << " items):\n";
            for (size_t i = 0; i < shelf.size(); ++i)
                std::cout << "[" << i << "] " << shelf[i]->getInfo();
            break;
        }

        case 4: {
            if (shelf.empty()) { std::cout << "Shelf is empty.\n"; break; }
            for (size_t i = 0; i < shelf.size(); ++i)
                std::cout << "[" << i << "] " << shelf[i]->getClassName()
                          << " - " << shelf[i]->getTitle() << "\n";
            int idx = readInt("Select index: ");
            if (idx < 0 || idx >= (int)shelf.size()) {
                std::cout << "Invalid index.\n"; break;
            }
            callUniqueMethod(shelf[idx]);
            break;
        }

        default:
            std::cout << "Unknown option.\n";
        }

        std::cout << "\nPress Enter to continue...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    for (auto* p : shelf) delete p;
    return 0;
}
