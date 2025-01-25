#include "archive.h"

#include <ranges>

#include <QDir>

#include <Windows.h>
#include <atlcomcli.h>
#include <InitGuid.h>
#include <Shlwapi.h>

#include "7z/CPP/7zip/Archive/IArchive.h"

#include "fnd/ScopedCall.h"

#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "zip/interface/error.h"

#include "ArchiveOpenCallback.h"
#include "file.h"
#include "InStreamWrapper.h"
#include "lib.h"
#include "PropVariant.h"

#include "bit7z/formatdetect.hpp"
#include "bit7z/guiddef.hpp"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

auto CreateStream(const QString & filename)
{
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

CComPtr<IInArchive> CreateInputArchive(const Lib & lib, const QString & filename)
{
	auto stream = CreateStream(filename);
	
	if (const auto & format = bit7z::detect_format_from_extension(filename.toStdWString()); format != bit7z::BitFormat::Auto)
		if (auto archive = CreateInputArchiveImpl(lib, stream, format))
			return archive;

	if (const auto & format = bit7z::detect_format_from_signature(stream); format != bit7z::BitFormat::Auto)
		if (auto archive = CreateInputArchiveImpl(lib, std::move(stream), format))
			return archive;

	Error::CannotOpenArchive(filename);
}

class Reader final : virtual public IZip
{
public:
	Reader(QString filename, std::shared_ptr<ProgressCallback> progress)
		: m_filename(std::move(filename))
		, m_progress(std::move(progress))
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
		const auto it = m_files.find(QDir::fromNativeSeparators(filename));
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return File::Read(m_archive, it->second, m_progress);
	}

	std::unique_ptr<IFile> Write(const QString & /*filename*/) override
	{
		assert(false && "Cannot write with reader");
		return {};
	}

	size_t GetFileSize(const QString & filename) const override
	{
		CreateFileList();
		const auto it = m_files.find(QDir::fromNativeSeparators(filename));
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return it->second.size;
	}

	const QDateTime & GetFileTime(const QString & filename) const override
	{
		CreateFileList();
		const auto it = m_files.find(filename);
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return it->second.time;
	}

private:
	void CreateFileList() const
	{
		if (!m_files.empty())
			return;

		if (!m_archive)
			m_archive = CreateInputArchive(m_lib, m_filename);

		UInt32 numItems = 0;
		m_archive->GetNumberOfItems(&numItems);
		for (UInt32 i = 0; i < numItems; i++)
		{
			CPropVariant prop;
			m_archive->GetProperty(i, kpidIsDir, &prop);
			if (prop.boolVal != VARIANT_FALSE)
				continue;

			m_archive->GetProperty(i, kpidSize, &prop);
			const auto size = prop.uhVal.QuadPart;

			auto time = [&]
			{
				if (FAILED(m_archive->GetProperty(i, kpidCTime, &prop)) || (!prop.filetime.dwHighDateTime && !prop.filetime.dwLowDateTime))
					return QDateTime {};

				SYSTEMTIME systemTime {};
				if (!FileTimeToSystemTime(&prop.filetime, &systemTime))
					return QDateTime {};

				return QDateTime(QDate(systemTime.wYear, systemTime.wMonth, systemTime.wDay), QTime(systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds));
			}();

			m_archive->GetProperty(i, kpidPath, &prop);
			if (prop.vt == VT_BSTR)
				m_files.try_emplace(QDir::fromNativeSeparators(QString::fromStdWString(prop.bstrVal)), FileItem { i, size, std::move(time) });
		}
	}

private:
	QString m_filename;
	std::shared_ptr<ProgressCallback> m_progress;
	Lib m_lib;
	mutable CComPtr<IInArchive> m_archive;
	mutable Files m_files;
};

}


std::unique_ptr<IZip> Archive::CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<Reader>(filename, std::move(progress));
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & /*filename*/, Format, std::shared_ptr<ProgressCallback> /*progress*/, bool /*appendMode*/)
{
	assert(false);
	return nullptr;
}

std::unique_ptr<IZip> Archive::CreateWriterStream(QIODevice & /*stream*/, Format, std::shared_ptr<ProgressCallback> /*progress*/, bool /*appendMode*/)
{
	assert(false);
	return nullptr;
}

}
