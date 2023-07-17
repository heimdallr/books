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
	[[nodiscard]] std::shared_ptr<QWidget> CreateTreeViewWidget(ItemType type) const override;
	[[nodiscard]] std::shared_ptr<IAddCollectionDialog> CreateAddCollectionDialog() const override;
	QMessageBox::StandardButton ShowWarning(const QString & title, const QString & text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) const override;

	[[nodiscard]] std::shared_ptr<AbstractTreeViewController> GetTreeViewController() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
