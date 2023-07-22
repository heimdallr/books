#include "DataItem.h"

#include "interface/constants/Localization.h"

#include <QCoreApplication>

using namespace HomeCompa::Flibrary;

namespace {

void AppendTitle(QString & title, const QString & str)
{
	if (title.isEmpty())
	{
		title = str;
		return;
	}

	if (!str.isEmpty())
		title.append(" ").append(str.data());
}

QString CreateAuthorTitle(const AuthorItem & item)
{
	QString title = item.title;
	AppendTitle(title, item.firstName);
	AppendTitle(title, item.middleName);

	if (title.isEmpty())
		title = QCoreApplication::translate(Constant::Localization::CONTEXT_ERROR, Constant::Localization::AUTHOR_NOT_SPECIFIED);

	return title;
}


}

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

size_t NavigationItem::GetColumnCount() const noexcept
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
	: NavigationItem(parent)
{
}

std::shared_ptr<DataItem> AuthorItem::Create(const DataItem * parent)
{
	return std::make_shared<AuthorItem>(parent);
}

const QString & AuthorItem::GetData([[maybe_unused]] const int column) const
{
	assert(column == 0);
	if (fullName.isEmpty())
		fullName = CreateAuthorTitle(*this);

	return fullName;
}

AuthorItem * AuthorItem::ToAuthorItem() noexcept
{
	return this;
}

BookItem::BookItem(const DataItem * parent)
	: DataItem(parent)
{
}

BookItem * BookItem::ToBookItem() noexcept
{
	return this;
}
