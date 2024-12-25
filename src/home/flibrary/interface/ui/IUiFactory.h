#pragma once

#include "GuiUtil/interface/IUiFactory.h"

class QAbstractItemView;
class QAbstractScrollArea;

namespace HomeCompa::Flibrary {

class IUiFactory : virtual public Util::IUiFactory // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	[[nodiscard]] virtual std::shared_ptr<class TreeView> CreateTreeViewWidget(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IAddCollectionDialog> CreateAddCollectionDialog(std::filesystem::path inpx) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IScriptDialog> CreateScriptDialog() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateBooks(QAbstractScrollArea & parent) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewDelegate> CreateTreeViewDelegateNavigation(QAbstractItemView & parent) const = 0;

	virtual void ShowAbout() const = 0;

public: // special
	[[nodiscard]] virtual std::filesystem::path GetNewCollectionInpx() const noexcept = 0;
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewController> GetTreeViewController() const noexcept = 0;
	[[nodiscard]] virtual QAbstractScrollArea & GetAbstractScrollArea() const noexcept = 0;
	[[nodiscard]] virtual QAbstractItemView & GetAbstractItemView() const noexcept = 0;
};

}
