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

#ifndef PLUGINS_FLATPAK_CACHE_CACHE_STORAGE_H
#define PLUGINS_FLATPAK_CACHE_CACHE_STORAGE_H

#include <chrono>
#include <optional>
#include <string>

/**
 * @brief Cache Storage Strategy interface
 *
 * Defines the contract for different cache storage implementations.
 * Currently, supports SQLite, but can be extended for Redis, file-based, etc.
 */
class ICacheStorage {
 public:
  virtual ~ICacheStorage() = default;

  /**
   * @brief Stores data in the cache with an expiration time.
   * @param key The unique identifier for the cached data
   * @param data The data to be stored in the cache
   * @param expiry The time point when the cached data should expire
   * @return true if the data was successfully stored, false otherwise
   */
  virtual bool Store(const std::string& key,
                     const std::string& data,
                     std::chrono::system_clock::time_point expiry) = 0;

  /**
   * @brief Retrieves a value from the cache storage using the specified key.
   * @param key The key string used to look up the cached value
   * @return std::optional<std::string> The cached value if found, std::nullopt
   * otherwise
   */
  virtual std::optional<std::string> Retrieve(const std::string& key) = 0;

  /**
   * @brief Checks if a cache entry has expired based on its key.
   * @param key The unique identifier of the cache entry to check for expiration
   * @return true if the cache entry has expired or doesn't exist, false if
   * still valid
   */
  virtual bool IsExpired(const std::string& key) = 0;

  /**
   * @brief Invalidates cached data entries.
   * @param key The cache key to invalidate. If empty string (default),
   *            all cache entries will be invalidated.
   */
  virtual void Invalidate(const std::string& key) = 0;

  /**
   * @brief Initializes the cache storage system.
   * @return true if initialization was successful, false otherwise
   */
  virtual bool Initialize() = 0;

  /**
   * @brief Gets the current size of the cache.
   * @return The cache size in bytes as a size_t value.
   */
  virtual size_t GetCacheSize() = 0;

  /**
   * @brief Removes all expired cache entries from storage
   * @return The number of cache entries that were removed during cleanup
   */
  virtual size_t CleanupExpired() = 0;
};

#endif  // PLUGINS_FLATPAK_CACHE_CACHE_STORAGE_H