#include <QCoreApplication>

#include <gtest/gtest.h>

#include "util/xml/Initializer.h"

int main(int argc, char* argv[])
{
	QCoreApplication                        app(argc, argv);
	HomeCompa::Util::XMLPlatformInitializer xmlPlatformInitializer;

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

namespace {

TEST(Logic, Main)
{
	SUCCEED();
}

}
