#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "cycle_counter.h"
#include "algo_hw.h"
}

// Demonstrate some basic assertions.
TEST(CycleCounterTest, Init) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  EXPECT_EQ(0, cycle_counter_number_timestamps());
  EXPECT_EQ(0, cycle_counter_get_half_cycles());
  EXPECT_EQ(0, cycle_counter_get_timestamp(0));
  TimerTime_t longestDuration = 0;
  bool ret = cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration);
  EXPECT_EQ(false, ret);
  EXPECT_EQ(0, longestDuration);
}

TEST(CycleCounterTest, AddOne) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(1, cycle_counter_number_timestamps());

  EXPECT_EQ(1, cycle_counter_get_half_cycles());
  
  // Make sure timestamp is not zero
  EXPECT_NE(0, cycle_counter_get_timestamp(0));

  // Make sure next timestamp is  zero
  EXPECT_EQ(0, cycle_counter_get_timestamp(1));
}

// Add multiple half cycles, test that they come back right, and check longest duration
TEST(CycleCounterTest, MultipleAdd) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(1, cycle_counter_number_timestamps());
  EXPECT_EQ(1, cycle_counter_get_half_cycles());
  EXPECT_NE(0, cycle_counter_get_timestamp(0));

  HAL_Delay(2);
  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(2, cycle_counter_number_timestamps());
  EXPECT_EQ(2, cycle_counter_get_half_cycles());
  EXPECT_NE(0, cycle_counter_get_timestamp(0));
  EXPECT_NE(0, cycle_counter_get_timestamp(1));
  EXPECT_EQ(2, cycle_counter_get_timestamp(0) - cycle_counter_get_timestamp(1));
  TimerTime_t longestDuration;
  EXPECT_EQ(false, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration)); // Averaging over 4 half cycles, need at least 4

  HAL_Delay(4);
  cycle_counter_add_half_cycle(true);
  
  EXPECT_EQ(3, cycle_counter_number_timestamps());
  EXPECT_EQ(3, cycle_counter_get_half_cycles());
  EXPECT_NE(0, cycle_counter_get_timestamp(0));
  EXPECT_NE(0, cycle_counter_get_timestamp(1));
  EXPECT_NE(0, cycle_counter_get_timestamp(2));
  EXPECT_EQ(false, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration)); // Averaging over 4 half cycles, need at least 4

  HAL_Delay(4);
  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(4, cycle_counter_number_timestamps());
  EXPECT_EQ(4, cycle_counter_get_half_cycles());
  EXPECT_NE(0, cycle_counter_get_timestamp(0));
  EXPECT_NE(0, cycle_counter_get_timestamp(1));
  EXPECT_NE(0, cycle_counter_get_timestamp(2));
  EXPECT_NE(0, cycle_counter_get_timestamp(3));

  EXPECT_EQ(false, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration)); // Averaging over 4 half cycles, need at least 4

  HAL_Delay(4);
  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(5, cycle_counter_number_timestamps());
  EXPECT_EQ(5, cycle_counter_get_half_cycles());
  EXPECT_NE(0, cycle_counter_get_timestamp(0));
  EXPECT_NE(0, cycle_counter_get_timestamp(1));
  EXPECT_NE(0, cycle_counter_get_timestamp(2));
  EXPECT_NE(0, cycle_counter_get_timestamp(3));
  EXPECT_NE(0, cycle_counter_get_timestamp(4));

  EXPECT_EQ(true, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration));
  printf("longestDuration: %d\n", longestDuration);
  EXPECT_EQ(abs(int(longestDuration - 8)) <= 1, true); // 4 ms * 2 half cycles = 8 ms

  printf("Timestamp 0: %d\n", cycle_counter_get_timestamp(0));
  printf("Timestamp 1: %d\n", cycle_counter_get_timestamp(1));
  printf("Timestamp 2: %d\n", cycle_counter_get_timestamp(2));
  
}

TEST(CycleCounterTest, AddALot) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);
  printf("Number of timestamps: %d\n", cycle_counter_number_timestamps());

  uint8_t numAdds = 100;
  TimerTime_t longestDuration;

  for(int i = 0; i < numAdds; i++) {
    HAL_Delay(2);
    cycle_counter_add_half_cycle(true);
  }
  
  printf("Number of timestamps: %d\n", cycle_counter_number_timestamps());
  EXPECT_NE(0, cycle_counter_number_timestamps());
  EXPECT_EQ(numAdds, cycle_counter_get_half_cycles());
  EXPECT_EQ(true, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration));
  printf("longestDuration: %d\n", longestDuration);
  EXPECT_EQ(abs(int(longestDuration - 4)) <= 1, true);
}

