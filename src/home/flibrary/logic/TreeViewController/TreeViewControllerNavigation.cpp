#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QTimer>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/constants/Enums.h"
#include "model/IModelObserver.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";

using ModelCreator = std::shared_ptr<QAbstractItemModel> (AbstractModelProvider::*)(DataItem::Ptr, IModelObserver &) const;

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
	QTimer navigationTimer;
	QTimer booksTimer;
	int mode = { -1 };

	Impl()
	{
		for ([[maybe_unused]]const auto & _ : MODE_DESCRIPTORS)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());

		navigationTimer.setSingleShot(true);
		navigationTimer.setInterval(std::chrono::milliseconds(200));

		booksTimer.setSingleShot(true);
		booksTimer.setInterval(std::chrono::milliseconds(200));
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

	m_dataProvider->SetNavigationRequestCallback([&] (DataItem::Ptr data)
	{
		const auto modelCreator = MODE_DESCRIPTORS[m_impl->mode].second.modelCreator;
		auto model = std::invoke(modelCreator, m_modelProvider, std::move(data), std::ref(*m_impl));
		m_impl->models[m_impl->mode].reset(std::move(model));
		Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
	});

	QObject::connect(&m_impl->navigationTimer, &QTimer::timeout, &m_impl->navigationTimer, [&] { m_dataProvider->RequestNavigation(); });
	QObject::connect(&m_impl->booksTimer, &QTimer::timeout, &m_impl->booksTimer, [&] { m_dataProvider->RequestBooks(); });

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

void TreeViewControllerNavigation::SetModeIndex(const int index)
{
	SetMode(MODE_DESCRIPTORS[index].first);
}

void TreeViewControllerNavigation::SetCurrentId(QString id)
{
	assert(!id.isEmpty());
	m_dataProvider->SetNavigationId(std::move(id));
	m_impl->booksTimer.start();
}

void TreeViewControllerNavigation::OnModeChanged(const QVariant & mode)
{
	m_impl->mode = GetModeIndex(mode);
	m_dataProvider->SetNavigationMode(static_cast<NavigationMode>(m_impl->mode));
	Perform(&IObserver::OnModeChanged, m_impl->mode);

	if (m_impl->models[m_impl->mode])
		return Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());

	m_impl->navigationTimer.start();
}

int TreeViewControllerNavigation::GetModeIndex(const QVariant & mode) const
{
	const auto strMode = mode.toString().toStdString();
	const auto enumMode = FindSecond(MODE_DESCRIPTORS, strMode.data(), MODE_DESCRIPTORS[0].second, PszComparer {}).navigationMode;
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
