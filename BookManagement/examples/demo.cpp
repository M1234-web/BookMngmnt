// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"
#include "bookmgmt/Journal.h"
#include "bookmgmt/EBook.h"

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

    return 0;
}
