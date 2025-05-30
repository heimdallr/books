#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IUiFactory.h"

namespace Hypodermic
{
class Container;
}

namespace HomeCompa::Flibrary
{

class UiFactory final : public IUiFactory
{
	NON_COPY_MOVABLE(UiFactory)

public:
	explicit UiFactory(Hypodermic::Container& container);
	~UiFactory() override;

private: // IUiFactory
	QObject* GetParentObject(QObject* defaultObject) const noexcept override;
	QWidget* GetParentWidget(QWidget* defaultWidget) const noexcept override;

	std::shared_ptr<TreeView> CreateTreeViewWidget(ItemType type) const override;
	std::shared_ptr<IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpxFolder) const override;
	std::shared_ptr<IScriptDialog> CreateScriptDialog() const override;
	std::shared_ptr<ITreeViewDelegate> CreateTreeViewDelegateBooks(QTreeView& parent) const override;
	std::shared_ptr<ITreeViewDelegate> CreateTreeViewDelegateNavigation(QAbstractItemView& parent) const override;
	std::shared_ptr<QDialog> CreateOpdsDialog() const override;
	std::shared_ptr<IComboBoxTextDialog> CreateComboBoxTextDialog(QString title) const override;
	std::shared_ptr<QWidget> CreateCollectionCleaner(AdditionalWidgetCallback callback) const override;
	std::shared_ptr<QMainWindow> CreateQueryWindow() const override;

	void ShowAbout() const override;
	QMessageBox::ButtonRole ShowCustomDialog(QMessageBox::Icon icon,
	                                         const QString& title,
	                                         const QString& text,
	                                         const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
	                                         QMessageBox::ButtonRole defaultButton) const override;
	QMessageBox::StandardButton ShowQuestion(const QString& text, const QMessageBox::StandardButtons& buttons, QMessageBox::StandardButton defaultButton) const override;
	QMessageBox::StandardButton ShowWarning(const QString& text, const QMessageBox::StandardButtons& buttons, QMessageBox::StandardButton defaultButton) const override;
	void ShowInfo(const QString& text) const override;
	void ShowError(const QString& text) const override;
	QString GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, QLineEdit::EchoMode mode) const override;
	std::optional<QFont> GetFont(const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options = {}) const override;

	QStringList GetOpenFileNames(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString GetOpenFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString GetSaveFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const override;
	QString GetExistingDirectory(const QString& key, const QString& title, const QString& dir, const QFileDialog::Options& options) const override;

private: // special
	std::filesystem::path GetNewCollectionInpxFolder() const noexcept override;
	std::shared_ptr<ITreeViewController> GetTreeViewController() const noexcept override;
	QTreeView& GetTreeView() const noexcept override;
	QAbstractItemView& GetAbstractItemView() const noexcept override;
	QString GetTitle() const noexcept override;
	AdditionalWidgetCallback GetAdditionalWidgetCallback() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
