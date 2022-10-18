#include "NavigationItem.h"
#include "ModelObserver.h"
#include "RoleBase.h"

#include "ProxyModelBaseT.h"

namespace HomeCompa::Flibrary {

namespace {


struct AuthorsRole
	: RoleBase
{
	// ReSharper disable once CppClassNeverUsed
	enum Value
	{
	};
};

class Model final
	: public ProxyModelBaseT<NavigationItem, AuthorsRole, ModelObserver>
{
public:
	Model(NavigationItems & items, QSortFilterProxyModel & proxyModel)
		: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
	{
		AddReadableRole(Role::Id, &NavigationItem::Id);
		AddReadableRole(Role::Title, &NavigationItem::Title);
	}
};

class ProxyModel final : public QSortFilterProxyModel
{
public:
	ProxyModel(NavigationItems & items, QObject * parent)
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

QAbstractItemModel * CreateAuthorsModel(NavigationItems & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}
