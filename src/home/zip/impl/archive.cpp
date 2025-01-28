#include "archive.h"

#include <ranges>

#include <QDir>

#include <Windows.h>
#include <atlcomcli.h>
#include <InitGuid.h>
#include <Shlwapi.h>
#include <plog/Log.h>

#include "7z-sdk/7z/CPP/7zip/Archive/IArchive.h"

#include "fnd/ScopedCall.h"
#include "fnd/FindPair.h"

#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "zip/interface/format.h"
#include "zip/interface/error.h"

#include "bit7z/formatdetect.hpp"
#include "bit7z/guiddef.hpp"
#include "bit7z/bitcompressionlevel.hpp"

#include "ArchiveOpenCallback.h"
#include "FileItem.h"
#include "InStreamWrapper.h"
#include "lib.h"
#include "PropVariant.h"
#include "reader.h"
#include "writer.h"

namespace HomeCompa::ZipDetails::SevenZip {

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

CComPtr<IStream> CreateStream(const BYTE * pInit, const UINT cbInit)
{
	return SHCreateMemStream(pInit, cbInit);
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

CComPtr<IOutArchive> GetArchiveWriter(const Lib & lib, IInArchive * inArchive, const bit7z::BitInFormat & format)
{
	CComPtr<IOutArchive> archive;
	if (inArchive)
		if (inArchive->QueryInterface(IID_IOutArchive, reinterpret_cast<void **>(&archive)) == S_OK)
			return archive;

	const auto guid = bit7z::format_guid(format);

	lib.CreateObject(guid, IID_IOutArchive, reinterpret_cast<void **>(&archive));
	return archive;
}

CComPtr<IOutArchive> CreateOutputArchive(const Lib & lib, IInArchive * inArchive, const bit7z::BitInFormat & format)
{
	auto archive = GetArchiveWriter(lib, inArchive, format);
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

std::unique_ptr<ArchiveWrapper> CreateInputArchive(const Lib & lib, const QString & filename)
{
	auto stream = CreateStream(filename);
	if (!stream)
		return std::make_unique<ArchiveWrapper>();
	
	if (auto archive = std::make_unique<ArchiveWrapper>(bit7z::detect_format_from_extension(filename.toStdWString())); archive->format != bit7z::BitFormat::Auto)
		if ((archive->archive = CreateInputArchiveImpl(lib, stream, archive->format)))
			return archive;

	if (auto archive = std::make_unique<ArchiveWrapper>(bit7z::detect_format_from_signature(stream)); archive->format != bit7z::BitFormat::Auto)
		if ((archive->archive = CreateInputArchiveImpl(lib, std::move(stream), archive->format)))
			return archive;

	Error::CannotOpenArchive(filename);
}

std::unique_ptr<ArchiveWrapper> CreateInputArchive(const Lib & lib, const BYTE * pInit, const UINT cbInit)
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
	result.files.reserve(numItems);
	for (UInt32 i = 0; i < numItems; i++)
	{
		CPropVariant prop;
		const bool isDir = [&]
		{
			archive->GetProperty(i, kpidIsDir, &prop);
			return prop.boolVal != VARIANT_FALSE;
		}();

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
		assert(prop.vt == VT_BSTR);

		auto fileName = QDir::fromNativeSeparators(QString::fromStdWString(prop.bstrVal));
		const auto it = result.index.try_emplace(fileName, result.files.size());
		if (it.second)
			result.files.emplace_back(i, std::move(fileName), size, std::move(time), isDir);
		else
			PLOGW << "something strange: " << fileName << " duplicates";
	}

	return result;
}

const bit7z::BitInOutFormat & GetInOutFormat(const bit7z::BitInFormat & /*inFormat*/, const Format format)
{
	static constexpr std::pair<Format, const bit7z::BitInOutFormat &> formats[]
	{
		{Format::SevenZip, bit7z::BitFormat::SevenZip},
		{Format::Zip, bit7z::BitFormat::Zip},
	};
	return FindSecond(formats, format);
}

class Reader : virtual public IZip
{
public:
	explicit Reader(std::shared_ptr<ProgressCallback> progress)
		: m_progress(std::move(progress))
	{
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		QStringList files;
		files.reserve(static_cast<int>(m_files.files.size()));
		std::ranges::transform(m_files.files | std::views::filter([] (const auto & item) { return !item.isDir; }), std::back_inserter(files), [] (const auto & item) { return item.name; });
		return files;
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		return File::Read(*m_archive->archive, m_files.GetFile(filename), *m_progress);
	}

	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		assert(false && "Cannot write with reader");
		return {};
	}

	bool Write(const std::vector<QString> & /*fileNames*/, const StreamGetter & /*streamGetter*/) override
	{
		assert(false && "Cannot write with reader");
		return false;
	}

	size_t GetFileSize(const QString & filename) const override
	{
		return m_files.GetFile(filename).size;
	}

	const QDateTime & GetFileTime(const QString & filename) const override
	{
		return m_files.GetFile(filename).time;
	}

protected:
	Lib m_lib;
	std::unique_ptr<ArchiveWrapper> m_archive;
	FileStorage m_files;
	std::shared_ptr<ProgressCallback> m_progress;
};

class ReaderFile : public Reader
{
public:
	ReaderFile(QString filename, std::shared_ptr<ProgressCallback> progress)
		: Reader(std::move(progress))
		, m_filename { std::move(filename) }
	{
		m_archive = CreateInputArchive(m_lib, m_filename);
		m_files = CreateFileList(m_archive->archive);
	}

protected:
	const QString m_filename;
};

class ReaderStream : public Reader
{
public:
	ReaderStream(QIODevice & stream, std::shared_ptr<ProgressCallback> progress)
		: Reader(std::move(progress))
		, m_bytes { stream.readAll() }
	{
		m_archive = CreateInputArchive(m_lib, reinterpret_cast<const BYTE*>(m_bytes.constData()), static_cast<UINT>(m_bytes.size()));
		m_files = CreateFileList(m_archive->archive);
	}

protected:
	QByteArray m_bytes;
};

class WriterFile final : public ReaderFile
{
public:
	WriterFile(QString filename, const Format format, std::shared_ptr<ProgressCallback> progress, const bool appendMode)
		: ReaderFile(std::move(filename), std::move(progress))
		, m_ioDevice { std::make_unique<QFile>(m_filename) }
	{
		if (!appendMode)
			m_archive = std::make_unique<ArchiveWrapper>();
		m_ioDevice->open(!appendMode || m_archive->format == bit7z::BitFormat::Auto ? QIODevice::WriteOnly : QIODevice::ReadWrite);

		const auto & outFormat = GetInOutFormat(m_archive->format, format);
		m_outArchive = CreateOutputArchive(m_lib, m_archive->archive, outFormat);
	}

private: // IZip
	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		return {};
	}

