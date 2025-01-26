#include "archive.h"

#include <ranges>

#include <QDir>

#include <Windows.h>
#include <atlcomcli.h>
#include <InitGuid.h>
#include <Shlwapi.h>

#include "7z/CPP/7zip/Archive/IArchive.h"

#include "fnd/ScopedCall.h"
#include "fnd/FindPair.h"

#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "zip/interface/format.h"
#include "zip/interface/error.h"

#include "file/reader.h"
#include "file/writer.h"

#include "bit7z/formatdetect.hpp"
#include "bit7z/guiddef.hpp"
#include "bit7z/bitcompressionlevel.hpp"

#include "util/QIODeviceStreamWrapper.h"

#include "ArchiveOpenCallback.h"
#include "InStreamWrapper.h"
#include "lib.h"
#include "PropVariant.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

CComPtr<IStream> CreateStream(const QString & filename)
{
	if (!QFile::exists(filename))
		return {};

	CComPtr<IStream> fileStream;
	if (FAILED(SHCreateStreamOnFileEx(filename.toStdWString().data(), STGM_READ, FILE_ATTRIBUTE_READONLY, FALSE, NULL, &fileStream)))
		Error::CannotOpenFile(filename);
	return fileStream;
}

CComPtr<IInArchive> GetArchiveReader(const Lib & lib, const bit7z::BitInFormat & format)
{
	const auto guid = bit7z::format_guid(format);

	CComPtr<IInArchive> archive;
	lib.CreateObject(guid, IID_IInArchive, reinterpret_cast<void **>(&archive));
	return archive;
}

CComPtr<IInArchive> CreateInputArchiveImpl(const Lib & lib, CComPtr<IStream> stream, const bit7z::BitInFormat & format)
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

CComPtr<IOutArchive> GetArchiveWriter(const Lib & lib, const bit7z::BitInFormat & format)
{
	const auto guid = bit7z::format_guid(format);

	CComPtr<IOutArchive> archive;
	lib.CreateObject(guid, IID_IOutArchive, reinterpret_cast<void **>(&archive));
	return archive;
}

CComPtr<IOutArchive> CreateOutputArchive(const Lib & lib, CComPtr<IStream> /*stream*/, const bit7z::BitInFormat & format)
{
	auto archive = GetArchiveWriter(lib, format);
	if (!archive)
		return {};

	CComPtr<ISetProperties> setProperties;
	[[maybe_unused]] HRESULT res = archive->QueryInterface(IID_ISetProperties, reinterpret_cast<void **>(&setProperties));

	const wchar_t * names[] { L"x" };
	const CPropVariant values[] { static_cast<uint32_t>(bit7z::BitCompressionLevel::Ultra) };
	res = setProperties->SetProperties(names, values, static_cast<uint32_t>(std::size(names)));

	return archive;
}

struct ArchiveWrapper
{
	const bit7z::BitInFormat & format;
	CComPtr<IInArchive> archive;

	ArchiveWrapper(const bit7z::BitInFormat & f = bit7z::BitFormat::Auto)
		: format { f }
	{
	}
};

ArchiveWrapper CreateInputArchive(const Lib & lib, const QString & filename)
{
	auto stream = CreateStream(filename);
	if (!stream)
		return {};
	
	if (auto archive = ArchiveWrapper { bit7z::detect_format_from_extension(filename.toStdWString()) }; archive.format != bit7z::BitFormat::Auto)
		if ((archive.archive = CreateInputArchiveImpl(lib, stream, archive.format)))
			return archive;

	if (auto archive = ArchiveWrapper { bit7z::detect_format_from_signature(stream) }; archive.format != bit7z::BitFormat::Auto)
		if ((archive.archive = CreateInputArchiveImpl(lib, std::move(stream), archive.format)))
			return archive;

	Error::CannotOpenArchive(filename);
}

