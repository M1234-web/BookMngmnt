#include "bookmgmt/Thesis.h"
#include <ostream>
#include <utility>

namespace bookmgmt {

Thesis::Thesis(std::string id, std::string title, std::string author,
               std::string university, std::string degree, std::string supervisor,
               int year)
    : Resource(std::move(id), std::move(title), university, year, Money::of(0)),
      author_(std::move(author)),
      university_(std::move(university)),
      degree_(std::move(degree)),
      supervisor_(std::move(supervisor)) {}

Money Thesis::costFor(int copies) const {
    requirePositive(copies);
    return Money::of(0); // Theses are free!
}

void Thesis::printDetails(std::ostream& os) const {
    os << "  author: " << author_ << "\n"
       << "  university: " << university_ << "\n"
       << "  degree: " << degree_ << "\n"
       << "  supervisor: " << supervisor_ << "\n";
}

}  // namespace bookmgmt