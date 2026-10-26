// Writes and reads back a JPEG through Qt's jpeg image plugin while FLibrary's libraries are loaded,
// to see whether their exported libjpeg symbols hijack the plugin ("Wrong JPEG library version").
#include <QGuiApplication>
#include <QImage>

#include <cstdio>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);
	const auto path = QString("%1/repro.jpg").arg(argc > 1 ? argv[1] : "/tmp");

	QImage image(32, 32, QImage::Format_RGB32);
	image.fill(Qt::darkCyan);
	const bool saved  = image.save(path, "JPG");
	const auto loaded = QImage(path);
	std::printf("saved=%d loaded=%d size=%dx%d\n", saved, !loaded.isNull(), loaded.width(), loaded.height());
	return (saved && !loaded.isNull()) ? 0 : 1;
}
