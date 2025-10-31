#include "DataItem.h"

#include <ranges>

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"

using namespace HomeCompa::Flibrary;

namespace
{

const QString               EMPTY_STRING;
constexpr BookItem::Mapping FULL { BookItem::ALL };

}

DataItem::DataItem(const size_t columnCount, IDataItem* parent)
	: m_parent(parent)
	, m_data(columnCount)
{
}

IDataItem* DataItem::GetParent() noexcept
{
	return m_parent;
}

IDataItem::Ptr& DataItem::AppendChild(Ptr child)
{
	auto* typed     = child->To<DataItem>();
	typed->m_parent = this;
	typed->m_row    = GetChildCount();
	return m_children.emplace_back(std::move(child));
}

void DataItem::RemoveChild(size_t row)
{
	assert(!m_children.empty());
	if (row == INVALID_INDEX)
		row = GetChildCount() - 1;

	m_children.erase(std::next(std::begin(m_children), static_cast<ptrdiff_t>(row)));
	std::for_each(std::next(std::begin(m_children), static_cast<ptrdiff_t>(row)), std::end(m_children), [](auto& item) {
		--item->template To<DataItem>()->m_row;
	});
}

void DataItem::RemoveAllChildren()
{
	m_children.clear();
}

void DataItem::SetChildren(std::vector<Ptr> children) noexcept
{
	m_children = std::move(children);
	for (const auto& child : m_children)
		child->To<DataItem>()->m_parent = this;
}

IDataItem::Ptr DataItem::GetChild(const size_t row) const noexcept
{
	return row < GetChildCount() ? m_children[row] : Ptr {};
}

size_t DataItem::GetChildCount() const noexcept
{
	return m_children.size();
}

size_t DataItem::GetRow() const noexcept
{
	return m_row;
}

bool DataItem::IsRemoved() const noexcept
{
	return m_removed;
}

void DataItem::SetRemoved(const bool value) noexcept
{
	m_removed = value;
}

int DataItem::RemapColumn(const int column) const noexcept
{
	return column;
}

int DataItem::GetColumnCount() const noexcept
{
	return static_cast<int>(std::size(m_data));
}

const QString& DataItem::GetData(const int column) const noexcept
{
	return GetRawData(RemapColumn(column));
}

const QString& DataItem::GetRawData(const int column) const noexcept
{
	return column >= 0 && column < static_cast<int>(std::size(m_data)) ? m_data[static_cast<size_t>(column)] : EMPTY_STRING;
}

const QString& DataItem::GetId() const noexcept
{
	return m_id;
}

IDataItem& DataItem::SetId(QString id) noexcept
{
	m_id = std::move(id);
	return *this;
}

IDataItem::Flags DataItem::GetFlags() const noexcept
{
	return m_flags;
}

IDataItem& DataItem::SetFlags(Flags flags) noexcept
{
	m_flags = flags;
	return *this;
}

IDataItem& DataItem::SetData(QString value, const int column) noexcept
{
	assert(column >= 0 && column < static_cast<int>(std::size(m_data)));
	m_data[static_cast<size_t>(column)] = std::move(value);
	return *this;
}

Qt::CheckState DataItem::GetCheckState() const noexcept
{
	if (m_children.empty())
		return Qt::Unchecked;

	if (m_children.front()->GetCheckState() == Qt::Checked)
	{
		return std::ranges::all_of(
				   m_children | std::views::drop(1),
				   [](const auto& item) {
					   return item->GetCheckState() == Qt::Checked;
				   }
			   )
		         ? Qt::Checked
		         : Qt::PartiallyChecked;
	}

	if (m_children.front()->GetCheckState() == Qt::Unchecked)
	{
		return std::ranges::all_of(
				   m_children | std::views::drop(1),
				   [](const auto& item) {
					   return item->GetCheckState() == Qt::Unchecked;
				   }
			   )
		         ? Qt::Unchecked
		         : Qt::PartiallyChecked;
	}

	return Qt::PartiallyChecked;
}

void DataItem::SetCheckState(const Qt::CheckState /*state*/) noexcept
{
}

void DataItem::Reduce()
{
}

IDataItem::Ptr DataItem::FindChild(const std::function<bool(const IDataItem&)>& functor) const
{
	const auto it = std::ranges::find_if(m_children, [&](const auto& item) {
		return functor(*item);
	});
	return it == m_children.end() ? Ptr {} : *it;
}

void DataItem::SortChildren(const std::function<bool(const IDataItem& lhs, const IDataItem& rhs)>& comparer)
{
	std::ranges::sort(m_children, [&](const auto& lhs, const auto& rhs) {
		return comparer(*lhs, *rhs);
	});
}

