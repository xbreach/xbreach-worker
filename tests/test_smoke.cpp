#include <gtest/gtest.h>

#include "version.hpp"

TEST(Smoke, VersionIsNotEmpty) {
    EXPECT_STRNE(xbreach::worker::version(), "");
}
