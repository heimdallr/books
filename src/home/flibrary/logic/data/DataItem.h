#pragma once

#include <vector>

#include "fnd/memory.h"

#include <QString>
#include <QVariant>

#include "interface/constants/Enums.h"

namespace HomeCompa::Flibrary {

#define DATA_ITEMS_X_MACRO \
DATA_ITEM(NavigationItem)  \
DATA_ITEM(AuthorItem)      \
DATA_ITEM(BookItem)

#define DATA_ITEM(NAME) struct NAME;
DATA_ITEMS_X_MACRO
#undef DATA_ITEM

class DataItem  // NOLINT(cppcoreguidelines-special-member-functions)
{
protected:
	explicit DataItem(const DataItem * parent = nullptr);

public:
	virtual ~DataItem();
	using Ptr = std::shared_ptr<DataItem>;
	using Items = std::vector<Ptr>;

public:
	const DataItem * GetParent() const noexcept;
	Ptr & AppendChild(Ptr child);
	void SetChildren(Items children) noexcept;
	const DataItem * GetChild(const size_t row) const noexcept;
	size_t GetChildCount() const noexcept;
	size_t GetRow() const noexcept;

public:
	virtual int GetColumnCount() const noexcept = 0;
	virtual const QString & GetId() const noexcept = 0;
	virtual const QString & GetData(int column = 0) const = 0;

	template<typename T>
	T * To() noexcept = delete;
	template<typename T>
	const T * To() const noexcept { return const_cast<DataItem *>(this)->To<T>(); }

#define DATA_ITEM(NAME) template<> NAME* To<>() noexcept { return To##NAME(); }
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

private:
#define DATA_ITEM(NAME) [[nodiscard]] virtual NAME* To##NAME() noexcept { return nullptr; }
	DATA_ITEMS_X_MACRO
#undef DATA_ITEM

protected:
	size_t m_row { 0 };
	const DataItem * m_parent { nullptr };
	Items m_children;
};

struct NavigationItem : DataItem
{
	QString id;
	QString title;

	explicit NavigationItem(const DataItem * parent = nullptr);

	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);

private: // DataItem
	int GetColumnCount() const noexcept override;
	const QString & GetId() const noexcept override;
	const QString & GetData(int column) const override;
	NavigationItem * ToNavigationItem() noexcept override;
};

struct AuthorItem : DataItem
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
	QString id;
	QString data[Column::Last];

	explicit AuthorItem(const DataItem * parent = nullptr);

	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);

private: // DataItem
	int GetColumnCount() const noexcept override;
	const QString & GetId() const noexcept override;
	const QString & GetData(int column) const override;
	AuthorItem * ToAuthorItem() noexcept override;
};

struct BookItem final : DataItem
{
	struct Column
	{
		enum Value
		{
			Author = 0,
			Title,
			Series,
			SeqNumber,
			Size,
			Genre,
			Folder,
			FileName,
			LibRate,
			UpdateDate,
			Lang,
			Last
		};
	};
	QString id;
	QString data[Column::Last];
	bool deleted { false };

	explicit BookItem(const DataItem * parent = nullptr);
	static std::shared_ptr<DataItem> Create(const DataItem * parent = nullptr);

private:
	int GetColumnCount() const noexcept override;
	const QString & GetId() const noexcept override;
	const QString & GetData(int column) const override;
	BookItem * ToBookItem() noexcept override;
};

}
