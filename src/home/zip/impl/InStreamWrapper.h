#pragma once

#include "fnd/unknown_impl.h"

#include "7z-sdk/7z/CPP/7zip/IStream.h"

namespace HomeCompa::ZipDetails::SevenZip
{

class InStreamWrapper final
	: public IInStream
	, public IStreamGetSize
{
	ADD_RELEASE_REF_IMPL

public:
	static CComPtr<InStreamWrapper> Create(CComPtr<IStream> baseStream);

private:
	explicit InStreamWrapper(CComPtr<IStream> baseStream);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;

private:
	// ISequentialInStream
	STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize) override;

	// IInStream
	STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override;

	// IStreamGetSize
	STDMETHOD(GetSize)(UInt64* size) override;

private:
	CComPtr<IStream> m_baseStream;
};

} // namespace HomeCompa::ZipDetails::SevenZip
