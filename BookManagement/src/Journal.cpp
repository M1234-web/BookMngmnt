#include "bookmgmt/Journal.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

Journal::Journal(std::string id, std::string title, std::string publisher, int year,
                 Money unitPrice, std::string issn, int issuesPerYear, 
                 int subscriptionYears)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, unitPrice),
      issn_(std::move(issn)),
      issuesPerYear_(issuesPerYear),
      subscriptionYears_(subscriptionYears) {
    if (subscriptionYears_ < 1) {
        throw std::invalid_argument("subscription length must be >= 1");
    }
}

Money Journal::costFor(int quantity) const {
    requirePositive(quantity); 
    std::int64_t totalMinorUnits = (unitPrice() * quantity * subscriptionYears_).minorUnits();
    
    // Q5: 10% discount for 10 or more print copies
    if (quantity >= 10) {
        totalMinorUnits = (totalMinorUnits * 90) / 100;
    }
    
    return Money::fromMinor(totalMinorUnits);
}

void Journal::printDetails(std::ostream& os) const {
    os << "  issn: " << issn_ << "\n"
       << "  issues per year: " << issuesPerYear_ << "\n"
       << "  subscription years: " << subscriptionYears_ << "\n";
}

}  // namespace bookmgmt