#include "UiFactory.h"

#include <QFileInfo>

#include "fnd/FindPair.h"

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/dialogs/IComboBoxTextDialog.h"
#include "interface/ui/dialogs/IScriptDialog.h"

#include "Hypodermic/Hypodermic.h"
#include "delegate/TreeViewDelegate/TreeViewDelegateBooks.h"
#include "delegate/TreeViewDelegate/TreeViewDelegateNavigation.h"
#include "dialogs/AddCollectionDialog.h"
#include "dialogs/FilterSettingsDialog.h"
#include "dialogs/OpdsDialog.h"
#include "util/localization.h"
#include "version/AppVersion.h"

#include "AuthorReview.h"
#include "CollectionCleaner.h"
#include "QueryWindow.h"
#include "TreeView.h"
#include "log.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{
constexpr auto CONTEXT = "Dialog";
constexpr auto ABOUT_TITLE = QT_TRANSLATE_NOOP("Dialog", "About FLibrary");
constexpr auto ABOUT_DESCRIPTION = QT_TRANSLATE_NOOP("Dialog", "Another e-library book cataloger");
constexpr auto ABOUT_VERSION = QT_TRANSLATE_NOOP("Dialog", "Version: %1 (%2)");
constexpr auto ABOUT_LICENSE = QT_TRANSLATE_NOOP("Dialog", "Distributed under license %1");
constexpr auto PERSONAL_BUILD = QT_TRANSLATE_NOOP("Dialog", "<p>Personal <a href='%1'>%2</a> build</p>");
constexpr const char* COMPONENTS[] = {
	"<hr><table style='font-size:50%'>",
	QT_TRANSLATE_NOOP("Dialog", "<tr><td style='text-align: center'>Components / Libraries</td></tr>"),
	// clang-format off
	"<tr><td><a href='https://wiki.qt.io/Main'>Qt</a> &copy; 2024 The Qt Company Ltd <a href='https://www.gnu.org/licenses/lgpl-3.0.html#license-text'>GNU LGPL v3</a></td></tr>",
	"<tr><td><a href='https://github.com/ybainier/Hypodermic'>Hypodermic</a> &copy; 2016 Hypodermic Project <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://github.com/SergiusTheBest/plog'>plog</a> &copy; 2022 <a href='https://github.com/SergiusTheBest'>Sergey Podobry</a> <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://xerces.apache.org/xerces-c/'>Xerces-C++ XML Parser</a> &copy; 2024 The Apache Xerces&trade; Project <a href='https://www.apache.org/licenses/LICENSE-2.0.html'>Apache License v2</a></td></tr>",
	"<tr><td><a href='https://www.boost.org/'>boost</a> &copy; boost C++ libraries <a href='https://www.boost.org/LICENSE_1_0.txt'>Boost Software License v1.0</a></td></tr>",
	"<tr><td><a href='https://www.7-zip.org/'>7z</a> &copy; 1999-2023 Igor Pavlov <a href='https://www.7-zip.org/license.txt'>GNU LGPL, BSD 3-clause License</a></td></tr>",
	"<tr><td><a href='https://github.com/libjxl/libjxl'>libjxl</a> &copy; the JPEG XL Project Authors <a href='https://opensource.org/license/bsd-3-clause'>BSD 3-clause License</a></td></tr>",
	"<tr><td><a href='https://github.com/rikyoz/bit7z'>bit7z</a> &copy; 2014-2022 <a href='https://github.com/rikyoz'>Riccardo Ostani</a> <a href='https://github.com/rikyoz/bit7z/blob/master/LICENSE'>MPL-2.0</a></td></tr>",
	"<tr><td><a href='https://www.sqlite.org/'>SQLite</a> <a href='https://www.sqlite.org/copyright.html'>Public Domain</a></td></tr>",
	"<tr><td><a href='https://github.com/iwongu/sqlite3pp'>sqlite3pp</a> &copy; 2023 <a href='https://github.com/iwongu'>Wongoo Lee</a> <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://github.com/heimdallr/ReactApp'>FLibrary React interface</a> &copy; 2025 alloroc2 <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://github.com/heimdallr/MyHomeLib/tree/master/Utils/MHLSQLiteExt'>MyHomeLib SQLite extension library</a> &copy; 2010 Nick Rymanov <a href='https://www.gnu.org/licenses/gpl-3.0.html#license-text'>GNU GPL</a></td></tr>",
	"<tr><td><a href='https://icu.unicode.org/'>ICU</a> &copy; 2016-2025 Unicode, Inc. <a href='https://www.unicode.org/copyright.html#License'>Open Source License</a></td></tr>",
	"<tr><td><a href='https://uxwing.com/'>UXWing</a> &copy; 2025 UXWing <a href='https://uxwing.com/license/'>License</a></td></tr>",
	// clang-format on
	"</table>"
};
constexpr auto ABOUT_TEXT = "%1<p>%2<p><a href='%3'>%3</a><p>%4%5";
TR_DEF

