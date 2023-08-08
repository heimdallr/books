#include "BooksContextMenuProvider.h"

#include <QModelIndex>
#include <QString>
#include <plog/Log.h>

#include "DatabaseUser.h"
#include "data/DataItem.h"
#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ModelRole.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {
constexpr auto CONTEXT = "BookContextMenu";
constexpr auto READ_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "Read");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("BookContextMenu", "Groups");
constexpr auto GROUPS_ADD_TO_NEW = QT_TRANSLATE_NOOP("BookContextMenu", "New group...");
constexpr auto GROUPS_ADD_TO = QT_TRANSLATE_NOOP("BookContextMenu", "Add to");
constexpr auto GROUPS_REMOVE_FROM = QT_TRANSLATE_NOOP("BookContextMenu", "Remove from");
constexpr auto GROUPS_REMOVE_FROM_ALL = QT_TRANSLATE_NOOP("BookContextMenu", "All");
constexpr auto REMOVE_BOOK = QT_TRANSLATE_NOOP("BookContextMenu", "Remove book");
constexpr auto REMOVE_BOOK_UNDO = QT_TRANSLATE_NOOP("BookContextMenu", "Undo book deletion");
constexpr auto SEND_TO = QT_TRANSLATE_NOOP("BookContextMenu", "Send to device");
constexpr auto SEND_AS_ARCHIVE = QT_TRANSLATE_NOOP("BookContextMenu", "In zip archive");
constexpr auto SEND_AS_IS = QT_TRANSLATE_NOOP("BookContextMenu", "In original format");

constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title, coalesce(gl.BookID, -1) from Groups_User g left join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = ?";

enum class MenuAction
{
	None = -1,
	ReadBook,
	RemoveBook,
	UndoRemoveBook,
	AddToNewGroup,
	AddToGroup,
	RemoveFromGroup,
	RemoveFromAllGroups,
	SendAsArchive,
	SendAsIs,
};

auto Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

IDataItem::Ptr & Add(const IDataItem::Ptr & dst, QString title = {}, const MenuAction id = MenuAction::None)
{
	auto item = MenuItem::Create();
	item->SetData(std::move(title), MenuItem::Column::Title);
	item->SetData(QString::number(static_cast<int>(id)), MenuItem::Column::Id);
	return dst->AppendChild(std::move(item));
}

void CreateGroupMenu(const IDataItem::Ptr & parent, const QString & id, DB::IDatabase & db)
{
	const auto add = Add(parent, Tr(GROUPS_ADD_TO), MenuAction::AddToGroup);
	const auto remove = Add(parent, Tr(GROUPS_REMOVE_FROM), MenuAction::RemoveFromGroup);

	const auto query = db.CreateQuery(GROUPS_QUERY);
	query->Bind(0, id.toInt());
	for (query->Execute(); !query->Eof(); query->Next())
	{
		const auto needAdd = query->Get<long long>(2) < 0;
		const auto & item = needAdd ? add : remove;
		Add(item, query->Get<const char *>(1), needAdd ? MenuAction::AddToGroup : MenuAction::RemoveFromGroup)
			->SetData(QString::number(query->Get<long long>(0)), MenuItem::Column::Parameter);
	}

	if (remove->GetChildCount() > 0)
	{
		Add(remove);
		Add(remove, Tr(GROUPS_REMOVE_FROM_ALL), MenuAction::RemoveFromAllGroups);
	}
	else
	{
		Add(remove);
		remove->SetData(QVariant(false).toString(), MenuItem::Column::Enabled);
	}

	if (add->GetChildCount() > 0)
		Add(add);

	Add(add, Tr(GROUPS_ADD_TO_NEW), MenuAction::AddToNewGroup);
}

}

class BooksContextMenuProvider::Impl
{
public:
	explicit Impl(std::shared_ptr<DatabaseUser> databaseUser)
		: m_databaseUser(std::move(databaseUser))
	{
	}

public:
	void Request(const QModelIndex & index, Callback callback)
	{
		m_databaseUser->Execute({ "Create context menu", [
			  id = index.data(Role::Id).toString()
			, type = index.data(Role::Type).value<ItemType>()
			, removed = index.data(Role::IsRemoved).toBool()
			, callback = std::move(callback)
			, db = m_databaseUser->Database()
		]() mutable
		{
			auto result = MenuItem::Create();

			if (type == ItemType::Books)
				Add(result, Tr(READ_BOOK), MenuAction::ReadBook);

			{
				const auto & send = Add(result, Tr(SEND_TO));
				Add(send, Tr(SEND_AS_ARCHIVE), MenuAction::SendAsArchive);
				Add(send, Tr(SEND_AS_IS), MenuAction::SendAsIs);
			}

			if (type == ItemType::Books)
				CreateGroupMenu(Add(result, Tr(GROUPS), MenuAction::None), id, *db);

			if (type == ItemType::Books)
				Add(result, Tr(removed ? REMOVE_BOOK_UNDO : REMOVE_BOOK), removed ? MenuAction::UndoRemoveBook : MenuAction::RemoveBook);

			return [callback = std::move(callback), result = std::move(result)] (size_t)
			{
				if (result->GetChildCount() > 0)
					callback(result);
			};
		} });
	}

private:
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
};

BooksContextMenuProvider::BooksContextMenuProvider(std::shared_ptr<DatabaseUser> databaseUser)
	: m_impl(std::move(databaseUser))
{
	PLOGD << "BooksContextMenuProvider created";
}

BooksContextMenuProvider::~BooksContextMenuProvider()
{
	PLOGD << "BooksContextMenuProvider destroyed";
}

void BooksContextMenuProvider::Request(const QModelIndex & index, Callback callback)
{
	m_impl->Request(index, std::move(callback));
}
