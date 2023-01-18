#pragma once

#include <memory>
#include <string_view>

namespace HomeCompa::DB {

class Transaction;
class Query;

class Database
{
public:
	virtual ~Database() = default;
	virtual [[nodiscard]] std::unique_ptr<Transaction> CreateTransaction() = 0;
	virtual [[nodiscard]] std::unique_ptr<Query> CreateQuery(const std::string_view & query) = 0;
};

}
