#include "version/AppVersion.h"

#include <QString>

#include <gtest/gtest.h>

#include "config/version.h"

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

namespace {

using namespace HomeCompa;

TEST(AppVersionTest, GetApplicationVersion)
{
	const auto version = HomeCompa::Flibrary::GetApplicationVersion();
	EXPECT_TRUE(version.contains(PRODUCT_VERSION));
}

}
