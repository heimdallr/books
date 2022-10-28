#include "NavigationListItem.h"
#include "ModelObserver.h"
#include "RoleBase.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {


struct NavigationListRole
	: RoleBase
{
	// ReSharper disable once CppClassNeverUsed
	enum Value
	{
	};
};

class Model final
	: public ProxyModelBaseT<NavigationListItem, NavigationListRole, ModelObserver>
{
public:
	Model(NavigationListItems & items, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
	{
		AddReadableRole(Role::Id, &Item::Id);
		AddReadableRole(Role::Title, &Item::Title);
	}
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(NavigationListItems & items, QObject * parent)
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

QAbstractItemModel * CreateNavigationModel(NavigationListItems & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}