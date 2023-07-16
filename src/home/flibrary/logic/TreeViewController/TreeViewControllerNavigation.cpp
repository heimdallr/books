#include "TreeViewControllerNavigation.h"

#include <qglobal.h>
#include <QVariant>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
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

class TreeViewControllerNavigation::Impl final
{
};

TreeViewControllerNavigation::TreeViewControllerNavigation(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
	)
{
	Setup();
}

TreeViewControllerNavigation::~TreeViewControllerNavigation() = default;

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
	const auto intMode = GetModeIndex(mode);
	m_dataProvider->SetNavigationMode(static_cast<NavigationMode>(intMode));
	SetMode(intMode);
}

int TreeViewControllerNavigation::GetModeIndex(const QVariant & mode) const
{
	const auto strMode = mode.toString().toStdString();
	const auto enumMode = FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
	return static_cast<int>(enumMode);
}
