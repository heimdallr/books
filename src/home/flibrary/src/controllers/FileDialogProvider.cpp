#pragma warning(push, 0)
#include <QCoreApplication>
#include <QFileDialog>
#pragma warning(pop)

#include "FileDialogProvider.h"

namespace HomeCompa::Flibrary {

FileDialogProvider::FileDialogProvider(QObject * parent)
	: QObject(parent)
{
}

QString FileDialogProvider::SelectFile(const QString & fileName)
{
	return QFileDialog::getOpenFileName(nullptr, QCoreApplication::translate("FileDialog", "Select database file"), fileName);
}

QString FileDialogProvider::SelectFolder(const QString & folderName)
{
	return QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("FileDialog", "Select archives folder"), folderName);
}

bool FileDialogProvider::FileExists(const QString & fileName)
{
	return QFile::exists(fileName);
}

}
