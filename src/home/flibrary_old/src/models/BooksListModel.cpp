#include <set>

#include <QTimer>

#include "BookModelBase.h"

namespace HomeCompa::Flibrary {

namespace {

class Model final
	: public BookModelBase
{
public:
	Model(Books & items, QSortFilterProxyModel & proxyModel)
		: BookModelBase(proxyModel, items)
	{
	}

public: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent = {}) const override
	{
		return BookModelBase::FilterAcceptsRow(row, parent);
	}

private: // ProxyModelBaseT
	QVariant GetDataLocal(const QModelIndex & index, const int role, const Item & item) const override
	{
		switch (role)
		{
			case Role::Checked:
				return item.Checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

			default:
				break;
		}

		return BookModelBase::GetDataLocal(index, role, item);
	}

	bool SetDataLocal(const QModelIndex & index, const QVariant & value, int role, Item & item) override
	{
		switch (role)
		{
			case Role::Checked:
				item.Checked = !item.Checked;
				emit dataChanged(index, index, { role });
				return true;

			case Role::KeyPressed:
				return OnKeyPressed(index, value, item);

			default:
				break;
		}

		return BookModelBase::SetDataLocal(index, value, role, item);
	}

private:
	bool OnKeyPressed(const QModelIndex & index, const QVariant & value, Item & item)
	{
		const auto [key, modifiers] = value.value<QPair<int, int>>();
		if (modifiers == Qt::NoModifier && key == Qt::Key_Space)
			return setData(index, !item.Checked, Role::Checked);

		return false;
	}
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

QAbstractItemModel * CreateBooksListModel(Books & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}
