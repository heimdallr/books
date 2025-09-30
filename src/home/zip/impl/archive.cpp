#include "archive.h"

#include "win.h"

#include <InitGuid.h>
#include <Shlwapi.h>

#include <ranges>

#include <QDir>

#include <interface/types.h>

#include "fnd/FindPair.h"
#include "fnd/IsOneOf.h"
#include "fnd/ScopedCall.h"

#include "7z-sdk/7z/CPP/7zip/Archive/IArchive.h"
#include "bit7z/bitcompressionlevel.hpp"
#include "bit7z/formatdetect.hpp"
#include "bit7z/guiddef.hpp"
#include "zip/interface/error.h"
#include "zip/interface/file.h"
#include "zip/interface/format.h"
#include "zip/interface/zip.h"

#include "ArchiveOpenCallback.h"
#include "FileItem.h"
#include "InStreamWrapper.h"
#include "PropVariant.h"
#include "lib.h"
#include "log.h"
#include "reader.h"
#include "remover.h"
#include "writer.h"

namespace HomeCompa::ZipDetails::SevenZip
{

namespace
{

CComPtr<IStream> CreateStream(const QString& filename)
{
	if (!QFile::exists(filename))
		return {};

	CComPtr<IStream> fileStream;
	if (FAILED(SHCreateStreamOnFileEx(filename.toStdWString().data(), STGM_READ, FILE_ATTRIBUTE_READONLY, FALSE, NULL, &fileStream)))
		Error::CannotOpenFile(filename);
	return fileStream;
}

CComPtr<IStream> CreateStream(const BYTE* pInit, const UINT cbInit)
{
	return SHCreateMemStream(pInit, cbInit);
}

CComPtr<IInArchive> GetArchiveReader(const Lib& lib, const bit7z::BitInFormat& format)
{
	const auto guid = bit7z::format_guid(format);

	CComPtr<IInArchive> archive;
	lib.CreateObject(guid, IID_IInArchive, reinterpret_cast<void**>(&archive));
	return archive;
}

CComPtr<IInArchive> CreateInputArchiveImpl(const Lib& lib, CComPtr<IStream> stream, const bit7z::BitInFormat& format)
{
	auto archive = GetArchiveReader(lib, format);
	if (!archive)
		return {};

	stream->Seek({}, STREAM_SEEK_SET, nullptr);
	const auto file         = InStreamWrapper::Create(std::move(stream));
	const auto openCallback = ArchiveOpenCallback::Create();

	if (const auto hr = archive->Open(file, nullptr, openCallback); hr == S_OK)
		return archive;

	archive->Close();
	return {};
}

const bit7z::BitInOutFormat& GetInOutFormat(const Format format)
{
	static constexpr std::pair<Format, const bit7z::BitInOutFormat&> formats[] {
#define ZIP_FORMAT_ITEM(NAME) { Format::NAME, bit7z::BitFormat::NAME },
		ZIP_FORMAT_ITEMS_X_MACRO
#undef ZIP_FORMAT_ITEM
	};

	return FindSecond(formats, format);
}

CComPtr<IOutArchive> CreateOutputArchive(IInArchive* inArchive)
{
	assert(inArchive);
	CComPtr<IOutArchive> archive;
	if (inArchive->QueryInterface(IID_IOutArchive, reinterpret_cast<void**>(&archive)) == S_OK)
		return archive;
	return {};
}

CComPtr<IOutArchive> CreateOutputArchive(const Lib& lib, const Format format)
{
	CComPtr<IOutArchive> archive;
	const auto           guid = bit7z::format_guid(GetInOutFormat(format));

	lib.CreateObject(guid, IID_IOutArchive, reinterpret_cast<void**>(&archive));
	return archive;
}

std::wstring GetCompressionMethodName(const CompressionMethod method)
{
	constexpr std::pair<CompressionMethod, const wchar_t*> methods[] {
		{      CompressionMethod::Copy,      L"Copy" },
        {      CompressionMethod::Ppmd,      L"PPMd" },
        {      CompressionMethod::Lzma,      L"LZMA" },
        {     CompressionMethod::Lzma2,     L"LZMA2" },
		{     CompressionMethod::BZip2,     L"BZip2" },
        {   CompressionMethod::Deflate,   L"Deflate" },
        { CompressionMethod::Deflate64, L"Deflate64" },
	};

	return FindSecond(methods, method);
}

constexpr auto CompressionLevelName          = L"x";
constexpr auto CompressionMethodName         = L"m";
constexpr auto CompressionMethodNameSevenZip = L"0";
constexpr auto SolidModeName                 = L"s";
constexpr auto ThreadCountName               = L"mt";

void SetArchiveProperties(IOutArchive& archive, const bit7z::BitInOutFormat& format, const std::unordered_map<PropertyId, QVariant>& properties)
{
	CComPtr<ISetProperties> setProperties;
	if (archive.QueryInterface(IID_ISetProperties, reinterpret_cast<void**>(&setProperties)) != S_OK)
		return;

	const auto getProperty = [&](const PropertyId id) {
		const auto it = properties.find(id);
		return it != properties.end() ? it->second : QVariant {};
	};
	std::vector<const wchar_t*> names;
	std::vector<CPropVariant>   values;

	if (const auto value = getProperty(PropertyId::CompressionLevel); value.isValid() && format.hasFeature(bit7z::FormatFeatures::CompressionLevel))
	{
		names.emplace_back(CompressionLevelName);
		values.emplace_back(static_cast<uint32_t>(value.value<CompressionLevel>()));
	}

	if (const auto value = getProperty(PropertyId::CompressionMethod); value.isValid() && format.hasFeature(bit7z::FormatFeatures::MultipleMethods))
	{
		names.emplace_back(format == bit7z::BitFormat::SevenZip ? CompressionMethodNameSevenZip : CompressionMethodName);
		values.emplace_back(GetCompressionMethodName(value.value<CompressionMethod>()));
	}

	if (const auto value = getProperty(PropertyId::SolidArchive); value.isValid() && format.hasFeature(bit7z::FormatFeatures::SolidArchive))
	{
		names.emplace_back(SolidModeName);
		values.emplace_back(value.toBool());
	}

	if (const auto value = getProperty(PropertyId::ThreadsCount); value.isValid())
	{
		names.emplace_back(ThreadCountName);
		values.emplace_back(value.toUInt());
	}

	[[maybe_unused]] const auto res = setProperties->SetProperties(names.data(), values.data(), static_cast<uint32_t>(std::size(names)));
	assert(res == S_OK);
}

struct ArchiveWrapper
{
	const bit7z::BitInFormat& format;
	CComPtr<IInArchive>       archive;

