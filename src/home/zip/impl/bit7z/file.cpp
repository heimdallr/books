#include "file.h"

#include <exception>

#include <bit7z/bit7z.hpp>

#include "util/QIODeviceStreamWrapper.h"

#include "zip/interface/error.h"
#include "zip/interface/file.h"
#include "zip/interface/stream.h"

#include <QBuffer>
#include <QFileInfo>
#include <plog/Log.h>

using namespace bit7z;

namespace HomeCompa::ZipDetails::Impl::Bit7z {

namespace {


class Writer : public QIODevice
{
public:
	Writer(const Bit7zLibrary & lib, QString zipFilename, std::ostream & oStream, QString filename)
		: m_lib { lib }
		, m_zipFilename { std::move(zipFilename) }
		, m_oStream { oStream }
		, m_filename { std::move(filename) }
	{
	}

private: // QIODevice
	qint64 readData(char* /*data*/, qint64 /*maxlen*/) override
	{
		assert(false);
		return 0;
	}

	qint64 writeData(const char* data, const qint64 len) override
	{
//		BitMemCompressor compressor(m_lib, BitFormat::Zip);
//		BitArchiveWriter compressor(m_lib, m_iStream, BitFormat::Zip);
		auto compressor = [this]
		{
			const QFileInfo fileInfo(m_zipFilename);
			if (fileInfo.size() == 0)
				return BitArchiveWriter(m_lib, BitFormat::Zip);

			return BitArchiveWriter(m_lib, m_zipFilename.toStdString(), BitFormat::Zip);
		}();
		compressor.setUpdateMode(UpdateMode::Append);
		compressor.setCompressionLevel(BitCompressionLevel::Ultra);
		auto * bytes = reinterpret_cast<byte_t *>(const_cast<char *>(data));
//		compressor.compressFile(std::vector(bytes, bytes + len), m_oStream, m_filename.toStdString());
		const std::vector compressorData(bytes, bytes + len);
		compressor.addFile(compressorData, m_filename.toStdString());
		try
		{
			compressor.compressTo(m_oStream);
		}
		catch(const std::exception & ex)
		{
			PLOGE << ex.what();
		}
		return len;
	}

private:
	const Bit7zLibrary & m_lib;
	QString m_zipFilename;
	std::ostream & m_oStream;
	QString m_filename;
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

class Bit7zReadImpl final : virtual public IFile
{
public:
	Bit7zReadImpl(const Bit7zLibrary & lib, std::istream & iStream, const FileItem & item)
	{
		const BitStreamExtractor extractor(lib);
		m_bytes.reserve(static_cast<qsizetype>(item.size));
		QBuffer buffer(&m_bytes);
		buffer.open(QIODevice::WriteOnly);
		const auto oStream = QStdOStream::create(buffer);

		iStream.seekg(0);
		extractor.extract(iStream, *oStream, item.index);
	}

private: // IFile
	std::unique_ptr<Stream> Read() override
	{
		return std::make_unique<StreamImpl>(std::make_unique<QBuffer>(&m_bytes));
	}

	std::unique_ptr<Stream> Write() override
	{
		throw std::runtime_error("not implemented");
	}

private:
	QByteArray m_bytes;
	std::unique_ptr<QBuffer> m_buffer;
};

QDateTime GetFileDateTime(const BitArchiveItemInfo & info)
{
	constexpr auto WINDOWS_TICK = 10000000ULL;
	constexpr auto SEC_TO_UNIX_EPOCH = 11644473600ULL;
	auto tm = info.itemProperty(BitProperty::MTime);
	if (tm.isEmpty())
	{
		tm = info.itemProperty(BitProperty::CTime);
		if (tm.isEmpty())
		{
			tm = info.itemProperty(BitProperty::ATime);
			if (tm.isEmpty())
				return {};
		}
	}

	const auto time = tm.getFileTime();
	return QDateTime::fromSecsSinceEpoch((static_cast<uint64_t>(time.dwHighDateTime) << 32 | static_cast<uint64_t>(time.dwLowDateTime)) / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

class Bit7zWriteImpl final : virtual public IFile
{
public:
	Bit7zWriteImpl(const Bit7zLibrary & lib, QString zipFilename, std::ostream & oStream, QString filename)
		: m_lib { lib }
		, m_zipFilename { std::move(zipFilename) }
		, m_oStream { oStream }
		, m_filename { std::move(filename) }
	{
	}

private: // IFile
	std::unique_ptr<Stream> Read() override
	{
		throw std::runtime_error("not implemented");
	}

	std::unique_ptr<Stream> Write() override
	{
		return std::make_unique<StreamImpl>(std::make_unique<Writer>(m_lib, m_zipFilename, m_oStream, m_filename));
	}

private:
	const Bit7zLibrary & m_lib;
	QString m_zipFilename;
	std::ostream & m_oStream;
	QString m_filename;
};

}

FileItem::FileItem(const BitArchiveItemInfo & info, const uint32_t index)
	: size { info.itemProperty(BitProperty::Size).getUInt64() }
	, time { GetFileDateTime(info) }
	, index { index }
{
}


std::unique_ptr<IFile> File::Read(const Bit7zLibrary & lib, std::istream & stream, const FileItem & item)
{
	return std::make_unique<Bit7zReadImpl>(lib, stream, item);
}

std::unique_ptr<IFile> File::Write(const Bit7zLibrary & lib, QString zipFilename, std::ostream & oStream, QString filename)
{
	return std::make_unique<Bit7zWriteImpl>(lib, std::move(zipFilename), oStream, std::move(filename));
}

}
