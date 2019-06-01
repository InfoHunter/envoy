#include "extensions/filters/http/dynamic_forward_proxy/config.h"

#include "extensions/filters/http/dynamic_forward_proxy/proxy_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace DynamicForwardProxy {

Http::FilterFactoryCb DynamicForwardProxyFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::dynamic_forward_proxy::v2alpha::FilterConfig& proto_config,
    const std::string&, Server::Configuration::FactoryContext& context) {
  ProxyFilterConfigSharedPtr filter_config(new ProxyFilterConfig(
      proto_config, context.singletonManager(), context.dispatcher(), context.threadLocal()));
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(std::make_shared<ProxyFilter>(filter_config));
  };
}

/**
 * Static registration for the dynamic forward proxy filter. @see RegisterFactory.
 */
REGISTER_FACTORY(DynamicForwardProxyFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace DynamicForwardProxy
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
