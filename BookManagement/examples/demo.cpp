// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/EBook.h"
#include "bookmgmt/AudioBook.h"
#include "bookmgmt/Thesis.h"

using namespace bookmgmt;

int main() {
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);

    std::cout << "=== Catalog ===\n";
    for (const Resource* r : catalog.all()) std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n" << catalog.get("R001");

    Budget budget(Money::of(20000));
    budget.setQuota(ResourceCategory::Book, {10, Money::of(8000)});
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";

    acq.processBatch({
        {"B001", 4},   // 1800  ok
        {"B002", 5},   // 6000  ok  -> book spend 7800
        {"B001", 1},   // 450   rejected: book spend quota (200 left)
        {"R001", 20},  // 5000  ok
        {"R002", 25},  // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15},  // 6000  ok  -> e-resource spend 11000
        {"R002", 5},   // 2000  rejected: e-resource spend quota (1000 left)
        {"X999", 1},   // rejected: unknown id
    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource* r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try {
        acq.purchase("B002", 1);
    } catch (const QuotaExceededError& e) {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }

    std::cout << "\n=== Q1: Journal Demonstration ===\n";
    
    // 1. Add Journal to catalog
    catalog.emplace<Journal>("J001", "Nature", "Springer", 2026, Money::of(100), "0028-0836", 52, 2);
    
    // 2. Set quota (We have about 6200 overall budget remaining from the initial 20000)
    budget.setQuota(ResourceCategory::Journal, {10, Money::of(2000)});
    
    // 3. Purchase Journal (100 * 2 copies * 2 years = 400)
    auto j_rec = acq.purchase("J001", 2);
    if (j_rec.approved) {
        std::cout << "Successfully purchased 2 copies of Journal!\n";
        std::cout << "Cost: " << j_rec.cost << "\n\n";
    }

    // 4. Print details to show overridden printDetails()
    std::cout << "Journal Details:\n" << catalog.get("J001");
    // -------------------------------------------------------------

    std::cout << "\n=== Q2: EBook Demonstration ===\n";
    catalog.emplace<EBook>("E001", "Clean Architecture E-Edition", 
                           std::vector<std::string>{"Robert C. Martin"}, "978-0134494166", 
                           "Prentice Hall", 2017, Money::of(45), "https://lib.example/clean-arch", 
                           FileFormat::PDF, false, LicenseModel::Perpetual, Money::of(200));
    
    budget.setQuota(ResourceCategory::EBook, {10, Money::of(1000)});
    auto ebookRec = acq.purchase("E001", 5); // 200 fee + (45 * 5) = 425
    
    if (ebookRec.approved) {
        std::cout << "Successfully purchased EBook! Cost: " << ebookRec.cost << "\n";
    }
    std::cout << "EBook Details:\n" << catalog.get("E001");

    // NEW CODE FOR Q3: DEMONSTRATING AUDIOBOOK & THESIS
    std::cout << "\n=== Q3: AudioBook & Thesis Demonstration ===\n";
    
    // 1. Add them to the catalog
    catalog.emplace<AudioBook>("A001", "Project Hail Mary", std::vector<std::string>{"Andy Weir"},
                               "Ray Porter", 960, "Audible", 2021, Money::of(30));
    catalog.emplace<Thesis>("T001", "Memory Management in C++", "Alice Smith",
                            "Stanford University", "PhD", "Dr. Bob", 2026);
    
    // 2. Set budgets
    budget.setQuota(ResourceCategory::AudioBook, {10, Money::of(500)});
    budget.setQuota(ResourceCategory::Thesis, {10, Money::of(0)}); // They are free!
    
    // 3. Purchase them
    auto ab_rec = acq.purchase("A001", 2); // 30 * 2 = 60
    if (ab_rec.approved) {
        std::cout << "Successfully purchased AudioBook! Cost: " << ab_rec.cost << "\n";
    }
    
    auto th_rec = acq.purchase("T001", 1); // Cost = 0
    if (th_rec.approved) {
        std::cout << "Successfully acquired Thesis! Cost: " << th_rec.cost << "\n";
    }

    // 4. Print details to verify formatting
    std::cout << "\nAudioBook Details:\n" << catalog.get("A001");
    std::cout << "\nThesis Details:\n" << catalog.get("T001");

    // NEW CODE FOR Q4: HARDCOVER PRICING DEMO

    std::cout << "\n=== Q4: Hardcover Book Pricing ===\n";
    catalog.emplace<Book>("B003", "Design Patterns (Hardcover)", 
                          std::vector<std::string>{"Gang of Four"}, "978-0201633610", 
                          "Addison-Wesley", 1994, Money::of(50), 1, Binding::Hardcover);
    
    // REMOVED the setQuota line so we don't break the existing budget!
    auto hc_rec = acq.purchase("B003", 2); 
    
    if (hc_rec.approved) {
        std::cout << "Successfully purchased Hardcover Books!\n"
                  << "Base Price: 50.00 each | Total Billed Cost (w/ 20% markup): " 
                  << hc_rec.cost << "\n";
    }

    //Q5: BULK DISCOUNTS DEMO

    std::cout << "\n=== Q5: Bulk Discounts ===\n";
    
    // Print item 10% discount
    const auto& q5_book = catalog.get("B001"); // Clean Code (Base price: 450.00)
    std::cout << "Buying 10 copies of 'Clean Code' (Base price: 450.00 each).\n"
              << "Expected cost without discount: 4500.00\n"
              << "Cost with 10% bulk discount: " << q5_book.costFor(10) << "\n\n";
              
    // Electronic item 50% discount past 50 seats
    const auto& q5_elec = catalog.get("R001"); // IEEE Xplore (Base: 150.00, Fee: 2000.00)
    std::cout << "Buying 60 seats of 'IEEE Xplore'.\n"
              << "Cost for first 50 seats: 2000.00 + (50 * 150.00) = 9500.00\n"
              << "Cost for next 10 seats (half price = 75.00 each): 750.00\n"
              << "Total calculated cost: " << q5_elec.costFor(60) << "\n";

    //Q6: TAXES DEMO

    std::cout << "\n=== Q6: Taxes Demonstration ===\n";
    
    // Configure taxes
    bookmgmt::AcquisitionManager::printTaxRatePercent = 5;       // 5% for Print
    bookmgmt::AcquisitionManager::electronicTaxRatePercent = 10; // 10% for Electronic
    
    std::cout << "Configured Print Tax = 5%, Electronic Tax = 10%.\n";
    
    // Buying a print item with tax
    auto tax_book_rec = acq.purchase("B001", 1); 
    if (tax_book_rec.approved) {
        std::cout << "Bought 'Clean Code' (Base Price: 450.00).\n"
                  << "Tax applied: " << tax_book_rec.tax << " | Total Charged to Budget: " 
                  << tax_book_rec.totalCost() << "\n\n";
    }

    // Re-print the report to show the new pre-tax/post-tax columns!
    std::cout << "=== Final Acquisition Report (with Tax Columns) ===\n";
    acq.printReport(std::cout);

    // Q7: TITLE LIMITS DEMO

    std::cout << "\n=== Q7: Title Limits Demonstration ===\n";
    catalog.emplace<Book>("B004", "Refactoring", std::vector<std::string>{"Fowler"}, "000", "Pub", 1999, Money::of(50));
    catalog.emplace<Book>("B005", "Pragmatic Programmer", std::vector<std::string>{"Hunt"}, "001", "Pub", 1999, Money::of(50));
    catalog.emplace<Book>("B006", "Clean Coder", std::vector<std::string>{"Martin"}, "002", "Pub", 2011, Money::of(50));
    
    // We already bought B001, B002, and B003 earlier in the demo.
    // Set limit to 5 so we have exactly enough room for 2 more new titles!
    budget.setQuota(ResourceCategory::Book, {100, Money::of(10000), 5});
    
    // Helper lambda to safely attempt a purchase and catch exceptions
    auto attemptPurchase = [&](const std::string& id) {
        std::cout << "Attempting to buy " << id << "... ";
        try {
            acq.purchase(id, 1);
            std::cout << "APPROVED\n";
        } catch (const bookmgmt::QuotaExceededError& e) {
            std::cout << "REJECTED\n  Reason: " << e.what() << "\n";
        }
    };

    attemptPurchase("B004"); // 4th title (Should succeed)
    attemptPurchase("B005"); // 5th title (Should succeed)
    attemptPurchase("B004"); // Already owned, doesn't count against limit! (Should succeed)
    attemptPurchase("B006"); // 6th title (Should fail)

    //Q8: CANCELLATION DEMO
    std::cout << "\n=== Q8: Cancellation Demonstration ===\n";
    std::cout << "Canceling Order #1 (Clean Code x4, 1800.00)...\n";
    acq.cancel(1);
    
    std::cout << "\n=== Final Acquisition Report (After Cancellation) ===\n";
    acq.printReport(std::cout);

    return 0;
}

