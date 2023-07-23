#include "DataItem.h"

#include <QCoreApplication>

using namespace HomeCompa::Flibrary;

namespace {
const QString EMPTY_STRING;
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

int DataItem::GetColumnCount() const noexcept
{
	return static_cast<int>(std::size(m_data));
}

const QString & DataItem::GetData(const int column) const noexcept
{
	return column >= 0 && column < GetColumnCount() ? m_data[column] : EMPTY_STRING;
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
	assert(column >= 0 && column < GetColumnCount());
	m_data[column] = std::move(value);
	return *this;
}


NavigationItem::NavigationItem(const DataItem * parent)
	: DataItem(1, parent)
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

BookItem::BookItem(const DataItem * parent)
	: DataItem(Column::Last, parent)
{
}

std::shared_ptr<DataItem> BookItem::Create(const DataItem * parent)
{
	return std::make_shared<BookItem>(parent);
}

BookItem * BookItem::ToBookItem() noexcept
{
	return this;
}
