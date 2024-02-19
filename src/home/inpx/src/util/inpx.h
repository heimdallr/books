#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "fnd/EnumBitmask.h"
#include "fnd/NonCopyMovable.h"

#include "export/inpxlib.h"

namespace HomeCompa::Inpx {

enum class CreateCollectionMode
{
	None                 = 0,
	AddUnIndexedFiles    = 1 << 0,
	ScanUnIndexedFolders = 1 << 1,
};

class INPXLIB_EXPORT Parser
{
	NON_COPY_MOVABLE(Parser)

public:
	using Callback = std::function<void(bool)>;
	using IniMap = std::map<std::wstring, std::filesystem::path>;

public:
	Parser();
	~Parser();

public:
	void CreateNewCollection(IniMap data, CreateCollectionMode mode, Callback callback);
	void UpdateCollection(IniMap data, CreateCollectionMode mode, Callback callback);

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

}

ENABLE_BITMASK_OPERATORS(HomeCompa::Inpx::CreateCollectionMode);
