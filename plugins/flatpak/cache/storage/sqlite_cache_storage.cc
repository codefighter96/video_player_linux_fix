#include "sqlite_cache_storage.h"
#include <zlib.h>
#include <cstddef>

SQLiteCacheStorage::SQLiteCacheStorage(std::string db_path,
                                       bool enable_compression)
    : db_(nullptr),
      db_path_(std::move(db_path)),
      enable_compression_(enable_compression) {}

SQLiteCacheStorage::~SQLiteCacheStorage() {
  if (db_) {
    sqlite3_close(db_);
  }
}

bool SQLiteCacheStorage::Initialize() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  int rc = sqlite3_open(db_path_.c_str(), &db_);
  if (rc != SQLITE_OK) {
    spdlog::error("Error while opening DB : {}", sqlite3_errmsg(db_));
    return false;
  }

  rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to enable WAL mode : {}",
                  sqlite3_errmsg(db_));
  }

  rc = sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr,
                    nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error(
        "[SQLiteCacheStorage] Failed to enable synchronous mode for DB : {}",
        sqlite3_errmsg(db_));
  }

  rc = sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error(
        "[SQLiteCacheStorage] Failed to enable foreign keys for DB : {}",
        sqlite3_errmsg(db_));
  }

  if (!CreateTables()) {
    spdlog::error("error while creating tables");
    sqlite3_close(db_);
    throw std::runtime_error("failed to create database schema");
  }

  UpdateCacheSize();
  return true;
}

bool SQLiteCacheStorage::Store(const std::string& key,
                               const std::string& data,
                               std::chrono::system_clock::time_point expiry) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  std::string processed_data = data;
  bool is_compressed = false;

  if (enable_compression_) {
    std::string compressed_data = CompressData(data);
    if (compressed_data.size() < data.size()) {
      processed_data = compressed_data;
      is_compressed = true;
    }
  }

  auto expiry_time = std::chrono::duration_cast<std::chrono::seconds>(
                         expiry.time_since_epoch())
                         .count();
  auto created_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  const char* insert_sql = R"(
        INSERT OR REPLACE INTO cache_entries 
        (key, data, expiry_time, created_time, data_size, is_compressed) 
        VALUES (?, ?, ?, ?, ?, ?);
    )";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to prepare statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return false;
  }

  sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_blob(stmt, 2, processed_data.c_str(),
                    static_cast<int>(processed_data.size()), SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, expiry_time);
  sqlite3_bind_int64(stmt, 4, created_time);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(data.size()));
  sqlite3_bind_int(stmt, 6, is_compressed ? 1 : 0);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc != SQLITE_DONE) {
    spdlog::error("[SQLiteCacheStorage] Failed to execute statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return false;
  }

  UpdateCacheSize();
  return true;
}

std::optional<std::string> SQLiteCacheStorage::Retrieve(
    const std::string& key) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* select_sql = R"(
        SELECT data, expiry_time, is_compressed 
        FROM cache_entries 
        WHERE key = ?;
    )";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to prepare statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

  std::optional<std::string> result = std::nullopt;

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    int64_t expiry_time = sqlite3_column_int64(stmt, 1);

    if (current_time < expiry_time) {
      const void* data = sqlite3_column_blob(stmt, 0);
      int data_size = sqlite3_column_bytes(stmt, 0);
      bool is_compressed = sqlite3_column_int(stmt, 2) != 0;

      std::string raw_data(static_cast<const char*>(data),
                           static_cast<size_t>(data_size));

      if (is_compressed) {
        std::string decompressed = DecompressData(raw_data);
        if (!decompressed.empty()) {
          result = decompressed;
        } else {
          spdlog::error(
              "[SQLiteCacheStorage] Failed to decompress data for key: {}",
              key);
        }
      } else {
        result = raw_data;
      }
    }
  } else if (rc != SQLITE_DONE) {
    spdlog::error("[SQLiteCacheStorage] Failed to execute select : {} ({})",
                  sqlite3_errmsg(db_), rc);
  }

  sqlite3_finalize(stmt);
  return result;
}

bool SQLiteCacheStorage::IsExpired(const std::string& key) {
  std::lock_guard<std::mutex> lock(db_mutex_);

  const char* select_sql =
      "SELECT expiry_time FROM cache_entries WHERE key = ?;";
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, select_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to prepare statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return true;
  }

  sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

  bool expired = true;

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    int64_t expiry_time = sqlite3_column_int64(stmt, 0);
    expired = current_time >= expiry_time;
  } else if (rc != SQLITE_DONE) {
    spdlog::error("[SQLiteCacheStorage] Failed to execute select : {} ({})",
                  sqlite3_errmsg(db_), rc);
  }

  sqlite3_finalize(stmt);
  return expired;
}

void SQLiteCacheStorage::Invalidate(const std::string& key) {
  std::lock_guard lock(db_mutex_);

  const char* delete_sql;
  sqlite3_stmt* stmt;
  int rc;

  if (key.empty()) {
    // Delete all entries
    delete_sql = "DELETE FROM cache_entries;";
    rc = sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
  } else {
    // Delete specific entry
    delete_sql = "DELETE FROM cache_entries WHERE key = ?;";
    rc = sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    }
  }

  if (rc == SQLITE_OK) {
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);

  UpdateCacheSize();
}

size_t SQLiteCacheStorage::GetCacheSize() {
  return cache_size.load();
}

