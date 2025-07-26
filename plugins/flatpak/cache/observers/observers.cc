#include "metrics_cache_observer.h"

MetricsCacheObserver::MetricsCacheObserver(
    flatpak_plugin::CacheMetrics* metrics)
    : metrics_(metrics) {}

void MetricsCacheObserver::OnCacheHit(const std::string& key,
                                      size_t data_size) {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  spdlog::debug("Cache hit: key='{}', size={}, thread={}", key, data_size,
                oss.str());
  metrics_->hits++;
  metrics_->cache_size_bytes += data_size;
  double hit_ratio = metrics_->GetHitRatio();
  spdlog::info(
      "Cache hit for key: {}, size: {}. Total hits: {}, Hit ratio: {:.2f}%",
      key, data_size, metrics_->hits.load(), hit_ratio);
}

void MetricsCacheObserver::OnCacheMiss(const std::string& key) {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  spdlog::debug("Cache miss: key='{}', thread={}", key, oss.str());
  metrics_->misses++;
  spdlog::info("Cache miss for key: {}", key);
}

void MetricsCacheObserver::OnCacheExpired(const std::string& key) {
  metrics_->expired_entries++;
  spdlog::info("Cache expired for key: {}", key);
}

void MetricsCacheObserver::OnNetworkFallback(const std::string& reason) {
  metrics_->network_calls++;
  spdlog::warn("Network fallback triggered: {}", reason);
}

void MetricsCacheObserver::OnNetworkError(const std::string& url,
                                          long error_code) {
  metrics_->network_errors++;
  spdlog::error("Network error for url: {}, error code: {}", url, error_code);
}

void MetricsCacheObserver::OnCacheCleanup(size_t entries_cleaned) {
  spdlog::info("Cache cleanup event, entries cleaned: {}", entries_cleaned);
}