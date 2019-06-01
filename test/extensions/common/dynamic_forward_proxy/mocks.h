#pragma once

#include "extensions/common/dynamic_forward_proxy/dns_cache.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace DynamicForwardProxy {

class MockDnsCache : public DnsCache {
public:
  MockDnsCache();
  ~MockDnsCache();

  MOCK_METHOD3(loadDnsCache,
               DnsCache::LoadDnsCacheHandlePtr(absl::string_view host, uint16_t default_port,
                                               LoadDnsCacheCallbacks& callbacks));
  AddUpdateCallbacksHandlePtr addUpdateCallbacks(UpdateCallbacks& callbacks) override {
    return AddUpdateCallbacksHandlePtr{addUpdateCallbacks_(callbacks)};
  }
  MOCK_METHOD1(addUpdateCallbacks_,
               DnsCache::AddUpdateCallbacksHandle*(UpdateCallbacks& callbacks));
};

class MockDnsCacheManager : public DnsCacheManager {
public:
  MockDnsCacheManager();
  ~MockDnsCacheManager();

  MOCK_METHOD1(
      getCache,
      DnsCacheSharedPtr(
          const envoy::config::common::dynamic_forward_proxy::v2alpha::DnsCacheConfig& config));

  std::shared_ptr<MockDnsCache> dns_cache_{new MockDnsCache()};
};

} // namespace DynamicForwardProxy
} // namespace Common
} // namespace Extensions
} // namespace Envoy
