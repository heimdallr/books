#include "RecentOpenBookController.h"

#include <QMenu>

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

constexpr auto QUERY = R"(select distinct b.BookID, b.Title, (
    select(group_concat(author, ', ')) from (
        select a.LastName || coalesce(' ' || substr(nullif(a.FirstName, ''), 1, 1)||'.', '') || coalesce(substr(nullif(a.MiddleName, ''), 1, 1)||'.', '') author
        from Authors a
        join Author_List l on l.AuthorID = a.AuthorID and l.BookID = b.BookID
        order by l.OrdNum
        limit 3
    )
) Author
from Export_List_User e
join Books b on b.BookID = e.BookID
where e.ExportType = 0 
order by e.CreatedAt desc
limit {}
)";

}

class RecentOpenBookController::Impl final : DB::IDatabaseObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(const ISettings& settings, std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IBookInteractor> bookInteractor)
		: m_databaseUser { std::move(databaseUser) }
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
		m_menu->clear();
		m_menu->menuAction()->setEnabled(false);
		if (m_maxMenuItemCount < 1)
			return m_menu->menuAction()->setVisible(false);

		auto db = m_databaseUser->CheckDatabase();
		if (!db)
			return;

		m_databaseUser->Execute({ "Update recent books menu", [this, db = std::move(db)] {
									 const auto                                 query = db->CreateQuery(std::format(QUERY, m_maxMenuItemCount));
									 std::vector<std::pair<long long, QString>> data;
									 for (query->Execute(); !query->Eof(); query->Next())
										 data.emplace_back(query->Get<long long>(0), m_menuItemTitleFormat.arg(query->Get<const char*>(2), query->Get<const char*>(1)));
									 return [this, data = std::move(data)](size_t) {
										 m_menu->menuAction()->setEnabled(!data.empty());
										 for (const auto& [id, title] : data)
										 {
											 auto* action = m_menu->addAction(title);
											 QObject::connect(action, &QAction::triggered, [this, id] {
												 m_bookInteractor->OnRecentBookMenuTriggered(id);
											 });
										 }
									 };
								 } });
	}

private:
	std::shared_ptr<const IDatabaseUser>   m_databaseUser;
	std::shared_ptr<const IBookInteractor> m_bookInteractor;
	const int                              m_maxMenuItemCount;
	const QString                          m_menuItemTitleFormat;
	QMenu*                                 m_menu { nullptr };
	Util::FunctorExecutionForwarder        m_forwarder;
};

RecentOpenBookController::RecentOpenBookController(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IBookInteractor> bookInteractor)
	: m_impl { *settings, std::move(databaseUser), std::move(bookInteractor) }
{
}

RecentOpenBookController::~RecentOpenBookController() = default;

void RecentOpenBookController::SetMenu(QMenu* menu)
{
	m_impl->SetMenu(menu);
}