DataItem* DataItem::ToDataItem() noexcept
{
	return this;
}

NavigationItem::NavigationItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<IDataItem> NavigationItem::Create(IDataItem* parent)
{
	return std::make_shared<NavigationItem>(parent);
}

NavigationItem* NavigationItem::ToNavigationItem() noexcept
{
	return this;
}

ItemType NavigationItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

GenreItem::GenreItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<IDataItem> GenreItem::Create(IDataItem* parent)
{
	return std::make_shared<GenreItem>(parent);
}

GenreItem* GenreItem::ToGenreItem() noexcept
{
	return this;
}

ItemType GenreItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

AuthorItem::AuthorItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<IDataItem> AuthorItem::Create(IDataItem* parent)
{
	return std::make_shared<AuthorItem>(parent);
}

AuthorItem* AuthorItem::ToAuthorItem() noexcept
{
	return this;
}

void AuthorItem::Reduce()
{
	QString last   = GetRawData(Column::LastName);
	QString first  = GetRawData(Column::FirstName);
	QString middle = GetRawData(Column::MiddleName);

	for (int i = 0; i < 2; ++i)
	{
		if (!last.isEmpty())
			break;

		last   = std::move(first);
		first  = std::move(middle);
		middle = QString(); //-V815
	}

	if (last.isEmpty())
		last = Loc::Tr(Loc::Ctx::ERROR_CTX, Loc::AUTHOR_NOT_SPECIFIED);

	auto name = last;

	const auto append = [&](const QString& str) {
		if (str.isEmpty())
			return;

		AppendTitle(name, str.first(1) + ".");
	};

	append(first);
	append(middle);

	SetData(name);
}

ItemType AuthorItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

std::shared_ptr<IDataItem> SeriesItem::Create(IDataItem* parent)
{
	return std::make_shared<SeriesItem>(parent);
}

SeriesItem::SeriesItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

SeriesItem* SeriesItem::ToSeriesItem() noexcept
{
	return this;
}

ItemType SeriesItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

std::shared_ptr<IDataItem> ReviewItem::Create(IDataItem* parent)
{
	return std::make_shared<ReviewItem>(parent);
}

ReviewItem::ReviewItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

ReviewItem* ReviewItem::ToReviewItem() noexcept
{
	return this;
}

ItemType ReviewItem::GetType() const noexcept
{
	return ItemType::Books;
}

const BookItem::Mapping* BookItem::mapping = &FULL;

BookItem::BookItem(IDataItem* parent, const size_t additionalFieldCount)
	: DataItem(Column::Last + additionalFieldCount, parent)
{
}

std::shared_ptr<IDataItem> BookItem::Create(IDataItem* parent, const size_t additionalFieldCount)
{
	return std::make_shared<BookItem>(parent, additionalFieldCount);
}

int BookItem::Remap(const int column) noexcept
{
	return mapping->columns[static_cast<size_t>(column)];
}

int BookItem::RemapColumn(const int column) const noexcept
{
	return Remap(column);
}

int BookItem::GetColumnCount() const noexcept
{
	return static_cast<int>(mapping->size);
}

BookItem* BookItem::ToBookItem() noexcept
{
	return this;
}

Qt::CheckState BookItem::GetCheckState() const noexcept
{
	return checkState;
}

void BookItem::SetCheckState(const Qt::CheckState state) noexcept
{
	checkState = state;
}

ItemType BookItem::GetType() const noexcept
{
	return ItemType::Books;
}

std::shared_ptr<IDataItem> MenuItem::Create(IDataItem* parent)
{
	return std::make_shared<MenuItem>(parent);
}

MenuItem::MenuItem(IDataItem* parent)
	: DataItem(Column::Last, parent)
{
}

MenuItem* MenuItem::ToMenuItem() noexcept
{
	return this;
}

ItemType MenuItem::GetType() const noexcept
{
	assert(false && "unexpected call");
	return ItemType::Unknown;
}

namespace HomeCompa::Flibrary
{

void AppendTitle(QString& title, const QString& str, const QString& delimiter)
{
	if (title.isEmpty())
	{
		title = str;
		return;
	}

	if (!str.isEmpty())
		title.append(delimiter).append(str);
}

QString GetAuthorFull(const IDataItem& author)
{
	auto result = author.GetData(AuthorItem::Column::LastName);
	AppendTitle(result, author.GetData(AuthorItem::Column::FirstName));
	AppendTitle(result, author.GetData(AuthorItem::Column::MiddleName));
	return result;
}

} // namespace HomeCompa::Flibrary
