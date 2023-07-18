#include "TreeViewControllerBooks.h"

#include <qglobal.h>
#include <QVariant>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
#include "data/Types.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Books";

constexpr std::pair<const char *, ViewMode> MODE_NAMES[]
{
	{ QT_TRANSLATE_NOOP("Books", "List"), ViewMode::List },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), ViewMode::Tree },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

}

class TreeViewControllerBooks::Impl final
{
};

TreeViewControllerBooks::TreeViewControllerBooks(std::shared_ptr<ISettings> settings
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
	PLOGD << "TreeViewControllerBooks created";
}

TreeViewControllerBooks::~TreeViewControllerBooks()
{
	PLOGD << "TreeViewControllerBooks destroyed";
}

std::vector<const char *> TreeViewControllerBooks::GetModeNames() const
{
	return GetModeNamesImpl(MODE_NAMES);
}

void TreeViewControllerBooks::SetModeIndex(const int index)
{
	SetMode(MODE_NAMES[index].first);
}

void TreeViewControllerBooks::OnModeChanged(const QVariant & mode)
{
	const auto intMode = GetModeIndex(mode);
	m_dataProvider->SetViewMode(static_cast<ViewMode>(intMode));
	Perform(&IObserver::OnModeChanged, intMode);
}

int TreeViewControllerBooks::GetModeIndex(const QVariant & mode) const
{
	const auto strMode = mode.toString().toStdString();
	const auto enumMode = FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerBooks::GetItemType() const noexcept
{
	return ItemType::Books;
}
