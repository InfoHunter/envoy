#include "extensions/clusters/dynamic_forward_proxy/cluster.h"

namespace Envoy {
namespace Extensions {
namespace Clusters {
namespace DynamicForwardProxy {

Cluster::Cluster(
    const envoy::api::v2::Cluster& cluster,
    const envoy::config::cluster::dynamic_forward_proxy::v2alpha::ClusterConfig& config,
    Runtime::Loader& runtime,
    Extensions::Common::DynamicForwardProxy::DnsCacheManagerFactory& cache_manager_factory,
    const LocalInfo::LocalInfo& local_info,
    Server::Configuration::TransportSocketFactoryContext& factory_context,
    Stats::ScopePtr&& stats_scope, bool added_via_api)
    : Upstream::BaseDynamicClusterImpl(cluster, runtime, factory_context, std::move(stats_scope),
                                       added_via_api),
      dns_cache_manager_(cache_manager_factory.get()),
      dns_cache_(dns_cache_manager_->getCache(config.dns_cache_config())),
      update_callbacks_handle_(dns_cache_->addUpdateCallbacks(*this)), local_info_(local_info),
      host_map_(std::make_shared<HostInfoMap>()) {
  // fixfix do initial load from cache.
}

void Cluster::onDnsHostAddOrUpdate(
    const std::string& host,
    const Extensions::Common::DynamicForwardProxy::DnsHostInfoSharedPtr& host_info) {

  ASSERT(host_info->address() != nullptr); // fixfix

  Upstream::HostVector hosts_added;
  HostInfoMapSharedPtr current_map = getCurrentHostMap();
  const auto host_map_it = current_map->find(host);
  if (host_map_it == current_map->end()) {
    const auto new_host_map = std::make_shared<HostInfoMap>(*current_map);
    const auto emplaced = new_host_map->try_emplace(
        host, host_info,
        std::make_shared<Upstream::LogicalHost>(info(), host, host_info->address(),
                                                dummy_locality_lb_endpoint_, dummy_lb_endpoint_));
    hosts_added.emplace_back(emplaced.first->second.logical_host_);

    // Swap in the new map. This will be picked up when the per-worker LBs are recreated via
    // the host set update.
    current_map = new_host_map;
    {
      absl::WriterMutexLock lock(&host_map_lock_);
      host_map_ = current_map;
    }
  } else {
    ASSERT(false); // fixfix
  }

  Upstream::PriorityStateManager priority_state_manager(*this, local_info_, nullptr);
  priority_state_manager.initializePriorityFor(dummy_locality_lb_endpoint_);
  for (const auto& host : (*current_map)) {
    priority_state_manager.registerHostForPriority(host.second.logical_host_,
                                                   dummy_locality_lb_endpoint_);
  }
  priority_state_manager.updateClusterPrioritySet(
      0, std::move(priority_state_manager.priorityState()[0].first), hosts_added, {}, absl::nullopt,
      absl::nullopt);
}

void Cluster::onDnsHostRemove(const std::string& /*host*/) {
  ASSERT(false); // fixfix
}

Upstream::HostConstSharedPtr
Cluster::LoadBalancer::chooseHost(Upstream::LoadBalancerContext* context) {
  const auto host_it =
      host_map_->find(context->downstreamHeaders()->Host()->value().getStringView());
  if (host_it == host_map_->end()) {
    ASSERT(false); // fixfix
  } else {
    host_it->second.shared_host_info_->touch();
    return host_it->second.logical_host_;
  }
}

class DnsCacheManagerFactoryImpl
    : public Extensions::Common::DynamicForwardProxy::DnsCacheManagerFactory {
public:
  DnsCacheManagerFactoryImpl(Singleton::Manager& singleton_manager, Event::Dispatcher& dispatcher,
                             ThreadLocal::SlotAllocator& tls)
      : singleton_manager_(singleton_manager), dispatcher_(dispatcher), tls_(tls) {}

  Extensions::Common::DynamicForwardProxy::DnsCacheManagerSharedPtr get() override {
    return Extensions::Common::DynamicForwardProxy::getCacheManager(singleton_manager_, dispatcher_,
                                                                    tls_);
  }

private:
  Singleton::Manager& singleton_manager_;
  Event::Dispatcher& dispatcher_;
  ThreadLocal::SlotAllocator& tls_;
};

std::pair<Upstream::ClusterImplBaseSharedPtr, Upstream::ThreadAwareLoadBalancerPtr>
ClusterFactory::createClusterWithConfig(
    const envoy::api::v2::Cluster& cluster,
    const envoy::config::cluster::dynamic_forward_proxy::v2alpha::ClusterConfig& proto_config,
    Upstream::ClusterFactoryContext& context,
    Server::Configuration::TransportSocketFactoryContext& socket_factory_context,
    Stats::ScopePtr&& stats_scope) {
  DnsCacheManagerFactoryImpl cache_manager_factory(context.singletonManager(), context.dispatcher(),
                                                   context.tls());
  auto new_cluster = std::make_shared<Cluster>(
      cluster, proto_config, context.runtime(), cache_manager_factory, context.localInfo(),
      socket_factory_context, std::move(stats_scope), context.addedViaApi());
  auto lb = std::make_unique<Cluster::ThreadAwareLoadBalancer>(*new_cluster);
  return std::make_pair(new_cluster, std::move(lb));
}

REGISTER_FACTORY(ClusterFactory, Upstream::ClusterFactory);

} // namespace DynamicForwardProxy
} // namespace Clusters
} // namespace Extensions
} // namespace Envoy
