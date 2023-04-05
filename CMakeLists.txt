cmake_minimum_required(VERSION 3.18)
project(fox-control-gateway LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;")

include(AppProject)
app_define_binary_target()

target_include_directories(${APP_BINARY_TARGET} PUBLIC "${CMAKE_SOURCE_DIR}/external")
target_include_atomic_queue(${APP_BINARY_TARGET})

app_maven_dependency("https://maven.covers1624.net" io.karma.kstd kstd 1.2.0.58)

FetchContent_Declare(
        cxxstreams
        GIT_REPOSITORY https://github.com/karmakrafts/cxxstreams.git
        GIT_TAG master)
FetchContent_Populate(cxxstreams)
target_include_directories(${APP_BINARY_TARGET} PUBLIC "${CMAKE_BINARY_DIR}/_deps/cxxstreams-src/include")

FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG master)
FetchContent_Populate(httplib)
target_include_directories(${APP_BINARY_TARGET} PUBLIC "${CMAKE_BINARY_DIR}/_deps/httplib-src")