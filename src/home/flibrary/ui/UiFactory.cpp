#include "UiFactory.h"

#include <Hypodermic/Hypodermic.h>

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/dialogs/IDialog.h"

#include "util/ISettings.h"

#include "dialogs/AddCollectionDialog.h"

#include "TreeView.h"

namespace HomeCompa::Flibrary {

struct UiFactory::Impl
{
	Hypodermic::Container & container;

	mutable std::shared_ptr<AbstractTreeViewController> treeViewController;

	explicit Impl(Hypodermic::Container & container_)
		: container(container_)
	{
	}
};

UiFactory::UiFactory(Hypodermic::Container & container)
	: m_impl(container)
{
}

UiFactory::~UiFactory() = default;

std::shared_ptr<QWidget> UiFactory::CreateTreeViewWidget(const ItemType type) const
{
	m_impl->treeViewController = m_impl->container.resolve<ILogicFactory>()->CreateTreeViewController(type);
	return m_impl->container.resolve<TreeView>();
}

std::shared_ptr<IAddCollectionDialog> UiFactory::CreateAddCollectionDialog() const
{
	return m_impl->container.resolve<IAddCollectionDialog>();
}

QMessageBox::StandardButton UiFactory::ShowWarning(const QString & title, const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	const auto dialog = m_impl->container.resolve<IWarningDialog>();
	return dialog->Show(title, text, buttons, defaultButton);
}

std::shared_ptr<AbstractTreeViewController> UiFactory::GetTreeViewController() const
{
	assert(m_impl->treeViewController);
	return std::move(m_impl->treeViewController);
}

}
