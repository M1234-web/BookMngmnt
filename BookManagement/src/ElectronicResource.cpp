#include "bookmgmt/ElectronicResource.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

const char* licenseName(LicenseModel m) {
    switch (m) {
        case LicenseModel::Perpetual: return "Perpetual";
        case LicenseModel::AnnualSubscription: return "Annual subscription";
    }
    return "Unknown";
}

ElectronicResource::ElectronicResource(std::string id, std::string title,
                                       std::string publisher, int year,
                                       Money pricePerSeat, std::string accessUrl,
                                       LicenseModel license, Money platformFee)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, pricePerSeat),
      accessUrl_(std::move(accessUrl)),
      license_(license),
      platformFee_(platformFee) {
    if (platformFee_.isNegative())
        throw std::invalid_argument("platform fee must not be negative");
}

Money ElectronicResource::costFor(int seats) const {
    requirePositive(seats);
    
    std::int64_t totalMinorUnits = platformFee_.minorUnits();
    
    if (seats <= 50) {
        // 50 or fewer seats: Full price
        totalMinorUnits += (unitPrice() * seats).minorUnits();
    } else {
        // Over 50 seats: First 50 are full price, the rest are half price
        totalMinorUnits += (unitPrice() * 50).minorUnits();
        
        std::int64_t extraSeats = seats - 50;
        totalMinorUnits += ((unitPrice() * extraSeats).minorUnits()) / 2;
    }
    
    return Money::fromMinor(totalMinorUnits);
}

void ElectronicResource::printDetails(std::ostream& os) const {
    os << "  access url: " << accessUrl_ << "\n"
       << "  license: " << licenseName(license_) << "\n"
       << "  platform fee: " << platformFee_ << "\n";
}

}  // namespace bookmgmt
