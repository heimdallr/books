#include "sqlite3ppext.h"

#include "ICommand.h"

namespace HomeCompa::DB::Impl::Sqlite {

namespace {

int Index(const size_t index)
{
	return static_cast<int>(index);
}

class Command
	: virtual public DB::ICommand
{
public:
	Command(sqlite3pp::database & db, const std::string_view command)
		: m_command(db, command.data())
	{
	}

private: // DB::Command
	void Execute() override
	{
		m_command.execute();
		m_command.reset();
	}

	int BindInt(const size_t index, const int value) override
	{
		return m_command.bind(Index(index) + 1, value);
	}

	int BindLong(const size_t index, const long long int value) override
	{
		return m_command.bind(Index(index) + 1, value);
	}

	int BindDouble(const size_t index, const double value) override
	{
		return m_command.bind(Index(index) + 1, value);
	}

	int BindString(const size_t index, const std::string & value) override
	{
		return m_command.bind(Index(index) + 1, value, sqlite3pp::copy);
	}

	int BindInt(const std::string_view name, const int value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindLong(const std::string_view name, const long long int value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindDouble(const std::string_view name, const double value) override
	{
		return m_command.bind(name.data(), value);
	}

	int BindString(const std::string_view name, const std::string & value) override
	{
		return m_command.bind(name.data(), value, sqlite3pp::copy);
	}

private:
	sqlite3pp::command m_command;
};

}

std::unique_ptr<DB::ICommand> CreateCommandImpl(sqlite3pp::database & db, std::string_view command)
{
	return std::make_unique<Command>(db, command);
}

}
