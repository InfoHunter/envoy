#include "extensions/common/dynamic_forward_proxy/dns_cache_impl.h"

#include "common/network/utility.h"

// TODO(mattklein123): Move DNS family helpers to a smaller include.
#include "common/upstream/upstream_impl.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace DynamicForwardProxy {

// fixfix TTL
// fixfix re-resolve
// fixfix stats

DnsCacheImpl::DnsCacheImpl(
    Event::Dispatcher& main_thread_dispatcher, ThreadLocal::SlotAllocator& tls,
    const envoy::config::common::dynamic_forward_proxy::v2alpha::DnsCacheConfig& config)
    : main_thread_dispatcher_(main_thread_dispatcher),
      dns_lookup_family_(Upstream::getDnsLookupFamilyFromEnum(config.dns_lookup_family())),
      resolver_(main_thread_dispatcher.createDnsResolver({})), tls_slot_(tls.allocateSlot()) {
  tls_slot_->set([](Event::Dispatcher&) { return std::make_shared<ThreadLocalHostInfo>(); });
  updateTlsHostsMap();
}

DnsCacheImpl::~DnsCacheImpl() {
  // fixfix cancel active queries.
}

DnsCacheImpl::LoadDnsCacheHandlePtr DnsCacheImpl::loadDnsCache(absl::string_view host,
                                                               uint16_t default_port,
                                                               LoadDnsCacheCallbacks& callbacks) {
  ENVOY_LOG(debug, "thread local lookup for host '{}'", host);
  auto& tls_host_info = tls_slot_->getTyped<ThreadLocalHostInfo>();
  auto tls_host = tls_host_info.host_map_->find(host);
  if (tls_host != tls_host_info.host_map_->end()) {
    ENVOY_LOG(debug, "thread local hit for host '{}'", host);
    return nullptr;
  } else {
    ENVOY_LOG(debug, "thread local miss for host '{}', posting to main thread", host);
    main_thread_dispatcher_.post(
        [this, host = std::string(host), default_port]() { startResolve(host, default_port); });
    return std::make_unique<LoadDnsCacheHandleImpl>(tls_host_info.pending_resolutions_, host,
                                                    callbacks);
  }
}

DnsCacheImpl::AddUpdateCallbacksHandlePtr
DnsCacheImpl::addUpdateCallbacks(UpdateCallbacks& callbacks) {
  return std::make_unique<AddUpdateCallbacksHandleImpl>(update_callbacks_, callbacks);
}

void DnsCacheImpl::startResolve(const std::string& host, uint16_t default_port) {
  // It's possible for multiple requests to race trying to start a resolution. If a host is
  // already in the map it's either in the process of being resolved or the resolution is already
  // heading out to the worker threads. Either way the pending resolution will be completed.
  const auto primary_host_it = primary_hosts_.find(host);
  if (primary_host_it != primary_hosts_.end()) {
    ENVOY_LOG(debug, "main thread resolve for host '{}' skipped. Entry present", host);
    return;
  }

  const auto colon_pos = host.find(':');
  std::string host_to_resolve = host;
  if (colon_pos != absl::string_view::npos) {
    host_to_resolve = host.substr(0, colon_pos);
    const auto port_str = host.substr(colon_pos + 1);
    uint64_t port64;
    if (port_str.empty() || !absl::SimpleAtoi(port_str, &port64) || port64 > 65535) {
      ASSERT(false); // fixfix
    }
    default_port = port64;
  }

  ENVOY_LOG(debug, "starting main thread resolve for host='{}' dns='{}' port='{}'", host,
            host_to_resolve, default_port);
  auto& primary_host = primary_hosts_.try_emplace(host, default_port).first->second;
  primary_host.active_query_ = resolver_->resolve(
      host_to_resolve, dns_lookup_family_,
      [this, host](const std::list<Network::Address::InstanceConstSharedPtr>&& address_list) {
        finishResolve(host, address_list);
      });
}

void DnsCacheImpl::finishResolve(
    const std::string& host,
    const std::list<Network::Address::InstanceConstSharedPtr>& address_list) {
  ENVOY_LOG(debug, "main thread resolve complete for host '{}'. {} results", host,
            address_list.size());
  const auto primary_host_it = primary_hosts_.find(host);
  ASSERT(primary_host_it != primary_hosts_.end());

  auto& primary_host_info = primary_host_it->second;
  ASSERT(primary_host_info.host_info_ == nullptr); // fixfix handle re-resolve.
  primary_host_info.active_query_ = nullptr;
  primary_host_info.host_info_ =
      std::make_shared<DnsHostInfoImpl>(main_thread_dispatcher_.timeSource());
  primary_host_info.host_info_->address_ =
      !address_list.empty()
          ? Network::Utility::getAddressWithPort(*address_list.front(), primary_host_info.port_)
          : nullptr;
  runAddUpdateCallbacks(host, primary_host_info.host_info_);
  updateTlsHostsMap();
}

void DnsCacheImpl::runAddUpdateCallbacks(const std::string& host,
                                         const DnsHostInfoSharedPtr& host_info) {
  for (auto callbacks : update_callbacks_) {
    callbacks->onDnsHostAddOrUpdate(host, host_info);
  }
}

void DnsCacheImpl::updateTlsHostsMap() {
  TlsHostMapSharedPtr new_host_map = std::make_shared<TlsHostMap>();
  for (const auto& primary_host : primary_hosts_) {
    // Do not include hosts that are new and have not yet been resolved.
    if (primary_host.second.host_info_ != nullptr) {
      new_host_map->emplace(primary_host.first, primary_host.second.host_info_);
    }
  }

  // fixfix cleanup
  tls_slot_->runOnAllThreads([this, new_host_map]() {
    auto& tls_host_info = tls_slot_->getTyped<ThreadLocalHostInfo>();
    tls_host_info.host_map_ = new_host_map;
    for (auto pending_resolution = tls_host_info.pending_resolutions_.begin();
         pending_resolution != tls_host_info.pending_resolutions_.end();) {
      if (tls_host_info.host_map_->count((*pending_resolution)->host_) != 0) {
        auto& callbacks = (*pending_resolution)->callbacks_;
        (*pending_resolution)->cancel();
        pending_resolution = tls_host_info.pending_resolutions_.erase(pending_resolution);
        callbacks.onLoadDnsCacheComplete();
      } else {
        ++pending_resolution;
      }
    }
  });
}

} // namespace DynamicForwardProxy
} // namespace Common
} // namespace Extensions
} // namespace Envoy
