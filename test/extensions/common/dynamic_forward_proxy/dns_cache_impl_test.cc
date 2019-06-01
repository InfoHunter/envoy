#include "extensions/common/dynamic_forward_proxy/dns_cache_impl.h"

#include "test/mocks/network/mocks.h"
#include "test/mocks/thread_local/mocks.h"

using testing::InSequence;
using testing::Return;
using testing::SaveArg;

// fixfix test inline resolve.

namespace Envoy {
namespace Extensions {
namespace Common {
namespace DynamicForwardProxy {
namespace {

std::list<Network::Address::InstanceConstSharedPtr>
makeAddressList(const std::list<std::string> address_list) {
  std::list<Network::Address::InstanceConstSharedPtr> ret;
  for (const auto& address : address_list) {
    ret.emplace_back(Network::Utility::parseInternetAddress(address));
  }
  return ret;
}

class MockUpdateCallbacks : public DnsCache::UpdateCallbacks {
public:
  MOCK_METHOD2(onDnsHostAddOrUpdate,
               void(const std::string& host, const DnsHostInfoSharedPtr& address));
  MOCK_METHOD1(onDnsHostRemove, void(const std::string& host));
};

class DnsCacheImplTest : public testing::Test {
public:
  DnsCacheImplTest() {
    envoy::config::common::dynamic_forward_proxy::v2alpha::DnsCacheConfig config;
    config.set_dns_lookup_family(envoy::api::v2::Cluster::V4_ONLY);

    EXPECT_CALL(dispatcher_, createDnsResolver(_)).WillOnce(Return(resolver_));
    dns_cache_ = std::make_unique<DnsCacheImpl>(dispatcher_, tls_, config);
    update_callbacks_handle_ = dns_cache_->addUpdateCallbacks(update_callbacks_);
  }

  NiceMock<Event::MockDispatcher> dispatcher_;
  std::shared_ptr<Network::MockDnsResolver> resolver_{std::make_shared<Network::MockDnsResolver>()};
  NiceMock<ThreadLocal::MockInstance> tls_;
  std::unique_ptr<DnsCache> dns_cache_;
  MockUpdateCallbacks update_callbacks_;
  DnsCache::AddUpdateCallbacksHandlePtr update_callbacks_handle_;
};

class MockLoadDnsCacheCallbacks : public DnsCache::LoadDnsCacheCallbacks {
public:
  MOCK_METHOD0(onLoadDnsCacheComplete, void());
};

MATCHER_P(SharedAddressEquals, expected, "") {
  const bool equal = expected == arg->address()->asString();
  if (!equal) {
    *result_listener << fmt::format("'{}' != '{}'", expected, arg->address()->asString());
  }
  return equal;
}

MATCHER(SharedAddressEmpty, "") {
  if (arg->address() != nullptr) {
    *result_listener << "address is not nullptr";
    return false;
  } else {
    return true;
  }
}

// Basic successful resolution.
TEST_F(DnsCacheImplTest, ResolveSuccess) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks;
  Network::DnsResolver::ResolveCb resolve_cb;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle = dns_cache_->loadDnsCache("foo.com", 80, callbacks);
  EXPECT_NE(handle, nullptr);

  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("foo.com", SharedAddressEquals("10.0.0.1:80")));
  EXPECT_CALL(callbacks, onLoadDnsCacheComplete());
  resolve_cb(makeAddressList({"10.0.0.1"}));
}

// Resolve failure that returns no addresses.
TEST_F(DnsCacheImplTest, ResolveFailure) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks;
  Network::DnsResolver::ResolveCb resolve_cb;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle = dns_cache_->loadDnsCache("foo.com", 80, callbacks);
  EXPECT_NE(handle, nullptr);

  EXPECT_CALL(update_callbacks_, onDnsHostAddOrUpdate("foo.com", SharedAddressEmpty()));
  EXPECT_CALL(callbacks, onLoadDnsCacheComplete());
  resolve_cb(makeAddressList({}));
}

// Cancel a cache load before the resolve completes.
TEST_F(DnsCacheImplTest, CancelResolve) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks;
  Network::DnsResolver::ResolveCb resolve_cb;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle = dns_cache_->loadDnsCache("foo.com", 80, callbacks);
  EXPECT_NE(handle, nullptr);

  handle.reset();
  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("foo.com", SharedAddressEquals("10.0.0.1:80")));
  resolve_cb(makeAddressList({"10.0.0.1"}));
}

// Two cache loads that are trying to resolve the same host. Make sure we only do a single resolve
// and fire both callbacks on completion.
TEST_F(DnsCacheImplTest, MultipleResolveSameHost) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks1;
  Network::DnsResolver::ResolveCb resolve_cb;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle1 = dns_cache_->loadDnsCache("foo.com", 80, callbacks1);
  EXPECT_NE(handle1, nullptr);

  MockLoadDnsCacheCallbacks callbacks2;
  DnsCache::LoadDnsCacheHandlePtr handle2 = dns_cache_->loadDnsCache("foo.com", 80, callbacks2);
  EXPECT_NE(handle2, nullptr);

  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("foo.com", SharedAddressEquals("10.0.0.1:80")));
  EXPECT_CALL(callbacks2, onLoadDnsCacheComplete());
  EXPECT_CALL(callbacks1, onLoadDnsCacheComplete());
  resolve_cb(makeAddressList({"10.0.0.1"}));
}

// Two cache loads that are resolving different hosts.
TEST_F(DnsCacheImplTest, MultipleResolveDifferentHost) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks1;
  Network::DnsResolver::ResolveCb resolve_cb1;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb1), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle1 = dns_cache_->loadDnsCache("foo.com", 80, callbacks1);
  EXPECT_NE(handle1, nullptr);

  MockLoadDnsCacheCallbacks callbacks2;
  Network::DnsResolver::ResolveCb resolve_cb2;
  EXPECT_CALL(*resolver_, resolve("bar.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb2), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle2 = dns_cache_->loadDnsCache("bar.com", 443, callbacks2);
  EXPECT_NE(handle2, nullptr);

  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("bar.com", SharedAddressEquals("10.0.0.1:443")));
  EXPECT_CALL(callbacks2, onLoadDnsCacheComplete());
  resolve_cb2(makeAddressList({"10.0.0.1"}));

  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("foo.com", SharedAddressEquals("10.0.0.2:80")));
  EXPECT_CALL(callbacks1, onLoadDnsCacheComplete());
  resolve_cb1(makeAddressList({"10.0.0.2"}));
}

// A successful resolve followed by a cache hit.
TEST_F(DnsCacheImplTest, CacheHit) {
  InSequence s;

  MockLoadDnsCacheCallbacks callbacks;
  Network::DnsResolver::ResolveCb resolve_cb;
  EXPECT_CALL(*resolver_, resolve("foo.com", _, _))
      .WillOnce(DoAll(SaveArg<2>(&resolve_cb), Return(&resolver_->active_query_)));
  DnsCache::LoadDnsCacheHandlePtr handle = dns_cache_->loadDnsCache("foo.com", 80, callbacks);
  EXPECT_NE(handle, nullptr);

  EXPECT_CALL(update_callbacks_,
              onDnsHostAddOrUpdate("foo.com", SharedAddressEquals("10.0.0.1:80")));
  EXPECT_CALL(callbacks, onLoadDnsCacheComplete());
  resolve_cb(makeAddressList({"10.0.0.1"}));

  EXPECT_EQ(nullptr, dns_cache_->loadDnsCache("foo.com", 80, callbacks));
}

} // namespace
} // namespace DynamicForwardProxy
} // namespace Common
} // namespace Extensions
} // namespace Envoy