size_t SQLiteCacheStorage::CleanupExpired() {
  std::lock_guard<std::mutex> lock(db_mutex_);

  auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  const char* delete_sql = "DELETE FROM cache_entries WHERE expiry_time <= ?;";
  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to prepare statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return 0;
  }

  sqlite3_bind_int64(stmt, 1, current_time);

  rc = sqlite3_step(stmt);
  size_t deleted_count = 0;

  if (rc == SQLITE_DONE) {
    deleted_count = static_cast<size_t>(sqlite3_changes(db_));
  } else {
    spdlog::error("[SQLiteCacheStorage] Failed to execute delete : {} ({})",
                  sqlite3_errmsg(db_), rc);
  }

  sqlite3_finalize(stmt);

  if (deleted_count > 0) {
    UpdateCacheSize();
  }

  return deleted_count;
}

std::map<std::string, int64_t> SQLiteCacheStorage::GetStatistics() const {
  std::lock_guard<std::mutex> lock(db_mutex_);

  std::map<std::string, int64_t> stats;

  const char* stats_sql = R"(
        SELECT 
            COUNT(*) as entry_count,
            SUM(data_size) as total_size,
            AVG(data_size) as avg_size,
            SUM(CASE WHEN is_compressed = 1 THEN 1 ELSE 0 END) as compressed_count
        FROM cache_entries;
    )";

  sqlite3_stmt* stmt;
  int rc = sqlite3_prepare_v2(db_, stats_sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] Failed to prepare statement : {} ({})",
                  sqlite3_errmsg(db_), rc);
    return stats;
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    stats["entries"] = sqlite3_column_int64(stmt, 0);
    stats["total_size"] = sqlite3_column_int64(stmt, 1);
    stats["avg_size"] = sqlite3_column_int64(stmt, 2);
    stats["compressed_count"] = sqlite3_column_int64(stmt, 3);
  } else if (rc != SQLITE_DONE) {
    spdlog::error(
        "[SQLiteCacheStorage] Failed to execute stats query : {} ({})",
        sqlite3_errmsg(db_), rc);
  }

  sqlite3_finalize(stmt);

  auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  const char* expired_sql =
      "SELECT COUNT(*) FROM cache_entries WHERE expiry_time <= ?;";
  rc = sqlite3_prepare_v2(db_, expired_sql, -1, &stmt, nullptr);
  if (rc == SQLITE_OK) {
    sqlite3_bind_int64(stmt, 1, current_time);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      stats["expired_count"] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }

  return stats;
}

bool SQLiteCacheStorage::CreateTables() const {
  const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS cache_entries (
            key TEXT PRIMARY KEY,
            data BLOB NOT NULL,
            expiry_time INTEGER NOT NULL,
            created_time INTEGER NOT NULL,
            data_size INTEGER NOT NULL,
            is_compressed INTEGER NOT NULL DEFAULT 0
        );
        
        CREATE INDEX IF NOT EXISTS idx_expiry_time ON cache_entries(expiry_time);
        CREATE INDEX IF NOT EXISTS idx_created_time ON cache_entries(created_time);
    )";

  char* error_msg = nullptr;
  int rc = sqlite3_exec(db_, create_table_sql, nullptr, nullptr, &error_msg);

  if (rc != SQLITE_OK) {
    spdlog::error("[SQLiteCacheStorage] SQL Error: {}", error_msg);
    sqlite3_free(error_msg);
    return false;
  }
  return true;
}

std::string SQLiteCacheStorage::CompressData(const std::string& data) const {
  if (!enable_compression_ || data.empty()) {
    return data;
  }

  uLongf compressed_size = compressBound(data.size());
  std::string compressed_data(compressed_size, '\0');

  int result =
      compress(reinterpret_cast<Bytef*>(&compressed_data[0]), &compressed_size,
               reinterpret_cast<const Bytef*>(data.c_str()), data.size());

  if (result != Z_OK) {
    spdlog::error("[SQLiteCacheStorage] Compression failed with error: {}",
                  result);
    return data;
  }

  compressed_data.resize(compressed_size);
  return compressed_data;
}

std::string SQLiteCacheStorage::DecompressData(
    const std::string& compressed_data) const {
  if (!enable_compression_ || compressed_data.empty()) {
    return compressed_data;
  }

  // Try different buffer sizes for decompression
  std::vector<uLongf> buffer_sizes = {compressed_data.size() * 4,
                                      compressed_data.size() * 10,
                                      compressed_data.size() * 50, 1024 * 1024};

  for (uLongf buffer_size : buffer_sizes) {
    std::string decompressed_data(buffer_size, '\0');
    uLongf decompressed_size = buffer_size;

    int result = uncompress(
        reinterpret_cast<Bytef*>(&decompressed_data[0]), &decompressed_size,
        reinterpret_cast<const Bytef*>(compressed_data.c_str()),
        compressed_data.size());

    if (result == Z_OK) {
      decompressed_data.resize(decompressed_size);
      return decompressed_data;
    } else if (result == Z_BUF_ERROR) {
      // Buffer too small
      continue;
    } else {
      // stop trying
      spdlog::error("[SQLiteCacheStorage] Decompression failed with error: {}",
                    result);
      break;
    }
  }

  spdlog::error("[SQLiteCacheStorage] All decompression attempts failed");
  return "";
}

void SQLiteCacheStorage::UpdateCacheSize() {
  const char* size_sql =
      "SELECT COALESCE(SUM(data_size), 0) FROM cache_entries;";

  sqlite3_stmt* stmt;

  int rc = sqlite3_prepare_v2(db_, size_sql, -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    spdlog::error(
        "[SQLiteCacheStorage] Failed to prepare cache size query: {} ({})",
        sqlite3_errmsg(db_), rc);
    return;
  }

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    cache_size.store(static_cast<size_t>(sqlite3_column_int64(stmt, 0)));
  } else if (rc != SQLITE_DONE) {
    spdlog::error(
        "[SQLiteCacheStorage] Failed to execute cache size query: {} ({})",
        sqlite3_errmsg(db_), rc);
  }

  sqlite3_finalize(stmt);
}