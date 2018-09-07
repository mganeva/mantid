#include "MantidQtWidgets/MplCpp/Artist.h"

namespace MantidQt {
namespace Widgets {
namespace MplCpp {

/**
 * @brief Create an Artist instance around an existing matplotlib Artist
 * @param obj A Python object pointing to a matplotlib artist
 * @throws std::invalid_argument if the type does not look like an Artist
 */
Artist::Artist(Python::Object obj) : InstanceHolder(std::move(obj)) {
  // Not a thorough instance check but should be good enough
  if (PyObject_HasAttrString(pyobj().ptr(), "remove") == 0) {
    throw std::invalid_argument(
        "object has no attribute 'remove. An Artist object was expected.");
  }
}

/**
 * Call .remove on the underlying artist
 */
void Artist::remove() { pyobj().attr("remove")(); }

} // namespace MplCpp
} // namespace Widgets
} // namespace MantidQt
