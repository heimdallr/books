#pragma once

#include <QLineEdit>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QAbstractButton;

namespace HomeCompa
{

class ISettings;

}

namespace HomeCompa::Util
{

class IUiFactory;

}

namespace Ui
{

class RelativePathLineEdit;

}

namespace HomeCompa::Flibrary
{

class RelativePathLineEdit : public QLineEdit
{
	Q_OBJECT
	NON_COPY_MOVABLE(RelativePathLineEdit)

	Q_PROPERTY(bool isDir MEMBER m_isDir)
	Q_PROPERTY(bool isSaveFile MEMBER m_isSaveFile)
	Q_PROPERTY(QString selectDialogTitle MEMBER m_selectDialogTitle)
	Q_PROPERTY(QString fileFilter MEMBER m_fileFilter)

signals:
	void Changed(const QString& value) const;
	void Edited(const QString& value) const;

public:
	RelativePathLineEdit(QWidget* parent = nullptr);
	~RelativePathLineEdit() override;

public:
	void    Setup(const QDialog* dialog, ISettings* settings, const Util::IUiFactory* uiFactory, const QAbstractButton* btn, const QString& defaultValue = {});
	QString GetText() const;
	void    SetText(const QString& value);

private:
	PropagateConstPtr<Ui::RelativePathLineEdit> m_ui;
	ISettings*                                  m_settings { nullptr };
	const Util::IUiFactory*                     m_uiFactory { nullptr };

	bool    m_isDir { false };
	bool    m_isSaveFile { false };
	QString m_selectDialogTitle;
	QString m_fileFilter;
};

} // namespace HomeCompa::Flibrary
