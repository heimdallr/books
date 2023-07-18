#pragma once

#include <vector>

#include "fnd/memory.h"

#include <QString>
#include <QVariant>

#include "interface/constants/Enums.h"

namespace HomeCompa::Flibrary {

struct NavigationItem;
struct BookItem;

class DataItem  // NOLINT(cppcoreguidelines-special-member-functions)
{
protected:
	explicit DataItem(const DataItem * parent = nullptr)
		: m_parent(parent)
	{
	}

public:
	virtual ~DataItem() = default;
	using Ptr = std::shared_ptr<DataItem>;

public:
	const DataItem* GetParent() const noexcept
	{
		return m_parent;
	}

	Ptr& AppendChild(Ptr child)
	{
		child->m_parent = this;
		child->m_row = GetChildCount();
		return m_children.emplace_back(std::move(child));
	}

	const DataItem * GetChild(const size_t row) const noexcept
	{
		return row < GetChildCount() ? m_children[row].get() : nullptr;
	}

	size_t GetChildCount() const noexcept
	{
		return m_children.size();
	}

	size_t GetRow() const noexcept
	{
		return m_row;
	}

	virtual size_t GetColumnCount() const noexcept = 0;
	virtual QVariant GetData(int column) const = 0;
	virtual ItemType GetType() const noexcept = 0;

	template<typename T>
	T * To() noexcept = delete;
	template<typename T>
	const T * To() const noexcept
	{
		return const_cast<DataItem *>(this)->To<T>();
	}
	template<>
	NavigationItem* To<>() noexcept { return ToNavigationItem(); }
	template<>
	BookItem* To<>() noexcept { return ToBookItem(); }

private:
	[[nodiscard]] virtual NavigationItem * ToNavigationItem() noexcept{ return nullptr; }
	[[nodiscard]] virtual BookItem * ToBookItem() noexcept{ return nullptr; }

protected:
	size_t m_row { 0 };
	const DataItem * m_parent { nullptr };
	std::vector<Ptr> m_children;
};

struct NavigationItem final : DataItem
{
	QString id;
	QString title;

	explicit NavigationItem(const DataItem * parent = nullptr)
		: DataItem(parent)
	{
	}

	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr)
	{
		return std::make_shared<NavigationItem>(parent);
	}

private: // DataItem
	size_t GetColumnCount() const noexcept override
	{
		return 1;
	}

	QVariant GetData([[maybe_unused]]const int column) const override
	{
		assert(column == 0);
		return title;
	}

	ItemType GetType() const noexcept override { return ItemType::Navigation; }
	NavigationItem * ToNavigationItem() noexcept override { return this; }
};

struct BookItem final : DataItem
{
	explicit BookItem(const DataItem * parent = nullptr)
		: DataItem(parent)
	{
	}

	ItemType GetType() const noexcept override { return ItemType::Books; }
	BookItem * ToBookItem() noexcept override { return this; }
};

}
