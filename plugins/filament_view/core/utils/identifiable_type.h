/*
 * Copyright 2020-2024 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <cxxabi.h>

/// @brief TypeID represents the ID of a type.
using TypeID = size_t;

/** Base class that keeps track of subclass IDs and provides a way to
 * identify them by type ID at runtime.
 * Simplified implementation that doesn't require subclasses to override
 * anything
 */
class IdentifiableType {
  public:
    IdentifiableType() = default;
    virtual ~IdentifiableType() = default;

    /** Returns the name of the type as a string. */
    [[nodiscard]] virtual std::string GetTypeName() {
      // Set the type name
      if (typeName_.empty()) {
        // RTTI type name
        // This is a compiler-specific way to get the type name
        const char* name = typeid(*this).name();

        // Demangle the name if possible
        int status = -4;
        char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        if (status == 0) {
          name = demangled;
        }

        typeName_ = std::string(name);
      }

      return typeName_;
    }

    [[nodiscard]] virtual size_t GetTypeID() const { return typeid(*this).hash_code(); }

    template<typename T> [[nodiscard]] static size_t StaticGetTypeID() {
      return typeid(T).hash_code();
    }

  private:
    std::string typeName_;
};
