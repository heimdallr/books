#include "TreeViewControllerBooks.h"

#include <QModelIndex>
#include <QPointer>
#include <QString>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"
#include "interface/constants/SettingsConstant.h"

#include "data/DataProvider.h"
#include "data/ModelProvider.h"
#include "database/DatabaseUtil.h"
#include "shared/BooksContextMenuProvider.h"

#include "log.h"

using namespace HomeCompa;
using namespace HomeCompa::Flibrary;

namespace {

constexpr auto CONTEXT = "Books";

constexpr auto VIEW_MODE_WITH_NAVIGATION_KEY_TEMPLATE   = "ui/%1/Mode_%2";
constexpr auto PREFER_SEPARATED_VIEW_MODE_BY_NAVIGATION = "Preferences/Books/SeparatedViewModeByNavigation";

using ModelCreator = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr) const;

struct ModeDescriptor
{
	ViewMode     viewMode;
	ModelCreator modelCreator;
};

constexpr std::pair<const char*, ModeDescriptor> MODE_NAMES[] {
	{ QT_TRANSLATE_NOOP("Books", "List"), { ViewMode::List, &IModelProvider::CreateBookListModel } },
	{ QT_TRANSLATE_NOOP("Books", "Tree"), { ViewMode::Tree, &IModelProvider::CreateBookTreeModel } },
};

static_assert(std::size(MODE_NAMES) == static_cast<size_t>(ViewMode::Last));

auto GetViewModeImpl(const std::string& strMode)
{
	return FindSecond(MODE_NAMES, strMode.data(), MODE_NAMES[0].second, PszComparer {});
}

class ModeController final : public AbstractTreeViewController::IModeController
{
public:
	ModeController(const ISettings& settings, PropagateConstPtr<IModeController> source)
		: m_source { std::move(source) }
		, m_setKey { settings.Get(PREFER_SEPARATED_VIEW_MODE_BY_NAVIGATION, false) ? &ModeController::SetKeyWrapper : &ModeController::SetKeyStub }
	{
	}

private: // IModeController
	QString GetMode() const override
	{
		return m_source->GetMode();
	}

	void SetMode(const QString& value) override
	{
		m_source->SetMode(value);
	}

	void SetKey(QString value) override
	{
		std::invoke(m_setKey, this, std::move(value));
	}

private:
	void SetKeyWrapper(QString value)
	{
		m_source->SetKey(std::move(value));
	}

	void SetKeyStub(QString /*value*/)
	{
	}

private:
	PropagateConstPtr<IModeController> m_source;
	void (ModeController::*m_setKey)(QString);
};

} // namespace

struct TreeViewControllerBooks::Impl
{
	ViewMode                                                  viewMode { ViewMode::Unknown };
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>    model { std::shared_ptr<QAbstractItemModel>() };
	std::weak_ptr<const ILogicFactory>                        logicFactory;
	PropagateConstPtr<IAnnotationController, std::shared_ptr> annotationController;
	std::shared_ptr<const IDatabaseUser>                      databaseUser;
	PropagateConstPtr<IBookInfoProvider, std::shared_ptr>     dataProvider;
	std::shared_ptr<const IUiFactory>                         uiFactory;
	std::shared_ptr<const IBookInteractor>                    bookInteractor;

	Impl(
		std::weak_ptr<const ILogicFactory>     logicFactory,
		std::shared_ptr<IAnnotationController> annotationController,
		std::shared_ptr<const IDatabaseUser>   databaseUser,
		std::shared_ptr<IBookInfoProvider>     dataProvider,
		std::shared_ptr<const IUiFactory>      uiFactory,
		std::shared_ptr<const IBookInteractor> bookInteractor
	)
		: logicFactory { std::move(logicFactory) }
		, annotationController { std::move(annotationController) }
		, databaseUser { std::move(databaseUser) }
		, dataProvider { std::move(dataProvider) }
		, uiFactory { std::move(uiFactory) }
		, bookInteractor { std::move(bookInteractor) }
	{
	}
};

TreeViewControllerBooks::TreeViewControllerBooks(
	std::shared_ptr<ISettings>                  settings,
	const std::shared_ptr<IModelProvider>&      modelProvider,
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<IBookInfoProvider>          dataProvider,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<const IBookInteractor>      bookInteractor,
	std::shared_ptr<IAnnotationController>      annotationController,
	std::shared_ptr<IDatabaseUser>              databaseUser
)
	: AbstractTreeViewController(CONTEXT, std::move(settings), modelProvider)
	, m_impl(logicFactory, std::move(annotationController), std::move(databaseUser), std::move(dataProvider), std::move(uiFactory), std::move(bookInteractor))
{
	m_modeController.reset(std::make_unique<ModeController>(*m_settings, std::move(m_modeController)));

	Setup();

	m_impl->dataProvider->SetBookRequestCallback([&](IDataItem::Ptr data) {
		assert(m_impl->viewMode != ViewMode::Unknown);
		const auto invoker = MODE_NAMES[static_cast<int>(m_impl->viewMode)].second.modelCreator;
		auto       model   = std::invoke(invoker, IModelProvider::Lock(m_modelProvider), std::move(data));
		m_impl->model.reset(std::move(model));
		Perform(&ITreeViewController::IObserver::OnModelChanged, m_impl->model.get());
	});

	m_impl->dataProvider->RegisterObserver(this);

	PLOGV << "TreeViewControllerBooks created";
}

TreeViewControllerBooks::~TreeViewControllerBooks()
{
	m_impl->dataProvider->UnregisterObserver(this);

	PLOGV << "TreeViewControllerBooks destroyed";
}

