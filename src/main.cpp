/**
 * @author F0x0
 * @since 05/04/2023
 */

#include <string>
#include <string_view>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <httplib.h>
#include <cxxopts/cxxopts.hpp>
#include "gateway.hpp"

auto main(int num_args, char** args) -> int {
    spdlog::set_default_logger(spdlog::create<spdlog::sinks::stdout_color_sink_mt>("FoxControl"));
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%n] [%^---%L---%$] [thread %t] %v");

    auto option_spec = cxxopts::Options("fox-control-gateway", "FoxControl task queueing HTTP gateway server");

    // @formatter:off
    option_spec.add_options()
        ("h,help", "Show this help dialog")
        ("v,version", "Show version information")
        ("V,verbose", "Enable verbose logging")
        ("a,address", "Specify the address on which to listen for HTTP requests", cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("e,endpoint", "Specify the name of the endpoint to take commands from", cxxopts::value<std::string>()->default_value("enqueue"))
        ("p,port", "Specify the port on which to listen for HTTP requests", cxxopts::value<kstd::u32>()->default_value("8080"))
        ("b,backlog", "Specify the maximum of tasks that can be queued up internally", cxxopts::value<kstd::u32>()->default_value("500"))
        ("P,password", "Specify the password with which to authenticate against the endpoint for queueing tasks", cxxopts::value<std::string>());
    // @formatter:on

    cxxopts::ParseResult options;

    try {
        options = option_spec.parse(num_args, args);
    }
    catch (const std::exception& error) {
        spdlog::error("Malformed arguments: {}", error.what());
        return 1;
    }

    if (options.count("help") > 0) {
        std::cout << option_spec.help() << std::endl;
        return 0;
    }

    if (options.count("verbose") > 0) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Verbose logging enabled");
    }
    else {
        spdlog::set_level(spdlog::level::info);
    }

    if (options.count("version") > 0) {
        spdlog::info("FoxControl Gateway Version 1.1");
        return 0;
    }

    const auto address = options["address"].as<std::string>();
    const auto endpoint = options["endpoint"].as<std::string>();
    const auto port = options["port"].as<kstd::u32>();
    const auto backlog = options["backlog"].as<kstd::u32>();
    const auto password = options["password"].as<std::string>();

    fox::Gateway gateway(address, endpoint, port, backlog, password);

    return 0;
}