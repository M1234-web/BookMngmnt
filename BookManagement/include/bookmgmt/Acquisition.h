#pragma once
// AcquisitionManager: turns purchase requests into orders, enforcing the
// Budget's quotas, updating Catalog holdings and keeping an order history.

#include <iosfwd>
#include <string>
#include <vector>
#include <map>

#include "bookmgmt/Budget.h"
#include "bookmgmt/Catalog.h"

namespace bookmgmt {

struct PurchaseRequest {
    std::string resourceId;
    int quantity;  // copies for print, seats for electronic
    std::string department = "Main";
};

struct PurchaseRecord {
    int orderNo;
    std::string resourceId;
    std::string title;
    ResourceCategory category;
    int quantity;
    Money cost;
    Money tax;
    bool approved;
    std::string reason;  // why it was rejected; empty if approved

    // Q8: Tracking cancellations safely
    bool isCancellation = false;
    int cancelledOrderNo = -1;
    std::string department = "Main";

    std::string vendor = "Default"; // Q12: Track which vendor was used

    //total billed amount
    Money totalCost() const { return cost + tax; }
};

class AcquisitionManager {
public:
    AcquisitionManager(Catalog& catalog, Budget& budget);

    void addDepartment(const std::string& deptName, Budget& budget);

    static int printTaxRatePercent;
    static int electronicTaxRatePercent;

    //Calculating tax for eac category
    static Money calculateTax(ResourceCategory c, Money preTaxCost);

    const PurchaseRecord& cancel(int orderNo);

    // Price of a request without buying anything. Throws NotFoundError.
    Money quote(const std::string& id, int quantity) const;

    // True if the purchase would be approved; if not, `reason` explains why.
    bool canPurchase(const std::string& id, int quantity,
                     std::string* reason = nullptr) const;
    bool canPurchase(const std::string& id, int quantity, const std::string& dept, std::string* reason = nullptr) const;

    // Buys immediately. Throws NotFoundError, QuotaExceededError,
    // BudgetExceededError or std::invalid_argument. On success the budget
    // and holdings are updated and the record is added to history.
    const PurchaseRecord& purchase(const std::string& id, int quantity, const std::string& dept = "Main");

    // Processes requests in order; each is approved or rejected on its own
    // (never throws for a rejected request). Every outcome is recorded.
    // EXTENSION POINT: priority ordering, all-or-nothing batches, ...
    std::vector<PurchaseRecord> processBatch(const std::vector<PurchaseRequest>& reqs, bool allOrNothing = false);

    const std::vector<PurchaseRecord>& history() const { return history_; }
    Money totalSpent() const;

    void printReport(std::ostream& os) const;

private:
    PurchaseRecord& record(const Resource* r, const std::string& id, int qty,
                           Money cost, Money tax, bool approved, std::string reason, const std::string& dept);

    Catalog& catalog_;
    Budget& budget_;
    std::vector<PurchaseRecord> history_;

    std::map<std::string, Budget*> budgets_;
    int nextOrderNo_ = 1;
};

}  // namespace bookmgmt