	bool Write(const std::vector<QString> & fileNames, const StreamGetter & streamGetter) override
	{
		return File::Write(m_files, *m_outArchive, *m_ioDevice, fileNames, streamGetter, *m_progress);
	}

private:
	std::unique_ptr<QIODevice> m_ioDevice;
	CComPtr<IOutArchive> m_outArchive;
};

class WriterStream final : public ReaderStream
{
public:
	WriterStream(QIODevice & stream, const Format format, std::shared_ptr<ProgressCallback> progress, const bool appendMode)
		: ReaderStream(stream, std::move(progress))
		, m_ioDevice { stream }
	{
		m_ioDevice.close();
		if (!appendMode)
			m_archive = std::make_unique<ArchiveWrapper>();
		m_ioDevice.open(!appendMode || m_archive->format == bit7z::BitFormat::Auto ? QIODevice::WriteOnly : QIODevice::ReadWrite);

		const auto & outFormat = GetInOutFormat(m_archive->format, format);
		m_outArchive = CreateOutputArchive(m_lib, m_archive->archive, outFormat);
	}

private: // IZip
	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		return {};
	}

	bool Write(const std::vector<QString> & fileNames, const StreamGetter & streamGetter) override
	{
		return File::Write(m_files, *m_outArchive, m_ioDevice, fileNames, streamGetter, *m_progress);
	}

private:
	QIODevice & m_ioDevice;
	CComPtr<IOutArchive> m_outArchive;
};

}

std::unique_ptr<IZip> Archive::CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<ReaderFile>(filename, std::move(progress));
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & filename, const Format format, std::shared_ptr<ProgressCallback> progress, bool appendMode)
{
	return std::make_unique<WriterFile>(filename, format, std::move(progress), appendMode);
}

std::unique_ptr<IZip> Archive::CreateWriterStream(QIODevice & stream, Format format, std::shared_ptr<ProgressCallback> progress, bool appendMode)
{
	return std::make_unique<WriterStream>(stream, format, std::move(progress), appendMode);
}

}
