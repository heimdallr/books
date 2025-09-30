#pragma once

#include <propidlbase.h>

#include <string>

#include "7z-sdk/7z/C/7zTypes.h"

namespace HomeCompa::ZipDetails::SevenZip
{

class CPropVariant : public PROPVARIANT
{
public:
	CPropVariant();
	~CPropVariant();
	CPropVariant(const CPropVariant& varSrc);
	CPropVariant(CPropVariant&& varSrc) noexcept;
	CPropVariant(const PROPVARIANT& varSrc);
	CPropVariant(BSTR bstrSrc);
	CPropVariant(LPCOLESTR lpszSrc);
	CPropVariant(const std::wstring& value);
	CPropVariant(bool bSrc);
	CPropVariant(Byte value);
	CPropVariant(Int16 value);
	CPropVariant(Int32 value);
	CPropVariant(UInt32 value);
	CPropVariant(UInt64 value);
	CPropVariant(FILETIME value);

	CPropVariant& operator=(const CPropVariant& varSrc);
	CPropVariant& operator=(CPropVariant&& varSrc) noexcept;
	CPropVariant& operator=(const PROPVARIANT& varSrc);
	CPropVariant& operator=(BSTR bstrSrc);
	CPropVariant& operator=(LPCOLESTR lpszSrc);
	CPropVariant& operator=(const char* s);
	CPropVariant& operator=(bool bSrc);
	CPropVariant& operator=(Byte value);
	CPropVariant& operator=(Int16 value);
	CPropVariant& operator=(Int32 value);
	CPropVariant& operator=(UInt32 value);
	CPropVariant& operator=(Int64 value);
	CPropVariant& operator=(UInt64 value);
	CPropVariant& operator=(FILETIME value);

	HRESULT Clear();
	HRESULT Copy(const PROPVARIANT* pSrc);
	HRESULT Attach(PROPVARIANT* pSrc);
	HRESULT Detach(PROPVARIANT* pDest);

	HRESULT InternalClear();
	void    InternalCopy(const PROPVARIANT* pSrc);

	int Compare(const CPropVariant& a1) const;
};

} // namespace HomeCompa::ZipDetails::SevenZip
