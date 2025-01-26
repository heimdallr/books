#include <atlcomcli.h>
#include <comdef.h>

#include "writer.h"

#include <QBuffer>
#include <QIODevice>
#include <QString>

#include <plog/Log.h>

#include "fnd/unknown_impl.h"

#include "7z/CPP/7zip/Archive/IArchive.h"
#include "bit7z/bitformat.hpp"

#include "zip/interface/file.h"
#include "zip/interface/stream.h"
#include "zip/interface/ProgressCallback.h"

#include "OutMemStream.h"
#include "PropVariant.h"

using namespace bit7z;

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

class SequentialInStream final : public ISequentialInStream
{
	UNKNOWN_IMPL(ISequentialInStream)

public:
	static CComPtr<ISequentialInStream> Create(QIODevice & stream)
	{
		return new SequentialInStream(stream);
	}

private:
	explicit SequentialInStream(QIODevice & inStream)
		: m_inStream{ inStream }
	{
	}

private: // ISequentialInStream
	HRESULT Read(void * data, const UInt32 size, UInt32 * processedSize) noexcept override
	{
		if (size == 0)
		{
			if (processedSize)
				*processedSize = 0;
			return S_OK;
		}

		const auto realSize = m_inStream.read(reinterpret_cast<char *>(data), size);
		if (processedSize)
			*processedSize = static_cast<UInt32>(realSize);

		return S_OK;
	}
private:
	QIODevice & m_inStream;
};

class ArchiveUpdateCallback : public IArchiveUpdateCallback
{
	ADD_RELEASE_REF_IMPL
public:
	static CComPtr<IArchiveUpdateCallback> Create(QIODevice & inStream, QString filename, ProgressCallback & progress)
	{
		return new ArchiveUpdateCallback(inStream, std::move(filename), progress);
	}

private:
	ArchiveUpdateCallback(QIODevice & inStream, QString filename, ProgressCallback & progress)
		: m_inStream { inStream }
		, m_filename { std::move(filename) }
		, m_progress { progress }
	{
	}

private: // IUnknown
	HRESULT QueryInterface(REFIID iid, void ** ppvObject) override//-V835
	{
		if (iid == __uuidof(IUnknown))  // NOLINT(clang-diagnostic-language-extension-token)
		{
			*ppvObject = reinterpret_cast<IUnknown *>(this);  // NOLINT(clang-diagnostic-reinterpret-base-class)
			AddRef();
			return S_OK;
		}

		if (iid == IID_IArchiveUpdateCallback)
		{
			*ppvObject = static_cast<IArchiveUpdateCallback *>(this);
			AddRef();
			return S_OK;
		}

		return E_NOINTERFACE;
	}

private: // IProgress
	HRESULT SetTotal(const UInt64 size) noexcept override
	{
		m_progress.OnStartWithTotal(static_cast<int64_t>(size));
		return S_OK;
	}

	HRESULT SetCompleted(const UInt64 * completeValue) noexcept override
	{
		if (m_progress.OnCheckBreak())
			return E_ABORT;

		if (completeValue)
			m_progress.OnSetCompleted(static_cast<int64_t>(*completeValue));
		return S_OK;
	}

private: // IArchiveUpdateCallback
	HRESULT GetUpdateItemInfo(UInt32 /*index*/,
		Int32 * newData,
		Int32 * newProperties,
		UInt32 * indexInArchive) noexcept override
	{
		if (newData != nullptr)
			*newData = 1; //1 = true, 0 = false;
		if (newProperties != nullptr)
			*newProperties = 1; //1 = true, 0 = false;
		if (indexInArchive != nullptr)
			*indexInArchive = static_cast<UInt32>(-1);

		return S_OK;
	}

