#include "DyLib.h"

#include <Windows.h>

#include <cassert>
#include <cwctype>
#include <sstream>

#include "StrUtil.h"

namespace HomeCompa::Util
{

namespace
{

void* InnerOpen(const std::filesystem::path& moduleName)
{
	const HMODULE handle = ::LoadLibraryW(moduleName.wstring().data());
	return reinterpret_cast<void*>(handle);
}

bool InnerClose(void* handle)
{
	return (0 != ::FreeLibrary(reinterpret_cast<HMODULE>(handle)));
}

auto InnerGetProc(void* handle, const std::string& procName)
{
	return ::GetProcAddress(reinterpret_cast<HMODULE>(handle), procName.data());
}

std::string InnerGetErrorDescription()
{
	const DWORD errCode = ::GetLastError();
	assert(errCode != 0);
	if (errCode == 0)
		return "Undefined";

	LPWSTR lpMsg = nullptr;
	LPVOID lpBuf = &lpMsg;
	size_t literalCount =
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)lpBuf, 0, NULL);
	assert(literalCount > 0);

	while (literalCount > 1 && std::iswspace(lpMsg[literalCount - 1]))
		--literalCount;

	auto message = ToMultiByte(std::wstring(lpMsg, lpMsg + literalCount));
	assert(nullptr == ::LocalFree(lpMsg));
	return message;
}

} // namespace

DyLib::DyLib() = default;

DyLib::DyLib(std::filesystem::path moduleName)
{
	this->Open(std::move(moduleName));
}

DyLib::~DyLib()
{
	if (this->IsOpen())
		this->Close();
}

bool DyLib::IsOpen() const
{
	const bool result = (m_handle != nullptr);
	assert(result == !m_moduleName.empty());
	return result;
}

void DyLib::Detach()
{
	m_handle = nullptr;
	m_moduleName.clear();
	m_errorDescription.clear();
}

bool DyLib::Open(std::filesystem::path moduleName)
{
	if (this->IsOpen())
		this->Close();

	void* const handle = InnerOpen(moduleName);
	if (!handle)
	{
		const auto         innerDescription = InnerGetErrorDescription();
		std::ostringstream stream;
		stream << "Cannot load module '" << moduleName.generic_string() << "', additional info: " << innerDescription;
		m_errorDescription = stream.str();
		return false;
	}

	m_errorDescription.clear();
	m_moduleName = std::move(moduleName);
	m_handle     = handle;
	return true;
}

void DyLib::Close()
{
	assert(this->IsOpen());
	const bool result = InnerClose(m_handle);
	this->Detach();
	if (result)
		return;

	assert(false);
	std::ostringstream stream;
	stream << "Cannot unload module '" << m_moduleName.generic_string() << "', additional info: " << InnerGetErrorDescription();
	m_errorDescription = stream.str();
}

void* DyLib::GetProc(const std::string& procName)
{
	if (!this->IsOpen())
	{
		assert(false);
		m_errorDescription = "Cannot find entry point in empty module";
		return nullptr;
	}

	if (const auto result = InnerGetProc(m_handle, procName))
	{
		m_errorDescription.clear();
		return result;
	}

	assert(false);
	std::ostringstream stream;
	stream << "Cannot find entry point '" << procName << "' in '" << m_moduleName.generic_string() << "', additional info: " << InnerGetErrorDescription();
	m_errorDescription = stream.str();
	return nullptr;
}

} // namespace HomeCompa::Util
