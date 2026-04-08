#include "RecentOpenBookController.h"

#include "database/interface/IDatabase.h"

#include "util/FunctorExecutionForwarder.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto MAX_MENU_ITEM_COUNT_KEY    = "Preferences/RecentBooksMenuMaxCount";
constexpr auto MENU_ITEM_TITLE_FORMAT_KEY = "Preferences/RecentBooksMenuTitleFormat";

constexpr auto MAX_MENU_ITEM_DEFAULT          = 16;
constexpr auto MENU_ITEM_TITLE_FORMAT_DEFAULT = "%1 \t %2";

constexpr auto TABLE_NAME = "Export_List_User";

} // namespace

class RecentOpenBookController::Impl final : DB::IDatabaseObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(const ISettings& settings, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IBookInteractor> bookInteractor)
		: m_uiFactory { std::move(uiFactory) }
		, m_databaseUser { std::move(databaseUser) }
		, m_bookInteractor { std::move(bookInteractor) }
		, m_maxMenuItemCount { settings.Get(MAX_MENU_ITEM_COUNT_KEY, MAX_MENU_ITEM_DEFAULT) }
		, m_menuItemTitleFormat { settings.Get(MENU_ITEM_TITLE_FORMAT_KEY, MENU_ITEM_TITLE_FORMAT_DEFAULT) }
	{
		if (m_maxMenuItemCount > 0)
			if (auto db = m_databaseUser->CheckDatabase())
				db->RegisterObserver(this);
	}

	void SetMenu(QMenu* menu)
	{
		m_menu = menu;
		Update();
	}

	~Impl() override
	{
		if (m_maxMenuItemCount > 0)
			if (auto db = m_databaseUser->CheckDatabase())
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
		m_uiFactory->UpdateRecentOpenBookControllerMenu(*m_menu, m_maxMenuItemCount, m_menuItemTitleFormat, [this](const long long id) {
			m_bookInteractor->OnRecentBookMenuTriggered(id);
		});
	}

private:
	std::shared_ptr<const IUiFactory>      m_uiFactory;
	std::shared_ptr<const IDatabaseUser>   m_databaseUser;
	std::shared_ptr<const IBookInteractor> m_bookInteractor;
	const int                              m_maxMenuItemCount;
	const QString                          m_menuItemTitleFormat;
	QMenu*                                 m_menu { nullptr };
	Util::FunctorExecutionForwarder        m_forwarder;
};

RecentOpenBookController::RecentOpenBookController(
	const std::shared_ptr<const ISettings>& settings,
	std::shared_ptr<const IUiFactory>       uiFactory,
	std::shared_ptr<const IDatabaseUser>    databaseUser,
	std::shared_ptr<const IBookInteractor>  bookInteractor
)
	: m_impl { *settings, std::move(uiFactory), std::move(databaseUser), std::move(bookInteractor) }
{
}

RecentOpenBookController::~RecentOpenBookController() = default;

void RecentOpenBookController::SetMenu(QMenu* menu)
{
	m_impl->SetMenu(menu);
}
