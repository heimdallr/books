#include "archive.h"

#include <ios>
#include <ranges>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>

#include <bit7z/bit7z.hpp>

#include <plog/Log.h>

#include "util/QIODeviceStreamWrapper.h"

#include "zip/interface/error.h"
#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "file.h"

namespace HomeCompa::ZipDetails::Impl::Bit7z {

namespace {

class Bit7zImpl final : virtual public IZip
{
public:
	enum class Mode
	{
		Read,
		Write,
	};

public:
	Bit7zImpl(const QString & filename, const Mode mode, const bool appendMode = false)
		: m_ioDevice(std::make_unique<QFile>(filename))
	{
		if (!m_ioDevice->open(mode == Mode::Read ? QIODevice::ReadOnly : appendMode ? QIODevice::ReadWrite : QIODevice::WriteOnly))
			mode == Mode::Read ? Error::CannotOpenArchive(filename) : Error::CannotCreateArchive(filename);

		Open(*m_ioDevice, mode);
	}

	Bit7zImpl(QIODevice & device, const Mode mode)
	{
		Open(device, mode);
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		QStringList result;
		result.reserve(static_cast<int>(m_files.size()));
		std::ranges::copy(m_files | std::views::keys, std::back_inserter(result));
		return result;
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		const auto it = m_files.find(filename);
		if (it == m_files.end())
			Error::CannotFindFileInArchive(filename);

		return File::Read(m_lib, *m_iStream, it->second);
	}

	std::unique_ptr<IFile> Write(const QString & filename) override
	{
		return File::Write(m_lib, *m_oStream, filename);
	}

	size_t GetFileSize(const QString & filename) const override
	{
		const auto it = m_files.find(filename);
		assert(it != m_files.end());
		return it->second.size;
	}

	const QDateTime & GetFileTime(const QString& filename) const override
	{
		const auto it = m_files.find(filename);
		assert(it != m_files.end());
		return it->second.time;
	}

private:
	void Open(QIODevice & device, const Mode mode)
	{
		if (mode == Mode::Read)
		{
			m_iStream = QStdIStream::create(device);
			const bit7z::BitArchiveReader reader(m_lib, *m_iStream);
			std::ranges::transform(reader.items(), std::inserter(m_files, m_files.end()), [n = uint32_t { 0 }](const bit7z::BitArchiveItemInfo & info) mutable
			{
				return std::make_pair(QString::fromStdString(info.itemProperty(bit7z::BitProperty::Path).getString()), FileItem(info, n++));
			});
			m_iStream->seekg(0, std::ios_base::beg);
			return;
		}

		assert(mode == Mode::Write);
		m_oStream = QStdOStream::create(device);
	}

private:
	bit7z::Bit7zLibrary m_lib { (QCoreApplication::applicationDirPath() + "/7z.dll").toStdString() };
	std::unique_ptr<QIODevice> m_ioDevice;
	std::unique_ptr<std::istream> m_iStream;
	std::unique_ptr<std::ostream> m_oStream;
	mutable std::unordered_map<QString, FileItem> m_files;
};

}

std::unique_ptr<IZip> Archive::CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> /*progress*/)
{
	return std::make_unique<Bit7zImpl>(filename, Bit7zImpl::Mode::Read);
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & filename, std::shared_ptr<ProgressCallback> /*progress*/, const bool appendMode)
{
	return std::make_unique<Bit7zImpl>(filename, Bit7zImpl::Mode::Write, appendMode);
}

std::unique_ptr<IZip> Archive::CreateWriterStream(QIODevice & stream, std::shared_ptr<ProgressCallback> /*progress*/, const bool /*appendMode*/)
{
	return std::make_unique<Bit7zImpl>(stream, Bit7zImpl::Mode::Write);
}

}
