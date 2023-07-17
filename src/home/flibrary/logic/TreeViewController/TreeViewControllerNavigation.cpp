#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QAbstractItemModel>
#include <QTimer>
#include <QVariant>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
#include "data/Model.h"
#include "data/ModelProvider.h"
#include "data/Types.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Navigation";

constexpr std::pair<const char *, NavigationMode> MODE_NAMES[]
{
	{ QT_TRANSLATE_NOOP("Navigation", "Authors"), NavigationMode::Authors },
	{ QT_TRANSLATE_NOOP("Navigation", "Series"), NavigationMode::Series },
	{ QT_TRANSLATE_NOOP("Navigation", "Genres"), NavigationMode::Genres },
	{ QT_TRANSLATE_NOOP("Navigation", "Archives"), NavigationMode::Archives },
	{ QT_TRANSLATE_NOOP("Navigation", "Groups"), NavigationMode::Groups },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(NavigationMode::Last));

}

struct TreeViewControllerNavigation::Impl final
	: IModelObserver
{
	std::vector<PropagateConstPtr<QAbstractItemModel, std::shared_ptr>> models;
	QTimer navigationTimer;
	int mode = -1;

	Impl()
	{
		for ([[maybe_unused]]const auto & _ : MODE_NAMES)
			models.emplace_back(std::shared_ptr<QAbstractItemModel>());

		navigationTimer.setSingleShot(true);
		navigationTimer.setInterval(std::chrono::milliseconds(200));
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
	QObject::connect(&m_impl->navigationTimer, &QTimer::timeout, &m_impl->navigationTimer, [&]
	{
		m_dataProvider->RequestNavigation([&] (DataItem::Ptr data)
		{
			m_impl->models[m_impl->mode].reset(m_modelProvider->CreateModel(std::move(data), *m_impl));
			Perform(&IObserver::OnModelChanged, m_impl->models[m_impl->mode].get());
		});
	});

	PLOGD << "TreeViewControllerNavigation created";
}

TreeViewControllerNavigation::~TreeViewControllerNavigation()
{
	PLOGD << "TreeViewControllerNavigation destroyed";
}

std::vector<const char *> TreeViewControllerNavigation::GetModeNames() const
{
	return GetModeNamesImpl(MODE_NAMES);
}

void TreeViewControllerNavigation::SetModeIndex(const int index)
{
	SetMode(MODE_NAMES[index].first);
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
	const auto enumMode = FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
	return static_cast<int>(enumMode);
}
