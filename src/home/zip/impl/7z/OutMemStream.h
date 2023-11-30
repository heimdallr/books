#pragma once

#include <memory>
#include <vector>

#include <7z/CPP/7zip/IStream.h>

class QByteArray;

namespace HomeCompa::ZipDetails {
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class OutMemStream final
	: public ISequentialOutStream
{
public:
	static CComPtr<ISequentialOutStream> Create(QByteArray & buffer, std::shared_ptr<ProgressCallback> progress);

private:
	explicit OutMemStream(QByteArray & buffer, std::shared_ptr<ProgressCallback> progress);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject) override;

private:
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

	// ISequentialOutStream
	STDMETHOD(Write)(const void * data, UInt32 size, UInt32 * processedSize) override;

private:
	long m_refCount { 0 };
	QByteArray & m_buffer;
	std::shared_ptr<ProgressCallback> m_progress;
};
}
