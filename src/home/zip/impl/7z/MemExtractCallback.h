#pragma once

#include <vector>

#include <QString>

#include "7z/CPP/7zip/Archive/IArchive.h"
#include "7z/CPP/7zip/IPassword.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

class ProgressCallback;

class MemExtractCallback final
	: public IArchiveExtractCallback
	, public ICryptoGetTextPassword
{
public:
	static CComPtr<MemExtractCallback> Create(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, QString password = {}, ProgressCallback * callback = nullptr);

private:
	MemExtractCallback(CComPtr<IInArchive> archiveHandler, QByteArray & buffer, QString password, ProgressCallback * callback);

public:
	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject) override;
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

private:
	// IProgress
	STDMETHOD(SetTotal)(UInt64 size) override;
	STDMETHOD(SetCompleted)(const UInt64 * completeValue) override;

	// Early exit, this is not part of any interface
	STDMETHOD(CheckBreak)();

	// IMemExtractCallback
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream ** outStream, Int32 askExtractMode) override;
	STDMETHOD(PrepareOperation)(Int32 askExtractMode) override;
	STDMETHOD(SetOperationResult)(Int32 resultEOperationResult) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR * password) override;

private:
	void GetPropertyFilePath(UInt32 index);
	void GetPropertyIsDir(UInt32 index);
	void GetPropertySize(UInt32 index);

	void EmitDoneCallback() const;
	void EmitFileDoneCallback(const QString & path = {}) const;

private:
	long m_refCount { 0 };

	CComPtr<IInArchive> m_archiveHandler;
	CComPtr<ISequentialOutStream> m_outMemStream;
	QByteArray & m_buffer;

	QString m_password;

	bool m_isDir { false };
	bool m_hasNewFileSize { false };
	UInt64 m_newFileSize { 0 };
	QString m_filePath;

	ProgressCallback * m_callback;
	std::unique_ptr<ProgressCallback> m_callbackStub;
};

}
