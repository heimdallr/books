#pragma once

#include <memory>
#include <vector>

#include "7z/CPP/7zip/IStream.h"
#include "fnd/unknown_impl.h"

class QIODevice;

namespace HomeCompa::ZipDetails {
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class OutMemStream final
	: public ISequentialOutStream
{
	UNKNOWN_IMPL(ISequentialOutStream)

public:
	static CComPtr<ISequentialOutStream> Create(QIODevice & stream, ProgressCallback & progress);

private:
	explicit OutMemStream(QIODevice & stream, ProgressCallback & progress);

	// ISequentialOutStream
	STDMETHOD(Write)(const void * data, UInt32 size, UInt32 * processedSize) override;

private:
	QIODevice & m_stream;
	ProgressCallback & m_progress;
};
}
