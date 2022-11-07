#include <QAbstractListModel>

#include "SimpleModel.h"

namespace HomeCompa::Flibrary {

namespace {

struct Role
{
	enum ValueBase
	{
		FakeFirst = Qt::UserRole + 1,
#define SIMPLE_MODEL_ITEM(NAME) NAME,
		SIMPLE_MODEL_ITEMS_XMACRO
#undef	SIMPLE_MODEL_ITEM
	};

};

class Model final
	: public QAbstractListModel
{
public:
	Model(SimpleModeItems items, QObject * parent)
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

private:
	SimpleModeItems m_items;
};

}

QAbstractItemModel * CreateSimpleModel(SimpleModeItems items, QObject * parent)
{
	return new Model(std::move(items), parent);
}

}
