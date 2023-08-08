#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QModelIndex>
#include <QVariant>

#include <plog/Log.h>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

#include "fnd/FindPair.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"
#include "model/IModelObserver.h"
#include "shared/GroupController.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";

constexpr auto REMOVE_GROUP_CONFIRM = QT_TRANSLATE_NOOP("Navigation", "Are you sure you want to delete the groups (%1)?");
constexpr auto INPUT_NEW_GROUP_NAME = QT_TRANSLATE_NOOP("Navigation", "Input new group name");
constexpr auto GROUP_NAME = QT_TRANSLATE_NOOP("Navigation", "Group name");
constexpr auto NEW_GROUP_NAME = QT_TRANSLATE_NOOP("Navigation", "New group");

using ModelCreator = std::shared_ptr<QAbstractItemModel> (AbstractModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;
using MenuRequester = IDataItem::Ptr(*)();

IDataItem::Ptr MenuRequesterStub()
{
	return nullptr;
}

enum class MenuAction
{
	CreateNewGroup,
	RemoveGroup,
};

template <typename T>
void Add(IDataItem::Ptr & dst, QString title, T id)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	dst->AppendChild(std::move(item));
}


IDataItem::Ptr MenuRequesterGroups()
{
	auto result = MenuItem::Create();
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Create new..."), MenuAction::CreateNewGroup);
	Add(result, QT_TRANSLATE_NOOP("Navigation", "Remove"), MenuAction::RemoveGroup);
	return result;
}

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
	NavigationMode navigationMode;
	MenuRequester menuRequester { &MenuRequesterStub };
};

constexpr std::pair<const char *, ModeDescriptor> MODE_DESCRIPTORS[]
{
	{ QT_TRANSLATE_NOOP("Navigation", "Authors") , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Authors } },
	{ QT_TRANSLATE_NOOP("Navigation", "Series")  , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Series } },
	{ QT_TRANSLATE_NOOP("Navigation", "Genres")  , { ViewMode::Tree, &AbstractModelProvider::CreateTreeModel, NavigationMode::Genres } },
	{ QT_TRANSLATE_NOOP("Navigation", "Archives"), { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Archives } },
	{ QT_TRANSLATE_NOOP("Navigation", "Groups")  , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Groups, &MenuRequesterGroups } },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));

class IContextMenuHandler  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IContextMenuHandler() = default;
	virtual void OnCreateNewGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
	virtual void OnRemoveGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const = 0;
};

using ContextMenuHandlerFunction = void (IContextMenuHandler::*)(const QList<QModelIndex> & indexList, const QModelIndex & index) const;

constexpr std::pair<MenuAction, ContextMenuHandlerFunction> MENU_HANDLERS[]
{
	{ MenuAction::CreateNewGroup, &IContextMenuHandler::OnCreateNewGroupTriggered },
	{ MenuAction::RemoveGroup, &IContextMenuHandler::OnRemoveGroupTriggered },
};

auto Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

}

