#pragma once

#include <memory>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>

class QDialog;
class QWidget;

namespace HomeCompa::Util
{

class IUiFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IUiFactory() = default;

	[[nodiscard]] virtual QObject* GetParentObject(QObject* defaultObject = nullptr) const noexcept = 0;
	[[nodiscard]] virtual QWidget* GetParentWidget(QWidget* defaultWidget = nullptr) const noexcept = 0;

	[[nodiscard]] virtual QMessageBox::ButtonRole ShowCustomDialog(QMessageBox::Icon icon,
	                                                               const QString& title,
	                                                               const QString& text,
	                                                               const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
	                                                               QMessageBox::ButtonRole defaultButton = QMessageBox::NoRole) const = 0;
	[[nodiscard]] virtual QMessageBox::StandardButton
	ShowQuestion(const QString& text, const QMessageBox::StandardButtons& buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	virtual QMessageBox::StandardButton
	ShowWarning(const QString& text, const QMessageBox::StandardButtons& buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	virtual void ShowInfo(const QString& text) const = 0;
	virtual void ShowError(const QString& text) const = 0;
	[[nodiscard]] virtual QString GetText(const QString& title, const QString& label, const QString& text = {}, const QStringList& comboBoxItems = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const = 0;
	[[nodiscard]] virtual std::optional<QFont> GetFont(const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options = {}) const = 0;

	[[nodiscard]] virtual QString GetOpenFileName(const QString& key, const QString& title, const QString& filter = {}, const QString& dir = {}, const QFileDialog::Options& options = {}) const = 0;
	[[nodiscard]] virtual QString GetSaveFileName(const QString& key, const QString& title, const QString& filter = {}, const QString& dir = {}, const QFileDialog::Options& options = {}) const = 0;
	[[nodiscard]] virtual QString GetExistingDirectory(const QString& key, const QString& title, const QString& dir = {}, const QFileDialog::Options& options = QFileDialog::ShowDirsOnly) const = 0;
};

} // namespace HomeCompa::Util
