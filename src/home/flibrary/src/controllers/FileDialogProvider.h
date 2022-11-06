#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"

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

public:
	explicit FileDialogProvider(QObject * parent = nullptr);
};

}
