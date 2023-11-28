#include "archive.h"

#include <ranges>

#include <atlcomcli.h>
#include <Windows.h>
#include <Shlwapi.h>

#include "7z/CPP/7zip/Archive/IArchive.h"

#include "fnd/ScopedCall.h"

#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "zip/interface/error.h"

#include "ArchiveOpenCallback.h"
#include "file.h"
#include "id.h"
#include "InStreamWrapper.h"
#include "lib.h"
#include "PropVariant.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

auto CreateStream(const QString & filename)
{
	CComPtr<IStream> fileStream;
	if (FAILED(SHCreateStreamOnFileEx(filename.toStdWString().data(), STGM_READ, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &fileStream)))
		Error::CannotOpenFile(filename);
	return fileStream;
}

CComPtr<IInArchive> GetArchiveReader(const Lib & lib, const Format format)
{
	const GUID * guid = GetCompressionGUID(format);

	CComPtr<IInArchive> archive;
	lib.CreateObject(*guid, IID_IInArchive, reinterpret_cast<void **>(&archive));
	return archive;
}

CComPtr<IInArchive> CreateInputArchiveImpl(const Lib & lib, CComPtr<IStream> stream, const Format format)
{
	auto archive = GetArchiveReader(lib, format);
	if (!archive)
		return {};

	stream->Seek({}, STREAM_SEEK_SET, nullptr);
	const auto file = InStreamWrapper::Create(std::move(stream));
	const auto openCallback = ArchiveOpenCallback::Create();

	if (const auto hr = archive->Open(file, nullptr, openCallback); hr == S_OK)
		return archive;

	archive->Close();
	return {};
}

CComPtr<IInArchive> DetectFormatAndCreateInputArchiveImpl(const Lib & lib, const CComPtr<IStream> & stream, Format & format)
{
	for (static constexpr Format formats[]
		{
#define ARCHIVE_FORMAT_ITEM(NAME) Format::NAME,
		ARCHIVE_FORMAT_ITEMS_X_MACRO
#undef  ARCHIVE_FORMAT_ITEM
		}; const auto f : formats)
	{
		if (auto archive = CreateInputArchiveImpl(lib, stream, f))
		{
			format = f;
			return archive;
		}
	}

	return {};
}

CComPtr<IInArchive> CreateInputArchive(const Lib & lib, const QString & filename, Format & format)
{
	auto stream = CreateStream(filename);
	if (auto archive = format == Unknown
		? DetectFormatAndCreateInputArchiveImpl(lib, stream, format)
		: CreateInputArchiveImpl(lib, std::move(stream), format))
		return archive;

	Error::CannotOpenArchive(filename);
}

class Reader final : virtual public IZip
{
	using Files = std::unordered_map<QString, uint32_t>;

public:
	explicit Reader(QString filename)
		: m_filename(std::move(filename))
	{
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		CreateFileList();
		QStringList files;
		files.reserve(static_cast<int>(m_files.size()));
		std::ranges::copy(m_files | std::views::keys, std::back_inserter(files));
		return files;
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		CreateFileList();
		const auto it = m_files.find(filename);
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		auto archive = CreateInputArchive(m_lib, m_filename, m_format);
		return File::Read(std::move(archive), it->second);
	}

	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		assert(false && "Cannot write with reader");
		return {};
	}

private:
	void CreateFileList() const
	{
		if (!m_files.empty())
			return;

		const auto archive = CreateInputArchive(m_lib, m_filename, m_format);
		ScopedCall archiveGuard([&] { archive->Close(); });

		UInt32 numItems = 0;
		archive->GetNumberOfItems(&numItems);
		for (UInt32 i = 0; i < numItems; i++)
		{
			CPropVariant prop;
			archive->GetProperty(i, kpidIsDir, &prop);
			if (prop.boolVal != VARIANT_FALSE)
				continue;

			// Get name of file
			archive->GetProperty(i, kpidPath, &prop);
			if (prop.vt == VT_BSTR)
				m_files.emplace(QString::fromStdWString(prop.bstrVal), i);
		}
	}

private:
	QString m_filename;
	Lib m_lib;
	mutable Files m_files;
	mutable Format m_format { Unknown };
};

}


std::unique_ptr<IZip> Archive::CreateReader(const QString & filename)
{
	return std::make_unique<Reader>(filename);
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & /*filename*/)
{
	return nullptr;
}

}
