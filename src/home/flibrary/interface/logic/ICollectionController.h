#pragma once

class QString;

namespace HomeCompa::Flibrary {

class ICollectionController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ICollectionController() = default;

public:
	[[nodiscard]] virtual bool AddCollection() = 0;

	[[nodiscard]] virtual bool IsEmpty() const noexcept = 0;

	[[nodiscard]] virtual bool IsCollectionNameExists(const QString& name) const = 0;
	[[nodiscard]] virtual QString GetCollectionDatabaseName(const QString & databaseFileName) const = 0;
	[[nodiscard]] virtual bool IsCollectionFolderHasInpx(const QString & archiveFolder) const = 0;
};

}
