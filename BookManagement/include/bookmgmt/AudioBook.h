#pragma once

#include <string>
#include <vector>
#include "bookmgmt/Resource.h"
#include "bookmgmt/Book.h" // Included to reuse the joinAuthors helper

namespace bookmgmt {

class AudioBook : public Resource {
public:
    /*
     * JUSTIFICATION FOR BASE CLASS:
     * We inherit directly from `Resource` rather than `Book` or `ElectronicResource`. 
     * Audiobooks lack physical `Binding` properties (unlike Book) and do not 
     * necessarily follow the seat-based, URL-accessed licensing model of 
     * ElectronicResource. `Resource` provides the perfect common baseline.
     */
    AudioBook(std::string id, std::string title, std::vector<std::string> authors,
              std::string narrator, int durationMinutes,
              std::string publisher, int year, Money unitPrice);

    const std::vector<std::string>& authors() const { return authors_; }
    const std::string& narrator() const { return narrator_; }
    int durationMinutes() const { return durationMinutes_; }

    ResourceCategory category() const override { return ResourceCategory::AudioBook; }
    bool isDigital() const override { return true; } 
    Money costFor(int copies) const override;

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::vector<std::string> authors_;
    std::string narrator_;
    int durationMinutes_;
};

}  // namespace bookmgmt