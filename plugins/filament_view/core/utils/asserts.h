#pragma once

#include <string>

namespace plugin_filament_view {

/// Convenience function to assert a condition at runtime.
/// Throws a runtime_error with a given message if the condition is false.
void runtime_assert(bool condition, const char* message);

inline void runtime_assert(bool condition, const std::string& message) {
  runtime_assert(condition, message.c_str());
}

#define debug_assert(condition, message) \
    assert(condition && message);

} // namespace plugin_filament_view
