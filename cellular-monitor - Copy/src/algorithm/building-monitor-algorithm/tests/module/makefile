CC=gcc
SOURCE_DIR=./../..

INTERFACE_DIR=$(SOURCE_DIR)/interfaces
ALGORITHM_DIR=$(SOURCE_DIR)/algorithm
DRIVERS_INC_DIR=$(SOURCE_DIR)/drivers
ALGORITHM_UTILITIES_DIR=$(ALGORITHM_DIR)/utilities
ALGORITHM_UTILITIES_FILES= $(ALGORITHM_UTILITIES_DIR)/timeServer.c $(ALGORITHM_UTILITIES_DIR)/cycle_counter.c $(ALGORITHM_UTILITIES_DIR)/lowest_flow_over_period_tracker.c $(ALGORITHM_UTILITIES_DIR)/noise_monitor.c $(ALGORITHM_UTILITIES_DIR)/communication_monitor.c $(ALGORITHM_UTILITIES_DIR)/magnetometer_controller.c $(ALGORITHM_UTILITIES_DIR)/axis_configuration_utility.c $(ALGORITHM_UTILITIES_DIR)/signal_history.c

MOCK_DIR=$(SOURCE_DIR)/tests/mocked
MOCKED_FILES= $(MOCK_DIR)/debug.c $(MOCK_DIR)/flash_utility.c $(MOCK_DIR)/signal.c $(MOCK_DIR)/hw.c $(MOCK_DIR)/hw_gpio.c $(MOCK_DIR)/mocked_app.c

UTILITIES_DIR=$(MOCK_DIR)/utilities
UTILITIES_FILES= $(UTILITIES_DIR)/utilities.c

DRIVERS_DIR=$(MOCK_DIR)/drivers
DRIVERS_FILES= $(DRIVERS_DIR)/bsp.c $(DRIVERS_DIR)/magnetometer.c $(DRIVERS_DIR)/simulated_MAG_driver_HL.c

FILTERS_DIR=$(SOURCE_DIR)/filters
FILTERS_FILES= $(FILTERS_DIR)/so_hpf.c #$(FILTERS_DIR)/so_butterworth_hpf.c $(FILTERS_DIR)/FIRFilter.c

CFLAGS= -Wall -I. -I$(DRIVERS_INC_DIR) -I$(INTERFACE_DIR) -I$(ALGORITHM_DIR) -I$(UTILITIES_DIR) -I$(DRIVERS_DIR) -I$(FILTERS_DIR) -I$(MOCK_DIR) -I$(ALGORITHM_UTILITIES_DIR) -D SIMULATED_MAG -D USE_MAGNETOMETER
CFLAGS += -DSIMULATION

ifeq ($(DISABLE_LOGGING),1)
CFLAGS += -DDISABLE_LOGGING
endif

all: logger follower # filterTest pollAndInterrupt min_max pulse_tracker

follower: main.c
	$(CC) $(CFLAGS) -D ALGORITHM_FOLLOWER \
	-o build/follower main.c \
	$(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
	$(ALGORITHM_UTILITIES_FILES) \
	$(ALGORITHM_DIR)/follower_algorithm.c \

# pulse_tracker: main.c
# 	$(CC) $(CFLAGS) -D ALGORITHM_PULSE_TRACKER \
# 	-o build/pulse_tracker main.c \
# 	$(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
# 	$(ALGORITHM_UTILITIES_FILES) \
# 	$(ALGORITHM_DIR)/pulse_tracker.c \

logger: main.c
	$(CC) $(CFLAGS) -D ALGORITHM_LOGGER \
	-o build/logger main.c \
	$(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
	$(ALGORITHM_UTILITIES_FILES) \
	$(ALGORITHM_DIR)/logger_algorithm.c \

# min_max: main.c
# 	$(CC) $(CFLAGS) -D ALGORITHM_MIN_MAX \
# 	-o build/interrupt main.c \
# 	$(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
# 	$(ALGORITHM_UTILITIES_FILES) \
# 	$(ALGORITHM_DIR)/cycle_tracker.c \

# pollAndInterrupt: main.c
# 	$(CC) $(CFLAGS) -D ALGORITHM_POLL_AND_INT \
# 	-o build/pollAndInterrupt main.c \
# 	$(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
# 	$(ALGORITHM_UTILITIES_FILES) \
# 	$(ALGORITHM_DIR)/signal_tracker.c $(ALGORITHM_DIR)/poll_and_int_algorithm.c \


# filter: filterTest.c
# 	$(CC) $(CFLAGS) -o build/filterTest filterTest.c $(MOCKED_FILES) $(UTILITIES_FILES) $(DRIVERS_FILES) $(FILTERS_FILES) \
# 	$(ALGORITHM_DIR)axis_configuration_utility.c $(ALGORITHM_DIR)signal_tracker.c $(ALGORITHM_DIR)noise_monitor.c $(ALGORITHM_DIR)polling_algorithm.c $(ALGORITHM_DIR)communication_monitor.c \
# 	$(ALGORITHM_DIR)cycle_counter.c $(ALGORITHM_DIR)lowest_flow_over_period_tracker.c \
# 	$(FILTERS_DIR)/so_butterworth_hpf.c $(FILTERS_DIR)/FIRFilter.c 
