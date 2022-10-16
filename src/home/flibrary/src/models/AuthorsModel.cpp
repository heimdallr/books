#include <QSortFilterProxyModel>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "AuthorsModelObserver.h"
#include "RoleBase.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {

struct AuthorsRole
	: RoleBase
{
	enum Value
	{
	};
};

struct Author
{
	long long int Id;
	QString Title;
};

void AppendTitle(QString & title, std::string_view str)
{
	if (!str.empty())
		title.append(" ").append(str.data());
}

QString CreateTitle(const DB::Query & query)
{
	QString title = query.Get<std::string>(2).data();
	AppendTitle(title, query.Get<std::string>(1));
	AppendTitle(title, query.Get<std::string>(3));
	return title;
}

class Model final
	: public ProxyModelBaseT<Author, AuthorsRole, ModelObserver>
{
public:
	Model(DB::Database & db, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, CreateItems(db))
	{
		AddReadableRole(Role::Title, &Author::Title);
	}

private: // ProxyModelBaseT
	const QString & GetFindString(const Item & item) const override
	{
		return item.Title;
	}

private:
	static Items CreateItems(DB::Database & db)
	{
		Items items;
		for (const auto query = db.CreateQuery("select a.AuthorID ID, a.FirstName FIRST_NAME, a.LastName LAST_NAME, a.MiddleName MIDDLE_NAME from Authors a order by a.LastName || a.FirstName || a.MiddleName"); !query->Eof(); query->Next())
		{
			items.emplace_back();
			items.back().Id = query->Get<long long int>(0);
			items.back().Title = CreateTitle(*query);
		}

		return items;
	}
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(DB::Database & db, QObject * parent)
		: QSortFilterProxyModel(parent)
		, m_model(db, *this)
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

QAbstractItemModel * CreateAuthorsModel(DB::Database & db, QObject * parent)
{
	return new ProxyModel(db, parent);
}

}
