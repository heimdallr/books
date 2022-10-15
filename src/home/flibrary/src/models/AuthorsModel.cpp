#include <QSortFilterProxyModel>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "AuthorsModelObserver.h"
#include "BaseRole.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {

struct AuthorsRole
	: BaseRole
{
};

struct Author
{
	long long int id;
	QString firstName;
	QString lastName;
	QString middleName;
};

class Model final
	: public ProxyModelBaseT<Author, AuthorsRole, AuthorsModelObserver>
{
public:
	Model(DB::Database & db, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, CreateItems(db))
	{
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex & parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(m_items));
	}

private: // ProxyModelBaseT
	QVariant GetDataLocal(const QModelIndex &, int role, const Item & item) const override
	{
		switch (role)
		{
			case Qt::DisplayRole:
			{
				QString result = item.lastName;
				if (!item.firstName.isEmpty())
					result.append(" ").append(item.firstName);
				if (!item.middleName.isEmpty())
					result.append(" ").append(item.middleName);
				return result;
			}

			default:
				assert(false && "unexpected role");
		}

		return {};
	}

	const QString & GetFindString(const Item & item) const override
	{
		return item.lastName;
	}

private:
	static Items CreateItems(DB::Database & db)
	{
		Items items;
		for (const auto query = db.CreateQuery("select a.AuthorID ID, a.FirstName FIRST_NAME, a.LastName LAST_NAME, a.MiddleName MIDDLE_NAME from Authors a order by a.LastName || a.FirstName || a.MiddleName"); !query->Eof(); query->Next())
		{
			items.emplace_back();
			items.back().id = query->Get<long long int>(0);
			items.back().firstName = query->Get<std::string>(1).data();
			items.back().lastName = query->Get<std::string>(2).data();
			items.back().middleName = query->Get<std::string>(3).data();
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
