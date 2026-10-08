#include "bookmgmt/EBook.h"

#include <ostream>
#include <utility>

namespace bookmgmt {

const char* fileFormatName(FileFormat format) {
    switch (format) {
        case FileFormat::PDF: return "PDF";
        case FileFormat::EPUB: return "EPUB";
        case FileFormat::HTML: return "HTML";
    }
    return "Unknown";
}

EBook::EBook(std::string id, std::string title, std::vector<std::string> authors,
             std::string isbn, std::string publisher, int year, Money pricePerSeat,
             std::string accessUrl, FileFormat format, bool drmProtected,
             LicenseModel license, Money platformFee)
    : ElectronicResource(std::move(id), std::move(title), std::move(publisher), year,
                         pricePerSeat, std::move(accessUrl), license, platformFee),
      authors_(std::move(authors)),
      isbn_(std::move(isbn)),
      format_(format),
      drmProtected_(drmProtected) {}

void EBook::printDetails(std::ostream& os) const {
    // 1. Call the parent class's version first to print accessUrl, license, etc.
    ElectronicResource::printDetails(os);
    
    // 2. Print the new EBook specific fields
    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  isbn: " << isbn_ << "\n"
       << "  format: " << fileFormatName(format_) << "\n"
       << "  drm-protected: " << (drmProtected_ ? "Yes" : "No") << "\n";
}

}  // namespace bookmgmt