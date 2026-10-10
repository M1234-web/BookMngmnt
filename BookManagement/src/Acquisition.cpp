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
    : catalog_(catalog), budget_(budget) {
    budgets_["Main"] = &budget; // Q9: Register the original budget as "Main"
}

void AcquisitionManager::addDepartment(const std::string& deptName, Budget& b) {
    budgets_[deptName] = &b;
}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    return catalog_.get(id).costFor(quantity);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    return canPurchase(id, quantity, "Main", reason);
}

// Q9: Department-aware canPurchase
bool AcquisitionManager::canPurchase(const std::string& id, int quantity, 
                                     const std::string& dept, std::string* reason) const {
    std::string why;
    auto it = budgets_.find(dept);
    if (it == budgets_.end()) {
        why = "department not found: " + dept;
    } else if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0) {
            why = "quantity must be positive";
        } else {
            Money cost = r->costFor(quantity); 
            Money tax = calculateTax(r->category(), cost); 
            why = it->second->check(r->category(), quantity, cost + tax, id);
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, Money cost, Money tax, bool approved,
                                           std::string reason, const std::string& dept) {
    // Determine the vendor dynamically
    std::string vendorName = r ? r->cheapestVendor() : "Unknown";

    history_.push_back(PurchaseRecord{
        nextOrderNo_++, id, r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, cost, tax, approved,
        std::move(reason), false, -1, dept, vendorName});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity, const std::string& dept) {
    auto it = budgets_.find(dept);
    if (it == budgets_.end()) throw std::invalid_argument("department not found: " + dept);
    Budget* targetBudget = it->second;

    const Resource& r = catalog_.get(id);        
    const Money cost = r.costFor(quantity); 
    const Money tax = calculateTax(r.category(), cost); 
    
    targetBudget->commit(r.category(), quantity, cost + tax, id); 
    catalog_.addHoldings(id, quantity);
    
    return record(&r, id, quantity, cost, tax, true, "", dept);
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs, bool allOrNothing) {
    
    if (!allOrNothing) {
        // --- ORIGINAL BATCH LOGIC (Processes independently) ---
        std::vector<PurchaseRecord> results;
        results.reserve(reqs.size());
        for (const auto& req : reqs) {
            const Resource* r = catalog_.find(req.resourceId);
            Money cost;
            Money tax; 
            std::string why;
            
            auto it = budgets_.find(req.department);
            Budget* targetBudget = (it != budgets_.end()) ? it->second : nullptr;

            if (!r) {
                why = "resource not found: " + req.resourceId;
            } else if (!targetBudget) {
                why = "department not found: " + req.department;
            } else if (req.quantity <= 0) {
                why = "quantity must be positive";
            } else {
                cost = r->costFor(req.quantity); 
                tax = calculateTax(r->category(), cost); 
                why = targetBudget->check(r->category(), req.quantity, cost + tax, req.resourceId); 
            }

            if (why.empty()) {
                results.push_back(purchase(req.resourceId, req.quantity, req.department));
            } else {
                results.push_back(record(r, req.resourceId, req.quantity, cost, tax, false, why, req.department));
            }
        }
        return results;
        
    } else {
        // --- Q11: ALL-OR-NOTHING TRANSACTION LOGIC ---
        struct CommitLog {
            std::string id; int qty; ResourceCategory cat; Money cost; Money tax; std::string dept; bool wasZero;
        };
        std::vector<CommitLog> commits;
        bool failed = false;
        std::string firstFailureWhy;

        for (const auto& req : reqs) {
            const Resource* r = catalog_.find(req.resourceId);
            Money cost; Money tax; std::string why;
            
            auto it = budgets_.find(req.department);
            Budget* targetBudget = (it != budgets_.end()) ? it->second : nullptr;

            if (!r) why = "resource not found: " + req.resourceId;
            else if (!targetBudget) why = "department not found: " + req.department;
            else if (req.quantity <= 0) why = "quantity must be positive";
            else {
                cost = r->costFor(req.quantity);
                tax = calculateTax(r->category(), cost);
                why = targetBudget->check(r->category(), req.quantity, cost + tax, req.resourceId);
            }

            if (!why.empty()) {
                failed = true;
                firstFailureWhy = why; 
                break; // Stop immediately on first failure
            }

            // Temporarily commit so cumulative budget checks inside the same batch work correctly
            bool wasZero = (catalog_.holdings(req.resourceId) == 0);
            targetBudget->commit(r->category(), req.quantity, cost + tax, req.resourceId);
            catalog_.addHoldings(req.resourceId, req.quantity);
            commits.push_back({req.resourceId, req.quantity, r->category(), cost, tax, req.department, wasZero});
        }

        std::vector<PurchaseRecord> results;
        results.reserve(reqs.size());

        if (failed) {
            // 1. Rollback all temporary commits in reverse order
            for (auto it = commits.rbegin(); it != commits.rend(); ++it) {
                catalog_.addHoldings(it->id, -(it->qty));
                bool removeTitle = it->wasZero && (catalog_.holdings(it->id) == 0);
                budgets_[it->dept]->refund(it->cat, it->qty, it->cost + it->tax, it->id, removeTitle);
            }
            
            // 2. Generate rejected records for the whole batch, showing the error that killed it
            std::string rejectReason = "batch failed: " + firstFailureWhy;
            for (const auto& req : reqs) {
                const Resource* r = catalog_.find(req.resourceId);
                Money cost; Money tax;
                if (r && req.quantity > 0) {
                    cost = r->costFor(req.quantity);
                    tax = calculateTax(r->category(), cost);
                }
                results.push_back(record(r, req.resourceId, req.quantity, cost, tax, false, rejectReason, req.department));
            }
        } else {
            // All passed! Just write them to the permanent history
            for (size_t i = 0; i < reqs.size(); ++i) {
                const auto& req = reqs[i];
                const auto& c = commits[i];
                const Resource* r = catalog_.find(req.resourceId);
                results.push_back(record(r, req.resourceId, req.quantity, c.cost, c.tax, true, "", req.department));
            }
        }
        return results;
    }
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        //if (rec.approved) sum += rec.cost;
        if (rec.approved) sum += rec.totalCost(); // Included tax as well
    return sum;
}