QString GetPersonalBuildString()
{
	// ReSharper disable once CppCompileTimeConstantCanBeReplacedWithBooleanConstant
	if constexpr (*PERSONAL_BUILD_NAME)
		return Loc::Tr(CONTEXT, PERSONAL_BUILD).arg(PERSONAL_BUILD_URI, PERSONAL_BUILD_NAME);
	else
		return QString {};
}

template <typename T>
void CreateStackedPage(Hypodermic::Container& container, const QObject* signalReceiver)
{
	auto collectionCleaner = container.resolve<T>();
	auto* collectionCleanerPtr = collectionCleaner.get();
	auto connection = std::make_shared<QMetaObject::Connection>();
	*connection = QObject::connect(
		collectionCleanerPtr,
		qOverload<std::shared_ptr<QWidget>, int>(&StackedPage::StateChanged),
		signalReceiver,
		[collectionCleaner = std::move(collectionCleaner), connection]([[maybe_unused]] const std::shared_ptr<QWidget>& widget, [[maybe_unused]] const int state) mutable
		{
			assert(widget.get() == collectionCleaner.get() && state == StackedPage::State::Created);
			QObject::disconnect(*connection);
		},
		Qt::QueuedConnection);
}

} // namespace

struct UiFactory::Impl
{
	Hypodermic::Container& container;

	mutable std::filesystem::path inpxFolder;
	mutable std::shared_ptr<ITreeViewController> treeViewController;
	mutable QTreeView* treeView { nullptr };
	mutable QAbstractItemView* abstractItemView { nullptr };
	mutable QString title;
	mutable long long authorId { -1 };

	explicit Impl(Hypodermic::Container& container)
		: container(container)
	{
	}
};

UiFactory::UiFactory(Hypodermic::Container& container)
	: m_impl(container)
{
	PLOGV << "UiFactory created";
}

UiFactory::~UiFactory()
{
	PLOGV << "UiFactory destroyed";
}

QObject* UiFactory::GetParentObject(QObject* defaultObject) const noexcept
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetParentObject(defaultObject);
}

QWidget* UiFactory::GetParentWidget(QWidget* defaultWidget) const noexcept
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetParentWidget(defaultWidget);
}

std::shared_ptr<TreeView> UiFactory::CreateTreeViewWidget(const ItemType type) const
{
	m_impl->treeViewController = m_impl->container.resolve<ILogicFactory>()->GetTreeViewController(type);
	return m_impl->container.resolve<TreeView>();
}

std::shared_ptr<IAddCollectionDialog> UiFactory::CreateAddCollectionDialog(std::filesystem::path inpxFolder) const
{
	m_impl->inpxFolder = std::move(inpxFolder);
	return m_impl->container.resolve<IAddCollectionDialog>();
}

std::shared_ptr<IScriptDialog> UiFactory::CreateScriptDialog() const
{
	return m_impl->container.resolve<IScriptDialog>();
}

std::shared_ptr<ITreeViewDelegate> UiFactory::CreateTreeViewDelegateBooks(QTreeView& parent) const
{
	m_impl->treeView = &parent;
	return m_impl->container.resolve<TreeViewDelegateBooks>();
}

std::shared_ptr<ITreeViewDelegate> UiFactory::CreateTreeViewDelegateNavigation(QAbstractItemView& parent) const
{
	m_impl->abstractItemView = &parent;
	return m_impl->container.resolve<TreeViewDelegateNavigation>();
}

std::shared_ptr<QDialog> UiFactory::CreateOpdsDialog() const
{
	return m_impl->container.resolve<OpdsDialog>();
}

std::shared_ptr<QDialog> UiFactory::CreateFilterSettingsDialog() const
{
	return m_impl->container.resolve<FilterSettingsDialog>();
}

std::shared_ptr<IComboBoxTextDialog> UiFactory::CreateComboBoxTextDialog(QString title) const
{
	m_impl->title = std::move(title);
	return m_impl->container.resolve<IComboBoxTextDialog>();
}