	HRESULT GetProperty(UInt32 /*index*/, PROPID propId, PROPVARIANT * value) noexcept override try
	{
		CPropVariant prop = [this, propId] () -> CPropVariant
		{
			switch (propId)
			{
				case kpidIsAnti:
					return false;
				case kpidAttrib:
					return uint32_t { 128 };
				case kpidPath:
					return m_filename.toStdWString();
				case kpidIsDir:
					return false;
				case kpidMTime:
					return FILETIME {};
				case kpidComment:
					return {};
				case kpidSize:
					return static_cast<uint64_t>(m_inStream.size());
				default:
					return {};
			}
		}();

		*value = prop;
		prop.bstrVal = nullptr;
		return S_OK;
	}
	catch (const std::exception & ex)
	{
		PLOGE << ex.what();
		return S_FALSE;
		//		return ex.hresultCode();
	}

	HRESULT GetStream(UInt32 /*index*/, ISequentialInStream ** inStream) noexcept override
	{
		if (m_progress.OnCheckBreak())
			return E_ABORT;

		auto inStreamLoc = SequentialInStream::Create(m_inStream);
		*inStream = inStreamLoc.Detach();
		return S_OK;
	}

	HRESULT SetOperationResult(Int32 /* operationResult */) noexcept override
	{
		//		mNeedBeClosed = true;
		return S_OK;
	}

private:
	QIODevice & m_inStream;
	const QString m_filename;
	ProgressCallback & m_progress;
};

class Writer
	: public QIODevice
{
public:
	Writer(IOutArchive & zip, QIODevice & oStream, QString filename, ProgressCallback & progress)
		: m_zip { zip }
		, m_oStream { oStream }
		, m_filename { std::move(filename) }
		, m_progress { progress }
	{
	}

private: // QIODevice
	qint64 readData(char * /*data*/, qint64 /*maxLen*/) override
	{
		assert(false);
		return 0;
	}

	qint64 writeData(const char * data, const qint64 len) override
	{
		QByteArray bytes(data, len);
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::ReadOnly);
		ProgressCallbackStub progressCallbackStub;
		auto sequentialOutStream = OutMemStream::Create(m_oStream, progressCallbackStub);
		auto archiveUpdateCallback = ArchiveUpdateCallback::Create(buffer, m_filename, m_progress);
		m_zip.UpdateItems(std::move(sequentialOutStream), 1, std::move(archiveUpdateCallback));
		m_progress.OnDone();
		return len;
	}

private:
	IOutArchive & m_zip;
	QIODevice & m_oStream;
	QString m_filename;
	ProgressCallback & m_progress;
};

class StreamImpl : public Stream
{
public:
	explicit StreamImpl(std::unique_ptr<QIODevice> ioDevice)
		: m_ioDevice(std::move(ioDevice))
	{
		m_ioDevice->open(QIODevice::WriteOnly);
	}

private: // Stream
	QIODevice & GetStream() override
	{
		return *m_ioDevice;
	}

private:
	std::unique_ptr<QIODevice> m_ioDevice;
};

class FileWriter final : virtual public IFile
{
public:
	FileWriter(IOutArchive & zip, QIODevice & oStream, QString filename, ProgressCallback & progress)
		: m_zip { zip }
		, m_oStream { oStream }
		, m_filename { std::move(filename) }
		, m_progress { progress }
	{
	}

private: // IFile
	std::unique_ptr<Stream> Read() override
	{
		throw std::runtime_error("Cannot read with writer");
	}

	std::unique_ptr<Stream> Write() override
	{
		return std::make_unique<StreamImpl>(std::make_unique<Writer>(m_zip, m_oStream, m_filename, m_progress));
	}

private:
	IOutArchive & m_zip;
	QIODevice & m_oStream;
	QString m_filename;
	ProgressCallback & m_progress;
};

}

namespace File {

std::unique_ptr<IFile> Write(IOutArchive & zip, QIODevice & oStream, QString filename, ProgressCallback & progress)
{
	return std::make_unique<FileWriter>(zip, oStream, std::move(filename), progress);
}

}

}
