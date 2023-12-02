#include <atlcomcli.h>

#include "OutMemStream.h"

#include <QByteArray>

#include "zip/interface/ProgressCallback.h"

using namespace HomeCompa::ZipDetails::Impl::SevenZip;

CComPtr<ISequentialOutStream> OutMemStream::Create(QByteArray & buffer, std::shared_ptr<ProgressCallback> progress)
{
	return new OutMemStream(buffer, std::move(progress));
}

OutMemStream::OutMemStream(QByteArray & buffer, std::shared_ptr<ProgressCallback> progress)
	: m_buffer(buffer)
	, m_progress(std::move(progress))
{
}

STDMETHODIMP OutMemStream::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (iid == __uuidof(IUnknown))  // NOLINT(clang-diagnostic-language-extension-token)
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
		*processedSize = 0;

	if (!data || size == 0)
		return E_FAIL;

	if (m_progress->OnCheckBreak())
		return E_ABORT;

	const auto* byte_data = static_cast<const char *>(data);
	m_buffer.append(byte_data, size);
	*processedSize = size;

	m_progress->OnIncrement(size);

	return S_OK;
}
