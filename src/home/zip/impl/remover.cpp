#include "win.h"

#include <comdef.h>

#include <QRegularExpression>

#include "fnd/unknown_impl.h"

#include "7z-sdk/7z/CPP/7zip/Archive/IArchive.h"
#include "7z-sdk/7z/CPP/7zip/IPassword.h"
#include "bit7z/bitformat.hpp"
#include "zip/interface/ProgressCallback.h"

#include "FileItem.h"
#include "OutMemStream.h"
#include "PropVariant.h"
#include "log.h"
#include "writer.h"

using namespace bit7z;

namespace HomeCompa::ZipDetails::SevenZip
{

namespace
{

class CryptoGetTextPassword final : public ICryptoGetTextPassword2
{
	UNKNOWN_IMPL(ICryptoGetTextPassword2) //-V835
public:
	static CComPtr<ICryptoGetTextPassword2> Create()
	{
		return new CryptoGetTextPassword();
	}

private: // ICryptoGetTextPassword2
	HRESULT CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password) noexcept override
	{
		*passwordIsDefined = 0;
		*password          = SysAllocString(L"");
		return S_OK;
	}
};

class ArchiveExtractCallbackMessage final : public IArchiveExtractCallbackMessage2
{
	UNKNOWN_IMPL(IArchiveExtractCallbackMessage2) //-V835

public:
	static CComPtr<IArchiveExtractCallbackMessage2> Create()
	{
		return new ArchiveExtractCallbackMessage();
	}

private: // IArchiveExtractCallbackMessage2
	HRESULT ReportExtractResult(UInt32 /*indexType*/, UInt32 /*index*/, Int32 /*opRes*/) noexcept override
	{
		return S_OK;
	}
};

class ArchiveUpdateCallback : public IArchiveUpdateCallback
{
	ADD_RELEASE_REF_IMPL
public:
	static CComPtr<IArchiveUpdateCallback> Create(FileStorage& files, ProgressCallback& progress)
	{
		return new ArchiveUpdateCallback(files, progress);
	}

private:
	ArchiveUpdateCallback(FileStorage& files, ProgressCallback& progress)
		: m_files { files }
		, m_progress { progress }
	{
	}

private: // IUnknown
	HRESULT QueryInterface(REFIID iid, void** ppvObject) override //-V835
	{
		if (iid == __uuidof(IUnknown)) // NOLINT(clang-diagnostic-language-extension-token)
		{
			*ppvObject = reinterpret_cast<IUnknown*>(this); // NOLINT(clang-diagnostic-reinterpret-base-class)
			AddRef();
			return S_OK;
		}

		if (iid == IID_IArchiveUpdateCallback)
		{
			*ppvObject = static_cast<IArchiveUpdateCallback*>(this);
			AddRef();
			return S_OK;
		}

		if (iid == IID_ICryptoGetTextPassword2)
		{
			auto obj   = CryptoGetTextPassword::Create();
			*ppvObject = obj.Detach();
			return S_OK;
		}

		if (iid == IID_IArchiveExtractCallbackMessage2)
		{
			auto obj   = ArchiveExtractCallbackMessage::Create();
			*ppvObject = obj.Detach();
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

	HRESULT SetCompleted(const UInt64* completeValue) noexcept override
	{
		if (m_progress.OnCheckBreak())
			return E_ABORT;

		if (completeValue)
			m_progress.OnSetCompleted(static_cast<int64_t>(*completeValue));
		return S_OK;
	}

private: // IArchiveUpdateCallback
	HRESULT GetUpdateItemInfo(const UInt32 indexSrc, Int32* newData, Int32* newProperties, UInt32* indexInArchive) noexcept override
	{
		const auto index = static_cast<size_t>(indexSrc);
		if (newData != nullptr)
			*newData = 0; //1 = true, 0 = false;
		if (newProperties != nullptr)
			*newProperties = 0; //1 = true, 0 = false;
		if (indexInArchive != nullptr)
		{
			assert(index < m_files.files.size());
			*indexInArchive = m_files.files[index].index;
		}

		return S_OK;
	}

	HRESULT GetProperty(UInt32 /*index*/, PROPID propId, PROPVARIANT* value) noexcept override
	try
	{
		CPropVariant prop = [&, propId]() -> CPropVariant {
			switch (propId)
			{
				case kpidIsAnti:
				case kpidIsDir:
					return false;
				case kpidAttrib:
					return uint32_t { 128 };
				case kpidMTime:
					return FILETIME {};
				case kpidPath: // return m_fileNames[index - m_files.files.size()].toStdWString();
				case kpidSize: // return m_sizeGetter(index - m_files.files.size());
				case kpidComment:
				default:
					return {};
			}
		}();

		*value       = prop;
		prop.bstrVal = nullptr;
		return S_OK;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
		return S_FALSE;
	}

	HRESULT GetStream(UInt32 /*index*/, ISequentialInStream** /*inStream*/) noexcept override
	{
		if (m_progress.OnCheckBreak())
			return E_ABORT;

		assert(false && "unexpected call");
		return S_OK;
	}

	HRESULT SetOperationResult(Int32 /* operationResult */) noexcept override
	{
		//		mNeedBeClosed = true;
		return S_OK;
	}

private:
	FileStorage&      m_files;
	ProgressCallback& m_progress;
};

} // namespace

namespace File
{

bool Remove(FileStorage& files, IOutArchive& zip, QIODevice& oStream, const std::vector<QString>& fileNames, ProgressCallback& progress)
{
	for (const auto& fileName : fileNames)
	{
		const auto rx = QRegularExpression::fromWildcard(fileName, Qt::CaseInsensitive);
		std::erase_if(files.files, [&](const auto& file) {
			return rx.match(file.name).hasMatch();
		});
	}

	if (files.files.size() == files.index.size())
		return true;

	ProgressCallbackStub progressCallbackStub;
	auto                 sequentialOutStream   = OutMemStream::Create(oStream, progressCallbackStub);
	auto                 archiveUpdateCallback = ArchiveUpdateCallback::Create(files, progress);
	const auto           result                = zip.UpdateItems(std::move(sequentialOutStream), static_cast<UInt32>(files.files.size()), std::move(archiveUpdateCallback));
	files.index.clear();
	for (size_t i = 0, sz = files.files.size(); i < sz; ++i)
	{
		files.files[i].index = static_cast<decltype(FileItem::index)>(i);
		files.index.try_emplace(files.files[i].name, i);
	}
	progress.OnDone();
	return result == S_OK;
}

} // namespace File

} // namespace HomeCompa::ZipDetails::SevenZip
