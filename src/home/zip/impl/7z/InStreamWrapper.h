#pragma once

#include "7z/CPP/7zip/IStream.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class InStreamWrapper final
	: public IInStream
	, public IStreamGetSize
{
public:
	static CComPtr<InStreamWrapper> Create(CComPtr<IStream> baseStream);

private:
	explicit InStreamWrapper(CComPtr<IStream> baseStream);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject) override;
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

private:
	// ISequentialInStream
	STDMETHOD(Read)(void * data, UInt32 size, UInt32 * processedSize) override;

	// IInStream
	STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 * newPosition) override;

	// IStreamGetSize
	STDMETHOD(GetSize)(UInt64 * size) override;

private:
	long m_refCount { 0 };
	CComPtr<IStream> m_baseStream;
};

}
