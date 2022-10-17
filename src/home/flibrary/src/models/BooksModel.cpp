#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "ModelObserver.h"
#include "RoleBase.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {

constexpr auto QUERY =
	"select b.BookID, b.Title "
	"from books b "
	"join Author_List al on al.BookID = b.BookID and al.AuthorID = :id"
	;

struct BooksRole
	: RoleBase
{
	// ReSharper disable once CppClassNeverUsed
	enum Value
	{
	};
};

struct Book
{
	long long int Id { 0 };
	QString Title;
};

class Model final
	: public ProxyModelBaseT<Book, BooksRole, ModelObserver>
{
public:
	Model(DB::Database & db, int authorId, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, CreateItems(db, authorId))
	{
		AddReadableRole(Role::Title, &Book::Title);
	}

private:
	static Items CreateItems(DB::Database & db, const int authorId)
	{
		Items items;
		const auto query = db.CreateQuery(QUERY);
		[[maybe_unused]] const auto result = query->Bind(":id", authorId);
		assert(result == 0);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			items.emplace_back();
			items.back().Id = query->Get<long long int>(0);
			items.back().Title = query->Get<const char *>(1);
		}

		return items;
	}
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(DB::Database & db, int authorId, QObject * parent)
		: QSortFilterProxyModel(parent)
		, m_model(db, authorId, *this)
	{
		QSortFilterProxyModel::setSourceModel(&m_model);
	}

	bool filterAcceptsRow(int sourceRow, const QModelIndex & sourceParent) const override
	{
		return m_model.FilterAcceptsRow(sourceRow, sourceParent);
	}

private:
	Model m_model;
};

}

QAbstractItemModel * CreateBooksModel(DB::Database & db, int authorId, QObject * parent)
{
	return new ProxyModel(db, authorId, parent);
}

}
