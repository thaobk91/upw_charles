#include <gtest/gtest.h>
#include <stdio.h>

extern "C" {
#include "signal_history.h"
#include "algo_hw.h"
}

// Demonstrate some basic assertions.
TEST(signal_history_test, add_0) {
  int16_t diff;
  EXPECT_EQ(false, signal_history_get_min_max_diff(&diff));
}

TEST(signal_history_test, add_1) {
  int16_t diff;
  signal_history_add(1000, &diff);
  EXPECT_EQ(false, signal_history_get_min_max_diff(&diff));
  EXPECT_EQ(0, diff);
}

TEST(signal_history_test, add_20_alternating) {
  const int16_t MIN = 1000;
  const int16_t MAX = 2000;
  int16_t diff;

  for (size_t i = 0; i < 20; i++)
  {
    if (i % 2 == 0)
    {
      signal_history_add(MIN, &diff);
    }
    else
    {
      signal_history_add(MAX, &diff);
    }
  }
  
  EXPECT_EQ(true, signal_history_get_min_max_diff(&diff));
  EXPECT_EQ(1000, diff);
}

TEST(signal_history_test, add_20_same) {
  const int16_t MIN = 1000;
  const int16_t MAX = 1000;
  int16_t diff;

  for (size_t i = 0; i < 20; i++)
  {
    if (i % 2 == 0)
    {
      signal_history_add(MIN, &diff);
    }
    else
    {
      signal_history_add(MAX, &diff);
    }
  }
  
  EXPECT_EQ(true, signal_history_get_min_max_diff(&diff));
  EXPECT_EQ(0, diff);
}

TEST(signal_history_test, add_20_large_then_20_small) {
  int16_t MIN = 1000;
  int16_t MAX = 2000;
  int16_t diff;

  for (size_t i = 0; i < 20; i++)
  {
    if (i % 2 == 0)
    {
      signal_history_add(MIN, &diff);
    }
    else
    {
      signal_history_add(MAX, &diff);
    }
  }
  
  EXPECT_EQ(true, signal_history_get_min_max_diff(&diff));
  EXPECT_EQ(1000, diff);

  MIN = 1000;
  MAX = 1100;
  for (size_t i = 0; i < 20; i++)
  {
    if (i % 2 == 0)
    {
      EXPECT_EQ(true, signal_history_add(MIN, &diff));
    }
    else
    {
     EXPECT_EQ(true,  signal_history_add(MAX, &diff));
    }
  }
  
  EXPECT_EQ(true, signal_history_get_min_max_diff(&diff));
  EXPECT_EQ(100, diff);
}