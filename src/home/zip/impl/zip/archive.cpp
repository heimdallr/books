#include "archive.h"

#include <ranges>

#include <quazip/quazip.h>

#include "zip/interface/error.h"
#include "zip/interface/zip.h"
#include "zip/interface/file.h"
#include "file.h"

namespace HomeCompa::ZipDetails::Impl::Zip {

namespace {

class QuaZipImpl final : virtual public IZip
{
	struct FileItem
	{
		size_t size;
		QDateTime time;
	};

public:
	QuaZipImpl(const QString & filename, const QuaZip::Mode mode)
		: m_zip(std::make_unique<QuaZip>(filename))
	{
		if (!m_zip->open(mode))
		{
			switch (mode)  // NOLINT(clang-diagnostic-switch-enum)
			{
				case QuaZip::Mode::mdUnzip:
					Error::CannotOpenFile(filename);

				case QuaZip::Mode::mdCreate:
					Error::CannotCreateArchive(filename);

				default:
					assert(false && "unexpected mode");
			}
		}

		if (mode == QuaZip::Mode::mdCreate)
			m_zip->setUtf8Enabled(true);
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		CreateFileList();
		QStringList result;
		result.reserve(static_cast<int>(m_files.size()));
		std::ranges::copy(m_files | std::views::keys, std::back_inserter(result));
		return result;
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		if (!m_zip->setCurrentFile(filename))
			Error::CannotFindFileInArchive(filename);

		return File::Read(*m_zip, filename);
	}

	std::unique_ptr<IFile> Write(const QString & filename) override
	{
		return File::Write(*m_zip, filename);
	}

	size_t GetFileSize(const QString & filename) const override
	{
		CreateFileList();
		const auto it = m_files.find(filename);
		assert(it != m_files.end());
		return it->second.size;
	}

	const QDateTime & GetFileTime(const QString& filename) const override
	{
		CreateFileList();
		const auto it = m_files.find(filename);
		assert(it != m_files.end());
		return it->second.time;
	}

private:
	void CreateFileList() const
	{
		if (!m_files.empty())
			return;

		std::ranges::transform(m_zip->getFileInfoList64(), std::inserter(m_files, m_files.end()), [] (const auto & item)
		{
			return std::make_pair(item.name, FileItem {item.uncompressedSize, item.dateTime});
		});
	}

private:
	std::unique_ptr<QuaZip> m_zip;
	mutable std::unordered_map<QString, FileItem> m_files;
};

}

std::unique_ptr<IZip> Archive::CreateReader(const QString & filename, std::shared_ptr<ProgressCallback> /*progress*/)
{
	return std::make_unique<QuaZipImpl>(filename, QuaZip::Mode::mdUnzip);
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & filename, std::shared_ptr<ProgressCallback> /*progress*/)
{
	return std::make_unique<QuaZipImpl>(filename, QuaZip::Mode::mdCreate);
}

}
