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

#ifndef PLUGINS_FLATPAK_CACHE_CACHE_CONFIG_H
#define PLUGINS_FLATPAK_CACHE_CACHE_CONFIG_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace flatpak_plugin {

/**
 * @enum CachePolicy
 * @brief Enumerates cache access strategies.
 * - CACHE_FIRST: Prefer cache, fallback to network if not found.
 * - NETWORK_FIRST: Prefer network, fallback to cache if network fails.
 * - CACHE_ONLY: Use cache exclusively.
 * - NETWORK_ONLY: Use network exclusively.
 */
enum class CachePolicy { CACHE_FIRST, NETWORK_FIRST, CACHE_ONLY, NETWORK_ONLY };

/**
 * @struct CacheConfig
 * @brief Configuration options for cache management.
 * @var db_path Path to the cache database (default: in-memory).
 * @var default_ttl Default time-to-live for cached entries (in seconds).
 * @var policy Cache access strategy.
 * @var enable_compression Enable compression for cached data.
 * @var max_cache_size_mb Maximum cache size in megabytes.
 * @var network_timeout Network operation timeout (in seconds).
 * @var max_retries Maximum number of network retries.
 * @var enable_auto_cleanup Enable automatic cache cleanup.
 * @var cleanup_interval Interval for automatic cleanup (in minutes).
 * @var enable_metrics Enable cache metrics collection.
 */
struct CacheConfig {
  std::string db_path = ":memory:";
  std::chrono::seconds default_ttl{3600};
  CachePolicy policy = CachePolicy::CACHE_FIRST;
  bool enable_compression = false;
  size_t max_cache_size_mb = 100;
  std::chrono::seconds network_timeout{30};
  int max_retries = 3;
  bool enable_auto_cleanup = true;
  std::chrono::minutes cleanup_interval{60};
  bool enable_metrics = true;
};

/**
 * @struct CacheMetrics
 * @brief Stores cache performance metrics and statistics.
 *
 * This structure tracks various metrics related to cache usage, including
 * hit/miss counts, network calls, cache size, expired entries, and network
 * errors. All counters are atomic for thread-safe updates. It also records the
 * cache start time for uptime calculation.
 */
struct CacheMetrics {
  std::atomic<uint64_t> hits{0};
  std::atomic<uint64_t> misses{0};
  std::atomic<uint64_t> network_calls{0};
  std::atomic<uint64_t> cache_size_bytes{0};
  std::atomic<uint64_t> expired_entries{0};
  std::atomic<uint64_t> network_errors{0};

  std::chrono::system_clock::time_point start_time{
      std::chrono::system_clock::now()};

  // Explicitly delete copy operations - force move/reference usage
  CacheMetrics(const CacheMetrics&) = delete;
  CacheMetrics& operator=(const CacheMetrics&) = delete;

  [[nodiscard]] double GetHitRatio() const {
    auto total = hits.load() + misses.load();
    return total > 0 ? (static_cast<double>(hits.load()) * 100.0) / total : 0.0;
  }

  [[nodiscard]] std::chrono::seconds GetUptime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - start_time);
  }
};

}  // namespace flatpak_plugin
#endif  // PLUGINS_FLATPAK_CACHE_CACHE_CONFIG_H