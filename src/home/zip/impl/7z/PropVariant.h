#pragma once

#include <propidlbase.h>
#include <7z/CPP/Common/MyTypes.h>


namespace HomeCompa::Zip::Impl::SevenZip {

class CPropVariant : public PROPVARIANT
{
public:

	CPropVariant();
	~CPropVariant();
	CPropVariant(const PROPVARIANT & varSrc);
	CPropVariant(const CPropVariant & varSrc);
	CPropVariant(BSTR bstrSrc);
	CPropVariant(LPCOLESTR lpszSrc);
	CPropVariant(bool bSrc);
	CPropVariant(Byte value);
	CPropVariant(Int16 value);
	CPropVariant(Int32 value);
	CPropVariant(UInt32 value);
	CPropVariant(UInt64 value);
	CPropVariant(const FILETIME & value);

	CPropVariant & operator=(const CPropVariant & varSrc);
	CPropVariant & operator=(const PROPVARIANT & varSrc);
	CPropVariant & operator=(BSTR bstrSrc);
	CPropVariant & operator=(LPCOLESTR lpszSrc);
	CPropVariant & operator=(const char * s);
	CPropVariant & operator=(bool bSrc);
	CPropVariant & operator=(Byte value);
	CPropVariant & operator=(Int16 value);
	CPropVariant & operator=(Int32 value);
	CPropVariant & operator=(UInt32 value);
	CPropVariant & operator=(Int64 value);
	CPropVariant & operator=(UInt64 value);
	CPropVariant & operator=(const FILETIME & value);

	HRESULT Clear();
	HRESULT Copy(const PROPVARIANT * pSrc);
	HRESULT Attach(PROPVARIANT * pSrc);
	HRESULT Detach(PROPVARIANT * pDest);

	HRESULT InternalClear();
	void InternalCopy(const PROPVARIANT * pSrc);

	int Compare(const CPropVariant & a1) const;
};

}
