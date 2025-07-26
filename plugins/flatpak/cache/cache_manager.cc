#include "cache_manager.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <exception>
#include <fstream>
#include <ios>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include "cache_config.h"
#include "interfaces/cache_observer.h"
#include "interfaces/cache_storage.h"
#include "network/curl_network_fetcher.h"
#include "storage/sqlite_cache_storage.h"

flatpak_plugin::CacheManager::CacheManager(const CacheConfig& config)
    : config_(std::move(config)), metrics_{} {}

flatpak_plugin::CacheManager::~CacheManager() {
  stop_cleanup_.store(true);
  cleanup_cv_.notify_all();
  if (cleanup_thread_.joinable()) {
    cleanup_thread_.join();
  }
}

bool flatpak_plugin::CacheManager::Initialize() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  spdlog::info(
      "Initializing CacheManager with config: db_path={}, ttl={}, policy={}, "
      "max_size={}",
      config_.db_path, config_.default_ttl.count(),
      static_cast<int>(config_.policy), config_.max_cache_size_mb);

  storage_ = std::make_unique<SQLiteCacheStorage>(config_.db_path,
                                                  config_.enable_compression);
  if (!storage_->Initialize()) {
    spdlog::error("Failed to initialize cache storage");
    return false;
  }

  network_fetcher_ = std::make_unique<CurlNetworkFetcher>(
      config_.network_timeout, config_.max_retries);

  if (config_.enable_metrics) {
    metrics_.hits = 0;
    metrics_.misses = 0;
    metrics_.cache_size_bytes = 0;
    metrics_.network_calls = 0;
    metrics_.network_errors = 0;
    metrics_.start_time = std::chrono::system_clock::now();
  }

  if (config_.enable_auto_cleanup) {
    cleanup_thread_ = std::thread(&CacheManager::CleanupWorker, this);
  }

  spdlog::info("Cache Manager initialized successfully");
  return true;
}

void flatpak_plugin::CacheManager::AddObserver(
    std::unique_ptr<ICacheObserver> observer) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  observers_.push_back(std::move(observer));
}

void flatpak_plugin::CacheManager::SetBearerToken(const std::string& token) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  if (network_fetcher_) {
    network_fetcher_->SetBearerToken(token);
  }
}

std::string flatpak_plugin::CacheManager::GenerateKey(
    const std::string& base_key,
    const std::vector<std::string>& params) {
  std::ostringstream oss;
  oss << base_key;
  for (const auto& param : params) {
    oss << ":" << param;
  }
  return oss.str();
}

void flatpak_plugin::CacheManager::NotifyObservers(
    const std::function<void(ICacheObserver*)>& notification) {
  for (const auto& observer : observers_) {
    if (observer) {
      notification(observer.get());
    }
  }
}

void flatpak_plugin::CacheManager::CleanupWorker() {
  std::unique_lock<std::mutex> lock(cleanup_mutex_);
  spdlog::info("Cleanup thread started, interval={} minutes",
               config_.cleanup_interval.count());

  while (!stop_cleanup_.load()) {
    cleanup_cv_.wait_for(lock, config_.cleanup_interval,
                         [this] { return stop_cleanup_.load(); });

    if (stop_cleanup_.load()) {
      break;
    }

    try {
      size_t cleaned = storage_->CleanupExpired();
      if (cleaned > 0) {
        spdlog::info("Cleaned up {} expired cache entries", cleaned);
        NotifyObservers([cleaned](ICacheObserver* observer) {
          observer->OnCacheCleanup(cleaned);
        });
      }
    } catch (const std::exception& e) {
      spdlog::error("Error during cache cleanup: {}", e.what());
    }
  }
}

template <typename T>
std::optional<T> flatpak_plugin::CacheManager::PerformCacheOperation(
    const std::string& key,
    std::function<std::optional<T>()> network_operation,
    CacheOperationTemplate<T>* cache_operation) {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  bool use_cache = (config_.policy == CachePolicy::CACHE_FIRST ||
                    config_.policy == CachePolicy::CACHE_ONLY);

  bool use_network = (config_.policy == CachePolicy::NETWORK_FIRST ||
                      config_.policy == CachePolicy::NETWORK_ONLY);

  std::optional<T> result;
  // try cache first
  if (use_cache) {
    auto cached_data = storage_->Retrieve(key);
    if (cached_data.has_value()) {
      result = cache_operation->DeserializeData(cached_data.value());
      if (result.has_value()) {
        if (config_.enable_metrics) {
          metrics_.hits++;
        }
        NotifyObservers(
            [key, data_size = cached_data->size()](ICacheObserver* observer) {
              observer->OnCacheHit(key, data_size);
            });
        return result;
      }
    }
    if (config_.enable_metrics) {
      metrics_.misses++;
    }
    NotifyObservers(
        [&key](ICacheObserver* observer) { observer->OnCacheMiss(key); });
  }

  // try network

  if (use_network && network_operation) {
    try {
      if (config_.enable_metrics) {
        metrics_.network_calls++;
      }

      result = network_operation();
      if (result.has_value()) {
        if (config_.policy != CachePolicy::NETWORK_ONLY) {
          auto serialized = cache_operation->SerializeData(result.has_value());
          auto expiry = std::chrono::system_clock::now() + config_.default_ttl;
          storage_->Store(key, serialized, expiry);
        }

        NotifyObservers([&key](ICacheObserver* observer) {
          observer->OnNetworkFallback("Data fetched from network");
        });
      }
    } catch (const std::exception& e) {
      if (config_.enable_metrics) {
        metrics_.network_errors++;
      }

      spdlog::error("Network operation failed for key{}: {}", key, e.what());
      NotifyObservers([&key, error_code = -1](ICacheObserver* observer) {
        observer->OnNetworkError(key, error_code);
      });
    }
  }

  return result;
}