	ArchiveWrapper(const bit7z::BitInFormat& f = bit7z::BitFormat::Auto)
		: format { f }
	{
	}
};

std::unique_ptr<ArchiveWrapper> CreateInputArchive(const Lib& lib, const QString& filename)
{
	auto stream = CreateStream(filename);
	if (!stream)
		return std::make_unique<ArchiveWrapper>();

	if (auto archive = std::make_unique<ArchiveWrapper>(bit7z::detect_format_from_extension(filename.toStdWString())); !IsOneOf(archive->format, bit7z::BitFormat::Auto, bit7z::BitFormat::Rar))
		if ((archive->archive = CreateInputArchiveImpl(lib, stream, archive->format)))
			return archive;

	if (auto archive = std::make_unique<ArchiveWrapper>(bit7z::detect_format_from_signature(stream)); archive->format != bit7z::BitFormat::Auto)
		if ((archive->archive = CreateInputArchiveImpl(lib, std::move(stream), archive->format)))
			return archive;

	Error::CannotOpenArchive(filename);
}

std::unique_ptr<ArchiveWrapper> CreateInputArchive(const Lib& lib, const BYTE* pInit, const UINT cbInit)
{
	if (!cbInit)
		return std::make_unique<ArchiveWrapper>();

	auto stream = CreateStream(pInit, cbInit);
	if (!stream)
		return std::make_unique<ArchiveWrapper>();

	if (auto archive = std::make_unique<ArchiveWrapper>(bit7z::detect_format_from_signature(stream)); archive->format != bit7z::BitFormat::Auto)
		if ((archive->archive = CreateInputArchiveImpl(lib, std::move(stream), archive->format)))
			return archive;

	Error::CannotCreateObject();
}

FileStorage CreateFileList(CComPtr<IInArchive> archive)
{
	FileStorage result;

	if (!archive)
		return result;

	UInt32 numItems = 0;
	archive->GetNumberOfItems(&numItems);
	result.files.reserve(static_cast<size_t>(numItems));
	for (UInt32 i = 0; i < numItems; i++)
	{
		CPropVariant prop;
		const bool   isDir = [&] {
            archive->GetProperty(i, kpidIsDir, &prop);
            return prop.boolVal != VARIANT_FALSE;
		}();

		archive->GetProperty(i, kpidSize, &prop);
		const auto size = prop.uhVal.QuadPart;

		auto time = [&] {
			prop    = CPropVariant {};
			auto hr = archive->GetProperty(i, kpidCTime, &prop);
			if (FAILED(hr) || prop.vt == VT_EMPTY || (prop.filetime.dwHighDateTime == 0 && prop.filetime.dwLowDateTime == 0))
			{
				hr = archive->GetProperty(i, kpidATime, &prop);
				if (FAILED(hr) || prop.vt == VT_EMPTY || (prop.filetime.dwHighDateTime == 0 && prop.filetime.dwLowDateTime == 0))
				{
					hr = archive->GetProperty(i, kpidMTime, &prop);
					if (FAILED(hr) || prop.vt == VT_EMPTY || (prop.filetime.dwHighDateTime == 0 && prop.filetime.dwLowDateTime == 0))
					{
						return QDateTime {};
					}
				}
			}

			SYSTEMTIME systemTime {};
			if (!FileTimeToSystemTime(&prop.filetime, &systemTime))
				return QDateTime {};

			return QDateTime(QDate(systemTime.wYear, systemTime.wMonth, systemTime.wDay), QTime(systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds), QTimeZone::utc());
		}();

		archive->GetProperty(i, kpidPath, &prop);
		assert(prop.vt == VT_BSTR);

		auto       fileName = QDir::fromNativeSeparators(QString::fromStdWString(prop.bstrVal));
		const auto it       = result.index.try_emplace(fileName, result.files.size());
		if (it.second)
			result.files.emplace_back(i, std::move(fileName), size, std::move(time), isDir);
		else
			PLOGW << "something strange: " << fileName << " duplicates";
	}

	return result;
}

class Reader : virtual public IZip
{
public:
	explicit Reader(std::shared_ptr<ProgressCallback> progress)
		: m_progress(std::move(progress))
	{
	}

private: // IZip
	void SetProperty(const PropertyId id, QVariant value) override
	{
		m_properties[id] = std::move(value);
	}

