#include "zip.h"

#include <ranges>

#include <QBuffer>
#include <QFileInfo>
#include <QVariant>

#include "fnd/FindPair.h"
#include "fnd/memory.h"

#include "impl/archive.h"
#include "zip/interface/file.h"
#include "zip/interface/zip.h"

using namespace HomeCompa;
using namespace ZipDetails;

namespace
{

std::shared_ptr<ProgressCallback> GetProgress(std::shared_ptr<ProgressCallback> progress)
{
	if (progress)
		return progress;

	return std::make_shared<ProgressCallbackStub>();
}

constexpr std::pair<const char*, Zip::Format> ZIP_FORMATS[] {
	{ "zip",      Zip::Format::Zip },
	{  "7z", Zip::Format::SevenZip },
};

class ZipFileController final : public IZipFileController
{
private:
	struct Item
	{
		QString                    name;
		QByteArray                 body;
		QDateTime                  time;
		std::unique_ptr<QFileInfo> info;
	};

private: // IZipFileProvider
	size_t GetCount() const noexcept override
	{
		return m_items.size();
	}

	size_t GetFileSize(const size_t index) const noexcept override
	{
		assert(index < GetCount());
		const auto& item = m_items[index];
		return static_cast<size_t>(item.info ? item.info->size() : item.body.size());
	}

	QString GetFileName(const size_t index) const override
	{
		assert(index < GetCount());
		const auto& item = m_items[index];
		assert(!item.name.isEmpty());
		return item.name;
	}

	QDateTime GetFileTime(const size_t index) const override
	{
		assert(index < GetCount());
		return m_items[index].time;
	}

	std::unique_ptr<QIODevice> GetStream(const size_t index) override
	{
		assert(index < GetCount());
		auto& item = m_items[index];
		return item.info ? std::unique_ptr<QIODevice> { std::make_unique<QFile>(item.info->filePath()) } : std::make_unique<QBuffer>(&item.body);
	}

private: // IZipFileController
	void AddFile(QString name, QByteArray body, QDateTime time) override
	{
		m_items.emplace_back(std::move(name), std::move(body), std::move(time));
	}

	void AddFile(const QString& path) override
	{
		assert(QFile::exists(path));
		auto info                                                                                        = std::make_unique<QFileInfo>(path);
		m_items.emplace_back(info->fileName(), QByteArray {}, info->fileTime(QFile::FileBirthTime)).info = std::move(info);
	}

private:
	std::vector<Item> m_items;
};

} // namespace

class Zip::Impl
{
public:
	Impl(const QString& filename, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateReader(filename, GetProgress(std::move(progress))))
		, m_file(std::unique_ptr<IFile> {})
	{
	}

	Impl(QIODevice& stream, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateReaderStream(stream, GetProgress(std::move(progress))))
		, m_file(std::unique_ptr<IFile> {})
	{
	}

	Impl(const QString& filename, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateWriter(filename, format, GetProgress(std::move(progress)), appendMode))
		, m_file(std::unique_ptr<IFile> {})
	{
	}

	Impl(QIODevice& stream, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateWriterStream(stream, format, GetProgress(std::move(progress)), appendMode))
		, m_file(std::unique_ptr<IFile> {})
	{
	}

	void SetProperty(const PropertyId id, QVariant value)
	{
		m_zip->SetProperty(id, std::move(value));
	}

	QStringList GetFileNameList() const
	{
		return m_zip->GetFileNameList();
	}

	std::unique_ptr<Stream> Read(const QString& filename)
	{
		m_file.reset();
		m_file.reset(m_zip->Read(filename));
		return m_file->Read();
	}

	bool Write(std::shared_ptr<IZipFileProvider> zipFileProvider)
	{
		return m_zip->Write(std::move(zipFileProvider));
	}

	bool Remove(const std::vector<QString>& fileNames)
	{
		return m_zip->Remove(fileNames);
	}

	size_t GetFileSize(const QString& filename) const
	{
		return m_zip->GetFileSize(filename);
	}

	const QDateTime& GetFileTime(const QString& filename) const
	{
		return m_zip->GetFileTime(filename);
	}

private:
	PropagateConstPtr<IZip>  m_zip;
	PropagateConstPtr<IFile> m_file;
};

bool Zip::IsArchive(const QString& filename)
{
	return SevenZip::Archive::IsArchive(filename);
}

QStringList Zip::GetTypes()
{
	return SevenZip::Archive::GetTypes();
}

Zip::Zip(const QString& filename, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(filename, std::move(progress)))
{
}

Zip::Zip(QIODevice& stream, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(stream, std::move(progress)))
{
}

Zip::Zip(const QString& filename, const Format format, bool appendMode, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(filename, format, appendMode, std::move(progress)))
{
}

Zip::Zip(QIODevice& stream, Format format, bool appendMode, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(stream, format, appendMode, std::move(progress)))
{
}

Zip::~Zip() = default;

void Zip::SetProperty(const PropertyId id, QVariant value)
{
	m_impl->SetProperty(id, std::move(value));
}

std::unique_ptr<Stream> Zip::Read(const QString& filename) const
{
	return m_impl->Read(filename);
}

QStringList Zip::GetFileNameList() const
{
	return m_impl->GetFileNameList();
}

bool Zip::Write(std::shared_ptr<IZipFileProvider> zipFileProvider)
{
	return m_impl->Write(std::move(zipFileProvider));
}

bool Zip::Remove(const std::vector<QString>& fileNames)
{
	return m_impl->Remove(fileNames);
}

size_t Zip::GetFileSize(const QString& filename) const
{
	return m_impl->GetFileSize(filename);
}

const QDateTime& Zip::GetFileTime(const QString& filename) const
{
	return m_impl->GetFileTime(filename);
}

Zip::Format Zip::FormatFromString(const QString& str)
{
	return FindSecond(ZIP_FORMATS, str.toStdString().data(), PszComparerCaseInsensitive {});
}

QString Zip::FormatToString(const Format format)
{
	return FindFirst(ZIP_FORMATS, format);
}

std::shared_ptr<IZipFileController> Zip::CreateZipFileController()
{
	return std::make_shared<ZipFileController>();
}

std::ostream& operator<<(std::ostream& stream, const Zip::Format format)
{
	return stream << FindFirst(ZIP_FORMATS, format);
}
