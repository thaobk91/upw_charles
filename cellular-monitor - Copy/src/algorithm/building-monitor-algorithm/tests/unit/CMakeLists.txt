cmake_minimum_required(VERSION 3.14)
project(tests)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(MOCKED_DIR ${SOURCE_DIR}/mocked)

set(ALGORIHTMS_SOURCE_DIR ${SOURCE_DIR}/algorithm)
set(ALGORIHTMS_UTILITIES_SOURCE_DIR ${ALGORIHTMS_SOURCE_DIR}/utilities)

set(UTILITIES_SOURCE_DIR ${MOCKED_DIR}/utilities)
set(DRIVERS_SOURCE_DIR ${MOCKED_DIR}/drivers)

include_directories(${SOURCE_DIR})
include_directories(${ALGORIHTMS_SOURCE_DIR})
include_directories(${ALGORIHTMS_UTILITIES_SOURCE_DIR})
include_directories(${MOCKED_DIR})
include_directories(${UTILITIES_SOURCE_DIR})
include_directories(${DRIVERS_SOURCE_DIR})

set(TIMESERVER_SOURCES ${UTILITIES_SOURCE_DIR}/timeserver.c ${MOCKED_DIR}/hw.c)
set(SOURCES ${TIMESERVER_SOURCES})

add_executable(
  hello_test
  hello_test.cc
)

add_executable(
  cycle_counter_test
  cycle_counter_test.cc
  ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/cycle_counter.c ${MOCKED_DIR}/flash_utility.c ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/lowest_flow_over_period_tracker.c
  ${SOURCES}
)

add_executable(
  signal_history_test
  signal_history_test.cc
  ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/signal_history.c
  ${SOURCES}
)

add_executable(
  cycle_counter_test
  cycle_counter_test.cc
  ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/cycle_counter.c ${MOCKED_DIR}/flash_utility.c ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/lowest_flow_over_period_tracker.c
  ${SOURCES}
)

add_executable(
  axis_configuration_utility_test
  axis_configuration_utility_test.cc
  ${ALGORIHTMS_UTILITIES_SOURCE_DIR}/axis_configuration_utility_test.c
  ${SOURCES}
)

# had to seperate
target_link_libraries(
  hello_test
  GTest::gtest_main
)

target_link_libraries(
  cycle_counter_test
  GTest::gtest_main
)

target_link_libraries(
  signal_history_test
  GTest::gtest_main
)


target_link_libraries(
  axis_configuration_utility_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(hello_test)
gtest_discover_tests(cycle_counter_test)
gtest_discover_tests(signal_history_test)
gtest_discover_tests(axis_configuration_utility_test)