	QStringList GetFileNameList() const override
	{
		QStringList files;
		files.reserve(static_cast<int>(m_files.files.size()));
		std::ranges::transform(
			m_files.files | std::views::filter([](const auto& item) {
				return !item.isDir;
			}),
			std::back_inserter(files),
			[](const auto& item) {
				return item.name;
			}
		);
		return files;
	}

	std::unique_ptr<IFile> Read(const QString& filename) const override
	{
		return File::Read(*m_archive->archive, m_files.GetFile(filename), *m_progress);
	}

	bool Write(std::shared_ptr<IZipFileProvider> zipFileProvider) override
	{
		assert(false && "Cannot write with reader");
		return false;
	}

	bool Remove(const std::vector<QString>& /*fileNames*/) override
	{
		assert(false && "Cannot remove with reader");
		return false;
	}

	size_t GetFileSize(const QString& filename) const override
	{
		return m_files.GetFile(filename).size;
	}

	const QDateTime& GetFileTime(const QString& filename) const override
	{
		return m_files.GetFile(filename).time;
	}

protected:
	Lib                                      m_lib;
	std::unique_ptr<ArchiveWrapper>          m_archive;
	FileStorage                              m_files;
	std::shared_ptr<ProgressCallback>        m_progress;
	std::unordered_map<PropertyId, QVariant> m_properties {
		{ PropertyId::CompressionLevel,               QVariant::fromValue(CompressionLevel::Ultra) },
		{     PropertyId::ThreadsCount, static_cast<uint32_t>(std::thread::hardware_concurrency()) },
	};
};

class ReaderFile : public Reader
{
public:
	ReaderFile(QString filename, std::shared_ptr<ProgressCallback> progress)
		: Reader(std::move(progress))
		, m_filename { std::move(filename) }
	{
		m_archive = CreateInputArchive(m_lib, m_filename);
		m_files   = CreateFileList(m_archive->archive);
	}

protected:
	const QString m_filename;
};

class ReaderStream : public Reader
{
public:
	ReaderStream(QIODevice& stream, std::shared_ptr<ProgressCallback> progress)
		: Reader(std::move(progress))
		, m_bytes { stream.isReadable() ? stream.readAll() : QByteArray {} }
	{
		m_archive = CreateInputArchive(m_lib, reinterpret_cast<const BYTE*>(m_bytes.constData()), static_cast<UINT>(m_bytes.size()));
		m_files   = CreateFileList(m_archive->archive);
	}

protected:
	QByteArray m_bytes;
};

class WriterFile final : public ReaderFile
{
public:
	WriterFile(QString filename, const Format format, std::shared_ptr<ProgressCallback> progress, const bool appendMode)
		: ReaderFile(std::move(filename), std::move(progress))
		, m_format { format }
		, m_ioDevice { std::make_unique<QFile>(m_filename) }
	{
		assert(m_archive->format != bit7z::BitFormat::Auto || format != Format::Auto);
		if (!appendMode)
			m_archive = std::make_unique<ArchiveWrapper>();
		m_ioDevice->open(!appendMode || m_archive->format == bit7z::BitFormat::Auto ? QIODevice::WriteOnly : QIODevice::ReadWrite);

		m_outArchive = m_archive->archive ? CreateOutputArchive(m_archive->archive) : CreateOutputArchive(m_lib, format);
		assert(m_outArchive);
	}

private: // IZip
	bool Write(std::shared_ptr<IZipFileProvider> zipFileProvider) override
	{
		if (!m_archive->archive)
			SetArchiveProperties(*m_outArchive, GetInOutFormat(m_format), m_properties);
		return File::Write(m_files, *m_outArchive, *m_ioDevice, std::move(zipFileProvider), *m_progress);
	}

