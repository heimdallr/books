#pragma once

#include <memory>
#include <vector>

#include "fnd/unknown_impl.h"

#include "7z-sdk/7z/CPP/7zip/IStream.h"

class QIODevice;

namespace HomeCompa::ZipDetails
{
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::SevenZip
{

class OutMemStream final : public IOutStream
{
	ADD_RELEASE_REF_IMPL

public:
	static CComPtr<ISequentialOutStream> Create(QIODevice& stream, ProgressCallback& progress);

private:
	explicit OutMemStream(QIODevice& stream, ProgressCallback& progress);

public:
	~OutMemStream();

private: // IUnknown
	HRESULT QueryInterface(REFIID iid, void** ppvObject) override;

	// ISequentialOutStream
	HRESULT Write(const void* data, UInt32 size, UInt32* processedSize) noexcept override;
	HRESULT Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) noexcept override;
	HRESULT SetSize(UInt64 newSize) noexcept override;

private:
	QIODevice&        m_stream;
	ProgressCallback& m_progress;
	int64_t           m_maxPos { 0 };
};

} // namespace HomeCompa::ZipDetails::SevenZip
