// Opens the native "choose folder" dialog the way FLibrary's collection dialog does, then exits after a few seconds.
// Linked against FLibrary's libraries so their exported symbols are in the global lookup scope, as in the real app.
#include <QApplication>
#include <QFileDialog>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QTimer::singleShot(4000, [] {
		std::puts("dialog stayed up for 4 s without crashing");
		std::fflush(stdout);
		std::_Exit(0);
	});
	const auto dir = QFileDialog::getExistingDirectory(nullptr, "Select archives folder", QDir::homePath());
	std::printf("dialog returned '%s'\n", qPrintable(dir));
	return 0;
}
