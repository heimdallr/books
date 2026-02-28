#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/IUiFactory.h"

namespace Hypodermic
{

class Container;

}

namespace HomeCompa::Util
{

class UiFactory final : public IUiFactory
{
	NON_COPY_MOVABLE(UiFactory)

public:
	explicit UiFactory(Hypodermic::Container& container);
	~UiFactory() override;

private:
	QObject* GetParentObject(QObject* defaultObject) const noexcept override;
	QWidget* GetParentWidget(QWidget* defaultWidget) const noexcept override;

	QMessageBox::ButtonRole ShowCustomDialog(
		QMessageBox::Icon                                               icon,
		const QString&                                                  title,
		const QString&                                                  text,
		const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
		QMessageBox::ButtonRole                                         defaultButton,
		const QString&                                                  detailedText
	) const override;
	QMessageBox::StandardButton ShowQuestion(const QString& text, const QMessageBox::StandardButtons& buttons, QMessageBox::StandardButton defaultButton) const override;
	QMessageBox::StandardButton ShowWarning(const QString& text, const QMessageBox::StandardButtons& buttons, QMessageBox::StandardButton defaultButton) const override;
	void                        ShowInfo(const QString& text) const override;
	void                        ShowError(const QString& text) const override;
	QString                     GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, QLineEdit::EchoMode mode) const override;
	std::optional<QFont>        GetFont(const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options = {}) const override;

	QStringList GetOpenFileNames(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString     GetOpenFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString     GetSaveFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString     GetExistingDirectory(const QString& key, const QString& title, const QString& dir, const QFileDialog::Options& options) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Util
