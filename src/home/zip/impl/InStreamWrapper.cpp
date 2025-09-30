#include "InStreamWrapper.h"

#include "win.h"

using namespace HomeCompa::ZipDetails::SevenZip;

CComPtr<InStreamWrapper> InStreamWrapper::Create(CComPtr<IStream> baseStream)
{
	return new InStreamWrapper(std::move(baseStream));
}

InStreamWrapper::InStreamWrapper(CComPtr<IStream> baseStream)
	: m_baseStream(std::move(baseStream))
{
}

HRESULT STDMETHODCALLTYPE InStreamWrapper::QueryInterface(REFIID iid, void** ppvObject) //-V835
{
	if (iid == __uuidof(IUnknown)) // NOLINT(clang-diagnostic-language-extension-token)
	{
		*ppvObject = reinterpret_cast<IUnknown*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_ISequentialInStream)
	{
		*ppvObject = static_cast<ISequentialInStream*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IInStream)
	{
		*ppvObject = static_cast<IInStream*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IStreamGetSize)
	{
		*ppvObject = static_cast<IStreamGetSize*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP InStreamWrapper::Read(void* data, const UInt32 size, UInt32* processedSize)
{
	ULONG         read = 0;
	const HRESULT hr   = m_baseStream->Read(data, size, &read);
	if (processedSize)
		*processedSize = read;

	return SUCCEEDED(hr) ? S_OK : hr;
}

STDMETHODIMP InStreamWrapper::Seek(const Int64 offset, const UInt32 seekOrigin, UInt64* newPosition)
{
	LARGE_INTEGER  move;
	ULARGE_INTEGER newPos;

	move.QuadPart    = offset;
	const HRESULT hr = m_baseStream->Seek(move, seekOrigin, &newPos);
	if (newPosition)
		*newPosition = newPos.QuadPart;

	return hr;
}

STDMETHODIMP InStreamWrapper::GetSize(UInt64* size)
{
	STATSTG       statInfo;
	const HRESULT hr = m_baseStream->Stat(&statInfo, STATFLAG_NONAME);
	if (SUCCEEDED(hr))
		*size = statInfo.cbSize.QuadPart;

	return hr;
}
