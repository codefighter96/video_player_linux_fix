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

#ifndef PLUGINS_FLATPAK_CACHE_INSTALLATION_CACHE_OPERATION_H
#define PLUGINS_FLATPAK_CACHE_INSTALLATION_CACHE_OPERATION_H

#include <chrono>
#include "cache_operation_template.h"

/**
 * @brief Handles caching operations for Flatpak installations with a specified
 * time-to-live (TTL).
 *
 * This class inherits from CacheOperationTemplate specialized for the
 * Installation type. It provides mechanisms to validate cache keys, serialize
 * and deserialize Installation data, determine expiry times, and validate
 * cached data. The TTL for cached entries can be configured, with a default
 * value of 6 hours.
 */
class FlatpakInstallationCacheOperation
    : public CacheOperationTemplate<Installation> {
 private:
  std::chrono::seconds ttl_;

 protected:
  bool ValidateKey(const std::string& key) override;

  std::string SerializeData(const Installation& data) override;

  std::optional<Installation> DeserializeData() override;

  std::chrono::system_clock::time_point GetExpiryTime() override;

  bool ValidateData(const Installation& data) override;

 public:
  explicit FlatpakInstallationCacheOperation(
      std::chrono::seconds ttl = std::chrono::hours(6));
};

#endif  // PLUGINS_FLATPAK_CACHE_INSTALLATION_CACHE_OPERATION_H