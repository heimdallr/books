#pragma once

#include <vector>

#include <7z/CPP/7zip/IStream.h>

class QByteArray;

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class OutMemStream final
	: public ISequentialOutStream
{
public:
	static CComPtr<ISequentialOutStream> Create(QByteArray & buffer);

private:
	explicit OutMemStream(QByteArray & buffer);

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
};
}
