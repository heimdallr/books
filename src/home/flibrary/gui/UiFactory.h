#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/ui/IUiFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Flibrary {

class UiFactory final : virtual public IUiFactory
{
	NON_COPY_MOVABLE(UiFactory)

public:
	explicit UiFactory(Hypodermic::Container & container);
	~UiFactory() override;

private: // IUiFactory
	[[nodiscard]] QObject * GetParentObject() const noexcept override;

	[[nodiscard]] std::shared_ptr<TreeView> CreateTreeViewWidget(ItemType type) const override;
	[[nodiscard]] std::shared_ptr<IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpx) const override;
	[[nodiscard]] std::shared_ptr<QAbstractItemDelegate> CreateTreeViewDelegateBooks(QAbstractScrollArea & parent) const override;

	void ShowAbout() const override;
	[[nodiscard]] QMessageBox::StandardButton ShowQuestion(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
	QMessageBox::StandardButton ShowWarning(const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;
	void ShowInfo(const QString & text) const override;
	void ShowError(const QString & text) const override;
	[[nodiscard]] QString GetText(const QString & title, const QString & label, const QString & text = {}, QLineEdit::EchoMode mode = QLineEdit::Normal) const override;
	[[nodiscard]] std::optional<QFont> GetFont(const QString & title, const QFont & font, QFontDialog::FontDialogOptions options = {}) const override;

	[[nodiscard]] QString GetOpenFileName(const QString & title, const QString & dir = {}, const QString & filter = {}, QFileDialog::Options options = {}) const override;
	[[nodiscard]] QString GetSaveFileName(const QString & title, const QString & dir = {}, const QString & filter = {}, QFileDialog::Options options = {}) const override;
	[[nodiscard]] QString GetExistingDirectory(const QString & title, const QString & dir = {}, QFileDialog::Options options = QFileDialog::ShowDirsOnly) const override;

private: // special
	[[nodiscard]] std::filesystem::path GetNewCollectionInpx() const noexcept override;
	[[nodiscard]] std::shared_ptr<AbstractTreeViewController> GetTreeViewController() const noexcept override;
	[[nodiscard]] QAbstractScrollArea & GetAbstractScrollArea() const noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
