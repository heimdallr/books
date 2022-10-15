#pragma once

#include "fnd/observable.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

template <typename ItemType, typename RoleType, typename ObserverType>
class ProxyModelBaseT
	: public QAbstractListModel
	, public Observable<ObserverType>
{
	NON_COPY_MOVABLE(ProxyModelBaseT)

protected:
	using Item = ItemType;
	using Items = std::vector<Item>;
	using Role = RoleType;
	using Observer = ObserverType;
	using RoleGetter = std::function<QVariant(const Item &)>;

protected:
	explicit ProxyModelBaseT(QSortFilterProxyModel & proxyModel, Items items)
		: m_proxyModel(proxyModel)
		, m_items(std::move(items))
		, m_roleNames(QAbstractListModel::roleNames())
	{
	}

protected:
	virtual QVariant GetDataLocal(const QModelIndex &, int role, const Item & item) const
	{
		if (const auto it = m_roleValues.find(role); it != m_roleValues.end())
			return (*it)(item);

		return assert(false && "Unknown role"), QVariant();
	}

	virtual QVariant GetDataGlobal(int) const
	{
		return assert(false && "Unknown role"), QVariant();
	}

	virtual bool SetDataLocal(const QModelIndex &, const QVariant &, int, Item &)
	{
		return assert(false && "Unknown role"), false;
	}

	virtual bool SetDataGlobal(const QVariant & value, int role)
	{
		switch (role)
		{
			case Role::ResetBegin:
				emit beginResetModel();
				return true;

			case Role::ResetEnd:
				emit endResetModel();
				m_proxyModel.invalidate();
				return true;

			case Role::ObserverRegister:
				Observable<Observer>::Register(value.value<Observer *>());
				return true;

			case Role::ObserverUnregister:
				Observable<Observer>::Unregister(value.value<Observer *>());
				return true;

			case Role::Find:
			{
				if (!value.isValid() || value.isNull())
					return false;

				const auto it = std::ranges::find_if(m_items, [&, value = value.toString()](const Item & item)
				{
					return GetFindString(item).startsWith(value, Qt::CaseInsensitive);
				});
				if (it == std::ranges::end(m_items))
					return false;

				Observable<Observer>::Perform(&Observer::HandleModelItemFound, static_cast<int>(std::ranges::distance(std::ranges::begin(m_items), it)));
				return true;
			}

			default:
				break;
		}

		return assert(false && "Unknown role"), false;
	}

	virtual const QString & GetFindString(const Item & item) const = 0;

protected: // QAbstractListModel
	int rowCount(const QModelIndex & /*parent*/ = QModelIndex()) const override
	{
		return static_cast<int>(std::size(m_items));
	}

	QHash<int, QByteArray> roleNames() const override
	{
		return m_roleNames;
	}

	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
	{
		return index.isValid()
			? GetDataLocal(index, role, GetItem(index.row()))
			: GetDataGlobal(role)
			;
	}

	bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override
	{
		return index.isValid()
			? SetDataLocal(index, value, role, GetItem(index.row()))
			: SetDataGlobal(value, role)
			;
	}

protected:
	Item & GetItem(int n)
	{
		assert(n < rowCount());
		return m_items[n];
	}

	const Item & GetItem(int n) const
	{
		return const_cast<ProxyModelBaseT *>(this)->GetItem(n);
	}

protected:
	QSortFilterProxyModel & m_proxyModel;
	Items m_items;

	QHash<int, QByteArray> m_roleNames;
	QHash<int, RoleGetter> m_roleValues;
};

}
