#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/IUiFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Util {

class UiFactory final : public IUiFactory
{
	NON_COPY_MOVABLE(UiFactory)

public:
	explicit UiFactory(Hypodermic::Container & container);
	~UiFactory() override;

private:
	[[nodiscard]] QObject * GetParentObject() const noexcept override;

	[[nodiscard]] QMessageBox::ButtonRole ShowCustomDialog(QMessageBox::Icon icon, const QString & title, const QString & text, const std::vector<std::pair<QMessageBox::ButtonRole, QString>> & buttons, QMessageBox::ButtonRole defaultButton) const override;
	[[nodiscard]] QMessageBox::StandardButton ShowQuestion(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
	QMessageBox::StandardButton ShowWarning(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
	void ShowInfo(const QString & text) const override;
	void ShowError(const QString & text) const override;
	[[nodiscard]] QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const override;
	[[nodiscard]] std::optional<QFont> GetFont(const QString & title, const QFont & font, QFontDialog::FontDialogOptions options = {}) const override;

	[[nodiscard]] QString GetOpenFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, QFileDialog::Options options) const override;
	[[nodiscard]] QString GetSaveFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, QFileDialog::Options options) const override;
	[[nodiscard]] QString GetExistingDirectory(const QString & key, const QString & title, const QString & dir, QFileDialog::Options options) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
