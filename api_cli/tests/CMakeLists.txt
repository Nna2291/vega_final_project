add_executable(unit_tests
    unit_tests.cpp
)

target_link_libraries(unit_tests
    PRIVATE moex_api GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)

add_executable(functional_tests
    functional_tests.cpp
)

target_link_libraries(functional_tests
    PRIVATE moex_api GTest::gtest_main
)

gtest_discover_tests(functional_tests)


