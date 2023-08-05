#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QTimer>
#include <QVariant>

#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/constants/Enums.h"
#include "model/IModelObserver.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";

using ModelCreator = std::shared_ptr<QAbstractItemModel> (AbstractModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
	NavigationMode navigationMode;
};

constexpr std::pair<const char *, ModeDescriptor> MODE_DESCRIPTORS[]
{
	{ QT_TRANSLATE_NOOP("Navigation", "Authors") , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Authors } },
	{ QT_TRANSLATE_NOOP("Navigation", "Series")  , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Series } },
	{ QT_TRANSLATE_NOOP("Navigation", "Genres")  , { ViewMode::Tree, &AbstractModelProvider::CreateTreeModel, NavigationMode::Genres } },
	{ QT_TRANSLATE_NOOP("Navigation", "Archives"), { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Archives } },
	{ QT_TRANSLATE_NOOP("Navigation", "Groups")  , { ViewMode::List, &AbstractModelProvider::CreateListModel, NavigationMode::Groups } },
};

static_assert(std::size(MODE_DESCRIPTORS) == static_cast<size_t>(NavigationMode::Last));

}

struct TreeViewControllerNavigation::Impl final
	: IModelObserver
{
	std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	int mode = { -1 };

	Impl()
	{
		for ([[maybe_unused]]const auto & _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());
	}
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<AbstractModelProvider> modelProvider
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
		, std::move(modelProvider)
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
