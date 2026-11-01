#include "RecentOpenBookController.h"

#include <QTimer>

#include "database/interface/IDatabase.h"

#include "interface/logic/IDatabaseProvider.h"

#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

constexpr auto TABLE_NAME = "Export_List_User";

} // namespace

class RecentOpenBookController::Impl final : IDatabaseProvider::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(const ICollectionProvider& collectionProvider, IDatabaseController& databaseController, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<const IDatabaseProvider> databaseProvider)
		: m_uiFactory { std::move(uiFactory) }
		, m_databaseProvider { std::move(databaseProvider) }
	{
		if (!collectionProvider.ActiveCollectionExists())
			return;

		m_timer.setInterval(std::chrono::seconds(1));
		m_timer.setSingleShot(true);
		QObject::connect(&m_timer, &QTimer::timeout, [this] {
			UpdateImpl();
		});

		databaseController.RegisterObserver(this);
	}

	void SetMenu(QMenu* menu)
	{
		m_menu = menu;
		Update();
	}

	~Impl() override = default;

private: // IDatabaseObserver
	void OnInsert(std::string_view /*dbName*/, const std::string_view tableName, int64_t /*rowId*/) override
	{
		if (tableName == TABLE_NAME)
			Update();
	}

	void OnDelete(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		OnInsert(dbName, tableName, rowId);
	}

	void OnUpdate(std::string_view /*dbName*/, std::string_view /*tableName*/, int64_t /*rowId*/) override
	{
	}

private:
	void Update()
	{
		m_forwarder.Forward([this] {
			m_timer.start();
		});
	}

	void UpdateImpl() const
	{
		assert(m_menu);
		m_uiFactory->UpdateRecentOpenBookControllerMenu(*m_menu);
	}

private:
	std::shared_ptr<const IUiFactory>        m_uiFactory;
	std::shared_ptr<const IDatabaseProvider> m_databaseProvider;
	QMenu*                                   m_menu { nullptr };
	Util::FunctorExecutionForwarder          m_forwarder;
	QTimer                                   m_timer;
};

RecentOpenBookController::RecentOpenBookController(
	const std::shared_ptr<const ICollectionProvider>& collectionProvider,
	const std::shared_ptr<IDatabaseController>&       databaseController,
	std::shared_ptr<const IUiFactory>                 uiFactory,
	std::shared_ptr<const IDatabaseProvider>          databaseProvider
)
	: m_impl(*collectionProvider, *databaseController, std::move(uiFactory), std::move(databaseProvider))
{
}

RecentOpenBookController::~RecentOpenBookController() = default;

void RecentOpenBookController::SetMenu(QMenu* menu)
{
	m_impl->SetMenu(menu);
}
