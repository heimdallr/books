#pragma once

#include <vector>
#include <QString>

#include "interface/logic/IDataItem.h"

#include "logicLib.h"

namespace HomeCompa::Flibrary {
class DataItem : public IDataItem
{
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
	explicit DataItem(size_t columnCount, const IDataItem * parent = nullptr);

protected: // IDataItem
	[[nodiscard]] const IDataItem * GetParent() const noexcept override;
	Ptr & AppendChild(Ptr child) override;
	void SetChildren(Items children) noexcept override;
	[[nodiscard]] Ptr GetChild(size_t row) const noexcept override;
	[[nodiscard]] size_t GetChildCount() const noexcept override;
	[[nodiscard]] size_t GetRow() const noexcept override;
	[[nodiscard]] const QString & GetId() const noexcept override;
	[[nodiscard]] const QString & GetData(int column = 0) const noexcept override;
	[[nodiscard]] const QString & GetRawData(int column = 0) const noexcept override;

	[[nodiscard]] bool IsRemoved() const noexcept override;
	[[nodiscard]] int RemapColumn(int column) const noexcept override;
	[[nodiscard]] int GetColumnCount() const noexcept override;

	IDataItem & SetId(QString id) noexcept override;
	IDataItem & SetData(QString value, int column = 0) noexcept override;

	[[nodiscard]] Qt::CheckState GetCheckState() const noexcept override;
	void SetCheckState(Qt::CheckState state) noexcept override;
	void Reduce() override;

	DataItem * ToDataItem() noexcept override;

protected:
	size_t m_row { 0 };
	const IDataItem * m_parent { nullptr };
	Items m_children;
	QString m_id;
	std::vector<QString> m_data;
};

class NavigationItem final : public DataItem
{
public:
	static std::shared_ptr<IDataItem> Create(const IDataItem * parent = nullptr);
	explicit NavigationItem(const IDataItem * parent);

private: // DataItem
	NavigationItem * ToNavigationItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

class AuthorItem final : public DataItem
{
public:
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

	static std::shared_ptr<IDataItem> Create(const IDataItem * parent = nullptr);
	explicit AuthorItem(const IDataItem * parent);

private: // DataItem
	AuthorItem * ToAuthorItem() noexcept override;
	void Reduce() override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

class LOGIC_API BookItem final : public DataItem
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

public:
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

	static std::shared_ptr<IDataItem> Create(const IDataItem * parent = nullptr);
	static int Remap(int column) noexcept;

	explicit BookItem(const IDataItem * parent = nullptr);

private: // DataItem
	[[nodiscard]] bool IsRemoved() const noexcept override;
	[[nodiscard]] int RemapColumn(int column) const noexcept override;
	[[nodiscard]] int GetColumnCount() const noexcept override;
	BookItem * ToBookItem() noexcept override;
	[[nodiscard]] Qt::CheckState GetCheckState() const noexcept override;
	void SetCheckState(Qt::CheckState state) noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

class MenuItem final : public DataItem
{
public:
	struct Column
	{
		enum Value
		{
			Title = 0,
			Id,
			Parameter,
			Enabled,
			Last
		};
	};

	static std::shared_ptr<IDataItem> Create(const IDataItem * parent = nullptr);
	explicit MenuItem(const IDataItem * parent);

private: // DataItem
	MenuItem * ToMenuItem() noexcept override;
	[[nodiscard]] ItemType GetType() const noexcept override;
};

void AppendTitle(QString & title, const QString & str, const QString & delimiter = " ");

}
