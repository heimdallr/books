#include <set>
#include <stack>

#include <QTimer>

#include "fnd/algorithm.h"

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
		const auto & item = GetItem(row);
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
			&& (m_showDeleted || !item.IsDeleted)
			&& (m_languageFilter.isEmpty() || item.IsDictionary ? item.Lang.contains(m_languageFilter) : item.Lang == m_languageFilter)
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
				{
					bool checked = false, unchecked = false;
					ForEachChild(index.row(), [&] (size_t row)
					{
						const auto & child = m_items[row];
						if (child.IsDictionary)
							return false;

						child.Checked ? checked = true : unchecked = true;

						return checked && unchecked;
					});
					assert(checked || unchecked);
					return checked ? unchecked ? Qt::PartiallyChecked : Qt::Checked : Qt::Unchecked;
				}

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
			{
				std::set<int> changed { index.row() };
				if (!item.IsDictionary)
				{
					item.Checked = value.toBool();
				}
				else
				{
					const auto checked = index.data(Role::Checked).toInt();
					ForEachChild(index.row(), [&] (size_t row)
					{
						changed.insert(static_cast<int>(row));
						auto & child = m_items[row];
						if (!child.IsDictionary)
							child.Checked = checked != Qt::Checked;
						return false;
					});
				}

				ForEachParent(index.row(), [&] (size_t row)
				{
					changed.insert(static_cast<int>(row));
					return false;
				});

				for (const auto & range : Util::CreateRanges(changed))
					emit dataChanged(this->index(range.first), this->index(range.second - 1), { Role::Checked });

				return true;
			}

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
			case Role::Language:
				return m_languageFilter;

			case Role::Languages:
			{
				std::set<QString> uniqueLanguages = GetLanguages();
				QStringList result { {} };
				result.reserve(static_cast<int>(std::size(uniqueLanguages)));
				std::ranges::copy(uniqueLanguages, std::back_inserter(result));
				return result;
			}

			case Role::Count:
				return std::ranges::count_if(m_items, [&, n = 0](const Item & item) mutable
				{
					if (item.IsDictionary)
						return ++n, false;

					return ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(n++);
				});


			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::GetDataGlobal(role);
	}

	bool SetDataGlobal(const QVariant & value, int role) override
	{
		switch (role)
		{
			case Role::ShowDeleted:
				return Util::Set(m_showDeleted, value.toBool(), *this, &Model::Invalidate);

			case Role::Language:
				m_languageFilter = value.toString();
				Invalidate();
				return true;

			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::SetDataGlobal(value, role);
	}

private:
	bool OnKeyPressed(const QModelIndex & index, const QVariant & value, Item & item)
	{
		const auto [key, modifiers] = value.value<QPair<int, int>>();
		if (modifiers == Qt::NoModifier)
		{
			switch (key)
			{
				case Qt::Key_Space:
					return setData(index, !item.Checked, Role::Checked);

				case Qt::Key_Left:
					if (item.Expanded && item.IsDictionary)
						return SetDataLocal(index, false, Role::Expanded, item);

					Perform(&ModelObserver::HandleModelItemFound, static_cast<int>(item.ParentId < std::size(m_items) ? item.ParentId : 0));
					return true;

				case Qt::Key_Right:
					if (!item.IsDictionary)
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

	void ForEachParent(size_t row, const std::function<bool(size_t)> & f) const
	{
		while (true)
		{
			row = m_items[row].ParentId;
			if (row > std::size(m_items))
				return;

			if (f(row))
				return;
		}
	}

	void ForEachChild(size_t row, const std::function<bool(size_t)> & f) const
	{
		std::stack<size_t> s;
		for (const auto n : m_children[row])
			s.push(n);

		while(!s.empty())
		{
			const auto m = s.top();
			s.pop();
			if (f(m))
				return;

			for (const auto n : m_children[m])
				s.push(n);
		}
	}

	void Reset() override
	{
		if (!GetLanguages().contains(m_languageFilter))
			m_languageFilter.clear();

		m_children.clear();
		m_children.resize(std::size(m_items));
		for (size_t i = 0, sz = std::size(m_items); i < sz; ++i)
			if (m_items[i].ParentId < std::size(m_items))
				m_children[m_items[i].ParentId].push_back(i);
	}

	std::set<QString> GetLanguages() const
	{
		std::set<QString> uniqueLanguages;
		for (const auto & item : m_items)
			if (!item.IsDictionary)
				uniqueLanguages.insert(item.Lang);

		return uniqueLanguages;
	}

private:
	std::vector<std::vector<size_t>> m_children;
	bool m_showDeleted { false };
	QString m_languageFilter;
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