struct TreeViewControllerNavigation::Impl final
	: IModelObserver
	, virtual IContextMenuHandler
{
	TreeViewControllerNavigation & self;
	std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	PropagateConstPtr<ILogicFactory, std::shared_ptr> logicFactory;
	PropagateConstPtr<IUiFactory, std::shared_ptr> uiFactory;
	mutable std::shared_ptr<GroupController> groupController;
	int mode = { -1 };

	Impl(TreeViewControllerNavigation & self
		, std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: self(self)
		, logicFactory(std::move(logicFactory))
		, uiFactory(std::move(uiFactory))
	{
		for ([[maybe_unused]]const auto & _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());
	}

private: // IContextMenuHandler
	void OnCreateNewGroupTriggered(const QList<QModelIndex> &, const QModelIndex &) const override
	{
		auto groupName = uiFactory->GetText(Tr(INPUT_NEW_GROUP_NAME), Tr(GROUP_NAME), Tr(NEW_GROUP_NAME));
		if (groupName.isEmpty())
			return;

		if (!groupController)
			groupController = logicFactory->CreateGroupController();

		groupController->CreateNewGroup(std::move(groupName), [&, gc = groupController] () mutable
		{
			gc.reset();
			groupController.reset();
			self.RequestNavigation();
		});
	}

	void OnRemoveGroupTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index) const override
	{
		const auto toId = [] (const QModelIndex & ind)
		{
			return ind.data(Role::Id).toInt();
		};

		GroupController::Ids ids { toId(index) };
		std::ranges::transform(indexList, std::inserter(ids, ids.end()), toId);
		if (false
			|| ids.empty()
			|| uiFactory->ShowQuestion(Tr(REMOVE_GROUP_CONFIRM).arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No
			)
			return;

		if (!groupController)
			groupController = logicFactory->CreateGroupController();

		groupController->RemoveGroups(std::move(ids), [&, gc = groupController]() mutable
		{
			gc.reset();
			groupController.reset();
			self.RequestNavigation();
		});
	}
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<AbstractModelProvider> modelProvider
	, std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<IUiFactory> uiFactory
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
		, std::move(modelProvider)
	)
	, m_impl(*this
		, std::move(logicFactory)
		, std::move(uiFactory)
	)
{
	Setup();

	m_dataProvider->SetNavigationRequestCallback([&] (IDataItem::Ptr data)
	{
		const auto modelCreator = MODE_DESCRIPTORS[m_impl->mode].second.modelCreator;
		auto model = std::invoke(modelCreator, m_modelProvider, std::move(data), std::ref(*m_impl));
		m_impl->models[m_impl->mode].reset(std::move(model));
		Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
	});

	PLOGD << "TreeViewControllerNavigation created";
}

TreeViewControllerNavigation::~TreeViewControllerNavigation()
{
	PLOGD << "TreeViewControllerNavigation destroyed";
}

void TreeViewControllerNavigation::RequestNavigation() const
{
	m_dataProvider->RequestNavigation();
}

std::vector<const char *> TreeViewControllerNavigation::GetModeNames() const
{
	return GetModeNamesImpl(MODE_DESCRIPTORS);
}

void TreeViewControllerNavigation::SetCurrentId(QString id)
{
	assert(!id.isEmpty());
	m_dataProvider->SetNavigationId(std::move(id));
}

void TreeViewControllerNavigation::OnModeChanged(const QString & mode)
{
	m_impl->mode = GetModeIndex(mode);
	m_dataProvider->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	Perform(&IObserver::OnModeChanged, m_impl->mode);

	if (m_impl->models[m_impl->mode])
		return Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());

	m_dataProvider->RequestNavigation();
}

int TreeViewControllerNavigation::GetModeIndex(const QString & mode) const
{
	const auto strMode = mode.toStdString();
	const auto enumMode = FindSecond(MODE_DESCRIPTORS, strMode.data(), MODE_DESCRIPTORS[0].second, PszComparerCaseInsensitive {}).navigationMode;
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerNavigation::GetItemType() const noexcept
{
	return ItemType::Navigation;
}

ViewMode TreeViewControllerNavigation::GetViewMode() const noexcept
{
	return MODE_DESCRIPTORS[m_impl->mode].second.viewMode;
}

IDataItem::Ptr TreeViewControllerNavigation::RequestContextMenu(const QModelIndex & /*index*/) const
{
	return MODE_DESCRIPTORS[m_impl->mode].second.menuRequester();
}

void TreeViewControllerNavigation::OnContextMenuTriggered(const QList<QModelIndex> & indexList, const QModelIndex & index, int id) const
{
	const auto invoker = FindSecond(MENU_HANDLERS, static_cast<MenuAction>(id));
	std::invoke(invoker, *m_impl, std::cref(indexList), std::cref(index));
}
