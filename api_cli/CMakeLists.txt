cmake_minimum_required(VERSION 3.16)
project(api_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

add_library(moex_api
    src/moex_client.cpp
    src/pricing_service.cpp
    src/price_pipe.cpp
    src/ticker_loader.cpp
    src/randomized_provider.cpp
)

target_include_directories(moex_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(CURL REQUIRED)
find_package(PostgreSQL REQUIRED)
target_link_libraries(moex_api
    PRIVATE
        CURL::libcurl
        PostgreSQL::PostgreSQL
)

add_executable(pricing_service
    src/main.cpp
)

target_link_libraries(pricing_service
    PRIVATE moex_api
)

if(BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

