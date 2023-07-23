#include "DataItem.h"

#include <QCoreApplication>

using namespace HomeCompa::Flibrary;

DataItem::DataItem(const DataItem * parent)
	: m_parent(parent)
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

NavigationItem::NavigationItem(const DataItem * parent)
	: DataItem(parent)
{
}

std::shared_ptr<DataItem> NavigationItem::Create(const DataItem * parent)
{
	return std::make_shared<NavigationItem>(parent);
}

int NavigationItem::GetColumnCount() const noexcept
{
	return 1;
}

const QString & NavigationItem::GetData([[maybe_unused]] const int column) const
{
	assert(column == 0);
	return title;
}

const QString & NavigationItem::GetId() const noexcept
{
	return id;
}

NavigationItem * NavigationItem::ToNavigationItem() noexcept
{
	return this;
}

AuthorItem::AuthorItem(const DataItem * parent)
	: DataItem(parent)
{
}

std::shared_ptr<DataItem> AuthorItem::Create(const DataItem * parent)
{
	return std::make_shared<AuthorItem>(parent);
}

int AuthorItem::GetColumnCount() const noexcept
{
	return Column::Last;
}

const QString & AuthorItem::GetId() const noexcept
{
	return id;
}

const QString & AuthorItem::GetData(const int column) const
{
	assert(column >= 0 && column < GetColumnCount());
	return data[column];
}

AuthorItem * AuthorItem::ToAuthorItem() noexcept
{
	return this;
}


BookItem::BookItem(const DataItem * parent)
	: DataItem(parent)
{
}

std::shared_ptr<DataItem> BookItem::Create(const DataItem * parent)
{
	return std::make_shared<BookItem>(parent);
}

int BookItem::GetColumnCount() const noexcept
{
	return Column::Last;
}

const QString & BookItem::GetId() const noexcept
{
	return id;
}

const QString & BookItem::GetData(const int column) const
{
	assert(column >= 0 && column < GetColumnCount());
	return data[column];
}

BookItem * BookItem::ToBookItem() noexcept
{
	return this;
}
