add_executable(vull-tests
    EntitySystemTest.cc)
target_compile_features(vull-tests PRIVATE cxx_std_20)
target_link_libraries(vull-tests PRIVATE GTest::Main vull-engine)
gtest_discover_tests(vull-tests)
