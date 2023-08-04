#include "TreeViewControllerBooks.h"

#include <QString>
#include <plog/Log.h>

#include "fnd/FindPair.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "interface/constants/Enums.h"
#include "interface/logic/IAnnotationController.h"
#include "model/IModelObserver.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Books";

using ModelCreator = std::shared_ptr<QAbstractItemModel>(AbstractModelProvider::*)(IDataItem::Ptr, IModelObserver &) const;

struct ModeDescriptor
{
	ViewMode viewMode;
	ModelCreator modelCreator;
};

constexpr std::pair<const char *, ModeDescriptor> MODE_NAMES[]
{
	{ QT_TRANSLATE_NOOP("Books", "List"), { ViewMode::List, &AbstractModelProvider::CreateListModel} },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), { ViewMode::Tree, &AbstractModelProvider::CreateTreeModel} },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

auto GetViewModeImpl(const std::string & strMode)
{
	return FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
}

}

struct TreeViewControllerBooks::Impl final
	: IModelObserver
{
	ViewMode viewMode { ViewMode::Unknown };
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> model { std::shared_ptr<QAbstractItemModel>() };
	PropagateConstPtr<IAnnotationController, std::shared_ptr> annotationController;

	explicit Impl(std::shared_ptr<IAnnotationController> annotationController)
		: annotationController(std::move(annotationController))
	{
	}
};

TreeViewControllerBooks::TreeViewControllerBooks(std::shared_ptr<ISettings> settings
	, std::shared_ptr<DataProvider> dataProvider
	, std::shared_ptr<AbstractModelProvider> modelProvider
	, std::shared_ptr<IAnnotationController> annotationController
)
	: AbstractTreeViewController(CONTEXT
		, std::move(settings)
		, std::move(dataProvider)
		, std::move(modelProvider)
	)
	, m_impl(std::move(annotationController))
{
	Setup();

	m_dataProvider->SetBookRequestCallback([&] (IDataItem::Ptr data)
	{
		assert(m_impl->viewMode != ViewMode::Unknown);
		const auto invoker = MODE_NAMES[static_cast<int>(m_impl->viewMode)].second.modelCreator;
		m_impl->model.reset(((*m_modelProvider).*invoker)(std::move(data), *m_impl));
		Perform(&IObserver::OnModelChanged, m_impl->model.get());
	});

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

void TreeViewControllerBooks::SetCurrentId(QString id)
{
	m_impl->annotationController->SetCurrentBookId(std::move(id));
}

void TreeViewControllerBooks::OnModeChanged(const QString & mode)
{
	m_impl->viewMode = GetViewModeImpl(mode.toStdString()).viewMode;
	m_dataProvider->SetBooksViewMode(m_impl->viewMode);
	Perform(&IObserver::OnModeChanged, static_cast<int>(m_impl->viewMode));
}

int TreeViewControllerBooks::GetModeIndex(const QString & mode) const
{
	const auto enumMode = GetViewModeImpl(mode.toStdString()).viewMode;
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
