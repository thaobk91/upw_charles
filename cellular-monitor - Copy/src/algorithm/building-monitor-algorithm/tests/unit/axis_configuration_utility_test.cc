#include <gtest/gtest.h>
#include "axis_configuration.h"
#include "axis_configuration_utility.h"

// Demonstrate some basic assertions.
TEST(AxisConfigurationUtility, MinOfPartOfArray) {
    int16_t array[5] = { 1, 2, 3, 4, 0};
    int16_t foundValue;
    uint8_t size = 4; // looking at first 4 
    findValueWithCorrelations(array, size, 10, 1, &foundValue, true);
    
    // Expect equality.
    EXPECT_EQ(1, foundValue);
}

TEST(AxisConfigurationUtility, MaxOfPartOfArray) {
    int16_t array[5] = { 1, 2, 3, 4, 5};
    int16_t foundValue;
    uint8_t size = 4; // looking at first 4 
    findValueWithCorrelations(array, size, 10, 1, &foundValue, false);
    
    // Expect equality.
    EXPECT_EQ(4, foundValue);
}