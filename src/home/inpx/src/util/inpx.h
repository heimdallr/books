#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "fnd/EnumBitmask.h"
#include "fnd/NonCopyMovable.h"

#include "export/inpxlib.h"

namespace HomeCompa::DB
{
class IDatabase;
class ITransaction;
}

namespace HomeCompa::Inpx
{

enum class CreateCollectionMode
{
	None = 0,
	AddUnIndexedFiles = 1 << 0,
	ScanUnIndexedFolders = 1 << 1,
	SkipLostBooks = 1 << 2,
};

struct UpdateResult
{
	size_t folders;
	size_t authors;
	size_t series;
	size_t books;
	size_t keywords;
	size_t genres;
	bool oldDataUpdateFound { false };
	bool error { false };
};

class INPXLIB_EXPORT Parser
{
	NON_COPY_MOVABLE(Parser)

public:
	using Callback = std::function<void(const UpdateResult&)>;
	using IniMap = std::map<std::wstring, std::filesystem::path>;

public:
	Parser();
	~Parser();

public:
	void CreateNewCollection(IniMap data, CreateCollectionMode mode, Callback callback);
	void UpdateCollection(IniMap data, CreateCollectionMode mode, Callback callback);
	static void FillInpx(const std::filesystem::path& collectionFolder, DB::ITransaction& transaction);
	static bool CheckForUpdate(const std::filesystem::path& collectionFolder, DB::IDatabase& database);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Inpx

ENABLE_BITMASK_OPERATORS(HomeCompa::Inpx::CreateCollectionMode);
