#include "bookmgmt/Acquisition.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

int AcquisitionManager::printTaxRatePercent = 0;
int AcquisitionManager::electronicTaxRatePercent = 0;

Money AcquisitionManager::calculateTax(ResourceCategory c, Money baseCost) {
    int rate = 0;
    if (c == ResourceCategory::ElectronicResource || 
        c == ResourceCategory::EBook || 
        c == ResourceCategory::AudioBook) {
        rate = electronicTaxRatePercent;
    } else {
        rate = printTaxRatePercent;
    }
    std::int64_t taxMinor = (baseCost.minorUnits() * rate) / 100;
    return Money::fromMinor(taxMinor);
}

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget)
    : catalog_(catalog), budget_(budget) {}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    return catalog_.get(id).costFor(quantity);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0)
            why = "quantity must be positive";
        else {
            Money cost = r->costFor(quantity); 
            Money tax = calculateTax(r->category(), cost); 
            why = budget_.check(r->category(), quantity, cost + tax);
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, Money cost, Money tax, bool approved,
                                           std::string reason) {
    history_.push_back(PurchaseRecord{
        nextOrderNo_++, id, r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, cost, tax, approved,
        std::move(reason)});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    const Money cost = r.costFor(quantity);      // may throw invalid_argument
    const Money tax = calculateTax(r.category(), cost);
    budget_.commit(r.category(), quantity, cost+tax);  // may throw quota/budget errors
    catalog_.addHoldings(id, quantity);
    return record(&r, id, quantity, cost, tax, true, {});
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs) {
    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());
    for (const auto& req : reqs) {
        const Resource* r = catalog_.find(req.resourceId);
        Money cost;
        Money tax;
        std::string why;
        if (!r) {
            why = "resource not found: " + req.resourceId;
        } else if (req.quantity <= 0) {
            why = "quantity must be positive";
        } else {
            cost = r->costFor(req.quantity);
            tax = calculateTax(r->category(), cost);
            why = budget_.check(r->category(), req.quantity, cost+tax);
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity));
        } else {
            results.push_back(record(r, req.resourceId, req.quantity, cost, tax, false, why));
        }
    }
    return results;
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        //if (rec.approved) sum += rec.cost;
        if (rec.approved) sum += rec.totalCost(); // Included tax as well
    return sum;
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    // Q6: Updated header to include Pre-tax and Tax columns
    os << "  #    Status      ID     Qty    Pre-Tax        Tax      Total         Title\n";
    for (const auto& rec : history_) {
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << (rec.approved ? "APPROVED  " : "REJECTED  ") << " " << std::setw(6)
           << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(10) << std::right << rec.cost.toString() << "  "
           << std::setw(8) << rec.tax.toString() << "  "
           << std::setw(10) << rec.totalCost().toString() << "  "
           << std::left << rec.title;
        if (!rec.approved) os << "\n        reason: " << rec.reason;
        os << "\n";
    }
    os << "Total spent (incl. tax): " << totalSpent() << "\n";
}

}  // namespace bookmgmt