Files CreateFileList(CComPtr<IInArchive> archive)
{
	Files result;

	if (!archive)
		return result;

	UInt32 numItems = 0;
	archive->GetNumberOfItems(&numItems);
	for (UInt32 i = 0; i < numItems; i++)
	{
		CPropVariant prop;
		archive->GetProperty(i, kpidIsDir, &prop);
		if (prop.boolVal != VARIANT_FALSE)
			continue;

		archive->GetProperty(i, kpidSize, &prop);
		const auto size = prop.uhVal.QuadPart;

		auto time = [&]
		{
			if (FAILED(archive->GetProperty(i, kpidCTime, &prop)) || (!prop.filetime.dwHighDateTime && !prop.filetime.dwLowDateTime))
				return QDateTime {};

			SYSTEMTIME systemTime {};
			if (!FileTimeToSystemTime(&prop.filetime, &systemTime))
				return QDateTime {};

			return QDateTime(QDate(systemTime.wYear, systemTime.wMonth, systemTime.wDay), QTime(systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds));
		}();

		archive->GetProperty(i, kpidPath, &prop);
		if (prop.vt == VT_BSTR)
			result.try_emplace(QDir::fromNativeSeparators(QString::fromStdWString(prop.bstrVal)), FileItem { i, size, std::move(time) });
	}

	return result;
}

class Reader : virtual public IZip
{
public:
	Reader(QString filename, std::shared_ptr<ProgressCallback> progress)
		: m_filename { std::move(filename) }
		, m_archive { CreateInputArchive(m_lib, m_filename) }
		, m_files { CreateFileList(m_archive.archive) }
		, m_progress(std::move(progress))
	{
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		QStringList files;
		files.reserve(static_cast<int>(m_files.size()));
		std::ranges::copy(m_files | std::views::keys, std::back_inserter(files));
		return files;
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		const auto it = m_files.find(QDir::fromNativeSeparators(filename));
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return File::Read(*m_archive.archive, it->second, *m_progress);
	}

	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		assert(false && "Cannot write with reader");
		return {};
	}

	size_t GetFileSize(const QString & filename) const override
	{
		const auto it = m_files.find(QDir::fromNativeSeparators(filename));
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return it->second.size;
	}

	const QDateTime & GetFileTime(const QString & filename) const override
	{
		const auto it = m_files.find(filename);
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return it->second.time;
	}

protected:
	Lib m_lib;
	const QString m_filename;
	ArchiveWrapper m_archive;
	const Files m_files;
	std::shared_ptr<ProgressCallback> m_progress;
};

const bit7z::BitInOutFormat & GetInOutFormat(const bit7z::BitInFormat & /*inFormat*/, const Format format)
{
	static constexpr std::pair<Format, const bit7z::BitInOutFormat &> formats[]
	{
		{Format::SevenZip, bit7z::BitFormat::SevenZip},
		{Format::Zip, bit7z::BitFormat::Zip},
	};
	return FindSecond(formats, format);
}

class Writer final : public Reader
{
public:
	Writer(QString filename, const Format format, std::shared_ptr<ProgressCallback> progress, const bool appendMode)
		: Reader(std::move(filename), std::move(progress))
		, m_ioDevice { std::make_unique<QFile>(m_filename) }
		, m_oStream { QStdOStream::create(*m_ioDevice) }
		, m_outFormat { GetInOutFormat(m_archive.format, format) }
		, m_outArchive { CreateOutputArchive(m_lib, {}, m_outFormat) }
		, m_appendMode { appendMode }
	{
	}

private: // IZip
	std::unique_ptr<IFile> Write(const QString & filename) override
	{
		const auto it = m_files.find(QDir::fromNativeSeparators(filename));
		m_ioDevice->open(!m_appendMode || m_archive.format == bit7z::BitFormat::Auto ? QIODevice::WriteOnly : QIODevice::ReadWrite);
		return File::Write(*m_outArchive, *m_oStream, filename, *m_progress);
	}

private:
	std::unique_ptr<QIODevice> m_ioDevice;
	std::unique_ptr<std::ostream> m_oStream;
	const bit7z::BitInOutFormat & m_outFormat;
	CComPtr<IOutArchive> m_outArchive;
	const bool m_appendMode;
};

}

std::unique_ptr<IZip> Archive::CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<Reader>(filename, std::move(progress));
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & filename, const Format format, std::shared_ptr<ProgressCallback> progress, bool appendMode)
{
	return std::make_unique<Writer>(filename, format, std::move(progress), appendMode);
}

std::unique_ptr<IZip> Archive::CreateWriterStream(QIODevice & /*stream*/, Format, std::shared_ptr<ProgressCallback> /*progress*/, bool /*appendMode*/)
{
	assert(false);
	return nullptr;
}

}
