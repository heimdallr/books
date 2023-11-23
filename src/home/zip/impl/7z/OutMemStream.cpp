#include <atlcomcli.h>

#include "OutMemStream.h"

#include <QByteArray>

using namespace HomeCompa::Zip::Impl::SevenZip;

CComPtr<ISequentialOutStream> OutMemStream::Create(QByteArray & buffer)
{
	return new OutMemStream(buffer);
}

OutMemStream::OutMemStream(QByteArray & buffer)
	: m_buffer(buffer)
{
}

STDMETHODIMP OutMemStream::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (iid == __uuidof(IUnknown))
	{
		*ppvObject = static_cast<IUnknown *>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_ISequentialOutStream)
	{
		*ppvObject = static_cast<ISequentialOutStream *>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) OutMemStream::AddRef()
{
	return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
}

STDMETHODIMP_(ULONG) OutMemStream::Release()
{
	const auto res = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
	if (res == 0)
		delete this;

	return res;
}

STDMETHODIMP OutMemStream::Write(const void * data, const UInt32 size, UInt32 * processedSize)
{
	if (processedSize)
	{
		*processedSize = 0;
	}
	if (!data || size == 0)
	{
		return E_FAIL;
	}

	const auto* byte_data = static_cast<const char *>(data);
	m_buffer.append(byte_data, size);
	*processedSize = size;

	return S_OK;
}
