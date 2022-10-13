#include <QSortFilterProxyModel>

#include "database/interface/Database.h"
#include "database/interface/Query.h"

#include "AuthorsModel.h"

namespace HomeCompa::Flibrary {

namespace {

struct Item
{
	long long int id;
	std::string firstName;
	std::string lastName;
	std::string middleName;
};

using Items = std::vector<Item>;

auto CreateItems(DB::Database & db)
{
	Items items;
	for (const auto query = db.CreateQuery("select a.AuthorID ID, a.FirstName FIRST_NAME, a.LastName LAST_NAME, a.MiddleName MIDDLE_NAME from Authors a"); !query->Eof(); query->Next())
	{
		items.emplace_back();
		items.back().id = query->Get<long long int>(0);
		items.back().firstName = query->Get<std::string>(1);
		items.back().lastName = query->Get<std::string>(2);
		items.back().middleName = query->Get<std::string>(3);
	}

	return items;
}
	
class Model final : public QAbstractListModel
{
public:
	explicit Model(DB::Database & db)
		: m_items(CreateItems(db))
	{
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex & parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(m_items));
	}

	QVariant data(const QModelIndex & index, int role) const override
	{
		assert(index.isValid() && index.row() < rowCount());
		const auto & item = m_items[index.row()];
		switch (role)
		{
			case Qt::DisplayRole:
				return QString("%1 %2 %3").arg(item.lastName.data(), item.firstName.data(), item.middleName.data());

			default:
				assert(false && "unexpected role");
		}

		return {};
	}

private:
	const Items m_items;
};
	
class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(DB::Database & db, QObject * parent)
		: QSortFilterProxyModel(parent)
		, m_model(db)
	{
		QSortFilterProxyModel::setSourceModel(&m_model);
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
