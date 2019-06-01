#include "test/extensions/common/dynamic_forward_proxy/mocks.h"

using testing::_;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace Common {
namespace DynamicForwardProxy {

MockDnsCache::MockDnsCache() = default;
MockDnsCache::~MockDnsCache() = default;

MockDnsCacheManager::MockDnsCacheManager() {
  ON_CALL(*this, getCache(_)).WillByDefault(Return(dns_cache_));
}
MockDnsCacheManager::~MockDnsCacheManager() = default;

} // namespace DynamicForwardProxy
} // namespace Common
} // namespace Extensions
} // namespace Envoy