const PurchaseRecord& AcquisitionManager::cancel(int orderNo) {
    auto it = std::find_if(history_.begin(), history_.end(), 
        [orderNo](const PurchaseRecord& r) { return r.orderNo == orderNo; });
    
    if (it == history_.end()) throw std::invalid_argument("order not found");
    if (!it->approved) throw std::invalid_argument("cannot cancel a rejected order");
    if (it->isCancellation) throw std::invalid_argument("cannot cancel a cancellation record");
    
    bool alreadyCancelled = std::any_of(history_.begin(), history_.end(), 
        [orderNo](const PurchaseRecord& r) { return r.isCancellation && r.cancelledOrderNo == orderNo; });
    if (alreadyCancelled) throw std::invalid_argument("order already cancelled");

    std::string id = it->resourceId;
    int qty = it->quantity;
    Money cost = it->cost;
    Money tax = it->tax;
    ResourceCategory cat = it->category;
    std::string dept = it->department; // Grab original dept

    catalog_.addHoldings(id, -qty);
    bool removeTitle = (catalog_.holdings(id) == 0);
    
    // Refund the CORRECT department
    auto budgetIt = budgets_.find(dept);
    if (budgetIt != budgets_.end()) {
        budgetIt->second->refund(cat, qty, cost + tax, id, removeTitle);
    }

    PurchaseRecord cancelRec = *it; 
    cancelRec.orderNo = nextOrderNo_++;
    cancelRec.quantity = -qty;
    cancelRec.cost = cost * -1;
    cancelRec.tax = tax * -1;
    cancelRec.isCancellation = true;
    cancelRec.cancelledOrderNo = orderNo;
    // department carries over from the copied record!
    
    history_.push_back(cancelRec);
    return history_.back();
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    os << "  #    Status      ID     Qty    Pre-Tax        Tax      Total       Dept        Vendor           Title\n";
    for (const auto& rec : history_) {
        std::string status = rec.isCancellation ? "CANCELLED " : (rec.approved ? "APPROVED  " : "REJECTED  ");
        
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << status << " " << std::setw(6)
           << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(10) << std::right << rec.cost.toString() << "  "
           << std::setw(8) << rec.tax.toString() << "  "
           << std::setw(10) << rec.totalCost().toString() << "  "
           << std::setw(10) << std::left << rec.department << "  "
           << std::setw(15) << std::left << rec.vendor << "  " // Q12: Print Vendor
           << std::left << rec.title;
           
        if (!rec.approved && !rec.isCancellation) {
            os << "\n        reason: " << rec.reason;
        } else if (rec.isCancellation) {
            os << "\n        reason: reversed order #" << rec.cancelledOrderNo;
        }
        os << "\n";
    }
    os << "Total spent (incl. tax): " << totalSpent() << "\n";
}

}  // namespace bookmgmt
