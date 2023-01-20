#include "sqlite3ppext.h"

#include "database/interface/Command.h"

namespace HomeCompa::DB::Impl::Sqlite {

namespace {

int Index(const size_t index)
{
	return static_cast<int>(index);
}

class Command
	: virtual public DB::Command
{
public:
	Command(sqlite3pp::database & db, std::string_view command)
		: m_command(db, command.data())
	{
	}

private: // DB::Command
	void Execute() override
	{
		m_command.execute();
		m_command.reset();
	}

	int BindInt(size_t index, int value) override
	{
		return m_command.bind(Index(index), value);
	}

	int BindLong(size_t index, long long int value) override
	{
		return m_command.bind(Index(index), value);
	}

	int BindDouble(size_t index, double value) override
	{
		return m_command.bind(Index(index), value);
	}

	int BindString(size_t index, const std::string & value) override
	{
		return m_command.bind(Index(index), value, sqlite3pp::copy);
	}

	int BindInt(std::string_view name, int value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindLong(std::string_view name, long long int value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindDouble(std::string_view name, double value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindString(std::string_view name, const std::string & value) override
	{
		return m_command.bind(name.data(), value, sqlite3pp::copy);
	}

private:
	sqlite3pp::command m_command;
};

}

std::unique_ptr<DB::Command> CreateCommandImpl(sqlite3pp::database & db, std::string_view command)
{
	return std::make_unique<Command>(db, command);
}

}
