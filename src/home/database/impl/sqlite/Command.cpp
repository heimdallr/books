#include "Database.h"
#include "ICommand.h"
#include "sqlite3ppext.h"

namespace HomeCompa::DB::Impl::Sqlite
{

namespace
{

int Index(const size_t index)
{
	return static_cast<int>(index);
}

class Command final : virtual public ICommand
{
public:
	Command(sqlite3pp::database& db, const std::string_view command)
		: m_command(db, LogStatement(command).data())
	{
	}

private: // DB::Command
	bool Execute() override
	{
		const auto res = m_command.execute();
		if (res != 0)
			PLOGE << "command failed: " << res;
		m_command.reset();
		return res == 0;
	}

	int Bind(const size_t index) override
	{
		return m_command.bind(Index(index) + 1);
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

	int BindString(const size_t index, const std::string_view value) override
	{
		return m_command.bind(Index(index) + 1, value, sqlite3pp::copy);
	}

	int Bind(const std::string_view name) override
	{
		return m_command.bind(name.data());
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

	int BindString(const std::string_view name, const std::string_view value) override
	{
		return m_command.bind(name.data(), value, sqlite3pp::copy);
	}

private:
	sqlite3pp::command m_command;
};

} // namespace

std::unique_ptr<ICommand> CreateCommandImpl(sqlite3pp::database& db, std::string_view command)
{
	return std::make_unique<Command>(db, command);
}

} // namespace HomeCompa::DB::Impl::Sqlite
