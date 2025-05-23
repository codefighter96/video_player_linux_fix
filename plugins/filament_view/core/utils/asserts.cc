#include <core/utils/asserts.h>

#include <cassert>
#include <stdexcept>
#include <string>

namespace plugin_filament_view {

/// Convenience function to assert a condition at runtime.
/// Throws a runtime_error with a given message if the condition is false.
/// TODO: refactor every `if(x != nullptr) throw ...` to use this
void runtime_assert(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace plugin_filament_view
