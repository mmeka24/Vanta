find_package(Catch2 3 CONFIG REQUIRED)

add_executable(vanta_tests unit/smoke_test.cpp)
target_link_libraries(vanta_tests PRIVATE vanta_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(vanta_tests)
