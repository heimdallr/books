#pragma once

#include <QString>

#include "7z/CPP/7zip/Archive/IArchive.h"
#include "7z/CPP/7zip/IPassword.h"

namespace HomeCompa::ZipDetails {
class ProgressCallback;
}

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class MemExtractCallback final
	: public IArchiveExtractCallback
	, public ICryptoGetTextPassword
{
public:
	static CComPtr<MemExtractCallback> Create(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, std::shared_ptr<ProgressCallback> callback, QString password = {});

private:
	MemExtractCallback(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, std::shared_ptr<ProgressCallback> callback, QString password);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject) override;
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

private:
	// IProgress
	STDMETHOD(SetTotal)(UInt64 size) override;
	STDMETHOD(SetCompleted)(const UInt64 * completeValue) override;

	// Early exit, this is not part of any interface
	STDMETHODIMP CheckBreak() const;

	// IMemExtractCallback
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream ** outStream, Int32 askExtractMode) override;
	STDMETHOD(PrepareOperation)(Int32 askExtractMode) override;
	STDMETHOD(SetOperationResult)(Int32 resultEOperationResult) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR * password) override;

private:
	void GetPropertyFilePath(UInt32 index);
	void GetPropertyIsDir(UInt32 index);

	void EmitDoneCallback() const;
	void EmitFileDoneCallback(const QString & path = {}) const;

private:
	CComPtr<IInArchive> m_archiveHandler;
	QByteArray & m_buffer;
	std::shared_ptr<ProgressCallback> m_callback;
	QString m_password;

	CComPtr<ISequentialOutStream> m_outMemStream;
	long m_refCount { 0 };
	bool m_isDir { false };
	QString m_filePath;
};

}
