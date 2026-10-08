#pragma once

#include <string>
#include <vector>

#include "bookmgmt/ElectronicResource.h"
// Include Book.h to reuse the joinAuthors utility function
#include "bookmgmt/Book.h" 

namespace bookmgmt {

enum class FileFormat { PDF, EPUB, HTML };

class EBook : public ElectronicResource {
public:
    EBook(std::string id, std::string title, std::vector<std::string> authors,
          std::string isbn, std::string publisher, int year, Money pricePerSeat,
          std::string accessUrl, FileFormat format, bool drmProtected,
          LicenseModel license = LicenseModel::AnnualSubscription,
          Money platformFee = Money{});

    const std::vector<std::string>& authors() const { return authors_; }
    const std::string& isbn() const { return isbn_; }
    FileFormat format() const { return format_; }
    bool drmProtected() const { return drmProtected_; }

    ResourceCategory category() const override { return ResourceCategory::EBook; }

    /*
     * Q2 WRITTEN ANSWER: 
     * The code duplicated between Book and EBook includes the `authors_` and `isbn_` 
     * fields, their constructor parameters, their getter methods, and the printing 
     * logic for them.
     * 
     * How to avoid it: 
     * We could use composition by moving these shared fields into a separate struct 
     * (e.g., `struct PublicationDetails`) and including it as a member variable in 
     * both `Book` and `EBook`. Alternatively, we could create an intermediate abstract 
     * base class (e.g., `AuthoredResource`) that inherits from `Resource`, which 
     * both `Book` and a new branch of `ElectronicResource` could inherit from.
     */

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::vector<std::string> authors_;
    std::string isbn_;
    FileFormat format_;
    bool drmProtected_;
};

const char* fileFormatName(FileFormat format);

}  // namespace bookmgmt