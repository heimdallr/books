#include <QAbstractListModel>

#include "SimpleModel.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = SimpleModelRole;

class Model final
	: public QAbstractListModel
{
public:
	Model(SimpleModelItems items, QObject * parent)
		: QAbstractListModel(parent)
		, m_items(std::move(items))
	{
	}

private: // QAbstractListModel
	QHash<int, QByteArray> roleNames() const override
	{
		return QHash<int, QByteArray>
		{
#define		SIMPLE_MODEL_ITEM(NAME) { Role::NAME, #NAME },
			SIMPLE_MODEL_ITEMS_XMACRO
#undef		SIMPLE_MODEL_ITEM
		};
	}

	int rowCount(const QModelIndex & parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(m_items));
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		assert(index.isValid());
		const auto & item = m_items[index.row()];
		switch (role)
		{
#define		SIMPLE_MODEL_ITEM(NAME) case Role::NAME: return item.NAME;
			SIMPLE_MODEL_ITEMS_XMACRO
#undef		SIMPLE_MODEL_ITEM

			default:
				assert(false && "Unexpected role");
				break;
		}

		return {};
	}

	bool setData([[maybe_unused]]const QModelIndex & index, const QVariant & value, int role) override
	{
		assert(!index.isValid());
		switch (role)
		{
			case Role::SetItems:
				emit beginResetModel();
				m_items = *value.value<SimpleModelItems *>();
				emit endResetModel();
				return true;

			default:
				break;
		}

		assert(false && "Unexpected role");
		return false;
	}

private:
	SimpleModelItems m_items;
};

}

QAbstractItemModel * CreateSimpleModel(SimpleModelItems items, QObject * parent)
{
	return new Model(std::move(items), parent);
}

}