TEST(CycleCounterTest, Reset) {
  TimerTime_t longestDuration;

  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  cycle_counter_add_half_cycle(true);

  cycle_counter_reset(true);

  EXPECT_EQ(0, cycle_counter_number_timestamps());
  EXPECT_EQ(0, cycle_counter_get_half_cycles());
  
  // Make sure timestamp is not zero
  EXPECT_EQ(0, cycle_counter_get_timestamp(0));
  EXPECT_EQ(false, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration));
}


TEST(CycleCounterTest, Decrement) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  cycle_counter_add_half_cycle(true);
  cycle_counter_add_half_cycle(true);
  cycle_counter_add_half_cycle(true);

  EXPECT_EQ(3, cycle_counter_number_timestamps());

  EXPECT_EQ(2, cycle_counter_decrement_half_cycles(1));
  EXPECT_EQ(2, cycle_counter_get_half_cycles());

  EXPECT_EQ(1, cycle_counter_decrement_half_cycles(1));
  EXPECT_EQ(1, cycle_counter_get_half_cycles());

  EXPECT_EQ(0, cycle_counter_decrement_half_cycles(1));
  EXPECT_EQ(0, cycle_counter_get_half_cycles());

  EXPECT_EQ(0, cycle_counter_decrement_half_cycles(1));
  EXPECT_EQ(0, cycle_counter_get_half_cycles());
}

TEST(CycleCounterTest, start_new_period) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  uint8_t numAdds = 10;
  TimerTime_t longestDuration;

  for(int i = 0; i < numAdds; i++) {
    HAL_Delay(2);
    cycle_counter_add_half_cycle(true);
  }
  
  cycle_counter_start_new_period();
  printf("Number of timestamps: %d\n", cycle_counter_number_timestamps());
  EXPECT_NE(0, cycle_counter_number_timestamps());
  EXPECT_EQ(numAdds, cycle_counter_get_half_cycles());
  EXPECT_EQ(false, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration));

  // Add another, should get longest duration again
  HAL_Delay(2);
  cycle_counter_add_half_cycle(true);
  EXPECT_EQ(true, cycle_counter_get_longest_duration_between_pulses_in_ms(&longestDuration));
  printf("longestDuration: %d\n", longestDuration);
  EXPECT_EQ(abs(int(longestDuration - 4)) <= 1, true);
}

TEST(CycleCounterTest, cycle_counter_delete_recent_cycles) {
  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  uint8_t numAdds = 10;
  TimerTime_t longestDuration;

  for(int i = 0; i < numAdds; i++) {
    HAL_Delay(2);
    cycle_counter_add_half_cycle(true);
  }
  
  printf("Number of timestamps before: %d\n", cycle_counter_number_timestamps());
  EXPECT_NE(0, cycle_counter_number_timestamps());
  EXPECT_EQ(numAdds, cycle_counter_get_half_cycles());

  // Should delete all of them since they're all recent
  cycle_counter_delete_recent_cycles();

  EXPECT_EQ(0, cycle_counter_number_timestamps());
  EXPECT_EQ(0, cycle_counter_get_half_cycles());
}

TEST(CycleCounterTest, cycle_counter_delete_recent_cycles_but_1) {
  HW_RTC_SetMultiplier(100); // 100x faster due to wait

  cycle_counter_init(CYCLE_MODE_PULSE_COUNTING);

  uint8_t numAdds = 10;
  TimerTime_t longestDuration;
  cycle_counter_add_half_cycle(true);
  HAL_Delay(5000);
  for(int i = 0; i < numAdds; i++) {
    HAL_Delay(2);
    cycle_counter_add_half_cycle(true);
  }
  
  printf("Number of timestamps before: %d\n", cycle_counter_number_timestamps());
  EXPECT_NE(0, cycle_counter_number_timestamps());
  EXPECT_EQ(numAdds + 1, cycle_counter_get_half_cycles());

  // Should delete all of them since they're all recent
  cycle_counter_delete_recent_cycles();

  EXPECT_EQ(1, cycle_counter_number_timestamps());
  EXPECT_EQ(1, cycle_counter_get_half_cycles());
}