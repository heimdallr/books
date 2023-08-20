#pragma once

namespace HomeCompa::Flibrary {

class IAddCollectionDialog  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct Result
	{
		enum Value
		{
			Cancel = 0,
			CreateNew,
			Add,
		};
	};

public:
	virtual ~IAddCollectionDialog() = default;

public:
	[[nodiscard]] virtual int Exec() = 0;
	[[nodiscard]] virtual QString GetName() const = 0;
	[[nodiscard]] virtual QString GetDatabaseFileName() const = 0;
	[[nodiscard]] virtual QString GetArchiveFolder() const = 0;
};

}
