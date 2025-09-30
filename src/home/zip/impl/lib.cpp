#include "lib.h"

#include <Windows.h>

#include <QCoreApplication>

#include "fnd/NonCopyMovable.h"

#include "zip/interface/error.h"

namespace HomeCompa::ZipDetails::SevenZip
{

namespace
{

constexpr auto LIBRARY_NAME = L"7z.dll";
constexpr auto ENTRY_POINT  = "CreateObject";

using ObjectCreator = UINT32(WINAPI*)(const GUID* clsID, const GUID* interfaceID, void** outObject);

struct Dll
{
	HMODULE handle;

	Dll()
		: handle(LoadLibrary(LIBRARY_NAME))
	{
		if (!handle)
			Error::CannotLoadLibrary(QString::fromStdWString(LIBRARY_NAME));
	}

	~Dll()
	{
		FreeLibrary(handle);
	}

	NON_COPY_MOVABLE(Dll)
};

struct EntryPoint
{
	ObjectCreator handle;

	explicit EntryPoint(const Dll& dll)
		: handle(reinterpret_cast<ObjectCreator>(GetProcAddress(dll.handle, ENTRY_POINT)))
	{
		if (!handle)
			Error::CannotFindEntryPoint(ENTRY_POINT);
	}
};

} // namespace

struct Lib::Impl
{
	Dll        dll;
	EntryPoint func { dll };
};

Lib::Lib() = default;

Lib::~Lib() = default;

bool Lib::CreateObject(const GUID& clsID, const GUID& interfaceID, void** outObject) const //-V835
{
	assert(m_impl->func.handle);
	return SUCCEEDED(m_impl->func.handle(&clsID, &interfaceID, outObject));
}

} // namespace HomeCompa::ZipDetails::SevenZip
