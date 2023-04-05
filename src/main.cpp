/**
 * @author F0x0
 * @since 05/04/2023
 */

#include <string>
#include <string_view>
#include <kstd/types.hpp>
#include <spdlog/spdlog.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cxxopts/cxxopts.hpp>

auto main(int num_args, char** args) -> int {
    auto option_spec = cxxopts::Options("fox-control-gateway", "FoxControl task queueing HTTP gateway server");

    // @formatter:off
    option_spec.add_options()
        ("h,help", "Show this help dialog")
        ("v,version", "Show version information")
        ("V,verbose", "Enable verbose logging")
        ("a,address", "Specify the address on which to listen for HTTP requests", cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("p,port", "Specify the port on which to listen for HTTP requests", cxxopts::value<kstd::u32>()->default_value("8080"));
    // @formatter:on

    return 0;
}