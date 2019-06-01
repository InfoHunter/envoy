#include "extensions/filters/http/dynamic_forward_proxy/proxy_filter.h"

#include "extensions/common/dynamic_forward_proxy/dns_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace DynamicForwardProxy {

ProxyFilterConfig::ProxyFilterConfig(
    const envoy::config::filter::http::dynamic_forward_proxy::v2alpha::FilterConfig& proto_config,
    Singleton::Manager& singleton_manager, Event::Dispatcher& main_thread_dispatcher,
    ThreadLocal::SlotAllocator& tls)
    : dns_cache_manager_(Common::DynamicForwardProxy::getCacheManager(singleton_manager,
                                                                      main_thread_dispatcher, tls)),
      dns_cache_(dns_cache_manager_->getCache(proto_config.dns_cache_config())) {}

Http::FilterHeadersStatus ProxyFilter::decodeHeaders(Http::HeaderMap& headers, bool) {
  // fixfix select default port based on upstream TLS configuration.
  cache_load_handle_ =
      config_->cache().loadDnsCache(headers.Host()->value().getStringView(), 80, *this);
  if (cache_load_handle_ == nullptr) {
    ENVOY_STREAM_LOG(debug, "DNS cache already loaded, continuing", *decoder_callbacks_);
    return Http::FilterHeadersStatus::Continue;
  }

  ENVOY_STREAM_LOG(debug, "waiting to load DNS cache", *decoder_callbacks_);
  return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
}

void ProxyFilter::onLoadDnsCacheComplete() {
  ENVOY_STREAM_LOG(debug, "load DNS cache complete, continuing", *decoder_callbacks_);
  decoder_callbacks_->continueDecoding();
}

} // namespace DynamicForwardProxy
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
