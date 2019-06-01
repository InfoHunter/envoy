licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "dns_cache_impl_test",
    srcs = ["dns_cache_impl_test.cc"],
    deps = [
        "//source/extensions/common/dynamic_forward_proxy:dns_cache_impl",
        "//test/mocks/network:network_mocks",
        "//test/mocks/thread_local:thread_local_mocks",
    ],
)

envoy_cc_mock(
    name = "mocks",
    srcs = ["mocks.cc"],
    hdrs = ["mocks.h"],
    deps = [
        "//source/extensions/common/dynamic_forward_proxy:dns_cache_interface",
    ],
)
