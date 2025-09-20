#pragma once

#include "gutil/interface/IUiFactory.h"

class QAbstractItemView;
class QMainWindow;
class QTreeView;

namespace HomeCompa::Flibrary
{

class IUiFactory : virtual public Util::IUiFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	[[nodiscard]] virtual std::shared_ptr<class TreeView> CreateTreeViewWidget(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpxFolder) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IScriptDialog> CreateScriptDialog() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateBooks(QTreeView& parent) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateNavigation(QAbstractItemView& parent) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QDialog> CreateOpdsDialog() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IComboBoxTextDialog> CreateComboBoxTextDialog(QString title) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QMainWindow> CreateQueryWindow() const = 0;
	virtual void CreateCollectionCleaner() const = 0;
	virtual void CreateAuthorReview(long long id) const = 0;

	virtual void ShowAbout() const = 0;

public: // special
	[[nodiscard]] virtual std::filesystem::path GetNewCollectionInpxFolder() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewController> GetTreeViewController() const noexcept = 0;
	[[nodiscard]] virtual QTreeView& GetTreeView() const noexcept = 0;
	[[nodiscard]] virtual QAbstractItemView& GetAbstractItemView() const noexcept = 0;
	[[nodiscard]] virtual QString GetTitle() const noexcept = 0;
	[[nodiscard]] virtual long long GetAuthorId() const noexcept = 0;
};

} // namespace HomeCompa::Flibrary
