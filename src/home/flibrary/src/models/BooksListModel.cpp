#include <ranges>
#include <set>

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
		AddReadableRole(Role::Id, &Book::Id);
		AddReadableRole(Role::Title, &Book::Title);
#define	BOOK_ROLE_ITEM(NAME) AddReadableRole(Role::NAME, &Book::NAME);
		BOOK_ROLE_ITEMS_XMACRO
#undef	BOOK_ROLE_ITEM
	}

public: // ProxyModelBaseT
	bool FilterAcceptsRow(const int row, const QModelIndex & parent = {}) const override
	{
		const auto & item = GetItem(row);
		return true
			&& ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(row, parent)
			&& (m_showDeleted || !item.IsDeleted)
			&& (m_languageFilter.isEmpty() || item.Lang == m_languageFilter)
			;
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

		return ProxyModelBaseT<Item, Role, Observer>::GetDataLocal(index, role, item);
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

			default:
				break;
		}

		return ProxyModelBaseT<Item, Role, Observer>::GetDataGlobal(role);
	}

	bool SetDataGlobal(const QVariant & value, const int role) override
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

	void Reset() override
	{
		if (!GetLanguages().contains(m_languageFilter))
			m_languageFilter.clear();
	}

private:
	bool OnKeyPressed(const QModelIndex & index, const QVariant & value, Item & item)
	{
		const auto [key, modifiers] = value.value<QPair<int, int>>();
		if (modifiers == Qt::NoModifier && key == Qt::Key_Space)
			return setData(index, !item.Checked, Role::Checked);

		return false;
	}

	std::set<QString> GetLanguages() const
	{
		std::set<QString> uniqueLanguages;
		std::ranges::transform(m_items, std::inserter(uniqueLanguages, uniqueLanguages.end()), [] (const Item & item) { return item.Lang; });
		return uniqueLanguages;
	}

private:
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

QAbstractItemModel * CreateBooksListModel(Books & items, QObject * parent)
{
	return new ProxyModel(items, parent);
}

}
