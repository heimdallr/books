#pragma once

#include <vector>

#include <QVariant>

#include "fnd/NonCopyMovable.h"

#include "logicLib.h"

namespace HomeCompa::Flibrary {
enum class ItemType;

#define DATA_ITEMS_X_MACRO \
DATA_ITEM(NavigationItem)  \
DATA_ITEM(AuthorItem)      \
DATA_ITEM(BookItem)

#define DATA_ITEM(NAME) struct NAME;
DATA_ITEMS_X_MACRO
#undef DATA_ITEM

class DataItem  // NOLINT(cppcoreguidelines-special-member-functions)
{
	NON_COPY_MOVABLE(DataItem)

protected:
	explicit DataItem(size_t columnCount, const DataItem * parent = nullptr);

public:
	virtual ~DataItem();
	using Ptr = std::shared_ptr<DataItem>;
	using Items = std::vector<Ptr>;

public:
	[[nodiscard]] const DataItem * GetParent() const noexcept;
	Ptr & AppendChild(Ptr child);
	void SetChildren(Items children) noexcept;
	[[nodiscard]] const DataItem * GetChild(size_t row) const noexcept;
	[[nodiscard]] size_t GetChildCount() const noexcept;
	[[nodiscard]] size_t GetRow() const noexcept;
	[[nodiscard]] const QString & GetId() const noexcept;
	[[nodiscard]] const QString & GetData(int column = 0) const noexcept;
	[[nodiscard]] const QString & GetRawData(int column = 0) const noexcept;

	[[nodiscard]] virtual bool IsRemoved() const noexcept;
	[[nodiscard]] virtual int RemapColumn(int column) const noexcept;
	[[nodiscard]] virtual int GetColumnCount() const noexcept;

	DataItem & SetId(QString id) noexcept;
	DataItem & SetData(QString value, int column = 0) noexcept;

	[[nodiscard]] virtual ItemType GetType() const noexcept = 0;
	[[nodiscard]] virtual Qt::CheckState GetCheckState() const noexcept;
	virtual void SetCheckState(Qt::CheckState state) noexcept;

public:
	template<typename T>
	T * To() noexcept = delete;
	template<typename T>
	const T * To() const noexcept { return const_cast<DataItem *>(this)->To<T>(); }

#define DATA_ITEM(NAME) template<> NAME* To<>() noexcept { return To##NAME(); }  // NOLINT(bugprone-macro-parentheses)
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

private:
#define DATA_ITEM(NAME) [[nodiscard]] virtual NAME* To##NAME() noexcept { return nullptr; }  // NOLINT(bugprone-macro-parentheses)
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

protected:
	size_t m_row { 0 };
	const DataItem * m_parent { nullptr };
	Items m_children;
	QString m_id;
	std::vector<QString> m_data;
};

struct NavigationItem final : DataItem
{
	struct Column
	{
		enum Value
		{
			Title = 0,
			Last
		};
	};
	explicit NavigationItem(const DataItem * parent = nullptr);

	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);

private: // DataItem
	NavigationItem * ToNavigationItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

struct AuthorItem final : DataItem
{
	struct Column
	{
		enum Value
		{
			LastName = 0,
			FirstName,
			MiddleName,
			Last
		};
	};

	explicit AuthorItem(const DataItem * parent = nullptr);

	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);

private: // DataItem
	AuthorItem * ToAuthorItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

struct LOGIC_API BookItem final : DataItem
{
#define BOOKS_COLUMN_ITEMS_X_MACRO \
BOOKS_COLUMN_ITEM(Author)          \
BOOKS_COLUMN_ITEM(Title)           \
BOOKS_COLUMN_ITEM(Series)          \
BOOKS_COLUMN_ITEM(SeqNumber)       \
BOOKS_COLUMN_ITEM(Size)            \
BOOKS_COLUMN_ITEM(Genre)           \
BOOKS_COLUMN_ITEM(Folder)          \
BOOKS_COLUMN_ITEM(FileName)        \
BOOKS_COLUMN_ITEM(LibRate)         \
BOOKS_COLUMN_ITEM(UpdateDate)      \
BOOKS_COLUMN_ITEM(Lang)

	struct Column
	{
		enum Value
		{
#define		BOOKS_COLUMN_ITEM(NAME) NAME,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef		BOOKS_COLUMN_ITEM
			Last
		};
	};
	static_assert(Column::Author == 0);

	static constexpr int ALL[]
	{
#define		BOOKS_COLUMN_ITEM(NAME) Column::NAME,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef		BOOKS_COLUMN_ITEM
	};

	struct Mapping
	{
		const int * const columns;
		const size_t size;
		template<size_t Size>
		using Data = const int[Size];
		template<size_t Size>
		explicit constexpr Mapping(const Data<Size> & data) noexcept
			: columns(data)
			, size(std::size(data))
		{
		}
		constexpr Mapping() noexcept
			: Mapping(ALL)
		{
		}
	};

	Qt::CheckState checkState { Qt::Unchecked };
	bool removed { false };
	static const Mapping * mapping;

	explicit BookItem(const DataItem * parent = nullptr);
	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);
	static int Remap(int column) noexcept;

private: // DataItem
	[[nodiscard]] bool IsRemoved() const noexcept override;
	[[nodiscard]] int RemapColumn(int column) const noexcept override;
	[[nodiscard]] int GetColumnCount() const noexcept override;
	BookItem * ToBookItem() noexcept override;
	[[nodiscard]] Qt::CheckState GetCheckState() const noexcept override;
	void SetCheckState(Qt::CheckState state) noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

}
