#include <atlcomcli.h>
#include <comdef.h>

#include "writer.h"

#include <plog/Log.h>

#include "fnd/unknown_impl.h"

#include "7z/CPP/7zip/Archive/IArchive.h"
#include "bit7z/bitformat.hpp"

#include "zip/interface/ProgressCallback.h"

#include "FileItem.h"
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
	static CComPtr<IArchiveUpdateCallback> Create(FileStorage & files, const std::vector<QString> & fileNames, const StreamGetter & streamGetter, ProgressCallback & progress)
	{
		return new ArchiveUpdateCallback(files, fileNames, streamGetter, progress);
	}

private:
	ArchiveUpdateCallback(FileStorage & files, const std::vector<QString> & fileNames, const StreamGetter & streamGetter, ProgressCallback & progress)
		: m_files { files }
		, m_fileNames { fileNames }
		, m_streamGetter { streamGetter }
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
	HRESULT GetUpdateItemInfo(const UInt32 index,
		Int32 * newData,
		Int32 * newProperties,
		UInt32 * indexInArchive) noexcept override
	{
		if (newData != nullptr)
			*newData = index >= m_files.files.size() ? 1 : 0; //1 = true, 0 = false;
		if (newProperties != nullptr)
			*newProperties = index >= m_files.files.size() ? 1 : 0; //1 = true, 0 = false;
		if (indexInArchive != nullptr)
			*indexInArchive = index >= m_files.files.size() ? static_cast<UInt32>(-1) : m_files.files[index].index;

		return S_OK;
	}

	HRESULT GetProperty(UInt32 index, PROPID propId, PROPVARIANT * value) noexcept override try
	{
		const auto & stream = GetStream(index);

		CPropVariant prop = [&, propId] () -> CPropVariant
		{
			switch (propId)
			{
				case kpidIsAnti:
					return false;
				case kpidAttrib:
					return uint32_t { 128 };
				case kpidPath:
					return m_fileNames[index - m_files.files.size()].toStdWString();
				case kpidIsDir:
					return false;
				case kpidMTime:
					return FILETIME {};
				case kpidComment:
					return {};
				case kpidSize:
					return static_cast<uint64_t>(stream.size());
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
	}

	HRESULT GetStream(const UInt32 index, ISequentialInStream ** inStream) noexcept override
	{
		if (m_progress.OnCheckBreak())
			return E_ABORT;

		auto inStreamLoc = SequentialInStream::Create(GetStream(index));
		*inStream = inStreamLoc.Detach();
		return S_OK;
	}

	HRESULT SetOperationResult(Int32 /* operationResult */) noexcept override
	{
		//		mNeedBeClosed = true;
		return S_OK;
	}

private:
	QIODevice& GetStream(const UInt32 index)
	{
		if (m_streams.size() <= index)
			m_streams.resize(index + 1);

		if (!m_streams[index])
			m_streams[index] = m_streamGetter(index - m_files.files.size());

		if (!m_streams[index]->isOpen())
			m_streams[index]->open(QIODevice::ReadOnly);

		return *m_streams[index];
	}

private:
	FileStorage & m_files;
	const std::vector<QString> & m_fileNames;
	const StreamGetter & m_streamGetter;
	ProgressCallback & m_progress;
	std::vector<std::unique_ptr<QIODevice>> m_streams;
};

}

namespace File {

bool Write(FileStorage & files, IOutArchive & zip, QIODevice & oStream, const std::vector<QString> & fileNames, const StreamGetter & streamGetter, ProgressCallback & progress)
{
	ProgressCallbackStub progressCallbackStub;
	auto sequentialOutStream = OutMemStream::Create(oStream, progressCallbackStub);
	auto archiveUpdateCallback = ArchiveUpdateCallback::Create(files, fileNames, streamGetter, progress);
	const auto result = zip.UpdateItems(std::move(sequentialOutStream), static_cast<UInt32>(files.files.size() + fileNames.size()), std::move(archiveUpdateCallback)) == S_OK;
	progress.OnDone();
	return result;
}

}

}
