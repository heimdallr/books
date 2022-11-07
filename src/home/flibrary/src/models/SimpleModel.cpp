#include <QAbstractListModel>

#include "SimpleModel.h"

namespace HomeCompa::Flibrary {

namespace {

struct Role
{
	enum ValueBase
	{
		Value = Qt::UserRole + 1,
		Title,
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
#define ITEM(NAME) { Role::NAME, #NAME }
		ITEM(Value),
		ITEM(Title)
#undef	ITEM
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
			case Role::Value:
				return item.Value;

			case Role::Title:
				return item.Title;

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
