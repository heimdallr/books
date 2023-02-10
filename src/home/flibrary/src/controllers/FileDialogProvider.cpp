#pragma warning(push, 0)
#include <QFileDialog>
#pragma warning(pop)

#include "FileDialogProvider.h"

namespace HomeCompa::Flibrary {

FileDialogProvider::FileDialogProvider(QObject * parent)
	: QObject(parent)
{
}

QString FileDialogProvider::SelectFile(const QString & title, const QString & fileName, const QString & filter)
{
	return QFileDialog::getOpenFileName(nullptr, title, fileName, filter);
}

QString FileDialogProvider::SaveFile(const QString & title, const QString & fileName, const QString & filter)
{
	return QFileDialog::getSaveFileName(nullptr, title, fileName, filter, nullptr, QFileDialog::DontConfirmOverwrite);
}

QString FileDialogProvider::SelectFolder(const QString & title, const QString & folderName)
{
	return QFileDialog::getExistingDirectory(nullptr, title, folderName);
}

bool FileDialogProvider::FileExists(const QString & fileName)
{
	return QFile::exists(fileName);
}

}