	bool Remove(const std::vector<QString>& fileNames) override
	{
		const auto n      = m_files.files.size();
		const auto result = File::Remove(m_files, *m_outArchive, *m_ioDevice, fileNames, *m_progress);
		PLOGI << m_filename << ". Files removed: " << n - m_files.files.size() << " out of " << n;
		return result;
	}

private:
	const Format               m_format;
	std::unique_ptr<QIODevice> m_ioDevice;
	CComPtr<IOutArchive>       m_outArchive;
};

class WriterStream final : public ReaderStream
{
public:
	WriterStream(QIODevice& stream, const Format format, std::shared_ptr<ProgressCallback> progress, const bool appendMode)
		: ReaderStream(stream, std::move(progress))
		, m_format { format }
		, m_ioDevice { stream }
	{
		assert(m_archive->format != bit7z::BitFormat::Auto || format != Format::Auto);
		m_ioDevice.close();
		if (!appendMode)
			m_archive = std::make_unique<ArchiveWrapper>();
		m_ioDevice.open(!appendMode || m_archive->format == bit7z::BitFormat::Auto ? QIODevice::WriteOnly : QIODevice::ReadWrite);

		m_outArchive = m_archive->archive ? CreateOutputArchive(m_archive->archive) : CreateOutputArchive(m_lib, format);
		assert(m_outArchive);
	}

private: // IZip
	bool Write(std::shared_ptr<IZipFileProvider> zipFileProvider) override
	{
		if (!m_archive->archive)
			SetArchiveProperties(*m_outArchive, GetInOutFormat(m_format), m_properties);
		return File::Write(m_files, *m_outArchive, m_ioDevice, std::move(zipFileProvider), *m_progress);
	}

