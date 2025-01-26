#include <atlcomcli.h>

#include "OutMemStream.h"

#include <QIODevice>

#include "zip/interface/ProgressCallback.h"

using namespace HomeCompa::ZipDetails::Impl::SevenZip;

CComPtr<ISequentialOutStream> OutMemStream::Create(QIODevice & stream, ProgressCallback & progress)
{
	return new OutMemStream(stream, progress);
}

OutMemStream::OutMemStream(QIODevice & stream, ProgressCallback & progress)
	: m_stream { stream }
	, m_progress { progress }
{
}

STDMETHODIMP OutMemStream::Write(const void * data, const UInt32 size, UInt32 * processedSize)
{
	if (processedSize)
		*processedSize = 0;

	if (!data || size == 0)
		return E_FAIL;

	if (m_progress.OnCheckBreak())
		return E_ABORT;

	const auto* byte_data = static_cast<const char *>(data);
	m_stream.write(byte_data, size);
	if (processedSize)
		*processedSize = size;

	m_progress.OnIncrement(size);

	return S_OK;
}
