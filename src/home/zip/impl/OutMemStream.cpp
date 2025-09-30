#include "OutMemStream.h"

#include <QFileDevice>
#include <QIODevice>

#include "fnd/unknown_impl.h"

#include "zip/interface/ProgressCallback.h"

using namespace HomeCompa::ZipDetails::SevenZip;

class StreamSetRestriction : public IStreamSetRestriction
{
	UNKNOWN_IMPL(IStreamSetRestriction) //-V835

public:
	static CComPtr<IStreamSetRestriction> Create()
	{
		return new StreamSetRestriction();
	}

private:
	HRESULT SetRestriction(UInt64 begin, UInt64 end) noexcept override
	{
		if (begin > end)
			return E_FAIL;

		return S_OK;
	}
};

CComPtr<ISequentialOutStream> OutMemStream::Create(QIODevice& stream, ProgressCallback& progress)
{
	return new OutMemStream(stream, progress);
}

OutMemStream::OutMemStream(QIODevice& stream, ProgressCallback& progress)
	: m_stream { stream }
	, m_progress { progress }
{
}

OutMemStream::~OutMemStream()
{
	if (auto* file = dynamic_cast<QFileDevice*>(&m_stream))
		file->resize(m_maxPos);
}

HRESULT OutMemStream::QueryInterface(REFIID iid, void** ppvObject) //-V835
{
	if (iid == __uuidof(IUnknown)) // NOLINT(clang-diagnostic-language-extension-token)
	{
		*ppvObject = reinterpret_cast<IUnknown*>(this); // NOLINT(clang-diagnostic-reinterpret-base-class)
		AddRef();
		return S_OK;
	}

	if (iid == IID_ISequentialOutStream)
	{
		*ppvObject = static_cast<ISequentialOutStream*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IOutStream)
	{
		*ppvObject = static_cast<IOutStream*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == IID_IStreamSetRestriction)
	{
		auto obj   = StreamSetRestriction::Create();
		*ppvObject = obj.Detach();
	}

	return E_NOINTERFACE;
}

HRESULT OutMemStream::Write(const void* data, const UInt32 size, UInt32* processedSize) noexcept
{
	if (processedSize)
		*processedSize = 0;

	if (!data || size == 0)
		return E_FAIL;

	if (m_progress.OnCheckBreak())
		return E_ABORT;

	const auto* byte_data = static_cast<const char*>(data);
	const auto  realSize  = m_stream.write(byte_data, size);
	if (processedSize)
		*processedSize = realSize;

	m_progress.OnIncrement(size);

	m_maxPos = std::max(m_maxPos, m_stream.pos());

	return S_OK;
}

HRESULT OutMemStream::Seek(Int64 offset, const UInt32 seekOrigin, UInt64* newPosition) noexcept
{
	offset = seekOrigin == 1 ? m_stream.pos() + offset : seekOrigin == 2 ? m_stream.size() - offset : (assert(seekOrigin == 0), offset);
	if (offset < 0)
		return __HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
	if (offset > m_stream.size())
		SetSize(offset);

	if (!m_stream.seek(offset))
		return STG_E_INVALIDFUNCTION;

	if (newPosition)
		*newPosition = m_stream.pos();
	return S_OK;
}

HRESULT OutMemStream::SetSize(UInt64 /*newSize*/) noexcept
{
	return S_OK;
}
