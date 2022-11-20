#pragma once

#include <QObject>

namespace HomeCompa {
class Settings;
}

namespace HomeCompa::Flibrary {

class FileDialogProvider
	: public QObject
{
	Q_OBJECT

public:
	Q_INVOKABLE static QString SelectFile(const QString & fileName);
	Q_INVOKABLE static QString SelectFolder(const QString & folderName);
	Q_INVOKABLE static bool FileExists(const QString & fileName);

public:
	explicit FileDialogProvider(QObject * parent = nullptr);
};

}
