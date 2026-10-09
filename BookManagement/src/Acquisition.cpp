#include <algorithm>
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
            why = budget_.check(r->category(), quantity, cost + tax, id);
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
    budget_.commit(r.category(), quantity, cost + tax, id);  // may throw quota/budget errors
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
            why = budget_.check(r->category(), req.quantity, cost + tax, req.resourceId);
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

const PurchaseRecord& AcquisitionManager::cancel(int orderNo) {
    // 1. Find the original order
    auto it = std::find_if(history_.begin(), history_.end(), 
        [orderNo](const PurchaseRecord& r) { return r.orderNo == orderNo; });
    
    if (it == history_.end()) throw std::invalid_argument("order not found");
    if (!it->approved) throw std::invalid_argument("cannot cancel a rejected order");
    if (it->isCancellation) throw std::invalid_argument("cannot cancel a cancellation record");
    
    // Ensure we haven't already cancelled this order!
    bool alreadyCancelled = std::any_of(history_.begin(), history_.end(), 
        [orderNo](const PurchaseRecord& r) { return r.isCancellation && r.cancelledOrderNo == orderNo; });
    if (alreadyCancelled) throw std::invalid_argument("order already cancelled");

    std::string id = it->resourceId;
    int qty = it->quantity;
    Money cost = it->cost;
    Money tax = it->tax;
    ResourceCategory cat = it->category;

    // 2. Reduce holdings in Catalog
    catalog_.addHoldings(id, -qty);
    
    // 3. Check if holdings dropped to 0 to free up the title quota slot!
    bool removeTitle = (catalog_.holdings(id) == 0);
    
    // 4. Refund budget and quota
    budget_.refund(cat, qty, cost + tax, id, removeTitle);

    // 5. Create cancellation record (invert quantity and cost to naturally balance the totalSpent math)
    PurchaseRecord cancelRec = *it; 
    cancelRec.orderNo = nextOrderNo_++;
    cancelRec.quantity = -qty;
    cancelRec.cost = cost * -1;
    cancelRec.tax = tax * -1;
    cancelRec.isCancellation = true;
    cancelRec.cancelledOrderNo = orderNo;
    
    history_.push_back(cancelRec);
    return history_.back();
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    os << "  #    Status      ID     Qty    Pre-Tax        Tax      Total         Title\n";
    for (const auto& rec : history_) {
        // Q8: Determine exactly what status text to show
        std::string status = rec.isCancellation ? "CANCELLED " : (rec.approved ? "APPROVED  " : "REJECTED  ");
        
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << status << " " << std::setw(6)
           << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(10) << std::right << rec.cost.toString() << "  "
           << std::setw(8) << rec.tax.toString() << "  "
           << std::setw(10) << rec.totalCost().toString() << "  "
           << std::left << rec.title;
           
        // Q8: Show the custom reason based on the record type
        if (!rec.approved && !rec.isCancellation) {
            os << "\n        reason: " << rec.reason;
        } else if (rec.isCancellation) {
            os << "\n        reason: reversed order #" << rec.cancelledOrderNo;
        }
        os << "\n";
    }
    // Negative costs in cancellation records naturally subtract from this!
    os << "Total spent (incl. tax): " << totalSpent() << "\n";
}

}  // namespace bookmgmt
