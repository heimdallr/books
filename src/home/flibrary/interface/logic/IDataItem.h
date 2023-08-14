#pragma once

#include <memory>
#include <vector>

#include <qnamespace.h>

#include "fnd/NonCopyMovable.h"

class QString;

namespace HomeCompa::Flibrary {
enum class ItemType;

#define DATA_ITEMS_X_MACRO \
DATA_ITEM(DataItem)        \
DATA_ITEM(NavigationItem)  \
DATA_ITEM(AuthorItem)      \
DATA_ITEM(BookItem)        \
DATA_ITEM(MenuItem)

#define DATA_ITEM(NAME) class NAME;  // NOLINT(bugprone-macro-parentheses)
DATA_ITEMS_X_MACRO
#undef DATA_ITEM

class IDataItem  // NOLINT(cppcoreguidelines-special-member-functions)
{
	NON_COPY_MOVABLE(IDataItem)

public:
	virtual ~IDataItem() = default;
	using Ptr = std::shared_ptr<IDataItem>;
	using Items = std::vector<Ptr>;
	static constexpr auto INVALID_INDEX = std::numeric_limits<size_t>::max();

protected:
	IDataItem() = default;

public:
	[[nodiscard]] virtual IDataItem * GetParent() noexcept = 0;
	[[nodiscard]] virtual const IDataItem * GetParent() const noexcept { return const_cast<IDataItem *>(this)->GetParent(); }
	virtual Ptr & AppendChild(Ptr child) = 0;
	virtual void RemoveChild(size_t row = INVALID_INDEX) = 0;
	virtual void SetChildren(Items children) noexcept = 0;
	[[nodiscard]] virtual Ptr GetChild(size_t row) const noexcept = 0;
	[[nodiscard]] virtual size_t GetChildCount() const noexcept = 0;
	[[nodiscard]] virtual size_t GetRow() const noexcept = 0;
	[[nodiscard]] virtual const QString & GetId() const noexcept = 0;
	[[nodiscard]] virtual const QString & GetData(int column = 0) const noexcept = 0;
	[[nodiscard]] virtual const QString & GetRawData(int column = 0) const noexcept = 0;

	[[nodiscard]] virtual bool IsRemoved() const noexcept = 0;
	[[nodiscard]] virtual int RemapColumn(int column) const noexcept = 0;
	[[nodiscard]] virtual int GetColumnCount() const noexcept = 0;

	virtual IDataItem & SetId(QString id) noexcept = 0;
	virtual IDataItem & SetData(QString value, int column = 0) noexcept = 0;

	[[nodiscard]] virtual ItemType GetType() const noexcept = 0;
	[[nodiscard]] virtual Qt::CheckState GetCheckState() const noexcept = 0;
	virtual void SetCheckState(Qt::CheckState state) noexcept = 0;
	virtual void Reduce() = 0;

public:
	template<typename T>
	T * To() noexcept = delete;
	template<typename T>
	const T * To() const noexcept { return const_cast<IDataItem *>(this)->To<T>(); }

#define DATA_ITEM(NAME) template<> NAME* To<>() noexcept { return To##NAME(); }  // NOLINT(bugprone-macro-parentheses)
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

private:
#define DATA_ITEM(NAME) [[nodiscard]] virtual NAME* To##NAME() noexcept { return nullptr; }  // NOLINT(bugprone-macro-parentheses)
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM
};

}
