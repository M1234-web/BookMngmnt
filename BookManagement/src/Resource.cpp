#include "bookmgmt/Resource.h"

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace bookmgmt {

const char* categoryName(ResourceCategory c) {
    switch (c) {
        case ResourceCategory::Book: return "Book";
        case ResourceCategory::ElectronicResource: return "ElectronicResource";
        case ResourceCategory::Journal: return "Journal";
        case ResourceCategory::EBook: return "EBook";
        case ResourceCategory::AudioBook: return "AudioBook";
        case ResourceCategory::Thesis: return "Thesis";
    }
    return "Unknown";
}

Resource::Resource(std::string id, std::string title, std::string publisher,
                   int year, Money unitPrice)
    : id_(std::move(id)),
      title_(std::move(title)),
      publisher_(std::move(publisher)),
      year_(year) {
    if (id_.empty()) throw std::invalid_argument("resource id must not be empty");
    if (title_.empty()) throw std::invalid_argument("resource title must not be empty");
    if (unitPrice.isNegative()) throw std::invalid_argument("price must not be negative");
    vendors_["Default"] = unitPrice; 
}

// Q12: Added a new vendor and their specific price
void Resource::addVendor(const std::string& vendorName, Money price) {
    if (price.isNegative()) throw std::invalid_argument("price must not be negative");
    vendors_[vendorName] = price;
}

// Q12: Dynamically calculate and return the cheapest price available
Money Resource::unitPrice() const {
    if (vendors_.empty()) return Money::of(0);
    auto it = std::min_element(vendors_.begin(), vendors_.end(), 
        [](const auto& a, const auto& b) { return a.second < b.second; });
    return it->second;
}

// Q12: Vendor offering the cheapest price
std::string Resource::cheapestVendor() const {
    if (vendors_.empty()) return "Unknown";
    auto it = std::min_element(vendors_.begin(), vendors_.end(), 
        [](const auto& a, const auto& b) { return a.second < b.second; });
    return it->first;
}

void Resource::setUnitPrice(Money price) {
    if (price.isNegative()) throw std::invalid_argument("price must not be negative");
    vendors_["Default"] = price;
}

void Resource::requirePositive(int quantity) {
    if (quantity <= 0) throw std::invalid_argument("quantity must be positive");
}

Money Resource::costFor(int quantity) const {
    requirePositive(quantity);
    return unitPrice() * quantity;
}

void Resource::print(std::ostream& os) const {
    os << categoryName(category()) << " " << id_ << "\n"
       << "  title: " << title_ << "\n"
       << "  publisher: " << publisher_ << "\n"
       << "  year: " << year_ << "\n"
       << "  cheapest price: " << unitPrice() << " (via " << cheapestVendor() << ")\n"
       << "  available vendors: ";
    
    // Print all vendors
    for (auto it = vendors_.begin(); it != vendors_.end(); ++it) {
        if (it != vendors_.begin()) os << ", ";
        os << it->first << " (" << it->second << ")";
    }
    os << "\n";
    printDetails(os);
}

void Resource::printDetails(std::ostream&) const {}

std::string Resource::summary() const {
    std::ostringstream os;
    os << "[" << categoryName(category()) << "] " << id_ << "  " << title_
       << " (" << year_ << ")  @ " << unitPrice(); 
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const Resource& r) {
    r.print(os);
    return os;
}

}  // namespace bookmgmt