std::shared_ptr<QMainWindow> UiFactory::CreateQueryWindow() const
{
	return m_impl->container.resolve<QueryWindow>();
}

void UiFactory::CreateCollectionCleaner() const
{
	CreateStackedPage<CollectionCleaner>(m_impl->container, this);
}

void UiFactory::CreateAuthorReview(const long long id) const
{
	assert(id >= 0 && m_impl->authorId < 0);
	m_impl->authorId = id;
	CreateStackedPage<AuthorReview>(m_impl->container, this);
}

void UiFactory::ShowAbout() const
{
	auto* parent = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget();
	QMessageBox msgBox(parent);
	msgBox.setFont(parent->font());
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setWindowTitle(Tr(ABOUT_TITLE));
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(QString(ABOUT_TEXT)
	                   .arg(Tr(ABOUT_DESCRIPTION),
	                        Tr(ABOUT_VERSION).arg(GetApplicationVersion(), GIT_HASH),
	                        "https://github.com/heimdallr/books",
	                        Tr(ABOUT_LICENSE).arg("<a href='https://opensource.org/license/mit'>MIT</a>"),
	                        GetPersonalBuildString()));
	msgBox.setStandardButtons(QMessageBox::Ok);
	QStringList text;
	std::ranges::transform(COMPONENTS, std::back_inserter(text), [](const char* str) { return Loc::Tr(CONTEXT, str); });
	msgBox.setInformativeText(text.join(""));
	msgBox.exec();
}

QMessageBox::ButtonRole UiFactory::ShowCustomDialog(const QMessageBox::Icon icon,
                                                    const QString& title,
                                                    const QString& text,
                                                    const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
                                                    const QMessageBox::ButtonRole defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowCustomDialog(icon, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowQuestion(const QString& text, const QMessageBox::StandardButtons& buttons, const QMessageBox::StandardButton defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowQuestion(text, buttons, defaultButton);
}

QMessageBox::StandardButton UiFactory::ShowWarning(const QString& text, const QMessageBox::StandardButtons& buttons, const QMessageBox::StandardButton defaultButton) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowWarning(text, buttons, defaultButton);
}

void UiFactory::ShowInfo(const QString& text) const
{
	m_impl->container.resolve<Util::IUiFactory>()->ShowInfo(text);
}

void UiFactory::ShowError(const QString& text) const
{
	m_impl->container.resolve<Util::IUiFactory>()->ShowError(text);
}

QString UiFactory::GetText(const QString& title, const QString& label, const QString& text, const QStringList& comboBoxItems, const QLineEdit::EchoMode mode) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetText(title, label, text, comboBoxItems, mode);
}

std::optional<QFont> UiFactory::GetFont(const QString& title, const QFont& font, const QFontDialog::FontDialogOptions& options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetFont(title, font, options);
}

QStringList UiFactory::GetOpenFileNames(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetOpenFileNames(key, title, filter, dir, options);
}

QString UiFactory::GetOpenFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetOpenFileName(key, title, filter, dir, options);
}

QString UiFactory::GetSaveFileName(const QString& key, const QString& title, const QString& filter, const QString& dir, const QFileDialog::Options& options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetSaveFileName(key, title, filter, dir, options);
}

QString UiFactory::GetExistingDirectory(const QString& key, const QString& title, const QString& dir, const QFileDialog::Options& options) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetExistingDirectory(key, title, dir, options);
}

std::filesystem::path UiFactory::GetNewCollectionInpxFolder() const noexcept
{
	auto result = std::move(m_impl->inpxFolder);
	m_impl->inpxFolder = std::filesystem::path {};
	return result;
}

std::shared_ptr<ITreeViewController> UiFactory::GetTreeViewController() const noexcept
{
	assert(m_impl->treeViewController);
	return std::move(m_impl->treeViewController);
}

QTreeView& UiFactory::GetTreeView() const noexcept
{
	assert(m_impl->treeView);
	return *m_impl->treeView;
}

QAbstractItemView& UiFactory::GetAbstractItemView() const noexcept
{
	assert(m_impl->abstractItemView);
	return *m_impl->abstractItemView;
}

QString UiFactory::GetTitle() const noexcept
{
	return std::move(m_impl->title);
}

long long UiFactory::GetAuthorId() const noexcept
{
	assert(m_impl->authorId >= 0);
	const auto result = m_impl->authorId;
	m_impl->authorId = -1;
	return result;
}
