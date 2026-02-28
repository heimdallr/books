#pragma once

#include <vector>

#include <QString>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IDataItem.h"

#include "export/logic.h"

namespace HomeCompa::Flibrary
{

class DataItem : public IDataItem
{
	DEFAULT_COPY_MOVABLE(DataItem)

public:
	struct Column
	{
		enum Value
		{
			Title = 0,
			Last
		};
	};

protected:
	explicit DataItem(size_t columnCount, IDataItem* parent = nullptr);
	~DataItem() override = default;

protected: // IDataItem
	[[nodiscard]] IDataItem*     GetParent() noexcept override;
	Ptr&                         AppendChild(Ptr child) override;
	void                         RemoveChild(size_t row, size_t count) override;
	void                         RemoveAllChildren() override;
	void                         SetChildren(Items children) noexcept override;
	[[nodiscard]] Ptr            GetChild(size_t row) const noexcept override;
	[[nodiscard]] size_t         GetChildCount() const noexcept override;
	[[nodiscard]] size_t         GetRow() const noexcept override;
	[[nodiscard]] const QString& GetId() const noexcept override;
	[[nodiscard]] Flags          GetFlags() const noexcept override;
	[[nodiscard]] const QString& GetData(int column = 0) const noexcept override;
	[[nodiscard]] const QString& GetRawData(int column = 0) const noexcept override;

	[[nodiscard]] bool IsRemoved() const noexcept override;
	void               SetRemoved(bool value) noexcept override;

	[[nodiscard]] int RemapColumn(int column) const noexcept override;
	[[nodiscard]] int GetColumnCount() const noexcept override;

	IDataItem& SetId(QString id) noexcept override;
	IDataItem& SetFlags(Flags flags) noexcept override;
	IDataItem& SetData(QString value, int column = 0) noexcept override;

	[[nodiscard]] Qt::CheckState GetCheckState() const noexcept override;
	void                         SetCheckState(Qt::CheckState state) noexcept override;
	void                         Reduce() override;

	Ptr  FindChild(const std::function<bool(const IDataItem&)>& functor) const override;
	void SortChildren(const std::function<bool(const IDataItem& lhs, const IDataItem& rhs)>& comparer) override;

	DataItem* ToDataItem() noexcept override;

protected:
	size_t               m_row { 0 };
	IDataItem*           m_parent { nullptr };
	Items                m_children;
	QString              m_id;
	Flags                m_flags { Flags::None };
	std::vector<QString> m_data;
	bool                 m_removed { false };
};

class NavigationItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(NavigationItem)

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit NavigationItem(IDataItem* parent);
	~NavigationItem() override = default;

private: // DataItem
	NavigationItem*        ToNavigationItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class LOGIC_EXPORT SettingsItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(SettingsItem)

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit SettingsItem(IDataItem* parent);
	~SettingsItem() override = default;

public:
	struct Column
	{
		enum Value
		{
			Key = 0,
			Value,
			Last
		};
	};

private: // DataItem
	SettingsItem*          ToSettingsItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class GenreItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(GenreItem)

public:
	struct Column
	{
		enum Value
		{
			Title = 0,
			Fb2Code,
			Last
		};
	};

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit GenreItem(IDataItem* parent);
	~GenreItem() override = default;

private: // DataItem
	GenreItem*             ToGenreItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class AuthorItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(AuthorItem)

public:
	struct Column
	{
		enum Value
		{
			Name = 0,
			LastName,
			FirstName,
			MiddleName,
			Last
		};
	};

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit AuthorItem(IDataItem* parent);
	~AuthorItem() override = default;

private: // DataItem
	AuthorItem*            ToAuthorItem() noexcept override;
	void                   Reduce() override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class SeriesItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(SeriesItem)

public:
	struct Column
	{
		enum Value
		{
			Title = 0,
			SeqNum,
			Last
		};
	};

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit SeriesItem(IDataItem* parent);
	~SeriesItem() override = default;

private: // DataItem
	SeriesItem*            ToSeriesItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class ReviewItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(ReviewItem)

public:
	struct Column
	{
		enum Value
		{
			Name = 0,
			Time,
			Comment,
			Last
		};
	};

public:
	static Ptr Create(IDataItem* parent = nullptr);
	explicit ReviewItem(IDataItem* parent);
	~ReviewItem() override = default;

private: // DataItem
	ReviewItem*            ToReviewItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

class LOGIC_EXPORT BookItem final : public DataItem
{
#define BOOKS_COLUMN_ITEMS_X_MACRO \
	BOOKS_COLUMN_ITEM(Author)      \
	BOOKS_COLUMN_ITEM(Title)       \
	BOOKS_COLUMN_ITEM(SeriesId)    \
	BOOKS_COLUMN_ITEM(SeqNumber)   \
	BOOKS_COLUMN_ITEM(UpdateDate)  \
	BOOKS_COLUMN_ITEM(LibRate)     \
	BOOKS_COLUMN_ITEM(Lang)        \
	BOOKS_COLUMN_ITEM(Year)        \
	BOOKS_COLUMN_ITEM(Folder)      \
	BOOKS_COLUMN_ITEM(FileName)    \
	BOOKS_COLUMN_ITEM(Size)        \
	BOOKS_COLUMN_ITEM(UserRate)    \
	BOOKS_COLUMN_ITEM(LibID)       \
	BOOKS_COLUMN_ITEM(Series)      \
	BOOKS_COLUMN_ITEM(Genre)       \
	BOOKS_COLUMN_ITEM(AuthorFull)