std::optional<flutter::EncodableList>
flatpak_plugin::CacheManager::GetApplicationsInstalled(bool force_refresh) {
  std::string key = GenerateKey("applications_installed");

  if (force_refresh) {
    InvalidateKey(key);
  }

  auto network_ops = [this]() -> std::optional<flutter::EncodableList> {
    // some flatpak API work , TODO on PR#3 in "[Feature] Add Flatpak remote
    // management and installation support"
    return std::nullopt;
  };

  // Create cache operation template for EncodableList
  // some flatpak API work , TODO on PR#3 in "[Feature] Add Flatpak remote
  // management and installation support"

  return std::nullopt;
}

std::optional<flutter::EncodableList>
flatpak_plugin::CacheManager::GetApplicationsRemote(
    const std::string& remote_id,
    bool force_refresh) {
  std::string key = GenerateKey("applications_installed", {remote_id});

  if (force_refresh) {
    InvalidateKey(key);
  }

  auto network_ops = [this,
                      remote_id]() -> std::optional<flutter::EncodableList> {
    // Implementation would fetch from remote Flatpak repository , TODO on PR#3
    // in "[Feature] Add Flatpak remote management and installation support"
    return std::nullopt;
  };

  return std::nullopt;
}

std::optional<Installation> flatpak_plugin::CacheManager::GetUserInstallation(
    bool force_refresh) {
  std::string key = GenerateKey("user_installation");

  if (force_refresh) {
    InvalidateKey(key);
  }

  auto network_ops = [this]() -> std::optional<Installation> {
    // Implementation would get user installation info , TODO on PR#3 in
    // "[Feature] Add Flatpak remote management and installation support"
    return std::nullopt;
  };

  return std::nullopt;
}

std::optional<flutter::EncodableList>
flatpak_plugin::CacheManager::GetSystemInstallations(bool force_refresh) {
  std::string key = GenerateKey("system_installations");

  if (force_refresh) {
    InvalidateKey(key);
  }

  auto network_ops = [this]() -> std::optional<flutter::EncodableList> {
    // Implementation would get system installations, TODO on PR#3 in "[Feature]
    // Add Flatpak remote management and installation support"
    return std::nullopt;
  };

  return std::nullopt;
}

std::optional<flutter::EncodableList> flatpak_plugin::CacheManager::GetRemotes(
    const std::string& installation_id,
    bool force_refresh) {
  std::string key = GenerateKey("remotes", {installation_id});

  if (force_refresh) {
    InvalidateKey(key);
  }

  auto network_ops =
      [this, installation_id]() -> std::optional<flutter::EncodableList> {
    // Implementation would get system installations, TODO on PR#3 in "[Feature]
    // Add Flatpak remote management and installation support"
    return std::nullopt;
  };

  return std::nullopt;
}

void flatpak_plugin::CacheManager::InvalidateAll() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  if (storage_) {
    storage_->Invalidate();
    NotifyObservers([](ICacheObserver* observer) {
      spdlog::info("All cache entries invalidated");
    });
  }
}

void flatpak_plugin::CacheManager::InvalidateKey(const std::string& key) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  storage_->Invalidate(key);
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  spdlog::info("Invalidated cache key: '{}', thread={}", key, oss.str());
  NotifyObservers(
      [&key](ICacheObserver* observer) { observer->OnCacheExpired(key); });
}

bool flatpak_plugin::CacheManager::IsHealthy() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  if (!storage_) {
    return false;
  }

  // check network
  if (network_fetcher_ && !network_fetcher_->IsNetworkAvailable()) {
    spdlog::warn("Network not available");
  }

  // check cache size
  size_t current_size = storage_->GetCacheSize();
  if (current_size > config_.max_cache_size_mb * 1024 * 1024) {
    spdlog::warn("Cache size {} exceeds limit {}", current_size,
                 config_.max_cache_size_mb * 1024 * 1024);
  }

  return true;
}

size_t flatpak_plugin::CacheManager::GetCacheSize() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  return storage_ ? storage_->GetCacheSize() : 0;
}

void flatpak_plugin::CacheManager::SetCachePolicy(CachePolicy policy) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  config_.policy = policy;
  spdlog::info("Cache policy changed to {}", static_cast<int>(policy));
}

flatpak_plugin::CachePolicy flatpak_plugin::CacheManager::GetCachePolicy()
    const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  return config_.policy;
}

size_t flatpak_plugin::CacheManager::ForceCleanup() {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  size_t cleaned = storage_->CleanupExpired();
  NotifyObservers([cleaned](ICacheObserver* observer) {
    observer->OnCacheCleanup(cleaned);
  });

  spdlog::info("Manual cleanup removed {} expired entries", cleaned);
  return cleaned;
}

const flatpak_plugin::CacheConfig& flatpak_plugin::CacheManager::GetConfig()
    const {
  return config_;
}

bool flatpak_plugin::CacheManager::ExportCache(const std::string& filepath) {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  try {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      spdlog::error("Failed to open export file: {}", filepath);
      return false;
    }
    // could enhance this import logic ....

    spdlog::info("Cache exported to {}", filepath);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to export cache: {}", e.what());
    return false;
  }
}

bool flatpak_plugin::CacheManager::ImportCache(const std::string& filepath) {
  std::lock_guard<std::mutex> lock(cache_mutex_);

  try {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      spdlog::error("Failed to open import file: {}", filepath);
      return false;
    }
    // could enhance this import logic ....
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to import cache: {}", e.what());
    return false;
  }
}

std::unique_ptr<flatpak_plugin::CacheManager>
flatpak_plugin::CacheManager::Builder::Build() {
  auto manager = std::make_unique<CacheManager>(config_);
  return manager;
}