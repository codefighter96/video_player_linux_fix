/*
 * Copyright 2023-2025 Toyota Connected North America
 * Copyright 2025 Ahmed Wafdy
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

#ifndef PLUGINS_FLATPAK_CACHE_CACHE_OBSERVER_H
#define PLUGINS_FLATPAK_CACHE_CACHE_OBSERVER_H

#include <string>

/**
 * @brief Interface for observing cache operations and events.
 *
 * Provides hooks for monitoring cache operations, collecting metrics,
 * and implementing custom behaviors on cache events.
 */
class ICacheObserver {
 public:
  virtual ~ICacheObserver() = default;

  /**
   * @brief Callback invoked when a cache hit occurs.
   * @param key The cache key that was successfully found
   * @param data_size The size in bytes of the cached data that was retrieved
   */
  virtual void OnCacheHit(const std::string& key, size_t data_size) = 0;

  /**
   * @brief Called when a cache lookup fails to find the requested key.
   * @param key The cache key that was not found in the cache
   */
  virtual void OnCacheMiss(const std::string& key) = 0;

  /**
   * @brief Called when a cache entry has expired and needs to be removed or
   * refreshed.
   * @param key The unique identifier of the cache entry that has expired
   */
  virtual void OnCacheExpired(const std::string& key) = 0;

  /**
   * @brief Called when the system falls back to network operation due to cache
   * failure
   * @param reason A string describing the reason for the network fallback, such
   * as cache miss, corruption, or other cache-related errors
   */
  virtual void OnNetworkFallback(const std::string& reason) = 0;

  /**
   * @brief Called when a network error occurs during cache operations
   * @param url The URL that caused the network error
   * @param error_code The specific error code indicating the type of network
   * failure (e.g., HTTP status codes, connection timeouts, DNS resolution
   * failures)
   */
  virtual void OnNetworkError(const std::string& url, long error_code) = 0;

  /**
   * @brief Called when cache cleanup operation completes
   * @param entries_cleaned The number of cache entries that were successfully
   *                       removed during the cleanup operation
   */
  virtual void OnCacheCleanup(size_t entries_cleaned) = 0;
};

#endif  // PLUGINS_FLATPAK_CACHE_CACHE_OBSERVER_H