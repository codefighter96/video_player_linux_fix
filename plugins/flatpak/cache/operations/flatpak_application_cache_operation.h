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

#ifndef PLUGINS_FLATPAK_CACHE_APPLICATION_CACHE_OPERATION_H
#define PLUGINS_FLATPAK_CACHE_APPLICATION_CACHE_OPERATION_H

#include <chrono>
#include "cache_operation_template.h"

/**
 * @brief Cache operation class for managing Flatpak application data with
 * time-to-live functionality.
 *
 * This class extends CacheOperationTemplate to provide specialized caching
 * operations for Flatpak applications. It handles serialization/deserialization
 * of Flutter EncodableList data and implements time-based cache expiration.
 * @tparam flutter::EncodableList The data type being cached (Flatpak
 * application data)
 */
class FlatpakApplicationCacheOperation
    : public CacheOperationTemplate<flutter::EncodableList> {
 private:
  std::chrono::seconds ttl_;

 protected:
  bool ValidateKey(const std::string& key) override;

  std::string SerializeData(const flutter::EncodableList& data) override;

  std::optional<flutter::EncodableList> DeserializeData() override;

  std::chrono::system_clock::time_point GetExpiryTime() override;

  bool ValidateData(const flutter::EncodableList& data) override;

 public:
  explicit FlatpakApplicationCacheOperation(
      std::chrono::seconds ttl = std::chrono::hours(2));
};

#endif  // PLUGINS_FLATPAK_CACHE_APPLICATION_CACHE_OPERATION_H