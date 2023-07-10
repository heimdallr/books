#include <set>
#include <stack>

#include <QTimer>

#include "fnd/algorithm.h"

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
	bool FilterAcceptsRow(const int row, const QModelIndex & /*parent*/) const override
	{
		return true
			&& BookModelBase::FilterAcceptsRow(row)
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
					return item.Checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;

				return ChildrenCheckedState(index.row());

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
			{
				std::set<int> changed { index.row() };
				if (!item.IsDictionary)
				{
					item.Checked = !item.Checked;
				}
				else
				{
					const auto checked = ChildrenCheckedState(index.row());
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

				for (auto && range : Util::CreateRanges(changed))
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

		return BookModelBase::SetDataLocal(index, value, role, item);
	}

	QVariant GetDataGlobal(const int role) const override
	{
		switch (role)
		{
			case Role::Count:
				return std::ranges::count_if(m_items, [&, n = 0](const Item & item) mutable
				{
					if (item.IsDictionary)
						return ++n, false;

					return BookModelBase::FilterAcceptsRow(n++);
				});


			default:
				break;
		}

		return BookModelBase::GetDataGlobal(role);
	}

private:
	int ChildrenCheckedState(const int row) const
	{
		bool checked = false, unchecked = false;
		ForEachChild(row, [&] (size_t n)
		{
			const auto & child = m_items[n];
			if (child.IsDictionary)
				return false;

			child.Checked ? checked = true : unchecked = true;

			return checked && unchecked;
		});
		assert(checked || unchecked);
		return
			checked
				? unchecked ? Qt::CheckState::PartiallyChecked
							: Qt::CheckState::Checked
				:			  Qt::CheckState::Unchecked;
	}

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

					Perform(&Observer::HandleModelItemFound, static_cast<int>(item.ParentId < std::size(m_items) ? item.ParentId : 0));
					return true;

				case Qt::Key_Right:
					if (!item.IsDictionary)
						return false;

					return item.Expanded
						? FindIncremented(index, 1)
						: SetDataLocal(index, true, Role::Expanded, item);

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
		BookModelBase::Reset();

		m_children.clear();
		m_children.resize(std::size(m_items));
		for (size_t i = 0, sz = std::size(m_items); i < sz; ++i)
			if (m_items[i].ParentId < std::size(m_items))
				m_children[m_items[i].ParentId].push_back(i);
	}

	void CheckVisible(int n) override
	{
		bool found = false;
		while(n >= 0)
		{
			auto & item = GetItem(n);
			if (item.IsDictionary && !item.Expanded)
				found = item.Expanded = true;

			n = static_cast<int>(item.ParentId);
		}

		if (found)
			Invalidate();
	}

	bool ChangeCheckedDictionary(int index, const std::function<bool(const Book &)> & f, std::set<int> & changed) override
	{
		bool result = false;
		for (const auto i : m_children[index])
		{
			const auto n = static_cast<int>(i);
			const auto book = GetItem(n);
			result = (book.IsDictionary
				? ChangeCheckedDictionary(n, f, changed)
				: ChangeCheckedBook(n, f, changed)
				) || result;
		}
		if (result)
			changed.insert(index);

		return result;
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
