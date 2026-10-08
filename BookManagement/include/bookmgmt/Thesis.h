#pragma once

#include <string>
#include "bookmgmt/Resource.h"

namespace bookmgmt {

class Thesis : public Resource {
public:
    /*
     * JUSTIFICATION FOR BASE CLASS:
     * We inherit directly from `Resource`. A thesis is a academic document
     * but lacks commercial publishing traits like an ISBN or binding (unlike Book),
     * and has no subscription or licensing fees (unlike ElectronicResource). 
     * Inheriting from `Resource` allows us to cleanly fix the unit price to 0.
     */
    Thesis(std::string id, std::string title, std::string author,
           std::string university, std::string degree, std::string supervisor,
           int year);

    const std::string& author() const { return author_; }
    const std::string& university() const { return university_; }
    const std::string& degree() const { return degree_; }
    const std::string& supervisor() const { return supervisor_; }

    ResourceCategory category() const override { return ResourceCategory::Thesis; }
    bool isDigital() const override { return false; }
    Money costFor(int copies) const override;

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::string author_;
    std::string university_;
    std::string degree_;
    std::string supervisor_;
};

}  // namespace bookmgmt