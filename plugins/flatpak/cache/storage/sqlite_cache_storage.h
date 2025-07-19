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

#ifndef PLUGINS_FLATPAK_CACHE_SQLITE_CACHE_STORAGE_H
#define PLUGINS_FLATPAK_CACHE_SQLITE_CACHE_STORAGE_H

#include "../interfaces/cache_storage.h"
#include <sqlite3.h>
#include <cstddef>
#include <map>
#include <mutex>
#include <atomic>
#include <spdlog/spdlog.h>
#include <zconf.h>
#include <zlib.h>
#include <chrono>
#include <cstdint>
#include <optional>



/**
 * @brief Implements a cache storage backend using SQLite as the underlying database.
 *
 * This class provides persistent cache storage functionality by leveraging SQLite.
 * It supports optional data compression, thread-safe operations, and cache size management.
 */
class SQLiteCacheStorage: public ICacheStorage {
private:
    sqlite3* db_;
    std::string db_path_;
    mutable std::mutex db_mutex_;
    std::atomic<size_t> cache_size{0};
    bool enable_compression_;

    bool CreateTables();

    std::string CompressData(const std::string& data);

    std::string DecompressData(const std::string& data);

    void UpdateCacheSize();

public:
    explicit SQLiteCacheStorage(std::string db_path, bool enable_compression = false);

    ~SQLiteCacheStorage();
    bool Initialize() override;

    bool Store(const std::string& key,
                     const std::string& data,
                     std::chrono::system_clock::time_point expiry) override;
                    
    std::optional<std::string> Retrieve(const std::string& key) override;

    bool IsExpired(const std::string& key) override;

    void Invalidate(const std::string& key = "") override;
    
    size_t GetCacheSize() override;

    size_t CleanupExpired() override;
    
    /**
    * @brief Retrieves cache statistics such as entry count and total size.
    * @return A map of statistic names to their values.
    */
    std::map<std::string, int64_t> GetStatistics() const;
};

#endif // PLUGINS_FLATPAK_CACHE_SQLITE_CACHE_STORAGE_H