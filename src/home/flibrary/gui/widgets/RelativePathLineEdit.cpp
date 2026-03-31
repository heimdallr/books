#include "ui_RelativePathLineEdit.h"

#include "RelativePathLineEdit.h"

#include <qabstractbutton.h>

#include <QFileInfo>

#include "gutil/interface/IUiFactory.h"
#include "util/ISettings.h"
#include "util/files.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto RECENT_RELATIVE = "ui/AddCollectionDialog/%1Relative";
constexpr auto RECENT_VALUE    = "ui/AddCollectionDialog/%1Value";

}

RelativePathLineEdit::RelativePathLineEdit(QWidget* parent)
	: QLineEdit(parent)
{
	m_ui->setupUi(this);

	connect(m_ui->actionToAbsolutePath, &QAction::triggered, this, [&] {
		m_ui->edit->setText(Util::ToAbsolutePath(m_ui->edit->text()));
		m_settings->Set(QString(RECENT_RELATIVE).arg(objectName()), false);
	});
	connect(m_ui->actionToRelativePath, &QAction::triggered, this, [&] {
		m_ui->edit->setText(Util::ToRelativePath(m_ui->edit->text()));
		m_settings->Set(QString(RECENT_RELATIVE).arg(objectName()), true);
	});
	connect(m_ui->edit, &QLineEdit::textChanged, this, [this](const QString& path) {
		m_ui->edit->removeAction(m_ui->actionToAbsolutePath);
		m_ui->edit->removeAction(m_ui->actionToRelativePath);

		emit Changed(m_ui->edit->text());

		if (path.isEmpty())
			return;

		if (const QFileInfo fileInfo(path); !fileInfo.isAbsolute())
			m_ui->edit->addAction(m_ui->actionToAbsolutePath, QLineEdit::TrailingPosition);
		else if (QCoreApplication::applicationFilePath().front().toUpper() == fileInfo.absoluteFilePath().front().toUpper())
			m_ui->edit->addAction(m_ui->actionToRelativePath, QLineEdit::TrailingPosition);
	});
	connect(m_ui->edit, &QLineEdit::textEdited, this, [this](const QString& path) {
		emit Edited(path);
	});
}

RelativePathLineEdit::~RelativePathLineEdit() = default;

void RelativePathLineEdit::Setup(const QDialog* dialog, ISettings* settings, const Util::IUiFactory* uiFactory, const QAbstractButton* btn, const QString& defaultValue)
{
	m_settings  = settings;
	m_uiFactory = uiFactory;
	m_ui->edit->setText(m_settings->Get(QString(RECENT_VALUE).arg(objectName()), defaultValue));

	connect(dialog, &QDialog::accepted, this, [this] {
		m_settings->Set(QString(RECENT_VALUE).arg(objectName()), m_ui->edit->text());
	});

	connect(btn, &QAbstractButton::clicked, this, [this] {
		auto path = m_isDir ? m_uiFactory->GetExistingDirectory(objectName(), m_selectDialogTitle, m_ui->edit->text())
		                    : m_uiFactory->GetSaveFileName(objectName(), m_selectDialogTitle, m_fileFilter, QFileInfo(m_ui->edit->text()).path(), QFileDialog::DontConfirmOverwrite);
		if (path.isEmpty())
			return;

		if (m_settings->Get(QString(RECENT_RELATIVE).arg(objectName()), false))
			path = Util::ToRelativePath(path);

		m_ui->edit->setText(path);
	});
}

QString RelativePathLineEdit::GetText() const
{
	return m_ui->edit->text();
}

void RelativePathLineEdit::SetText(const QString& value)
{
	m_ui->edit->setText(value);
}
