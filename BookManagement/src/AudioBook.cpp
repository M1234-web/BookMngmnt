#include "bookmgmt/AudioBook.h"
#include <ostream>
#include <utility>

namespace bookmgmt {

AudioBook::AudioBook(std::string id, std::string title, std::vector<std::string> authors,
                     std::string narrator, int durationMinutes,
                     std::string publisher, int year, Money unitPrice)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, unitPrice),
      authors_(std::move(authors)),
      narrator_(std::move(narrator)),
      durationMinutes_(durationMinutes) {
    if (durationMinutes_ <= 0) {
        throw std::invalid_argument("Duration must be strictly positive");
    }
}

Money AudioBook::costFor(int copies) const {
    requirePositive(copies);
    return unitPrice() * copies;
}

void AudioBook::printDetails(std::ostream& os) const {
    os << "  authors: " << joinAuthors(authors_) << "\n"
       << "  narrator: " << narrator_ << "\n"
       << "  duration: " << durationMinutes_ << " minutes\n";
}

}  // namespace bookmgmt