#include "DataItem.h"

#include <ranges>

#include <QCoreApplication>

#include "interface/constants/Enums.h"

using namespace HomeCompa::Flibrary;

namespace {

const QString EMPTY_STRING;
constexpr BookItem::Mapping FULL { BookItem::ALL };

}

DataItem::DataItem(const size_t columnCount, const DataItem * parent)
	: m_parent(parent)
	, m_data(columnCount)
{
}

DataItem::~DataItem() = default;

const DataItem * DataItem::GetParent() const noexcept
{
	return m_parent;
}

DataItem::Ptr & DataItem::AppendChild(Ptr child)
{
	child->m_parent = this;
	child->m_row = GetChildCount();
	return m_children.emplace_back(std::move(child));
}

void DataItem::SetChildren(std::vector<Ptr> children) noexcept
{
	m_children = std::move(children);
	for (const auto & child : m_children)
		child->m_parent = this;
}

const DataItem * DataItem::GetChild(const size_t row) const noexcept
{
	return row < GetChildCount() ? m_children[row].get() : nullptr;
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
	return true;
}

int DataItem::RemapColumn(const int column) const noexcept
{
	return column;
}

int DataItem::GetColumnCount() const noexcept
{
	return static_cast<int>(std::size(m_data));
}

const QString & DataItem::GetData(const int column) const noexcept
{
	return GetRawData(RemapColumn(column));
}

const QString & DataItem::GetRawData(const int column) const noexcept
{
	return column >= 0 && column < static_cast<int>(std::size(m_data)) ? m_data[column] : EMPTY_STRING;
}

const QString & DataItem::GetId() const noexcept
{
	return m_id;
}

DataItem & DataItem::SetId(QString id) noexcept
{
	m_id = std::move(id);
	return *this;
}

DataItem & DataItem::SetData(QString value, const int column) noexcept
{
	assert(column >= 0 && column < static_cast<int>(std::size(m_data)));
	m_data[column] = std::move(value);
	return *this;
}

Qt::CheckState DataItem::GetCheckState() const noexcept
{
	if (m_children.empty())
		return Qt::Unchecked;

	if (m_children.front()->GetCheckState() == Qt::Checked)
	{
		return std::ranges::all_of(m_children | std::views::drop(1), [] (const auto & item)
		{
			return item->GetCheckState() == Qt::Checked;
		}) ? Qt::Checked : Qt::PartiallyChecked;
	}

	if (m_children.front()->GetCheckState() == Qt::Unchecked)
	{
		return std::ranges::all_of(m_children | std::views::drop(1), [] (const auto & item)
		{
			return item->GetCheckState() == Qt::Unchecked;
		}) ? Qt::Unchecked : Qt::PartiallyChecked;
	}

	return Qt::PartiallyChecked;
}

void DataItem::SetCheckState(const Qt::CheckState /*state*/) noexcept
{
}

NavigationItem::NavigationItem(const DataItem * parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<DataItem> NavigationItem::Create(const DataItem * parent)
{
	return std::make_shared<NavigationItem>(parent);
}

NavigationItem * NavigationItem::ToNavigationItem() noexcept
{
	return this;
}

ItemType NavigationItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

AuthorItem::AuthorItem(const DataItem * parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<DataItem> AuthorItem::Create(const DataItem * parent)
{
	return std::make_shared<AuthorItem>(parent);
}

AuthorItem * AuthorItem::ToAuthorItem() noexcept
{
	return this;
}

ItemType AuthorItem::GetType() const noexcept
{
	return ItemType::Navigation;
}

const BookItem::Mapping * BookItem::mapping = &FULL;

BookItem::BookItem(const DataItem * parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<DataItem> BookItem::Create(const DataItem * parent)
{
	return std::make_shared<BookItem>(parent);
}

int BookItem::Remap(const int column) noexcept
{
	return mapping->columns[column];
}

bool BookItem::IsRemoved() const noexcept
{
	return removed;
}

int BookItem::RemapColumn(const int column) const noexcept
{
	return Remap(column);
}

int BookItem::GetColumnCount() const noexcept
{
	return static_cast<int>(mapping->size);
}

BookItem * BookItem::ToBookItem() noexcept
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