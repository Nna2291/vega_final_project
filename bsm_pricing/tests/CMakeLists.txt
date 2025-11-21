add_executable(unit_tests
    unit_tests.cpp
)

target_link_libraries(unit_tests
    PRIVATE bsm_lib GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)


