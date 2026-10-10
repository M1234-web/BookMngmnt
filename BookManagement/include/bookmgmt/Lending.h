#pragma once
// LendingManager: tracks patron borrows and digital sessions, enforcing 
// the available holdings limits stored in the Catalog.

#include <string>
#include <map>
#include <set>
#include <vector>

#include "bookmgmt/Catalog.h"

namespace bookmgmt {

class LendingManager {
public:
    explicit LendingManager(Catalog& catalog);

    // --- PRINT RESOURCES ---
    // Throws if resource is digital, or if no copies are available.
    void borrowPrint(const std::string& patronId, const std::string& resourceId);
    void returnPrint(const std::string& patronId, const std::string& resourceId);

    // --- ELECTRONIC RESOURCES ---
    // Throws if resource is physical, or if no seats are available.
    void openSession(const std::string& patronId, const std::string& resourceId);
    void closeSession(const std::string& patronId, const std::string& resourceId);

    // --- STATUS QUERIES ---
    int activeLoans(const std::string& resourceId) const;
    int availableUnits(const std::string& resourceId) const;
    std::vector<std::string> getPatronItems(const std::string& patronId) const;

private:
    Catalog& catalog_;
    
    // Tracks total units currently in use per resource to enforce limits
    std::map<std::string, int> inUseCounts_;
    
    // Tracks which patron has which resources (supports multiple of same item via multiset)
    std::map<std::string, std::multiset<std::string>> patronRecords_;
};

}  // namespace bookmgmt