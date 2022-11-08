#include <set>

#include "BookModelBase.h"

namespace HomeCompa::Flibrary {

namespace {

std::set<QString> GetLanguages(const Books & books)
{
	std::set<QString> uniqueLanguages;
	for (const auto & item : books)
		if (!item.IsDictionary)
			uniqueLanguages.insert(item.Lang);

	return uniqueLanguages;
}

bool RemoveRestoreAvailableImpl(const Books & books, const long long id, const bool flag)
{
	bool removedFound = false, notRemovedFound = false;
	for (const auto & book : books)
	{
		if (book.Checked && !book.IsDictionary)
		{
			(book.IsDeleted ? removedFound : notRemovedFound) = true;
			if (removedFound && notRemovedFound)
				return false;
		}
	}

	if (!(removedFound || notRemovedFound))
	{
		const auto it = std::ranges::find_if(books, [id] (const Book & book)
		{
			return book.Id == id;
		});
		return true
			&& it != std::cend(books)
			&& !it->IsDictionary
			&& it->IsDeleted == flag
			;
	}

	return flag ? removedFound : notRemovedFound;
}

}

BookModelBase::BookModelBase(QSortFilterProxyModel & proxyModel, Items & items)
	: ProxyModelBaseT<Item, Role, Observer>(proxyModel, items)
{
		AddReadableRole(Role::Id, &Book::Id);
		AddReadableRole(Role::Title, &Book::Title);
#define	BOOK_ROLE_ITEM(NAME) AddReadableRole(Role::NAME, &Book::NAME);
		BOOK_ROLE_ITEMS_XMACRO
#undef	BOOK_ROLE_ITEM
}

bool BookModelBase::FilterAcceptsRow(const int row, const QModelIndex & parent) const
{
	const auto & item = GetItem(row);
	return true
		&& ProxyModelBaseT<Item, Role, Observer>::FilterAcceptsRow(row, parent)
		&& (m_showDeleted || !item.IsDeleted)
		&& (m_languageFilter.isEmpty() || item.IsDictionary ? item.Lang.contains(m_languageFilter) : item.Lang == m_languageFilter)
		;
}

QVariant BookModelBase::GetDataGlobal(int role) const
{
	switch (role)
	{
		case Role::Language:
			return m_languageFilter;

		case Role::Languages:
		{
			std::set<QString> uniqueLanguages = GetLanguages(m_items);
			QStringList result { {} };
			result.reserve(static_cast<int>(std::size(uniqueLanguages)));
			std::ranges::copy(uniqueLanguages, std::back_inserter(result));
			return result;
		}

		default:
			break;
	}

	return  ProxyModelBaseT<Item, Role, Observer>::GetDataGlobal(role);
}

bool BookModelBase::SetDataGlobal(const QVariant & value, const int role)
{
	switch (role)
	{
		case Role::Language:
			m_languageFilter = value.toString();
			Invalidate();
			return true;

		case Role::ShowDeleted:
			return Util::Set(m_showDeleted, value.toBool(), *this, &BookModelBase::Invalidate);

		case Role::RemoveAvailable:
		case Role::RestoreAvailable:
			return RemoveRestoreAvailableImpl(m_items, value.toLongLong(), role == Role::RestoreAvailable);

		default:
			break;
	}

	return  ProxyModelBaseT<Item, Role, Observer>::SetDataGlobal(value, role);
}

void BookModelBase::Reset()
{
	if (!GetLanguages(m_items).contains(m_languageFilter))
		m_languageFilter.clear();
}

}
