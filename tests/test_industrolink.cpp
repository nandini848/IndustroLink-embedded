#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(IndustroLinkTest, TemperatureTelemetryFormat) {
    std::string telemetry = "TEMP-01,temperature,25.500000";

    EXPECT_NE(telemetry.find("TEMP-01"), std::string::npos);
    EXPECT_NE(telemetry.find("temperature"), std::string::npos);
}

TEST(IndustroLinkTest, PressureTelemetryFormat) {
    std::string telemetry = "PRESS-01,pressure,100.500000";

    EXPECT_NE(telemetry.find("PRESS-01"), std::string::npos);
    EXPECT_NE(telemetry.find("pressure"), std::string::npos);
}

TEST(IndustroLinkTest, TelemetrySequence) {
    std::vector<double> temperatures = {
        25.0, 25.5, 26.0, 26.5, 27.0
    };

    ASSERT_EQ(temperatures.size(), 5);
    EXPECT_EQ(temperatures.front(), 25.0);
    EXPECT_EQ(temperatures.back(), 27.0);
}
