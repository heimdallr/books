#include <atlcomcli.h>
#include <comdef.h>

#include "writer.h"

#include <QIODevice>
#include <QString>

#include <stdexcept>
#include <7z/CPP/7zip/Archive/IArchive.h>
#include <plog/Log.h>

#include "PropVariant.h"
#include "zip/interface/file.h"
#include "zip/interface/stream.h"
#include "bit7z/bitformat.hpp"

using namespace bit7z;

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

class Writer
	: public QIODevice
	, ISequentialOutStream
	, IArchiveUpdateCallback
	, ISequentialInStream
{
public:
	Writer(IOutArchive & zip, std::ostream & oStream, QString filename, ProgressCallback & progress)
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
		m_filedata = data;
		m_filesize = static_cast<uint64_t>(len);
		m_zip.UpdateItems(this, 1, this);
		return len;
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

		if (iid == IID_ISequentialInStream)
		{
			*ppvObject = static_cast<ISequentialInStream *>(this);
			AddRef();
			return S_OK;
		}

		if (iid == IID_ISequentialOutStream)
		{
			*ppvObject = static_cast<ISequentialOutStream *>(this);
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

	ULONG AddRef() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
	}

	ULONG Release() override
	{
		return static_cast<ULONG>(InterlockedDecrement(&m_refCount));
	}

private: // ISequentialOutStream
	HRESULT Write(const void * data, const UInt32 size, UInt32 * processedSize) noexcept override
	{
		if (processedSize != nullptr)
			*processedSize = 0;

		if (data == nullptr || size == 0)
			return E_FAIL;

		const auto * byteData = static_cast<const char *>(data); //-V2571
		m_oStream.write(byteData, size);

		if (processedSize != nullptr)
			*processedSize = size;

		return S_OK;
	}

private: // IProgress
	HRESULT SetTotal(UInt64 /*size*/) noexcept override
	{
//		if (mHandler.totalCallback())
//		{
//			mHandler.totalCallback()(size);
//		}
		return S_OK;
	}

	HRESULT SetCompleted(const UInt64 * /*completeValue*/) noexcept override
	{
//		if (completeValue != nullptr && mHandler.progressCallback())
//		{
//			return mHandler.progressCallback()(*completeValue) ? S_OK : E_ABORT;
//		}
		return S_OK;
	}

private: // IArchiveUpdateCallback
	HRESULT GetUpdateItemInfo(UInt32 /*index*/,
			Int32 * newData,
			Int32 * newProperties,
			UInt32 * indexInArchive) noexcept override
	{
		if (newData != nullptr)
		{
			*newData = 1; //1 = true, 0 = false;
		}
		if (newProperties != nullptr)
		{
			*newProperties = 1; //1 = true, 0 = false;
		}
		if (indexInArchive != nullptr)
		{
			*indexInArchive = static_cast<UInt32>(-1);
		}

		return S_OK;
	}

	HRESULT GetProperty(UInt32 /*index*/, PROPID propId, PROPVARIANT * value) noexcept override try
	{
		CPropVariant prop = [this, propId]() -> CPropVariant
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
				return m_filesize;
			default:
				return {};
		}
		}();
//		if (propId == kpidIsAnti)
//		{
//			prop = false;
//		}
//		else
//		{
//			const auto property = static_cast<BitProperty>(propId);
//			if (mOutputArchive.creator().storeSymbolicLinks() || property != BitProperty::SymLink)
//			{
//				prop = mOutputArchive.outputItemProperty(index, property);
//			}
//		}
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
		//		RINOK(finalize())
		//
		//			if (mHandler.fileCallback())
		//			{
		//				const BitPropVariant filePath = mOutputArchive.outputItemProperty(index, BitProperty::Path);
		//				if (filePath.isString())
		//				{
		//					mHandler.fileCallback()(filePath.getString());
		//				}
		//			}
		//
		//		return mOutputArchive.outputItemStream(index, inStream);
		return QueryInterface(IID_ISequentialInStream, reinterpret_cast<void **>(inStream));
	}

	HRESULT SetOperationResult(Int32 /* operationResult */) noexcept override
	{
		//		mNeedBeClosed = true;
		return S_OK;
	}

private:
	HRESULT Read(void * data, const UInt32 size, UInt32 * processedSize) noexcept override
	{
		if (size == 0)
		{
			if (processedSize)
				*processedSize = 0;
			return S_OK;
		}

		const auto realSize = std::min(static_cast<ptrdiff_t>(size), static_cast<ptrdiff_t>(m_filesize) - m_position);
		std::copy_n(m_filedata + m_position, realSize, reinterpret_cast<char *>(data));
		m_position += realSize;
		if (processedSize)
			*processedSize = static_cast<UInt32>(realSize);

		return S_OK;
	}

private:
	IOutArchive & m_zip;
	std::ostream & m_oStream;
	QString m_filename;
	ProgressCallback & m_progress;

	ptrdiff_t m_position { 0 };
	const char * m_filedata { nullptr };
	uint64_t m_filesize { 0 };

	LONG m_refCount { 1 };
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
	FileWriter(IOutArchive & zip, std::ostream & oStream, QString filename, ProgressCallback & progress)
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
	std::ostream & m_oStream;
	QString m_filename;
	ProgressCallback & m_progress;
};

}

namespace File {

std::unique_ptr<IFile> Write(IOutArchive & zip, std::ostream & oStream, QString filename, ProgressCallback & progress)
{
	return std::make_unique<FileWriter>(zip, oStream, std::move(filename), progress);
}

}

}
