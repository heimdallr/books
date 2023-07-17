#pragma once

#include <vector>

#include <QString>

#include "fnd/memory.h"

namespace HomeCompa::Flibrary {

enum class NavigationMode
{
	Unknown = -1,
	Authors,
	Series,
	Genres,
	Archives,
	Groups,
	Last
};

enum class ViewMode
{
	Unknown = -1,
	List,
	Tree,
	Last
};

enum class ItemType
{
	Unknown = -1,
	Navigation,
	Books,
};

struct NavigationItem;
struct BookItem;

struct DataItem  // NOLINT(cppcoreguidelines-special-member-functions)
{
	virtual ~DataItem() = default;

	using Ptr = PropagateConstPtr<DataItem>;
	DataItem * parent { nullptr };
	std::vector<Ptr> children;

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
};

struct NavigationItem final : DataItem
{
	QString id;
	QString title;

	ItemType GetType() const noexcept override { return ItemType::Navigation; }
	NavigationItem * ToNavigationItem() noexcept override { return this; }
	static std::unique_ptr<DataItem> create() { return std::make_unique<NavigationItem>(); }
};

struct BookItem final : DataItem
{
	ItemType GetType() const noexcept override { return ItemType::Books; }
	BookItem * ToBookItem() noexcept override { return this; }
};

}