std::vector<std::pair<const char*, int>> TreeViewControllerBooks::GetModeNames() const
{
	std::vector<std::pair<const char*, int>> result;
	std::ranges::transform(MODE_NAMES, std::back_inserter(result), [](const auto& item) {
		return std::make_pair(item.first, static_cast<int>(item.second.viewMode));
	});
	return result;
}

void TreeViewControllerBooks::SetCurrentId(const ItemType type, QString id, bool /*force*/)
{
	m_impl->annotationController->SetCurrentBookId(type == ItemType::Books ? std::move(id) : QString {});
}

const QString& TreeViewControllerBooks::GetNavigationId() const noexcept
{
	return m_impl->dataProvider->GetNavigationID();
}

void TreeViewControllerBooks::OnModeChanged(const QString& mode)
{
	m_impl->viewMode = GetViewModeImpl(mode.toStdString()).viewMode;
	m_impl->dataProvider->SetBooksViewMode(m_impl->viewMode);
	Perform(&ITreeViewController::IObserver::OnModeChanged, static_cast<int>(m_impl->viewMode));
}

int TreeViewControllerBooks::GetModeIndex(const QString& mode) const
{
	const auto enumMode = GetViewModeImpl(mode.toStdString()).viewMode;
	return static_cast<int>(enumMode);
}

ItemType TreeViewControllerBooks::GetItemType() const noexcept
{
	return ItemType::Books;
}

QString TreeViewControllerBooks::GetItemName() const
{
	return Loc::Books;
}

ViewMode TreeViewControllerBooks::GetViewMode() const noexcept
{
	return m_impl->viewMode;
}

void TreeViewControllerBooks::RequestContextMenu(const QModelIndex& index, const RequestContextMenuOptions options, RequestContextMenuCallback callback)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->Request(index, options, [&, menuProvider, id = index.data(Role::Id).toString(), callback = std::move(callback)](const IDataItem::Ptr& item, const std::set<QString>& ids) mutable {
		callback(id, item, ids);
		menuProvider.reset();
	});
}

void TreeViewControllerBooks::OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item)
{
	auto menuProvider = ILogicFactory::Lock(m_impl->logicFactory)->CreateBooksContextMenuProvider();
	menuProvider->OnContextMenuTriggered(
		model,
		index,
		indexList,
		std::move(item),
		[this, model = QPointer(model), menuProvider, id = index.data(Role::Id).toString()](const IDataItem::Ptr& itemResult, const std::set<QString>& ids) mutable {
			Perform(&ITreeViewController::IObserver::OnContextMenuTriggered, std::cref(id), std::cref(itemResult));
			if (!model.isNull() && !m_settings->Get(Constant::Settings::PREFER_KEEP_CHECK, false))
				model->setData({}, QVariant::fromValue(ids), Role::Uncheck);
			menuProvider.reset();
		}
	);
}

void TreeViewControllerBooks::OnDoubleClicked(const QModelIndex& index) const
{
	if (index.data(Role::Type).value<ItemType>() == ItemType::Navigation)
	{
		const auto navigationModeVar = index.data(Role::NavigationMode);
		if (!navigationModeVar.isValid())
			return;

		const auto navigationMode = navigationModeVar.value<NavigationMode>();
		if (navigationMode == NavigationMode::Unknown)
			return;

		const auto ids = index.data(Role::Id).toString().split('|', Qt::SkipEmptyParts);
		if (ids.isEmpty())
			return;

		if (ids.size() == 1)
			return ILogicFactory::Lock(m_impl->logicFactory)->FindBook(navigationMode, ids.front());

		static constexpr std::pair<NavigationMode, const char*> descriptions[] {
			{ NavigationMode::Authors, "select " FLIBRARY_SQL_FULL_AUTHOR_NAME " from Authors a where AuthorID in (%1)" },
			{  NavigationMode::Series,                          "select SeriesTitle from Series where SeriesID in (%1)" },
		};

		const auto* queryText = FindSecond(descriptions, navigationMode, nullptr);
		if (!queryText)
			return;

		const auto db    = m_impl->databaseUser->Database();
		const auto query = db->CreateQuery(QString(queryText).arg(QStringList::fromVector(QVector<QString>(ids.size(), QString { '?' })).join(',')));
		for (auto binder = query->Binder(); const auto& id : ids)
			binder << id.toLongLong();

		return m_impl->uiFactory->ExecMenu(
			*query | std::views::transform([](const auto& item) {
				return item.template Get<QString>(0);
			}) | std::ranges::to<QStringList>(),
			[&](const int n) {
				ILogicFactory::Lock(m_impl->logicFactory)->FindBook(navigationMode, ids[static_cast<qsizetype>(n)]);
			}
		);
	}

	if (index.data(Role::Type).value<ItemType>() != ItemType::Books)
		return;

	const auto id     = index.data(Role::Id).toLongLong();
	const auto column = index.data(Role::Remap).toInt();
	const auto value  = index.data(Qt::DisplayRole).toString();

	m_impl->bookInteractor->OnDoubleClicked(id, QString("%1|%2").arg(column).arg(value));
}

void TreeViewControllerBooks::OnNavigationModeChanged()
{
	m_modeController->SetKey(QString(VIEW_MODE_WITH_NAVIGATION_KEY_TEMPLATE).arg(m_context, NAVIGATION_NAMES[static_cast<size_t>(m_impl->dataProvider->GetNavigationMode())].first));
	OnModeChanged(m_modeController->GetMode());
}

void TreeViewControllerBooks::OnBooksSelected(NavigationMode, IDataItem::Ptr)
{
}
