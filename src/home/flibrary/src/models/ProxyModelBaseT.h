#pragma once

#include "fnd/algorithm.h"
#include "fnd/observable.h"

namespace HomeCompa::Flibrary {

template <typename ItemType, typename RoleType, typename ObserverType>
class ProxyModelBaseT
	: public QAbstractListModel
	, public Observable<ObserverType>
{
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
		m_roleNames.insert(Role::Click, "Click");
	}

public:
	virtual bool FilterAcceptsRow(int row, const QModelIndex & /*parent*/ = {}) const
	{
		return m_filerString.isEmpty() || data(index(row, 0), Qt::DisplayRole).toString().contains(m_filerString, Qt::CaseInsensitive);
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

	virtual bool SetDataLocal(const QModelIndex & index, const QVariant &, int role, Item &)
	{
		switch (role)
		{
			case Role::Click:
				return Observable<Observer>::Perform(&Observer::HandleItemClicked, index.row()), true;

			default:
				break;
		}

		return assert(false && "Unknown role"), false;
	}

	virtual bool SetDataGlobal(const QVariant & value, int role)
	{
		switch (role)
		{
			case Role::ResetBegin:
				return emit beginResetModel(), true;

			case Role::ResetEnd:
				emit endResetModel();
				m_proxyModel.invalidate();
				return true;

			case Role::ObserverRegister:
				return Observable<Observer>::Register(value.value<Observer *>()), true;

			case Role::ObserverUnregister:
				return Observable<Observer>::Unregister(value.value<Observer *>()), true;

			case Role::Find:
			{
				if (!value.isValid() || value.isNull())
					return false;

				Util::Set(m_filerString, {}, m_proxyModel, &QSortFilterProxyModel::invalidate);

				const auto it = std::ranges::find_if(m_items, [&, value = value.toString()](const Item & item)
				{
					return GetFindString(item).startsWith(value, Qt::CaseInsensitive);
				});
				if (it == std::ranges::end(m_items))
					return false;

				Observable<Observer>::Perform(&Observer::HandleModelItemFound, static_cast<int>(std::ranges::distance(std::ranges::begin(m_items), it)));
				return true;
			}

			case Role::Filter:
				if (Util::Set(m_filerString, value.toString(), m_proxyModel, &QSortFilterProxyModel::invalidate))
					return Observable<Observer>::Perform(&Observer::HandleInvalidated), true;
				return false;

			case Role::TranslateIndexFromGlobal:
			{
				const auto request = value.value<TranslateIndexFromGlobalRequest>();
				*request.localIndex = m_proxyModel.mapFromSource(index(request.globalIndex)).row();
				return true;
			}

			case Role::CheckIndexVisible:
			{
				const auto request = value.value<CheckIndexVisibleRequest>();
				if (*request.visibleIndex >= 0 && *request.visibleIndex < rowCount())
					if (FilterAcceptsRow(*request.visibleIndex))
						return true;

				for (int i = 0, sz = rowCount(); i < sz; ++i)
					if (FilterAcceptsRow(i))
						return (*request.visibleIndex = i), true;

				*request.visibleIndex = -1;
				return false;
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

	QString m_filerString;
};

}
