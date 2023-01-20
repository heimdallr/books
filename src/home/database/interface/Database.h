#pragma once

#include <memory>
#include <string_view>

#include "fnd/observer.h"

namespace HomeCompa::DB {

class Transaction;
class Query;

class DatabaseObserver : public Observer
{
public:
	virtual void OnInsert(char const * dbName, char const * tableName, int64_t rowId) = 0;
	virtual void OnUpdate(char const * dbName, char const * tableName, int64_t rowId) = 0;
	virtual void OnDelete(char const * dbName, char const * tableName, int64_t rowId) = 0;
};

class Database
{
public:
	virtual ~Database() = default;
	virtual [[nodiscard]] std::unique_ptr<Transaction> CreateTransaction() = 0;
	virtual [[nodiscard]] std::unique_ptr<Query> CreateQuery(const std::string_view & query) = 0;

	virtual void RegisterObserver(DatabaseObserver * observer) = 0;
	virtual void UnregisterObserver(DatabaseObserver * observer) = 0;
};

}
