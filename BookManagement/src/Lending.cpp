#include "bookmgmt/Lending.h"
#include <stdexcept>

namespace bookmgmt {

LendingManager::LendingManager(Catalog& catalog) : catalog_(catalog) {}

int LendingManager::activeLoans(const std::string& id) const {
    auto it = inUseCounts_.find(id);
    return it != inUseCounts_.end() ? it->second : 0;
}

int LendingManager::availableUnits(const std::string& id) const {
    // Holdings (total owned) minus the ones currently in use
    return catalog_.holdings(id) - activeLoans(id);
}

void LendingManager::borrowPrint(const std::string& patron, const std::string& id) {
    const Resource& r = catalog_.get(id); // Throws if not in catalog
    if (r.isDigital()) {
        throw std::invalid_argument("Cannot borrow a digital resource. Use openSession instead.");
    }
    if (availableUnits(id) <= 0) {
        throw std::invalid_argument("No print copies available to borrow.");
    }
    inUseCounts_[id]++;
    patronRecords_[patron].insert(id);
}

void LendingManager::returnPrint(const std::string& patron, const std::string& id) {
    const Resource& r = catalog_.get(id);
    if (r.isDigital()) {
        throw std::invalid_argument("Cannot return a digital resource. Use closeSession instead.");
    }
    auto& items = patronRecords_[patron];
    auto it = items.find(id);
    if (it == items.end()) {
        throw std::invalid_argument("Patron does not have this print resource borrowed.");
    }
    items.erase(it); // Erases only one copy
    inUseCounts_[id]--;
}

void LendingManager::openSession(const std::string& patron, const std::string& id) {
    const Resource& r = catalog_.get(id);
    if (!r.isDigital()) {
        throw std::invalid_argument("Cannot open session on a print resource. Use borrowPrint instead.");
    }
    if (availableUnits(id) <= 0) {
        throw std::invalid_argument("No digital seats available for this resource.");
    }
    inUseCounts_[id]++;
    patronRecords_[patron].insert(id);
}

void LendingManager::closeSession(const std::string& patron, const std::string& id) {
    const Resource& r = catalog_.get(id);
    if (!r.isDigital()) {
        throw std::invalid_argument("Cannot close session on a print resource. Use returnPrint instead.");
    }
    auto& items = patronRecords_[patron];
    auto it = items.find(id);
    if (it == items.end()) {
        throw std::invalid_argument("Patron does not have an active session for this resource.");
    }
    items.erase(it);
    inUseCounts_[id]--;
}

std::vector<std::string> LendingManager::getPatronItems(const std::string& patron) const {
    auto it = patronRecords_.find(patron);
    if (it == patronRecords_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

} // namespace bookmgmt