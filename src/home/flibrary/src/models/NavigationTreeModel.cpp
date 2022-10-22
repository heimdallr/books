#include "NavigationTreeItem.h"
#include "ModelObserver.h"
#include "RoleBase.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {


struct NavigationTreeRole
	: RoleBase
{
	// ReSharper disable once CppClassNeverUsed
	enum Value
	{
	};
};

class Model final
	: public ProxyModelBaseT<NavigationTreeItem, NavigationTreeRole, ModelObserver>
{
public:
	Model(NavigationTreeItems & items, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
	{
		AddReadableRole(Role::Id, &Item::Id);
		AddReadableRole(Role::Title, &Item::Title);
	}

//private: // ProxyModelBaseT
//	void Reset() override
//	{
//		for (size_t i = 0, sz = std::size(m_items); i < sz; ++i)
//			m_treeIndex.emplace(m_items[i].ParentId, i);
//
//		std::vector<size_t> sortedIndex;
//
//		std::stack<std::pair<QString, size_t>> s;
//		s.emplace("0", -1);
//		while (!s.empty())
//		{
//			auto [parent, index] = std::move(s.top());
//			sortedIndex.push_back(index);
//			s.pop();
//			const auto [begin, end] = m_treeIndex.equal_range(parent);
//			for (auto rbegin = std::make_reverse_iterator(end), rend = std::make_reverse_iterator(begin); rbegin != rend; ++rbegin)
//				s.emplace(m_items[rbegin->second].Id, rbegin->second);
//		}
//	}
//
//private:
//	std::multimap<QString, size_t> m_treeIndex;
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
