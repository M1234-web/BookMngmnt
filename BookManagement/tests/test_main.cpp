// Minimal self-contained test runner (no external framework needed).

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bookmgmt/bookmgmt.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/EBook.h"
#include "bookmgmt/AudioBook.h"
#include "bookmgmt/Thesis.h"

using namespace bookmgmt;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExType)            \
    do {                                      \
        bool thrown_ = false;                 \
        try {                                 \
            (void)(expr);                     \
        } catch (const ExType&) {             \
            thrown_ = true;                   \
        } catch (...) {                       \
        }                                     \
        CHECK(thrown_ && "expected " #ExType); \
    } while (0)

static void testMoney() {
    CHECK(Money::of(12, 5).toString() == "12.05");
    CHECK(Money::of(-3, 50).toString() == "-3.50");
    CHECK(Money::fromMinor(7).toString() == "0.07");
    CHECK(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    CHECK(Money::of(3) * 4 == Money::of(12));
    CHECK(Money::of(1) < Money::of(2));
    CHECK_THROWS(Money::of(1, 100), std::invalid_argument);
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    CHECK(b.category() == ResourceCategory::Book);
    CHECK(!b.isDigital());
    CHECK(b.costFor(3) == Money::of(300));
    CHECK_THROWS(b.costFor(0), std::invalid_argument);
    CHECK(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(e.isDigital());
    CHECK(e.costFor(5) == Money::of(150));

    CHECK(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    CHECK(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);

    CHECK_THROWS(Book("", "T", {}, "", "", 2000, Money::of(1)), std::invalid_argument);
    CHECK_THROWS(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1)),
                 std::invalid_argument);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    CHECK(c.size() == 3);
    CHECK(c.contains("B1"));
    CHECK(c.find("nope") == nullptr);
    CHECK_THROWS(c.get("nope"), NotFoundError);
    CHECK_THROWS(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1)),
                 DuplicateIdError);

    CHECK(c.searchTitle("clean").size() == 2);
    CHECK(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    CHECK(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    CHECK(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    CHECK(c.holdings("B1") == 3);
    CHECK_THROWS(c.addHoldings("B1", -5), std::invalid_argument);

    c.remove("R1");
    CHECK(c.size() == 2);
    CHECK_THROWS(c.remove("R1"), NotFoundError);
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    CHECK(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    CHECK(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    CHECK(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    CHECK(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    CHECK(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    CHECK(b.spent() == Money::of(300));
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 1);
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    CHECK_THROWS(b.commit(ResourceCategory::Book, 2, Money::of(10)), QuotaExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800)),
                 BudgetExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1)),
                 std::invalid_argument);
    CHECK(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    CHECK(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    CHECK(acq.canPurchase("B1", 3, &why) && why.empty());
    CHECK(!acq.canPurchase("B1", 4, &why) && !why.empty());
    CHECK(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    CHECK(rec.approved && rec.cost == Money::of(200) && rec.orderNo == 1);
    CHECK(c.holdings("B1") == 2);

    CHECK_THROWS(acq.purchase("B1", 2), QuotaExceededError);
    CHECK_THROWS(acq.purchase("nope", 1), NotFoundError);
    CHECK(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    CHECK(res.size() == 5);
    CHECK(res[0].approved && res[0].cost == Money::of(150));
    CHECK(!res[1].approved);  // 1050 > remaining 150
    CHECK(res[2].approved);
    CHECK(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    CHECK(!res[4].approved);
    CHECK(acq.totalSpent() == Money::of(450));
    CHECK(b.spent() == acq.totalSpent());
    CHECK(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    CHECK(acq.history().size() == 6);
}

static void testJournal() {
    Journal j("J1", "Nature", "Springer", 2026, Money::of(100), "0028-0836", 52, 3);
    
    CHECK(j.category() == ResourceCategory::Journal);
    CHECK(!j.isDigital());
    
    // costFor = unitPrice (100) * quantity (2) * subscriptionYears (3) = 600
    CHECK(j.costFor(2) == Money::of(600));
    
    // Test validation
    CHECK_THROWS(Journal("J2", "Bad", "Pub", 2026, Money::of(10), "1234", 12, 0), 
                 std::invalid_argument);
    CHECK_THROWS(j.costFor(0), std::invalid_argument);
}

static void testEBook() {
    EBook ebook("EB1", "Advanced C++", {"Author One", "Author Two"}, "123-456", 
                "TechPub", 2026, Money::of(50), "https://ebook.example", 
                FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(100));
    
    CHECK(ebook.category() == ResourceCategory::EBook);
    CHECK(ebook.isDigital() == true);
    
    // Pricing inherited from ElectronicResource: Platform fee(100) + (UnitPrice(50) * 3 seats) = 250
    CHECK(ebook.costFor(3) == Money::of(250));
    CHECK(ebook.drmProtected() == true);
}

static void testAudioBook() {
    AudioBook ab("AB1", "Dune", {"Frank Herbert"}, "Scott Brick", 1260, 
                 "Macmillan", 2007, Money::of(25));
                 
    CHECK(ab.category() == ResourceCategory::AudioBook);
    CHECK(ab.isDigital() == true);
    CHECK(ab.costFor(3) == Money::of(75)); // 25 * 3 copies = 75
    
    // Check validation: Duration must be positive
    CHECK_THROWS(AudioBook("AB2", "Test", {"A"}, "N", 0, "P", 2020, Money::of(10)), std::invalid_argument);
}

static void testThesis() {
    Thesis th("TH1", "C++ Compilation Speed", "Jane Doe", 
              "MIT", "PhD", "Dr. Smith", 2026);
              
    CHECK(th.category() == ResourceCategory::Thesis);
    CHECK(th.isDigital() == false);
    CHECK(th.costFor(5) == Money::of(0)); // Theses are always free!
    CHECK(th.publisher() == "MIT");       // Mapped university to base publisher field
}

static void testQ4Pricing() {
    // Paperback: Default pricing (100 * 3 = 300)
    Book paperback("B3", "Standard Paper", {"Author"}, "111", "Pub", 2026, 
                   Money::of(100), 1, Binding::Paperback);
    CHECK(paperback.costFor(3) == Money::of(300));

    // Hardcover: 20% markup on unit price (100 * 3 = 300 * 1.2 = 360)
    Book hardcover("B4", "Fancy Edition", {"Author"}, "222", "Pub", 2026, 
                   Money::of(100), 1, Binding::Hardcover);
    CHECK(hardcover.costFor(3) == Money::of(360));
}

static void testQ5BulkDiscounts() {
    // 1. Print item (Book): 10% off for 10+ copies
    Book b("B_Q5", "Title", {"A"}, "123", "Pub", 2026, Money::of(100), 1, Binding::Paperback);
    CHECK(b.costFor(9) == Money::of(900));  // Normal (100 * 9)
    CHECK(b.costFor(10) == Money::of(900)); // Bulk discount: 100 * 10 = 1000 - 10% = 900
    
    // 2. Print item (Journal): 10% off for 10+ copies
    Journal j("J_Q5", "Title", "Pub", 2026, Money::of(100), "123", 12, 1);
    CHECK(j.costFor(9) == Money::of(900));  // Normal (100 * 9)
    CHECK(j.costFor(10) == Money::of(900)); // Bulk discount: 100 * 10 = 1000 - 10% = 900
    
    // 3. Electronic item: 50% off beyond 50th seat
    ElectronicResource e("E_Q5", "DB", "Pub", 2026, Money::of(10), "url", 
                         LicenseModel::AnnualSubscription, Money::of(100));
    // 50 seats = platform(100) + 50*10(500) = 600
    CHECK(e.costFor(50) == Money::of(600));
    // 60 seats = platform(100) + 50*10(500) + 10*5(50) = 650
    CHECK(e.costFor(60) == Money::of(650));
}

static void testQ6Taxes() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "123", "Pub", 2026, Money::of(100)); 
    Budget b(Money::of(1000));
    AcquisitionManager acq(c, b);

    // Set taxes to 10%
    AcquisitionManager::printTaxRatePercent = 10;
    
    auto rec = acq.purchase("B1", 1);
    CHECK(rec.cost == Money::of(100));   // Pre-tax cost
    CHECK(rec.tax == Money::of(10));     // 10% Tax
    CHECK(rec.totalCost() == Money::of(110)); // Total
    CHECK(b.spent() == Money::of(110));  // Budget checked post-tax

    // Reset taxes so other tests don't break
    AcquisitionManager::printTaxRatePercent = 0;
}

static void testQ7TitleLimits() {
    Catalog c;
    c.emplace<Book>("B1", "Book 1", std::vector<std::string>{"A"}, "1", "P", 2026, Money::of(10));
    c.emplace<Book>("B2", "Book 2", std::vector<std::string>{"A"}, "2", "P", 2026, Money::of(10));
    c.emplace<Book>("B3", "Book 3", std::vector<std::string>{"A"}, "3", "P", 2026, Money::of(10));
    
    Budget b(Money::of(1000));
    // Max 100 units, $1000 spend, BUT only 2 unique titles allowed!
    b.setQuota(ResourceCategory::Book, {100, Money::of(1000), 2}); 
    
    AcquisitionManager acq(c, b);
    
    CHECK(acq.canPurchase("B1", 5)); // 1st title
    acq.purchase("B1", 5);
    
    CHECK(acq.canPurchase("B2", 5)); // 2nd title
    acq.purchase("B2", 5);
    
    CHECK(acq.canPurchase("B1", 10)); // B1 is already bought, doesn't count against title limit!
    acq.purchase("B1", 10);
    
    CHECK(!acq.canPurchase("B3", 1)); // 3rd title -> REJECTED
}

static void testQ8Cancellation() {
    Catalog c;
    c.emplace<Book>("B1", "Book 1", std::vector<std::string>{"A"}, "1", "P", 2026, Money::of(100));
    Budget b(Money::of(1000));
    AcquisitionManager acq(c, b);

    // Buy 2 copies
    auto rec = acq.purchase("B1", 2);
    CHECK(c.holdings("B1") == 2);
    CHECK(b.spent() == Money::of(200));

    // Cancel order
    auto cancelRec = acq.cancel(rec.orderNo);
    
    CHECK(c.holdings("B1") == 0); // Holdings reduced
    CHECK(b.spent() == Money::of(0)); // Budget refunded
    CHECK(cancelRec.isCancellation == true); // Flag set
    CHECK(cancelRec.quantity == -2); // Negative quantity
    CHECK(cancelRec.totalCost() == Money::of(-200)); // Negative total
    
    // Confirm we cannot cancel it a second time!
    CHECK_THROWS(acq.cancel(rec.orderNo), std::invalid_argument);
}

static void testQ9Departments() {
    Catalog c;
    c.emplace<Book>("B1", "Book 1", std::vector<std::string>{"A"}, "1", "P", 2026, Money::of(100));
    
    Budget mainBudget(Money::of(1000));
    AcquisitionManager acq(c, mainBudget); // Main is automatically registered
    
    Budget physicsBudget(Money::of(200));
    acq.addDepartment("Physics", physicsBudget);
    
    // Test that the Physics budget works
    CHECK(acq.canPurchase("B1", 1, "Physics"));
    acq.purchase("B1", 1, "Physics");
    CHECK(physicsBudget.spent() == Money::of(100));
    CHECK(mainBudget.spent() == Money::of(0)); // Main is untouched
    
    // Exceed Physics budget
    CHECK(!acq.canPurchase("B1", 2, "Physics"));
    
    // Fallback default still charges Main
    acq.purchase("B1", 5); 
    CHECK(mainBudget.spent() == Money::of(500));
}

static void testQ10Rollover() {
    Budget b1(Money::of(1000));
    b1.setQuota(ResourceCategory::Book, {10, Money::of(500), 5}); // Set a quota
    
    // Spend 200, leaving 800 unspent
    b1.commit(ResourceCategory::Book, 2, Money::of(200), "B1");
    
    // Roll over to next year, keeping 50% of the unspent 800 (which is 400)
    // Base 1000 + 400 carryover = 1400 new total
    Budget b2 = b1.rollover(50);
    
    // Verify math
    CHECK(b2.total() == Money::of(1400));
    CHECK(b2.spent() == Money::of(0));
    CHECK(b2.remaining() == Money::of(1400));
    
    // Verify quotas carried over but usage reset
    auto q = b2.quotaFor(ResourceCategory::Book);
    CHECK(q.has_value());
    CHECK(q->maxUnits == 10);
    
    auto u = b2.usageFor(ResourceCategory::Book);
    CHECK(u.units == 0);      // Reset!
    CHECK(u.titles.empty());  // Reset!
}

int main() {
    testMoney();
    testResourcesAndCost();
    testCatalog();
    testBudget();
    testAcquisition();
    testJournal();
    testEBook();
    testAudioBook();
    testThesis();
    testQ4Pricing();
    testQ5BulkDiscounts();
    testQ6Taxes();
    testQ7TitleLimits();
    testQ8Cancellation();
    testQ9Departments();
    testQ10Rollover();
    
    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
