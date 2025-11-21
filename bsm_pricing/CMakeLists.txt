cmake_minimum_required(VERSION 3.16)
project(bsm_pricing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

add_library(bsm_lib
    src/price_pipe.cpp
    src/option_pricer.cpp
    src/bsm_service.cpp
    src/influx_writer.cpp
)

target_include_directories(bsm_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# HTTP клиент для записи в InfluxDB
find_package(CURL REQUIRED)
target_link_libraries(bsm_lib
    PRIVATE CURL::libcurl
)

add_executable(bsm_pricing
    src/main.cpp
)

target_link_libraries(bsm_pricing
    PRIVATE bsm_lib
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


