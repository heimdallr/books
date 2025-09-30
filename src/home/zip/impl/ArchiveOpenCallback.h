#pragma once

#include "win.h"

#include <QString>

#include "7z-sdk/7z/CPP/7zip/Archive/IArchive.h"
#include "7z-sdk/7z/CPP/7zip/IPassword.h"

namespace HomeCompa::ZipDetails::SevenZip
{

class ArchiveOpenCallback final
	: public IArchiveOpenCallback
	, public ICryptoGetTextPassword
{
public:
	static CComPtr<ArchiveOpenCallback> Create(QString password = {});

private:
	explicit ArchiveOpenCallback(QString password);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

private:
	// IArchiveOpenCallback
	STDMETHOD(SetTotal)(const UInt64* files, const UInt64* bytes) override;
	STDMETHOD(SetCompleted)(const UInt64* files, const UInt64* bytes) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR* password) override;

private:
	LONG    m_refCount { 0 };
	QString m_password;
};

} // namespace HomeCompa::ZipDetails::SevenZip
