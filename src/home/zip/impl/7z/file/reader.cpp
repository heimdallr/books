#include <atlcomcli.h>
#include <comdef.h>

#include "reader.h"

#include <QBuffer>

#include "7z/CPP/7zip/Archive/IArchive.h"
#include "7z/CPP/7zip/IPassword.h"

#include "zip/interface/file.h"
#include "zip/interface/ProgressCallback.h"
#include "zip/interface/stream.h"

#include "PropVariant.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

constexpr auto EMPTY_FILE_ALIAS = "[Content]";

class StreamImpl final
	: public Stream
	, public IArchiveExtractCallback
	, public ICryptoGetTextPassword
	, public ISequentialOutStream
{
public:
	StreamImpl(IInArchive & zip, const FileItem & fileItem, ProgressCallback & progress)
		: m_zip { zip }
		, m_fileItem { fileItem }
		, m_progress { progress }
	{
		m_progress.OnStartWithTotal(static_cast<int64_t>(m_fileItem.size));

		const UInt32 indices[] = { fileItem.n };
		m_zip.Extract(indices, 1, 0, this);
		m_progress.OnDone();
	}

private: // Stream
	QIODevice & GetStream() override
	{
		m_buffer = std::make_unique<QBuffer>(&m_bytes);
		m_buffer->open(QIODevice::ReadOnly);
		return *m_buffer;
	}

private: // IUnknown
	HRESULT QueryInterface(REFIID iid, void ** ppvObject) override//-V835
	{
		if (iid == __uuidof(IUnknown))  // NOLINT(clang-diagnostic-language-extension-token)
		{
			*ppvObject = reinterpret_cast<IUnknown *>(this);
			AddRef();
			return S_OK;
		}

		if (iid == IID_IArchiveExtractCallback)
		{
			*ppvObject = static_cast<IArchiveExtractCallback *>(this);
			AddRef();
			return S_OK;
		}

		if (iid == IID_ICryptoGetTextPassword)
		{
			*ppvObject = static_cast<ICryptoGetTextPassword *>(this);
			AddRef();
			return S_OK;
		}

		if (iid == IID_ISequentialOutStream)
		{
			*ppvObject = static_cast<ISequentialOutStream *>(this);
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	ULONG AddRef() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
	}

	ULONG Release() override
	{
		return static_cast<ULONG>(InterlockedDecrement(&m_refCount));
	}

private: // IProgress
	HRESULT SetTotal(const UInt64 /*size*/) noexcept override
	{
		// SetTotal is never called for ZIP and 7z formats
		return CheckBreak();
	}

	HRESULT SetCompleted(const UInt64 * /*completeValue*/) noexcept override
	{
		//Callback Event calls
		/*
		NB:
		- For ZIP format SetCompleted only called once per 1000 files in central directory and once per 100 in local ones.
		- For 7Z format SetCompleted is never called.
		*/
		return CheckBreak();
	}

private: // IArchiveExtractCallback
	HRESULT GetStream(const UInt32 index, ISequentialOutStream ** outStream, const Int32 askExtractMode) noexcept override
	{
		try
		{
			// Retrieve all the various properties for the file at this index.
			GetPropertyFilePath(index);
			if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
				return S_OK;

			GetPropertyIsDir(index);
		}
		catch (_com_error & ex)
		{
			return ex.Error();
		}

		if (!m_isDir)
			QueryInterface(IID_ISequentialOutStream, reinterpret_cast<void **>(outStream));

		return CheckBreak();
	}

	HRESULT PrepareOperation(Int32 /*askExtractMode*/) noexcept override
	{
		return S_OK;
	}

	HRESULT SetOperationResult(const Int32 operationResult) noexcept override
	{
		if (operationResult != NArchive::NExtract::NOperationResult::kOK)
			return E_FAIL;

		EmitFileDoneCallback();
		return CheckBreak();
	}

private: // ICryptoGetTextPassword
	HRESULT CryptoGetTextPassword(BSTR * password) noexcept override
	{
		if (!m_password.isEmpty())
			*password = SysAllocString(m_password.toStdWString().data());

		return S_OK;
	}


private: // ISequentialOutStream
	HRESULT Write(const void * data, const UInt32 size, UInt32 * processedSize) noexcept override
	{
		if (processedSize)
			*processedSize = 0;

		if (!data || size == 0)
			return E_FAIL;

		if (m_progress.OnCheckBreak())
			return E_ABORT;

		const auto * byte_data = static_cast<const char *>(data);
		m_bytes.append(byte_data, size);
		if (processedSize)
			*processedSize = size;

		m_progress.OnIncrement(size);

		return S_OK;
	}
private:
	HRESULT CheckBreak() const
	{
		return m_progress.OnCheckBreak() ? E_ABORT : S_OK;
	}

	void GetPropertyFilePath(const UInt32 index)
	{
		CPropVariant prop;
		const HRESULT hr = m_zip.GetProperty(index, kpidPath, &prop);
		if (hr != S_OK)
			_com_issue_error(hr);

		if (prop.vt == VT_EMPTY)
			m_filePath = EMPTY_FILE_ALIAS;
		else if (prop.vt != VT_BSTR)
			_com_issue_error(E_FAIL);
		else
			m_filePath = QString::fromStdWString(prop.bstrVal);
	}

	void GetPropertyIsDir(const UInt32 index)
	{
		CPropVariant prop;
		const HRESULT hr = m_zip.GetProperty(index, kpidIsDir, &prop);
		if (hr != S_OK)
			_com_issue_error(hr);

		if (prop.vt == VT_EMPTY)
			m_isDir = false;
		else if (prop.vt != VT_BOOL)
			_com_issue_error(E_FAIL);
		else
			m_isDir = prop.boolVal != VARIANT_FALSE;
	}

	void EmitFileDoneCallback(const QString & path = {}) const
	{
		m_progress.OnFileDone(path);
	}

private:
	IInArchive & m_zip;
	const FileItem & m_fileItem;
	ProgressCallback & m_progress;
	QByteArray m_bytes;
	std::unique_ptr<QBuffer> m_buffer;
	LONG m_refCount { 1 };
	QString m_password;
	bool m_isDir { false };
	QString m_filePath;
};

class FileReader final : virtual public IFile
{
public:
	FileReader(IInArchive & zip, const FileItem & fileItem, ProgressCallback & progress)
		: m_zip { zip }
		, m_fileItem { fileItem }
		, m_progress { progress }
	{
	}

private: // IFile
	std::unique_ptr<Stream> Read() override
	{
		return std::make_unique<StreamImpl>(m_zip, m_fileItem, m_progress);
	}

	std::unique_ptr<Stream> Write() override
	{
		throw std::runtime_error("Cannot write with reader");
	}

private:
	IInArchive & m_zip;
	const FileItem & m_fileItem;
	ProgressCallback & m_progress;
};

}

namespace File {

std::unique_ptr<IFile> Read(IInArchive & zip, const FileItem & fileItem, ProgressCallback & progress)
{
	return std::make_unique<FileReader>(zip, fileItem, progress);
}

}

}
