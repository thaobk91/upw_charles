#
# Copyright (c) NOWi Sensors LLC 2024s
##

cmake_minimum_required(VERSION 3.20.0)

if (WIN32)
  set(CMAKE_OBJECT_PATH_MAX 260)
endif ()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cell_monitor)

if(CONFIG_ALGORITHM_LOGGING)
else()
    add_definitions(-DDISABLE_LOGGING)
endif()

# NORDIC SDK APP START
zephyr_include_directories(boards)
zephyr_include_directories(src/cloud)
zephyr_include_directories(src/wdt)
zephyr_include_directories(src/buzzer)
zephyr_include_directories(src/twi)
zephyr_include_directories(src/led)
zephyr_include_directories(src/algorithm)
zephyr_include_directories(src/utils)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/algorithm)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/algorithm/utilities)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/drivers)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/drivers/lis2mdl)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/filters)
zephyr_include_directories(src/algorithm/building-monitor-algorithm/interfaces)

if(CONFIG_ADD_CLAIM_CERTIFICATES)
    zephyr_include_directories(certs)
endif()

if(CONFIG_PROVISIONING)
    zephyr_include_directories(src/at_handler)
endif()

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/led/led_manager.c)
target_sources(app PRIVATE src/button_manager.c)
target_sources(app PRIVATE src/cloud/cloud.c)
target_sources(app PRIVATE src/cloud/codec.c)
target_sources(app PRIVATE src/wdt/wdt.c)
target_sources(app PRIVATE src/buzzer/buzzer.c)
target_sources(app PRIVATE src/interaction_manager.c)

target_sources(app PRIVATE src/twi/twi_driver.c)
target_sources(app PRIVATE src/algorithm/app.c)
target_sources(app PRIVATE src/algorithm/hw.c)
target_sources(app PRIVATE src/algorithm/flash_utility.c)
target_sources(app PRIVATE src/algorithm/error_handler.c)

target_sources(app PRIVATE src/algorithm/log_handler.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/axis_configuration_utility.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/communication_monitor.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/cycle_counter.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/lowest_flow_over_period_tracker.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/magnetometer_controller.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/noise_monitor.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/signal_history.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/utilities/timeServer.c)


# Add simulated or real magnetometer driver files based on config
if(CONFIG_SIMULATED_MAG)
    add_definitions(-DUSE_MAGNETOMETER -DSIMULATED_MAG)
    zephyr_include_directories(src/algorithm/building-monitor-algorithm/tests/mocked)
    zephyr_include_directories(src/algorithm/building-monitor-algorithm/tests/mocked/drivers)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/tests/mocked/drivers/magnetometer.c)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/tests/mocked/drivers/simulated_MAG_driver_HL.c)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/tests/mocked/signal.c)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/tests/mocked/hw_gpio.c)
else()
    add_definitions(-DUSE_MAGNETOMETER -DLIS2MDL)
    target_sources(app PRIVATE src/algorithm/hw_gpio.c)
    target_sources(app PRIVATE src/algorithm/magnetometer.c)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/drivers/lis2mdl/LIS2MDL_MAG_driver.c)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/drivers/lis2mdl/LIS2MDL_MAG_driver_HL.c)    
endif()

target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/filters/so_hpf.c)

# Always add pulse tracker and sensus protocol algorithms
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/pulse_tracker.c)
target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/sensus_protocol_app.c)

if(CONFIG_ALGORITHM STREQUAL "LOGGER")
    add_definitions(-DDALGORITHM_LOGGER)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/logger_algorithm.c)
elseif(CONFIG_ALGORITHM STREQUAL "MIN_MAX")
    add_definitions(-DALGORITHM_MIN_MAX)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/cycle_tracker.c)
elseif(CONFIG_ALGORITHM STREQUAL "ALGORITHM_DO_NOTHING")
    add_definitions(-DALGORITHM_DO_NOTHING)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/do_nothing_algorithm.c)
else()
    add_definitions(-DALGORITHM_FOLLOWER)
    target_sources(app PRIVATE src/algorithm/building-monitor-algorithm/algorithm/follower_algorithm.c)
endif()


# Note that here, we're adding CMAKE_SOURCE_DIR to the include path for nanopb.
# This is needed because the below call to nanopb_generate_cpp() is using
# 'RELPATH .'
#set(NANOPB_OPTIONS "-I${CMAKE_SOURCE_DIR}")
#nanopb_generate_cpp(proto_sources proto_headers RELPATH .
#    src/protobuf/msg.proto
#)
# we need to be able to include generated header files
###zephyr_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

zephyr_nanopb_sources(app src/protobuf/msg.proto)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# NORDIC SDK APP END

zephyr_include_directories(src)

