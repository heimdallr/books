#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "fnd/observer.h"

namespace HomeCompa::DB
{

class ITransaction;
class IQuery;

class IDatabaseObserver : public Observer
{
public:
	virtual void OnInsert(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
	virtual void OnUpdate(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
	virtual void OnDelete(std::string_view dbName, std::string_view tableName, int64_t rowId) = 0;
};

class DatabaseFunctionContext
{
public:
	virtual ~DatabaseFunctionContext() = default;

	virtual void SetResult(int) = 0;
};

using DatabaseFunction = std::function<void(DatabaseFunctionContext&)>;

class IDatabase
{
public:
	virtual ~IDatabase() = default;
	virtual [[nodiscard]] std::unique_ptr<ITransaction> CreateTransaction() = 0;
	virtual [[nodiscard]] std::unique_ptr<IQuery> CreateQuery(std::string_view query) = 0;

	virtual void CreateFunction(std::string_view name, DatabaseFunction function) = 0;

	virtual void RegisterObserver(IDatabaseObserver* observer) = 0;
	virtual void UnregisterObserver(IDatabaseObserver* observer) = 0;
};

} // namespace HomeCompa::DB
