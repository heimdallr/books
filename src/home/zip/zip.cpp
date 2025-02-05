#include "zip.h"

#include <ranges>

#include "fnd/FindPair.h"
#include "fnd/memory.h"

#include "zip/interface/file.h"
#include "zip/interface/zip.h"

#include "impl/archive.h"

#include <QBuffer>
#include <QVariant>

using namespace HomeCompa;
using namespace ZipDetails;

namespace {

std::shared_ptr<ProgressCallback> GetProgress(std::shared_ptr<ProgressCallback> progress)
{
	if (progress)
		return progress;

	return std::make_shared<ProgressCallbackStub>();
}

constexpr std::pair<const char *, Zip::Format> ZIP_FORMATS[]
{
	{ "zip", Zip::Format::Zip },
	{ "7z", Zip::Format::SevenZip },
};

}

class Zip::Impl
{
public:
	Impl(const QString & filename, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateReader(filename, GetProgress(std::move(progress))))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	Impl(const QString & filename, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateWriter(filename, format, GetProgress(std::move(progress)), appendMode))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	Impl(QIODevice & stream, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(SevenZip::Archive::CreateWriterStream(stream, format, GetProgress(std::move(progress)), appendMode))
		, m_file(std::unique_ptr<IFile>{})
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

	std::unique_ptr<Stream> Read(const QString & filename)
	{
		m_file.reset();
		m_file.reset(m_zip->Read(filename));
		return m_file->Read();
	}

	bool Write(const std::vector<QString> & fileNames, const StreamGetter & streamGetter, const SizeGetter & sizeGetter)
	{
		return m_zip->Write(fileNames, streamGetter, sizeGetter);
	}

	void Remove(const std::vector<QString>& fileNames)
	{
		m_zip->Remove(fileNames);
	}

	size_t GetFileSize(const QString & filename) const
	{
		return m_zip->GetFileSize(filename);
	}

	const QDateTime & GetFileTime(const QString & filename) const
	{
		return m_zip->GetFileTime(filename);
	}

private:
	PropagateConstPtr<IZip> m_zip;
	PropagateConstPtr<IFile> m_file;
};

Zip::Zip(const QString & filename, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(filename, std::move(progress)))
{
}

Zip::Zip(const QString & filename, const Format format, bool appendMode, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(filename, format, appendMode, std::move(progress)))
{
}

Zip::Zip(QIODevice & stream, Format format, bool appendMode, std::shared_ptr<ProgressCallback> progress)
	: m_impl(std::make_unique<Impl>(stream, format, appendMode, std::move(progress)))
{
}

Zip::~Zip() = default;

void Zip::SetProperty(const PropertyId id, QVariant value)
{
	m_impl->SetProperty(id, std::move(value));
}

std::unique_ptr<Stream> Zip::Read(const QString & filename) const
{
	return m_impl->Read(filename);
}

QStringList Zip::GetFileNameList() const
{
	return m_impl->GetFileNameList();
}

bool Zip::Write(const std::vector<QString> & fileNames, const StreamGetter & streamGetter, const SizeGetter & sizeGetter)
{
	return sizeGetter
		? m_impl->Write(fileNames, streamGetter, sizeGetter)
		: m_impl->Write(fileNames, streamGetter, [&] (const size_t index)
	{
		const auto stream = streamGetter(index);
		if (!stream->isOpen())
			stream->open(QIODevice::ReadOnly);
		return static_cast<size_t>(stream->size());
	});

}

bool Zip::Write(std::vector<std::pair<QString, QByteArray>> data)
{
	std::vector<QString> fileNames;
	fileNames.reserve(data.size());
	std::ranges::move(data | std::views::keys, std::back_inserter(fileNames));
	return Write(fileNames, [&] (const size_t index)
	{
		return std::make_unique<QBuffer>(&data[index].second);
	}, [&] (const size_t index)
	{
		return static_cast<size_t>(data[index].second.size());
	});
}

void Zip::Remove(const std::vector<QString>& fileNames)
{
	m_impl->Remove(fileNames);
}

size_t Zip::GetFileSize(const QString & filename) const
{
	return m_impl->GetFileSize(filename);
}

const QDateTime & Zip::GetFileTime(const QString & filename) const
{
	return m_impl->GetFileTime(filename);
}

Zip::Format Zip::FormatFromString(const QString & str)
{
	return FindSecond(ZIP_FORMATS, str.toStdString().data(), PszComparer {});
}

QString Zip::FormatToString(const Format format)
{
	return FindFirst(ZIP_FORMATS, format);
}

std::ostream & operator<<(std::ostream & stream, const Zip::Format format)
{
	return stream << FindFirst(ZIP_FORMATS, format);
}
