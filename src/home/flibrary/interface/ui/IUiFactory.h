#pragma once

#include <memory>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>

class QAbstractItemView;
class QAbstractScrollArea;
class QDialog;
class QWidget;

namespace HomeCompa::Flibrary {

class IUiFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IUiFactory() = default;

	[[nodiscard]] virtual QObject * GetParentObject() const noexcept = 0;

	[[nodiscard]] virtual std::shared_ptr<class TreeView> CreateTreeViewWidget(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpx) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IScriptDialog> CreateScriptDialog() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateBooks(QAbstractScrollArea & parent) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateNavigation(QAbstractItemView & parent) const = 0;

	virtual void ShowAbout() const = 0;
	[[nodiscard]] virtual QMessageBox::ButtonRole ShowCustomDialog(QMessageBox::Icon icon, const QString & title, const QString & text, const std::vector<std::pair<QMessageBox::ButtonRole, QString>> & buttons, QMessageBox::ButtonRole defaultButton = QMessageBox::NoRole) const = 0;
	[[nodiscard]] virtual QMessageBox::StandardButton ShowQuestion(const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	virtual QMessageBox::StandardButton ShowWarning(const QString & text, QMessageBox::StandardButtons buttons = QMessageBox::Ok, QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) const = 0;
	virtual void ShowInfo(const QString & text) const = 0;
	virtual void ShowError(const QString & text) const = 0;
	[[nodiscard]] virtual QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const = 0;
	[[nodiscard]] virtual std::optional<QFont> GetFont(const QString & title, const QFont & font, QFontDialog::FontDialogOptions options = {}) const = 0;

	[[nodiscard]] virtual QString GetOpenFileName(const QString & key, const QString & title, const QString & filter = {}, const QString & dir = {}, QFileDialog::Options options = {}) const = 0;
	[[nodiscard]] virtual QString GetSaveFileName(const QString & key, const QString & title, const QString & filter = {}, const QString & dir = {}, QFileDialog::Options options = {}) const = 0;
	[[nodiscard]] virtual QString GetExistingDirectory(const QString & key, const QString & title, const QString & dir = {}, QFileDialog::Options options = QFileDialog::ShowDirsOnly) const = 0;

public: // special
	[[nodiscard]] virtual std::filesystem::path GetNewCollectionInpx() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewController> GetTreeViewController() const noexcept = 0;
	[[nodiscard]] virtual QAbstractScrollArea & GetAbstractScrollArea() const noexcept = 0;
	[[nodiscard]] virtual QAbstractItemView & GetAbstractItemView() const noexcept = 0;
};

}
