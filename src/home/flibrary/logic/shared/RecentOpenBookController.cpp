#include "RecentOpenBookController.h"

#include "database/interface/IDatabase.h"

#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto TABLE_NAME = "Export_List_User";

} // namespace

class RecentOpenBookController::Impl final : DB::IDatabaseObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_uiFactory { std::move(uiFactory) }
		, m_databaseUser { std::move(databaseUser) }
	{
		if (const auto db = m_databaseUser->CheckDatabase())
			db->RegisterObserver(this);
	}

	void SetMenu(QMenu* menu)
	{
		m_menu = menu;
		Update();
	}

	~Impl() override
	{
		if (const auto db = m_databaseUser->CheckDatabase())
			db->UnregisterObserver(this);
	}

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
	void Update() const
	{
		m_forwarder.Forward([this] {
			UpdateImpl();
		});
	}

	void UpdateImpl() const
	{
		assert(m_menu);
		m_uiFactory->UpdateRecentOpenBookControllerMenu(*m_menu);
	}

private:
	std::shared_ptr<const IUiFactory>    m_uiFactory;
	std::shared_ptr<const IDatabaseUser> m_databaseUser;
	QMenu*                               m_menu { nullptr };
	Util::FunctorExecutionForwarder      m_forwarder;
};

RecentOpenBookController::RecentOpenBookController(
	std::shared_ptr<const IUiFactory>       uiFactory,
	std::shared_ptr<const IDatabaseUser>    databaseUser
)
	: m_impl { std::move(uiFactory), std::move(databaseUser) }
{
}

RecentOpenBookController::~RecentOpenBookController() = default;

void RecentOpenBookController::SetMenu(QMenu* menu)
{
	m_impl->SetMenu(menu);
}