	bool Remove(const std::vector<QString>& fileNames) override
	{
		const auto n      = m_files.files.size();
		const auto result = File::Remove(m_files, *m_outArchive, m_ioDevice, fileNames, *m_progress);
		PLOGI << "files removed: " << n - m_files.files.size() << " out of " << n;
		return result;
	}

private:
	const Format         m_format;
	QIODevice&           m_ioDevice;
	CComPtr<IOutArchive> m_outArchive;
};

} // namespace

std::unique_ptr<IZip> Archive::CreateReader(const QString& filename, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<ReaderFile>(filename, std::move(progress));
}

std::unique_ptr<IZip> Archive::CreateReaderStream(QIODevice& stream, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<ReaderStream>(stream, std::move(progress));
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString& filename, const Format format, std::shared_ptr<ProgressCallback> progress, bool appendMode)
{
	return std::make_unique<WriterFile>(filename, format, std::move(progress), appendMode);
}

std::unique_ptr<IZip> Archive::CreateWriterStream(QIODevice& stream, Format format, std::shared_ptr<ProgressCallback> progress, bool appendMode)
{
	return std::make_unique<WriterStream>(stream, format, std::move(progress), appendMode);
}

bool Archive::IsArchive(const QString& filename)
{
	return bit7z::detect_format_from_extension(filename.toStdWString()) != bit7z::BitFormat::Auto;
}

QStringList Archive::GetTypes()
{
	return QStringList {} << "7z"
	                      << "zip"
	                      << "rar"
	                      << "bzip2"
	                      << "bz2"
	                      << "tbz2"
	                      << "tbz"
	                      << "gz"
	                      << "gzip"
	                      << "tgz"
	                      << "tar"
	                      << "ova"
	                      << "wim"
	                      << "swm"
	                      << "xz"
	                      << "txz"
	                      << "zipx"
	                      << "jar"
	                      << "xpi"
	                      << "odt"
	                      << "ods"
	                      << "odp"
	                      << "docx"
	                      << "xlsx"
	                      << "pptx"
	                      << "epub"
	                      << "001"
	                      << "ar"
	                      << "deb"
	                      << "apm"
	                      << "arj"
	                      << "cab"
	                      << "chm"
	                      << "chi"
	                      << "msi"
	                      << "doc"
	                      << "xls"
	                      << "ppt"
	                      << "msg"
	                      << "obj"
	                      << "cpio"
	                      << "cramfs"
	                      << "dmg"
	                      << "dll"
	                      << "exe"
	                      << "dylib"
	                      << "ext"
	                      << "ext2"
	                      << "ext3"
	                      << "ext4"
	                      << "fat"
	                      << "flv"
	                      << "gpt"
	                      << "hfs"
	                      << "hfsx"
	                      << "hxs"
	                      << "ihex"
	                      << "lzh"
	                      << "lha"
	                      << "lzma"
	                      << "lzma86"
	                      << "mbr"
	                      << "mslz"
	                      << "mub"
	                      << "nsis"
	                      << "ntfs"
	                      << "pmd"
	                      << "ppmd"
	                      << "qcow"
	                      << "qcow2"
	                      << "qcow2c"
	                      << "rpm"
	                      << "squashfs"
	                      << "swf"
	                      << "te"
	                      << "udf"
	                      << "scap"
	                      << "uefif"
	                      << "vmdk"
	                      << "vdi"
	                      << "vhd"
	                      << "xar"
	                      << "pkg"
	                      << "z"
	                      << "taz";
}

} // namespace HomeCompa::ZipDetails::SevenZip
