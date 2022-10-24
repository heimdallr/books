#include <QTimer>

#include "Book.h"
#include "BookRole.h"
#include "ModelObserver.h"
#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {

class Model final
	: public ProxyModelBaseT<Book, BookRole, ModelObserver>
{
public:
	Model(Books & items, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
	{
		AddReadableRole(Role::Title, &Book::Title);
#define	BOOK_ROLE_ITEM(NAME) AddReadableRole(Role::NAME, &Book::NAME);
		BOOK_ROLE_ITEMS_XMACRO
#undef	BOOK_ROLE_ITEM
	}

public: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent) const override
	{
		return true
			&& ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(row, parent)
			&& [&items = m_items] (size_t index)
				{
					while (true)
					{
						if (items[index].TreeLevel == 0)
							return true;

						index = items[index].ParentId;
						if (!items[index].Expanded)
							return false;
					}
				}(static_cast<size_t>(row))
			;
	}

private: // ProxyModelBaseT
	QVariant GetDataLocal(const QModelIndex & index, const int role, const Item & item) const override
	{
		switch (role)
		{
			case Role::Checked:
				if (!item.IsDictionary)
					return item.Checked;

				return Qt::PartiallyChecked;

			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::GetDataLocal(index, role, item);
	}

	bool SetDataLocal(const QModelIndex & index, const QVariant & value, int role, Item & item) override
	{
		switch (role)
		{
			case Role::Checked:
				if (!item.IsDictionary)
				{
					item.Checked = value.toBool();
					emit dataChanged(index, index, { role });
					return true;
				}
				return false;

			case Role::Expanded:
				item.Expanded = value.toBool();
				emit dataChanged(index, index, { Role::Expanded });
				Invalidate();
				return true;

			case Role::KeyPressed:
				return OnKeyPressed(index, value, item);

			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::SetDataLocal(index, value, role, item);
	}

private:
	bool OnKeyPressed(const QModelIndex & index, const QVariant & value, Item & item)
	{
		const auto [key, modifiers] = value.value<QPair<int, int>>();
		if (modifiers == Qt::NoModifier && key == Qt::Key_Space)
		{
			item.Checked = !item.Checked;
			emit dataChanged(index, index, { Role::Checked });
			return true;
		}

		return false;
	}

	void Reset() override
	{
	}

private:
	std::vector<std::vector<size_t>> m_children;
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(Books & items, QObject * parent)
		: QSortFilterProxyModel(parent)
		, m_model(items, *this)
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

QAbstractItemModel * CreateBooksTreeModel(Books & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}
