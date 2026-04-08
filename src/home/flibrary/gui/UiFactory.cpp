#include "UiFactory.h"

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenuBar>
#include <QShortcut>
#include <QToolTip>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/dialogs/IComboBoxTextDialog.h"

#include "Hypodermic/Hypodermic.h"
#include "delegate/TreeViewDelegate/TreeViewDelegateBooks.h"
#include "delegate/TreeViewDelegate/TreeViewDelegateNavigation.h"
#include "dialogs/AddCollectionDialog.h"
#include "dialogs/FilterSettingsDialog.h"
#include "dialogs/HotkeyDialog.h"
#include "dialogs/OpdsDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/script/ScriptDialog.h"
#include "logic/data/DataItem.h"
#include "utilgui/GeometryRestorable.h"
#include "version/AppVersion.h"
#include "widgets/ClickableLabel.h"

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

constexpr auto        CONTEXT           = "Dialog";
constexpr auto        ABOUT_TITLE       = QT_TRANSLATE_NOOP("Dialog", "About FLibrary");
constexpr auto        ABOUT_DESCRIPTION = QT_TRANSLATE_NOOP("Dialog", "Another e-library book cataloger");
constexpr auto        ABOUT_VERSION     = QT_TRANSLATE_NOOP("Dialog", "Version: %1 (%2) %3");
constexpr auto        ABOUT_LICENSE     = QT_TRANSLATE_NOOP("Dialog", "Distributed under license %1");
constexpr auto        PERSONAL_BUILD    = QT_TRANSLATE_NOOP("Dialog", "<p>Personal <a href='%1'>%2</a> build</p>");
constexpr auto        VERSION_COPIED    = QT_TRANSLATE_NOOP("Dialog", "The program version has been copied to the clipboard");
constexpr const char* COMPONENTS[]      = {
	"<hr><table style='font-size:50%'>",
	QT_TRANSLATE_NOOP("Dialog", "<tr><td style='text-align: center'>Components / Libraries</td></tr>"),
	// ReSharper disable StringLiteralTypo
	// clang-format off
	"<tr><td><a href='https://wiki.qt.io/Main'>Qt</a> &copy; 2024 The Qt Company Ltd <a href='https://www.gnu.org/licenses/lgpl-3.0.html#license-text'>GNU LGPL v3</a></td></tr>",
	"<tr><td><a href='https://github.com/ybainier/Hypodermic'>Hypodermic</a> &copy; 2016 Hypodermic Project <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://github.com/SergiusTheBest/plog'>plog</a> &copy; 2022 <a href='https://github.com/SergiusTheBest'>Sergey Podobry</a> <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://xerces.apache.org/xerces-c/'>Xerces-C++ XML Parser</a> &copy; 2024 The Apache Xerces&trade; Project <a href='https://www.apache.org/licenses/LICENSE-2.0.html'>Apache License v2</a></td></tr>",
	"<tr><td><a href='https://www.boost.org/'>boost</a> &copy; boost C++ libraries <a href='https://www.boost.org/LICENSE_1_0.txt'>Boost Software License v1.0</a></td></tr>",
	"<tr><td><a href='https://www.7-zip.org/'>7z</a> &copy; 1999-2023 Igor Pavlov <a href='https://www.7-zip.org/license.txt'>GNU LGPL, BSD 3-clause License</a></td></tr>",
	"<tr><td><a href='https://github.com/libjxl/libjxl'>libjxl</a> &copy; the JPEG XL Project Authors <a href='https://opensource.org/license/bsd-3-clause'>BSD 3-clause License</a></td></tr>",
	"<tr><td><a href='https://github.com/rikyoz/bit7z'>bit7z</a> &copy; 2014-2024 <a href='https://github.com/rikyoz'>Riccardo Ostani</a> <a href='https://github.com/rikyoz/bit7z/blob/master/LICENSE'>MPL-2.0</a></td></tr>",
	"<tr><td><a href='https://www.sqlite.org/'>SQLite</a> <a href='https://www.sqlite.org/copyright.html'>Public Domain</a></td></tr>",
	"<tr><td><a href='https://github.com/iwongu/sqlite3pp'>sqlite3pp</a> &copy; 2023 <a href='https://github.com/iwongu'>Wongoo Lee</a> <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://github.com/heimdallr/ReactApp'>FLibrary React interface</a> &copy; 2025 alloroc2 <a href='https://opensource.org/license/mit'>MIT</a></td></tr>",
	"<tr><td><a href='https://icu.unicode.org/'>ICU</a> &copy; 2016-2025 Unicode, Inc. <a href='https://www.unicode.org/copyright.html#License'>Open Source License</a></td></tr>",
	"<tr><td><a href='https://cimg.eu/'>CImg</a> &copy; 2004, David Tschumperlé - GREYC UMR CNRS 6072, Image team. <a href='http://www.cecill.info/licences/Licence_CeCILL-C_V1-en.txt'>CeCILL-C</a></td></tr>",
	"<tr><td><a href='https://uxwing.com/'>UXWing</a> &copy; 2025 UXWing <a href='https://uxwing.com/license/'>License</a></td></tr>",
	// clang-format on
	// ReSharper enable StringLiteralTypo
	"</table>"
};
constexpr auto ABOUT_TEXT        = "%1<p>%2<p><a href='%3'>%3</a><p>%4%5";
constexpr auto COPY_VERSION_LINK = "copy://version";
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
	auto  collectionCleaner    = container.resolve<T>();
	auto* collectionCleanerPtr = collectionCleaner.get();
	auto  connection           = std::make_shared<QMetaObject::Connection>();
	*connection                = QObject::connect(
		collectionCleanerPtr,
		qOverload<std::shared_ptr<QWidget>, int>(&StackedPage::StateChanged),
		signalReceiver,
		[collectionCleaner = std::move(collectionCleaner), connection]([[maybe_unused]] const std::shared_ptr<QWidget>& widget, [[maybe_unused]] const int state) mutable {
			assert(widget.get() == collectionCleaner.get() && state == StackedPage::State::Created);
			QObject::disconnect(*connection);
		},
		Qt::QueuedConnection
	);
}

} // namespace

