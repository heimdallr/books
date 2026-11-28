#pragma once

#include <QAbstractItemModel>

#include <gmock/gmock.h>

namespace HomeCompa::Flibrary::Test {

class MockAbstractItemModel : public QAbstractItemModel
{
public: // QAbstractItemModel
	MOCK_METHOD(QModelIndex, index, (int, int, const QModelIndex&), (const, override));
	MOCK_METHOD(QModelIndex, parent, (const QModelIndex&), (const, override));
	MOCK_METHOD(int, rowCount, (const QModelIndex&), (const, override));
	MOCK_METHOD(int, columnCount, (const QModelIndex&), (const, override));
	MOCK_METHOD(QVariant, data, (const QModelIndex&, int), (const, override));
	MOCK_METHOD(bool, setData, (const QModelIndex&, const QVariant&, int), (override));

public:
	QModelIndex CreateIndex(const int row, const int column, void* adata = nullptr)
	{
		return createIndex(row, column, adata);
	}
};

static_assert(!std::is_abstract_v<MockAbstractItemModel>, "MockAbstractItemModel is abstract!");

}
