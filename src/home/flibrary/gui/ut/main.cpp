#include <QApplication>

#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
#ifdef Q_OS_LINUX
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen"); // CI machines have no display
#endif

	QApplication app(argc, argv);
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
