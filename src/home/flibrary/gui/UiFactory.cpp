#include "UiFactory.h"

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include <QFileInfo>
#include <QPushButton>

#include "fnd/FindPair.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/dialogs/IScriptDialog.h"
#include "interface/ui/IRateStarsProvider.h"

#include "delegate/TreeViewDelegate/TreeViewDelegateBooks.h"
#include "delegate/TreeViewDelegate/TreeViewDelegateNavigation.h"
#include "dialogs/AddCollectionDialog.h"

#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "GuiUtil/interface/IUiFactory.h"
#include "util/ISettings.h"
#include "util/localization.h"
#include "version/AppVersion.h"

#include "ItemViewToolTipper.h"
#include "TreeView.h"

namespace HomeCompa::Flibrary {

namespace {
constexpr auto CONTEXT = "Dialog";
constexpr auto ABOUT_TITLE = QT_TRANSLATE_NOOP("Dialog", "About FLibrary");
constexpr auto ABOUT_TEXT = QT_TRANSLATE_NOOP("Dialog", "Another e-library book cataloger<p>Version: %1<p><a href='%2'>%2</a>");
}

struct UiFactory::Impl
{
	Hypodermic::Container & container;

	mutable std::filesystem::path inpx;
	mutable std::shared_ptr<ITreeViewController> treeViewController;
	mutable QAbstractScrollArea * abstractScrollArea { nullptr };
	mutable QAbstractItemView * abstractItemView { nullptr };

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

UiFactory::UiFactory(Hypodermic::Container & container)
	: m_impl(container)
{
	PLOGD << "UiFactory created";
}

UiFactory::~UiFactory()
{
	PLOGD << "UiFactory destroyed";
}

QObject * UiFactory::GetParentObject() const noexcept
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetParentObject();
}

std::shared_ptr<TreeView> UiFactory::CreateTreeViewWidget(const ItemType type) const
{
	m_impl->treeViewController = m_impl->container.resolve<ILogicFactory>()->GetTreeViewController(type);
	return m_impl->container.resolve<TreeView>();
}

std::shared_ptr<IAddCollectionDialog> UiFactory::CreateAddCollectionDialog(std::filesystem::path inpx) const
{
	m_impl->inpx = std::move(inpx);
	return m_impl->container.resolve<IAddCollectionDialog>();
}

std::shared_ptr<IScriptDialog> UiFactory::CreateScriptDialog() const
{
	return m_impl->container.resolve<IScriptDialog>();
}

std::shared_ptr<ITreeViewDelegate> UiFactory::CreateTreeViewDelegateBooks(QAbstractScrollArea & parent) const
{
	m_impl->abstractScrollArea = &parent;
	return m_impl->container.resolve<TreeViewDelegateBooks>();
}

std::shared_ptr<ITreeViewDelegate> UiFactory::CreateTreeViewDelegateNavigation(QAbstractItemView & parent) const
{
	m_impl->abstractItemView = &parent;
	return m_impl->container.resolve<TreeViewDelegateNavigation>();
}

void UiFactory::ShowAbout() const
{
	auto * parent = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget();
	QMessageBox msgBox(parent);
	msgBox.setFont(parent->font());
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setWindowTitle(Loc::Tr(CONTEXT, ABOUT_TITLE));
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(Loc::Tr(CONTEXT, ABOUT_TEXT).arg(GetApplicationVersion(), "https://github.com/heimdallr/books"));
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.exec();
}

QMessageBox::ButtonRole UiFactory::ShowCustomDialog(const QMessageBox::Icon icon, const QString & title, const QString & text, const std::vector<std::pair<QMessageBox::ButtonRole, QString>> & buttons, const QMessageBox::ButtonRole defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowCustomDialog(icon, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowQuestion(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowQuestion(text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowWarning(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowWarning(text, buttons, defaultButton);
}

void UiFactory::ShowInfo(const QString & text) const
{
	m_impl->container.resolve<Util::IUiFactory>()->ShowInfo(text);
}

void UiFactory::ShowError(const QString & text) const
{
	m_impl->container.resolve<Util::IUiFactory>()->ShowError(text);
}

QString UiFactory::GetText(const QString & title, const QString & label, const QString & text, const QLineEdit::EchoMode mode) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetText(title, label, text, mode);
}

std::optional<QFont> UiFactory::GetFont(const QString & title, const QFont & font, const QFontDialog::FontDialogOptions options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetFont(title, font, options);
}

QString UiFactory::GetOpenFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, const QFileDialog::Options options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetOpenFileName(key, title, filter, dir, options);
}

QString UiFactory::GetSaveFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, const QFileDialog::Options options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetSaveFileName(key, title, filter, dir, options);
}

QString UiFactory::GetExistingDirectory(const QString & key, const QString & title, const QString & dir, const QFileDialog::Options options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetExistingDirectory(key, title, dir, options);
}

std::filesystem::path UiFactory::GetNewCollectionInpx() const noexcept
{
	auto result = std::move(m_impl->inpx);
	m_impl->inpx = std::filesystem::path {};
	return result;
}

std::shared_ptr<ITreeViewController> UiFactory::GetTreeViewController() const noexcept
{
	assert(m_impl->treeViewController);
	return std::move(m_impl->treeViewController);
}

QAbstractScrollArea & UiFactory::GetAbstractScrollArea() const noexcept
{
	assert(m_impl->abstractScrollArea);
	return *m_impl->abstractScrollArea;
}

QAbstractItemView & UiFactory::GetAbstractItemView() const noexcept
{
	assert(m_impl->abstractItemView);
	return *m_impl->abstractItemView;
}

}
