#include "UiFactory.h"

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include <QFileInfo>
#include <QPushButton>

#include "fnd/FindPair.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/dialogs/IDialog.h"
#include "interface/ui/dialogs/IScriptDialog.h"
#include "interface/ui/IRateStarsProvider.h"

#include "util/ISettings.h"

#include "delegate/TreeViewDelegate/TreeViewDelegateBooks.h"

#include "dialogs/AddCollectionDialog.h"

#include "ItemViewToolTipper.h"
#include "ParentWidgetProvider.h"
#include "TreeView.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto RECENT_DIR_KEY = "ui/RecentDir/%1";

class RecentDir
{
public:
	RecentDir(std::shared_ptr<ISettings> settings, const QString & key)
		: m_settings(std::move(settings))
		, m_key(QString(RECENT_DIR_KEY).arg(key))
	{
	}

	QString GetDir(const QString & dir) const
	{
		return dir.isEmpty() ? m_settings->Get(m_key).toString() : dir;
	}

	void SetDir(QString name)
	{
		if (name.isEmpty())
			return;

		if (const QFileInfo fileInfo(name); fileInfo.isFile())
			name = fileInfo.dir().absolutePath();

		m_settings->Set(m_key, name);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	const QString m_key;
};

}

struct UiFactory::Impl
{
	Hypodermic::Container & container;

	mutable std::filesystem::path inpx;
	mutable std::shared_ptr<ITreeViewController> treeViewController;
	mutable QAbstractScrollArea * abstractScrollArea { nullptr };

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
	return m_impl->container.resolve<ParentWidgetProvider>()->GetWidget();
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

void UiFactory::ShowAbout() const
{
	const auto dialog = m_impl->container.resolve<IAboutDialog>();
	(void)dialog->Show();
}

QMessageBox::ButtonRole UiFactory::ShowCustomDialog(const QMessageBox::Icon icon, const QString & title, const QString & text, const std::vector<std::pair<QMessageBox::ButtonRole, QString>> & buttons, const QMessageBox::ButtonRole defaultButton) const
{
	auto * parentWidget = m_impl->container.resolve<ParentWidgetProvider>()->GetWidget();
	m_impl->container.resolve<ISettings>();

	QMessageBox msgBox(parentWidget);
	msgBox.setFont(parentWidget->font());
	msgBox.setIcon(icon);
	msgBox.setWindowTitle(title);
	msgBox.setText(text);

	std::vector<std::pair<QAbstractButton *, QMessageBox::ButtonRole>> msgBoxButtons;
	msgBoxButtons.reserve(buttons.size());
	std::ranges::transform(buttons, std::back_inserter(msgBoxButtons), [&] (const auto & item)
	{
		auto * button = msgBox.addButton(item.second, item.first);
		if (item.first == defaultButton)
			msgBox.setDefaultButton(button);
		return std::make_pair(button, item.first);
	});

	msgBox.exec();

	return FindSecond(msgBoxButtons, msgBox.clickedButton(), QMessageBox::NoRole);
}

QMessageBox::StandardButton UiFactory::ShowQuestion(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	const auto dialog = m_impl->container.resolve<IQuestionDialog>();
	return dialog->Show(text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowWarning(const QString & text, const QMessageBox::StandardButtons buttons, const QMessageBox::StandardButton defaultButton) const
{
	const auto dialog = m_impl->container.resolve<IWarningDialog>();
	return dialog->Show(text, buttons, defaultButton);
}

void UiFactory::ShowInfo(const QString & text) const
{
	const auto dialog = m_impl->container.resolve<IInfoDialog>();
	(void)dialog->Show(text);
}

void UiFactory::ShowError(const QString & text) const
{
	const auto dialog = m_impl->container.resolve<IErrorDialog>();
	(void)dialog->Show(text);
}

QString UiFactory::GetText(const QString & title, const QString & label, const QString & text, const QLineEdit::EchoMode mode) const
{
	const auto dialog = m_impl->container.resolve<IInputTextDialog>();
	return dialog->GetText(title, label, text, mode);
}

std::optional<QFont> UiFactory::GetFont(const QString & title, const QFont & font, const QFontDialog::FontDialogOptions options) const
{
	bool ok = false;
	return QFontDialog::getFont(&ok, font, m_impl->container.resolve<ParentWidgetProvider>()->GetWidget(), title, options);
}

QString GetFileSystemObj(std::shared_ptr<ISettings> settings, const QString & key, const QString & dir, const std::function<QString(const QString &)> & f)
{
	RecentDir recentDir(std::move(settings), key);
	auto result = f(recentDir.GetDir(dir));
	recentDir.SetDir(result);
	return result;
}

QString UiFactory::GetOpenFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, const QFileDialog::Options options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(), key, dir, [&](const QString & recentDir)
	{
		return QFileDialog::getOpenFileName(m_impl->container.resolve<ParentWidgetProvider>()->GetWidget(), title, recentDir, filter, nullptr, options);
	});
}

QString UiFactory::GetSaveFileName(const QString & key, const QString & title, const QString & filter, const QString & dir, const QFileDialog::Options options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(), key, dir, [&] (const QString & recentDir)
	{
		return QFileDialog::getSaveFileName(m_impl->container.resolve<ParentWidgetProvider>()->GetWidget(), title, recentDir, filter, nullptr, options);
	});
}

QString UiFactory::GetExistingDirectory(const QString & key, const QString & title, const QString & dir, const QFileDialog::Options options) const
{
	return GetFileSystemObj(m_impl->container.resolve<ISettings>(), key, dir, [&] (const QString & recentDir)
	{
		return QFileDialog::getExistingDirectory(m_impl->container.resolve<ParentWidgetProvider>()->GetWidget(), title, recentDir, options);
	});
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

}
