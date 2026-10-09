#include "bookmgmt/Budget.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
// Every category, in the order Budget::print() lists them.
const ResourceCategory kAllCategories[] = {ResourceCategory::Book,
                                           ResourceCategory::ElectronicResource,
                                           ResourceCategory::Journal,
                                           ResourceCategory::EBook,
                                           ResourceCategory::AudioBook, 
                                           ResourceCategory::Thesis};
}

Budget::Budget(Money total) : total_(total) {
    if (total_.isNegative()) throw std::invalid_argument("budget must not be negative");
}

void Budget::setQuota(ResourceCategory c, Quota q) {
    if (q.maxUnits < 0 || q.maxSpend.isNegative())
        throw std::invalid_argument("quota limits must not be negative");
    quotas_[c] = q;
}

void Budget::removeQuota(ResourceCategory c) { quotas_.erase(c); }

std::optional<Quota> Budget::quotaFor(ResourceCategory c) const {
    auto it = quotas_.find(c);
    if (it == quotas_.end()) return std::nullopt;
    return it->second;
}

Usage Budget::usageFor(ResourceCategory c) const {
    auto it = usage_.find(c);
    return it == usage_.end() ? Usage{} : it->second;
}

std::optional<int> Budget::unitsRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxUnits - usageFor(c).units;
}

std::optional<Money> Budget::spendRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxSpend - usageFor(c).spent;
}

Budget::Failure Budget::evaluate(ResourceCategory c, int units, Money cost, const std::string& id,
                                 std::string& why) const {
    if (units <= 0) {
        why = "quantity must be positive";
        return Failure::BadInput;
    }
    if (cost.isNegative()) {
        why = "cost must not be negative";
        return Failure::BadInput;
    }
    if (auto left = unitsRemaining(c); left && units > *left) {
        why = std::string(categoryName(c)) + " unit quota exceeded: requested " +
              std::to_string(units) + ", " + std::to_string(*left) + " remaining";
        return Failure::Quota;
    }
    if (auto left = spendRemaining(c); left && cost > *left) {
        why = std::string(categoryName(c)) + " spend quota exceeded: cost " +
              cost.toString() + ", " + left->toString() + " remaining";
        return Failure::Quota;
    }
    
    // Q7: Check title limit (only if an ID was provided)
    auto q = quotaFor(c);
    Usage u = usageFor(c);
    if (q && q->maxTitles && !id.empty()) {
        if (u.titles.find(id) == u.titles.end()) {
            if (u.titles.size() >= static_cast<size_t>(*(q->maxTitles))) {
                why = std::string(categoryName(c)) + " title quota exceeded: limit " + 
                      std::to_string(*(q->maxTitles));
                return Failure::Quota;
            }
        }
    }

    if (cost > remaining()) {
        why = "overall budget exceeded: cost " + cost.toString() + ", " +
              remaining().toString() + " remaining";
        return Failure::Overall;
    }
    why.clear();
    return Failure::None;
}

std::string Budget::check(ResourceCategory c, int units, Money cost, const std::string& id) const {
    std::string why;
    evaluate(c, units, cost, id, why);
    return why;
}

void Budget::commit(ResourceCategory c, int units, Money cost, const std::string& id) {
    std::string why;
    switch (evaluate(c, units, cost, id, why)) {
        case Failure::None: break;
        case Failure::BadInput: throw std::invalid_argument(why);
        case Failure::Quota: throw QuotaExceededError(why);
        case Failure::Overall: throw BudgetExceededError(why);
    }
    Usage& u = usage_[c];
    u.units += units;
    u.spent += cost;
    if (!id.empty()) u.titles.insert(id); // Q7: Remember this ID
    spent_ += cost;
}

void Budget::print(std::ostream& os) const {
    os << "Budget: total " << total_ << ", spent " << spent_ << ", remaining "
       << remaining() << "\n";
    // Q7: Added 'Titles used/max' column
    os << std::left << std::setw(22) << "  Category" << std::setw(18) << "Units used/max"
       << std::setw(18) << "Spend used/max" << "Titles used/max\n";
    for (ResourceCategory c : kAllCategories) {
        const Usage u = usageFor(c);
        const auto q = quotaFor(c);
        const std::string units =
            std::to_string(u.units) + "/" + (q ? std::to_string(q->maxUnits) : "-");
        const std::string spend =
            u.spent.toString() + "/" + (q ? q->maxSpend.toString() : "-");
        // Q7: Format the title strings
        const std::string titles = 
            std::to_string(u.titles.size()) + "/" + (q && q->maxTitles ? std::to_string(*(q->maxTitles)) : "-");
            
        os << "  " << std::setw(20) << categoryName(c) << std::setw(18) << units 
           << std::setw(18) << spend << titles << "\n";
    }
}

// Q8: Refund budget and quota limits
void Budget::refund(ResourceCategory c, int units, Money cost, const std::string& id, bool removeTitle) {
    Usage& u = usage_[c];
    u.units -= units;
    u.spent -= cost;
    
    // If holdings drop to zero, free up the title slot!
    if (removeTitle && !id.empty()) {
        u.titles.erase(id);
    }
    
    spent_ -= cost;
}

}  // namespace bookmgmt
