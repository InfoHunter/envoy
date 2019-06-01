#pragma once

#include "envoy/config/filter/http/dynamic_forward_proxy/v2alpha/dynamic_forward_proxy.pb.h"
#include "envoy/singleton/manager.h"

#include "extensions/common/dynamic_forward_proxy/dns_cache.h"
#include "extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace DynamicForwardProxy {

class ProxyFilterConfig {
public:
  ProxyFilterConfig(
      const envoy::config::filter::http::dynamic_forward_proxy::v2alpha::FilterConfig& proto_config,
      Singleton::Manager& manager, Event::Dispatcher& main_thread_dispatcher,
      ThreadLocal::SlotAllocator& tls);

  Extensions::Common::DynamicForwardProxy::DnsCache& cache() { return *dns_cache_; }

private:
  const Extensions::Common::DynamicForwardProxy::DnsCacheManagerSharedPtr dns_cache_manager_;
  const Extensions::Common::DynamicForwardProxy::DnsCacheSharedPtr dns_cache_;
};

using ProxyFilterConfigSharedPtr = std::shared_ptr<ProxyFilterConfig>;

class ProxyFilter : public Http::PassThroughDecoderFilter,
                    public Extensions::Common::DynamicForwardProxy::DnsCache::LoadDnsCacheCallbacks,
                    Logger::Loggable<Logger::Id::dfproxy> {
public:
  ProxyFilter(const ProxyFilterConfigSharedPtr& config) : config_(config) {}

  // Http::PassThroughDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers, bool end_stream) override;

  // Extensions::Common::DynamicForwardProxy::DnsCache::LoadDnsCacheCallbacks
  void onLoadDnsCacheComplete() override;

private:
  const ProxyFilterConfigSharedPtr config_;
  Extensions::Common::DynamicForwardProxy::DnsCache::LoadDnsCacheHandlePtr cache_load_handle_;
};

} // namespace DynamicForwardProxy
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
