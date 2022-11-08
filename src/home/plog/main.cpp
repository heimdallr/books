#include <Windows.h>

#include "ProductConstant.h"

#include "plog/Initializers/RollingFileInitializer.h"

extern "C" BOOL WINAPI DllMain(
	HINSTANCE /*instance*/,
	DWORD     reason,
	LPVOID    /*reserved*/
)
{
	// Perform actions based on the reason for calling.
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
		{
			std::wstring tempPath(MAX_PATH, 0);
			const auto getTempPath = [&tempPath]
			{
				return static_cast<size_t>(GetTempPath(static_cast<DWORD>(tempPath.length()), tempPath.data()));
			};
			if (size_t length = getTempPath(); length > tempPath.length())
			{
				tempPath.resize(length + 2);
				length = getTempPath();
				assert(length <= tempPath.length());
			}
			tempPath.erase(std::ranges::find(tempPath, L'\0'), std::end(tempPath));
			const std::string companyId(HomeCompa::Flibrary::Constant::COMPANY_ID), productId(HomeCompa::Flibrary::Constant::PRODUCT_ID);
			tempPath.append(std::wstring(companyId.cbegin(), companyId.cend())).append(L"_").append(std::wstring(productId.cbegin(), productId.cend())).append(L".log");
			init(plog::Severity::verbose, tempPath.data());
		}
		break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
		default:
			break;
	}

	return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
