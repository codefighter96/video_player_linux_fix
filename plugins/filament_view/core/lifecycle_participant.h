/*
 * Copyright 2025 Toyota Connected North America
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

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace plugin_filament_view {

/// Enum that defines the lifecycle states of a participant
enum class LifecycleState {
  /// The participant has never been initialized
  Uninitialized,
  /// The participant has been initialized and is ready for use
  Initialized,
  /// The participant has been destroyed and can no longer be used
  Destroyed
};

template<typename InitParams>
/// Abstract class including the main lifecycle methods
class LifecycleParticipant {
  private:
    LifecycleState _state = LifecycleState::Uninitialized;

  public:
    virtual ~LifecycleParticipant() = default;

    /// Called when the object is initialized
    void initialize(const InitParams& params) {
      if (_state != LifecycleState::Uninitialized) {
        throw std::runtime_error("LifecycleParticipant is already initialized");
      }

      onInitialize(params);
      _state = LifecycleState::Initialized;
    }

    inline bool isInitialized() const { return _state == LifecycleState::Initialized; }

    /// @brief Similar to isInitialized, but throws an exception if not initialized
    /// @throws std::runtime_error if not initialized
    inline void assertInitialized() const {
      if (!isInitialized()) {
        throw std::runtime_error("LifecycleParticipant is not initialized");
      }
    }

    /// Updates the logic of the object for the duration of the frame
    virtual void update(double /*deltaTime*/) = 0;

    void destroy() {
      if (_state != LifecycleState::Initialized) {
        spdlog::warn("LifecycleParticipant is not initialized or already destroyed");
        return;
      }

      onDestroy();
      _state = LifecycleState::Destroyed;
    }

  protected:
    /// Called when the object is initialized. Should set up any resources or state.
    virtual void onInitialize(const InitParams& params) = 0;

    /// Called when the object is destroyed. Should free any memory and resources in use
    virtual void onDestroy() = 0;
};

}  // namespace plugin_filament_view
