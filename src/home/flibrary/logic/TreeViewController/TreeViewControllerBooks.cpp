#include "TreeViewControllerBooks.h"

#include <QString.h>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataItem.h"
#include "data/DataProvider.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Books";

constexpr std::pair<const char *, ViewMode> MODE_NAMES[]
{
	{ QT_TRANSLATE_NOOP("Books", "List"), ViewMode::List },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), ViewMode::Tree },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

auto GetViewModeImpl(const std::string & strMode)
{
	return FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
}

}

struct TreeViewControllerBooks::Impl final
{
	ViewMode viewMode { ViewMode::Unknown };
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

void TreeViewControllerBooks::SetCurrentId(QString /*id*/)
{
	// Get annotation
}

void TreeViewControllerBooks::OnModeChanged(const QVariant & mode)
{
	m_impl->viewMode = GetViewModeImpl(mode.toString().toStdString());
	m_dataProvider->SetViewMode(m_impl->viewMode);
	Perform(&IObserver::OnModeChanged, static_cast<int>(m_impl->viewMode));
}

int TreeViewControllerBooks::GetModeIndex(const QVariant & mode) const
{
	const auto enumMode = GetViewModeImpl(mode.toString().toStdString());
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerBooks::GetItemType() const noexcept
{
	return ItemType::Books;
}

ViewMode TreeViewControllerBooks::GetViewMode() const noexcept
{
	return m_impl->viewMode;
}