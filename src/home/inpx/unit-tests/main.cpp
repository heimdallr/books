#include <QCoreApplication>

#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
	[[maybe_unused]] QCoreApplication app(argc, argv);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

namespace {

TEST(Inpx, Main)
{
	SUCCEED();
}

}
