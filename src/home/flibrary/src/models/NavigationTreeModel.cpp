#include <stack>
#include <unordered_map>

#include "NavigationModelObserver.h"
#include "NavigationTreeItem.h"
#include "NavigationTreeRole.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {


class Model final
	: public ProxyModelBaseT<NavigationTreeItem, NavigationTreeRole, NavigationModelObserver>
{
public:
	Model(NavigationTreeItems & items, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
	{
		AddReadableRole(Role::Id, &Item::Id);
		AddReadableRole(Role::Title, &Item::Title);
#define NAVIGATION_TREE_ROLE_ITEM(NAME) AddReadableRole(Role::NAME, &Item::NAME);
		NAVIGATION_TREE_ROLE_ITEMS_XMACRO
#undef	NAVIGATION_TREE_ROLE_ITEM
	}

public: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent) const override
	{
		const auto & item = GetItem(row);
		return true
			&& (item.ChildrenCount == 0
				? ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(row, parent)
				: std::ranges::any_of(m_children[row], [&] (size_t n) { return ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(static_cast<int>(n)); })
				)
			&& std::ranges::all_of(m_parents[row], [&] (size_t n) { return m_items[n].Expanded; })
			;
	}

private: // ProxyModelBaseT
	bool SetDataLocal(const QModelIndex & index, const QVariant & value, const int role, Item & item) override
	{
		switch (role)
		{
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

	QVariant GetDataGlobal(const int role) const override
	{
		switch (role)
		{
			case Role::Count:
				return std::ranges::count_if(m_items, [&, n = 0] (const Item & item) mutable
				{
					if (item.ChildrenCount > 0)
						return ++n, false;

					return ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(n++);
				});

			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::GetDataGlobal(role);
	}

private:
	bool OnKeyPressed(const QModelIndex & index, const QVariant & value, Item & item)
	{
		const auto [key, modifiers] = value.value<QPair<int, int>>();
		if (modifiers == Qt::NoModifier)
		{
			switch (key)
			{
				case Qt::Key_Left:
					if (item.Expanded && item.ChildrenCount > 0)
						return SetDataLocal(index, false, Role::Expanded, item);

					Perform(&ModelObserver::HandleModelItemFound, static_cast<int>(m_index[item.ParentId]));
					return true;

				case Qt::Key_Right:
					if (item.ChildrenCount == 0)
						return false;

					if (!item.Expanded)
						return SetDataLocal(index, true, Role::Expanded, item);

					Perform(&ModelObserver::HandleModelItemFound, index.row() + 1);
					return true;

				default:
					break;
			}
		}

		return false;
	}

	void Reset() override
	{
		m_index.clear();
		for (size_t i = 0, sz = std::size(m_items); i < sz; ++i)
			m_index[m_items[i].Id] = i;

		m_parents.clear();
		m_parents.resize(std::size(m_items));

		m_children.clear();
		m_children.resize(std::size(m_items));

		for (size_t i = 0, sz = std::size(m_items); i < sz; ++i)
		{
			auto parentId = m_items[i].ParentId;
			if (!parentId.isEmpty())
			{
				const auto parentIndex = m_index[parentId];
				m_children[parentIndex].push_back(i);
			}

			while(!parentId.isEmpty())
			{
				const auto parentIndex = m_index[parentId];
				m_parents[i].push_back(parentIndex);
				parentId = m_items[parentIndex].ParentId;
			}
		}
	}

private:
	std::unordered_map<QString, size_t> m_index;
	std::vector<std::vector<size_t>> m_parents;
	std::vector<std::vector<size_t>> m_children;
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(NavigationTreeItems & items, QObject * parent)
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

QAbstractItemModel * CreateNavigationModel(NavigationTreeItems & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}
