#include "PropVariant.h"

#include "7z-sdk/7z/CPP/Common/Defs.h"

namespace HomeCompa::ZipDetails::SevenZip
{

CPropVariant::CPropVariant()
	: PROPVARIANT()
{
	vt         = VT_EMPTY;
	wReserved1 = 0;
}

CPropVariant::~CPropVariant()
{
	Clear();
}

CPropVariant::CPropVariant(bool bSrc)
	: PROPVARIANT()
{
	vt         = VT_BOOL;
	wReserved1 = 0;
	boolVal    = (bSrc ? VARIANT_TRUE : VARIANT_FALSE);
}

CPropVariant::CPropVariant(Byte value)
	: PROPVARIANT()
{
	vt         = VT_UI1;
	wReserved1 = 0;
	bVal       = value;
}

CPropVariant::CPropVariant(Int16 value)
	: PROPVARIANT()
{
	vt         = VT_I2;
	wReserved1 = 0;
	iVal       = value;
}

CPropVariant::CPropVariant(Int32 value)
	: PROPVARIANT()
{
	vt         = VT_I4;
	wReserved1 = 0;
	lVal       = value;
}

CPropVariant::CPropVariant(UInt32 value)
	: PROPVARIANT()
{
	vt         = VT_UI4;
	wReserved1 = 0;
	ulVal      = value;
}

CPropVariant::CPropVariant(UInt64 value)
	: PROPVARIANT()
{
	vt             = VT_UI8;
	wReserved1     = 0;
	uhVal.QuadPart = value;
}

CPropVariant::CPropVariant(FILETIME value)
	: PROPVARIANT()
{
	vt         = VT_FILETIME;
	wReserved1 = 0;
	filetime   = value;
}

CPropVariant::CPropVariant(const PROPVARIANT& varSrc)
	: PROPVARIANT()
{
	vt = VT_EMPTY;
	InternalCopy(&varSrc);
}

CPropVariant::CPropVariant(const CPropVariant& varSrc)
	: PROPVARIANT(varSrc)
{
	vt = VT_EMPTY;
	InternalCopy(&varSrc);
}

CPropVariant::CPropVariant(CPropVariant&&) noexcept = default;

CPropVariant::CPropVariant(BSTR bstrSrc)
	: PROPVARIANT()
{
	vt    = VT_EMPTY;
	*this = bstrSrc;
}

CPropVariant::CPropVariant(LPCOLESTR lpszSrc)
	: PROPVARIANT()
{
	vt    = VT_EMPTY;
	*this = lpszSrc;
}

CPropVariant::CPropVariant(const std::wstring& value)
	: PROPVARIANT()
{
	vt    = VT_EMPTY;
	*this = value.data();
}

CPropVariant& CPropVariant::operator=(const CPropVariant& varSrc)
{
	InternalCopy(&varSrc);
	return *this;
}

CPropVariant& CPropVariant::operator=(CPropVariant&&) noexcept = default;

CPropVariant& CPropVariant::operator=(const PROPVARIANT& varSrc)
{
	InternalCopy(&varSrc);
	return *this;
}

CPropVariant& CPropVariant::operator=(const BSTR bstrSrc)
{
	*this = static_cast<LPCOLESTR>(bstrSrc);
	return *this;
}

static const char* kMemException = "out of memory";

CPropVariant& CPropVariant::operator=(const LPCOLESTR lpszSrc)
{
	InternalClear();
	vt         = VT_BSTR;
	wReserved1 = 0;
	bstrVal    = ::SysAllocString(lpszSrc);
	if (!bstrVal && !lpszSrc)
		throw kMemException;

	return *this;
}

CPropVariant& CPropVariant::operator=(const char* s)
{
	InternalClear();
	vt             = VT_BSTR;
	wReserved1     = 0;
	const auto len = strlen(s);
	bstrVal        = ::SysAllocStringByteLen(nullptr, static_cast<UINT>(len * sizeof(OLECHAR)));
	if (!bstrVal)
		throw kMemException;

	for (size_t i = 0; i <= len; i++)
		bstrVal[i] = s[i];

	return *this;
}

CPropVariant& CPropVariant::operator=(const bool bSrc)
{
	if (vt != VT_BOOL)
	{
		InternalClear();
		vt = VT_BOOL;
	}
	boolVal = bSrc ? VARIANT_TRUE : VARIANT_FALSE;
	return *this;
}

#define SET_PROP_FUNC(type, id, dest)                 \
	CPropVariant& CPropVariant::operator=(type value) \
	{                                                 \
		if (vt != (id))                               \
		{                                             \
			InternalClear();                          \
			vt = id;                                  \
		}                                             \
		(dest) = value;                               \
		return *this;                                 \
	}

SET_PROP_FUNC(Byte, VT_UI1, bVal)
SET_PROP_FUNC(Int16, VT_I2, iVal)
SET_PROP_FUNC(Int32, VT_I4, lVal)
SET_PROP_FUNC(UInt32, VT_UI4, ulVal)
SET_PROP_FUNC(UInt64, VT_UI8, uhVal.QuadPart)
SET_PROP_FUNC(FILETIME, VT_FILETIME, filetime)

static HRESULT MyPropVariantClear(PROPVARIANT* prop)
{
	switch (prop->vt)
	{
		case VT_UI1:
		case VT_I1:
		case VT_I2:
		case VT_UI2:
		case VT_BOOL:
		case VT_I4:
		case VT_UI4:
		case VT_R4:
		case VT_INT:
		case VT_UINT:
		case VT_ERROR:
		case VT_FILETIME:
		case VT_UI8:
		case VT_R8:
		case VT_CY:
		case VT_DATE:
			prop->vt         = VT_EMPTY;
			prop->wReserved1 = 0;
			return S_OK;
	}
	return ::VariantClear(reinterpret_cast<VARIANTARG*>(prop));
}

HRESULT CPropVariant::Clear()
{
	return MyPropVariantClear(this);
}

HRESULT CPropVariant::Copy(const PROPVARIANT* pSrc)
{
	::VariantClear(reinterpret_cast<tagVARIANT*>(this));
	switch (pSrc->vt)
	{
		case VT_UI1:
		case VT_I1:
		case VT_I2:
		case VT_UI2:
		case VT_BOOL:
		case VT_I4:
		case VT_UI4:
		case VT_R4:
		case VT_INT:
		case VT_UINT:
		case VT_ERROR:
		case VT_FILETIME:
		case VT_UI8:
		case VT_R8:
		case VT_CY:
		case VT_DATE:
			memmove((PROPVARIANT*)this, pSrc, sizeof(PROPVARIANT));
			return S_OK;
	}
	return ::VariantCopy(reinterpret_cast<tagVARIANT*>(this), reinterpret_cast<tagVARIANT*>(const_cast<PROPVARIANT*>(pSrc)));
}

HRESULT CPropVariant::Attach(PROPVARIANT* pSrc)
{
	const HRESULT hr = Clear();
	if (FAILED(hr))
		return hr;

	memcpy(this, pSrc, sizeof(PROPVARIANT));
	pSrc->vt = VT_EMPTY;
	return S_OK;
}

HRESULT CPropVariant::Detach(PROPVARIANT* pDest)
{
	const HRESULT hr = MyPropVariantClear(pDest);
	if (FAILED(hr))
		return hr;

	memcpy(pDest, this, sizeof(PROPVARIANT));
	vt = VT_EMPTY;
	return S_OK;
}

HRESULT CPropVariant::InternalClear()
{
	const HRESULT hr = Clear();
	if (FAILED(hr))
	{
		vt    = VT_ERROR;
		scode = hr;
	}
	return hr;
}

void CPropVariant::InternalCopy(const PROPVARIANT* pSrc)
{
	const HRESULT hr = Copy(pSrc);
	if (FAILED(hr))
	{
		if (hr == E_OUTOFMEMORY)
			throw kMemException;
		vt    = VT_ERROR;
		scode = hr;
	}
}

int CPropVariant::Compare(const CPropVariant& a) const
{
	if (vt != a.vt)
		return MyCompare(vt, a.vt);
	switch (vt)
	{
		case VT_UI1:
			return MyCompare(bVal, a.bVal);
		case VT_I2:
			return MyCompare(iVal, a.iVal);
		case VT_UI2:
			return MyCompare(uiVal, a.uiVal);
		case VT_I4:
			return MyCompare(lVal, a.lVal);
		case VT_UI4:
			return MyCompare(ulVal, a.ulVal);
			// case VT_UINT: return MyCompare(uintVal, a.uintVal);
		case VT_I8:
			return MyCompare(hVal.QuadPart, a.hVal.QuadPart);
		case VT_UI8:
			return MyCompare(uhVal.QuadPart, a.uhVal.QuadPart);
		case VT_BOOL:
			return -MyCompare(boolVal, a.boolVal);
		case VT_FILETIME:
			return ::CompareFileTime(&filetime, &a.filetime);
		case VT_EMPTY:
			// case VT_I1: return MyCompare(cVal, a.cVal);
		case VT_BSTR: // Not implemented
			// return MyCompare(aPropVarint.cVal);
		default:
			return 0;
	}
}

} // namespace HomeCompa::ZipDetails::SevenZip