	DEFAULT_COPY_MOVABLE(BookItem)

public:
	struct Column
	{
		enum Value
		{

#define BOOKS_COLUMN_ITEM(NAME) NAME,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
				Last
		};
	};

	static_assert(Column::Author == 0);

	static constexpr int ALL[] {
#define BOOKS_COLUMN_ITEM(NAME) Column::NAME,
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
	};

	struct Mapping
	{
		const int* const columns;
		const size_t     size;
		template <size_t Size>
		using Data = const int[Size];

		template <size_t Size>
		explicit constexpr Mapping(const Data<Size>& data) noexcept
			: columns(data)
			, size(std::size(data))
		{
		}

		constexpr Mapping() noexcept
			: Mapping(ALL)
		{
		}
	};

	Qt::CheckState        checkState { Qt::Unchecked };
	static const Mapping* mapping;

	static Ptr Create(IDataItem* parent = nullptr, size_t additionalFieldCount = 0);
	static int Remap(int column) noexcept;

	BookItem(IDataItem* parent, size_t additionalFieldCount);
	~BookItem() override = default;

private: // DataItem
	[[nodiscard]] int            RemapColumn(int column) const noexcept override;
	[[nodiscard]] int            GetColumnCount() const noexcept override;
	BookItem*                    ToBookItem() noexcept override;
	[[nodiscard]] Qt::CheckState GetCheckState() const noexcept override;
	void                         SetCheckState(Qt::CheckState state) noexcept override;
	[[nodiscard]] ItemType       GetType() const noexcept override;
	[[nodiscard]] Ptr            Clone() const override;
};

class MenuItem final : public DataItem
{
	DEFAULT_COPY_MOVABLE(MenuItem)

public:
	struct Column
	{
		enum Value
		{
			Title = 0,
			Id,
			Parameter,
			Enabled,
			Checkable,
			Checked,
			HasError,
			Last
		};
	};

	static Ptr Create(IDataItem* parent = nullptr);
	explicit MenuItem(IDataItem* parent);
	~MenuItem() override = default;

private: // DataItem
	MenuItem*              ToMenuItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
	[[nodiscard]] Ptr      Clone() const override;
};

LOGIC_EXPORT void    AppendTitle(QString& title, const QString& str, const QString& delimiter = " ");
LOGIC_EXPORT QString GetAuthorFull(const IDataItem& author);

} // namespace HomeCompa::Flibrary