struct UiFactory::Impl
{
	Hypodermic::Container& container;

	mutable std::filesystem::path                inpxFolder;
	mutable std::shared_ptr<ITreeViewController> treeViewController;
	mutable QTreeView*                           treeView { nullptr };
	mutable QAbstractItemView*                   abstractItemView { nullptr };
	mutable QString                              title;
	mutable long long                            authorId { -1 };

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

int UiFactory::GetParentWidgetFontSize() const noexcept
{
	return m_impl->container.resolve<Util::IUiFactory>()->GetParentWidgetFontSize();
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

std::shared_ptr<QDialog> UiFactory::CreateScriptDialog() const
{
	return m_impl->container.resolve<ScriptDialog>();
}

std::shared_ptr<QDialog> UiFactory::CreateSettingsDialog() const
{
	return m_impl->container.resolve<SettingsDialog>();
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

std::shared_ptr<QDialog> UiFactory::CreateHotkeyDialog() const
{
	return m_impl->container.resolve<HotkeyDialog>();
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

void UiFactory::ExecuteContextMenu(QLineEdit* lineEdit) const
{
	QMenu menu(lineEdit);
	for (const auto& item : IScriptController::s_commandMacros | std::views::values)
	{
		const auto menuItemTitle = QString("%1\t%2").arg(Loc::Tr(IScriptController::s_context, item), item);
		menu.addAction(menuItemTitle, [=, value = QString(item)] {
			auto       currentText     = lineEdit->text();
			const auto currentPosition = lineEdit->cursorPosition();
			lineEdit->setText(currentText.insert(currentPosition, value));
			lineEdit->setCursorPosition(currentPosition + static_cast<int>(value.size()));
		});
	}
	menu.setFont(lineEdit->font());
	menu.exec(QCursor::pos());
}

void UiFactory::ShowAbout() const
{
	auto*       parent = m_impl->container.resolve<IParentWidgetProvider>()->GetWidget();
	QMessageBox msgBox(parent);
	msgBox.setFont(parent->font());
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setWindowTitle(Tr(ABOUT_TITLE));
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(
		QString(ABOUT_TEXT)
			.arg(
				Tr(ABOUT_DESCRIPTION),
				Tr(ABOUT_VERSION)
					.arg(GetApplicationVersion(), GIT_HASH, QString("<font size='%1px;'><a href='%2'>%3</a></font>").arg(msgBox.font().pointSize() * 9 / 10).arg(COPY_VERSION_LINK, QChar { 0x29C9 })),
				"https://github.com/heimdallr/books",
				Tr(ABOUT_LICENSE).arg("<a href='https://opensource.org/license/mit'>MIT</a>"),
				GetPersonalBuildString()
			)
	);

	for (auto* label : msgBox.findChildren<QLabel*>())
	{
		label->setOpenExternalLinks(false);
		connect(label, &QLabel::linkActivated, [&](const QString& link) {
			if (link != COPY_VERSION_LINK)
				return (void)QDesktopServices::openUrl(link);

			QGuiApplication::clipboard()->setText(QString("%1 (%2)").arg(GetApplicationVersion(), GIT_HASH));
			QToolTip::setFont(msgBox.font());
			QToolTip::showText(QCursor::pos(), Tr(VERSION_COPIED));
		});
	}

	msgBox.setStandardButtons(QMessageBox::Ok);
	QStringList text;
	std::ranges::transform(COMPONENTS, std::back_inserter(text), [](const char* str) {
		return Loc::Tr(CONTEXT, str);
	});
	msgBox.setInformativeText(QString(R"(<font size="%1px;">%2</font>)").arg(msgBox.font().pointSize() * 9 / 10).arg(text.join("")));
	msgBox.show();
	Util::MoveToParentCenter(msgBox);
	msgBox.exec();
}

QMessageBox::ButtonRole UiFactory::ShowCustomDialog(
	const QMessageBox::Icon                                         icon,
	const QString&                                                  title,
	const QString&                                                  text,
	const std::vector<std::pair<QMessageBox::ButtonRole, QString>>& buttons,
	const QMessageBox::ButtonRole                                   defaultButton,
	const QString&                                                  detailedText
) const
{
	return m_impl->container.resolve<Util::IUiFactory>()->ShowCustomDialog(icon, title, text, buttons, defaultButton, detailedText);
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
	auto result        = std::move(m_impl->inpxFolder);
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
	m_impl->authorId  = -1;
	return result;
}

namespace
{

QString GetName(const QString& parent, const QString& child)
{
	return QString("%1/%2").arg(parent, child);
}

QString RemoveAmp(QString str)
{
	return str.remove('&');
}

}

IDataItem::Ptr UiFactory::AddMenuBarToHotkeys(const ISettings& settings, const QMenuBar& menuBar, const QString& title, const std::function<void(const IDataItem::Ptr&, QAction*)>& functor) const
{
	auto menuBarItem = SettingsItem::Create();
	menuBarItem->SetData(menuBar.objectName(), SettingsItem::Column::Key);
	menuBarItem->SetData(title, SettingsItem::Column::Title);

	const auto addChild = [&](IDataItem& parent, const QObject& obj, QString objTitle) -> IDataItem::Ptr& {
		auto child = SettingsItem::Create();
		child->SetData(GetName(parent.GetData(SettingsItem::Column::Key), obj.objectName()), SettingsItem::Column::Key);
		child->SetData(RemoveAmp(std::move(objTitle)), SettingsItem::Column::Title);
		return parent.AppendChild(std::move(child));
	};

	const auto enumerate = [&](const QList<QMenu*>& menuList, IDataItem& parent, std::unordered_set<const QAction*>& menuActions, const auto& r) -> void {
		for (const auto* menu : menuList)
		{
			if (menu->title().isEmpty())
				continue;

			menuActions.emplace(menu->menuAction());
			auto& child = addChild(parent, *menu, menu->title());

			std::unordered_set<const QAction*> actions;
			r(menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *child, actions, r);

			for (auto* action : menu->actions() | std::views::filter([&](const QAction* item) {
									return !(item->isSeparator() || actions.contains(item));
								}))
			{
				if (action->objectName().isEmpty())
				{
					PLOGW << action->text() << ": objectName is empty";
					continue;
				}

				auto& actionItem = addChild(*child, *action, action->text());
				functor(actionItem, action);
				if (const auto shortCut = settings.Get(GetName(Constant::Settings::HOTKEYS_ROOT, actionItem->GetData(SettingsItem::Column::Key))); shortCut.isValid())
					action->setShortcut(QKeySequence(shortCut.toString(), QKeySequence::PortableText));
				actionItem->SetData(action->shortcut().toString(), SettingsItem::Column::Value);
			}
		}
	};

	std::unordered_set<const QAction*> actions;
	enumerate(menuBar.findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *menuBarItem, actions, enumerate);

	return menuBarItem;
}

IDataItem::Ptr UiFactory::AddComboBoxToHotkeys(const ISettings& settings, QComboBox& comboBox, const QString& title, const std::function<void(const IDataItem::Ptr&, QShortcut*)>& functor) const
{
	auto comboBoxItem = SettingsItem::Create();
	comboBoxItem->SetData(comboBox.objectName(), SettingsItem::Column::Key);
	comboBoxItem->SetData(title, SettingsItem::Column::Title);

	for (int i = 0, sz = comboBox.count(); i < sz; ++i)
	{
		auto& child = comboBoxItem->AppendChild(SettingsItem::Create());
		child->SetData(GetName(comboBoxItem->GetData(SettingsItem::Column::Key), comboBox.itemData(i).toString()), SettingsItem::Column::Key);
		child->SetData(comboBox.itemText(i), SettingsItem::Column::Title);

		auto* shortcut = new QShortcut(&comboBox);
		shortcut->setObjectName(comboBox.itemData(i).toString());
		connect(shortcut, &QShortcut::activated, [comboBox = &comboBox, i] {
			comboBox->setCurrentIndex(i);
		});
		functor(child, shortcut);
		if (const auto shortCut = settings.Get(GetName(Constant::Settings::HOTKEYS_ROOT, child->GetData(SettingsItem::Column::Key))); shortCut.isValid())
			shortcut->setKey(QKeySequence(shortCut.toString(), QKeySequence::PortableText));
		child->SetData(shortcut->key().toString(), SettingsItem::Column::Value);
	}

	return comboBoxItem;
}

void UiFactory::UpdateRecentOpenBookControllerMenu(QMenu& menu, const int maxMenuItemCount, QString menuItemTitleFormat, std::function<void(long long)> onMenuTriggered) const
{
	static constexpr auto QUERY = R"(select distinct b.BookID, b.Title, (
    select(group_concat(author, ', ')) from (
        select a.LastName || coalesce(' ' || substr(nullif(a.FirstName, ''), 1, 1)||'.', '') || coalesce(substr(nullif(a.MiddleName, ''), 1, 1)||'.', '') author
        from Authors a
        join Author_List l on l.AuthorID = a.AuthorID and l.BookID = b.BookID
        order by l.OrdNum
        limit 3
    )
) Author
from Export_List_User e
join Books b on b.BookID = e.BookID
where e.ExportType = 0 
order by e.CreatedAt desc
limit {}
)";

	menu.clear();
	menu.menuAction()->setEnabled(false);
	if (maxMenuItemCount < 1)
		return menu.menuAction()->setVisible(false);

	const auto databaseUser = m_impl->container.resolve<IDatabaseUser>();

	auto db = databaseUser->CheckDatabase();
	if (!db)
		return;

	databaseUser->Execute(
		{ "Update recent books menu", [&menu, maxMenuItemCount, menuItemTitleFormat = std::move(menuItemTitleFormat), onMenuTriggered = std::move(onMenuTriggered), db = std::move(db)]() mutable {
			 const auto                                 query = db->CreateQuery(std::format(QUERY, maxMenuItemCount));
			 std::vector<std::pair<long long, QString>> data;
			 for (query->Execute(); !query->Eof(); query->Next())
				 data.emplace_back(query->Get<long long>(0), menuItemTitleFormat.arg(query->Get<const char*>(2), query->Get<const char*>(1)));

			 return [&menu, onMenuTriggered = std::move(onMenuTriggered), data = std::move(data)](size_t) {
				 menu.menuAction()->setEnabled(!data.empty());
				 for (const auto& [id, title] : data)
				 {
					 auto* action = menu.addAction(title);
					 connect(action, &QAction::triggered, [onMenuTriggered, id] {
						 onMenuTriggered(id);
					 });
				 }
			 };
		 } }
	);
}
