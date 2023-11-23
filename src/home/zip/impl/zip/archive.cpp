#include "archive.h"

#include <quazip/quazip.h>

#include "interface/error.h"
#include "interface/zip.h"
#include "interface/file.h"
#include "file.h"

namespace HomeCompa::Zip::Impl::Zip {

namespace {

class QuaZipImpl final : virtual public IZip
{
public:
	QuaZipImpl(const QString & filename, const QuaZip::Mode mode)
		: m_zip(std::make_unique<QuaZip>(filename))
	{
		if (!m_zip->open(mode))
		{
			switch (mode)
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
		return m_zip->getFileNameList();
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

private:
	std::unique_ptr<QuaZip> m_zip;
};

}

std::unique_ptr<IZip> Archive::CreateReader(const QString & filename)
{
	return std::make_unique<QuaZipImpl>(filename, QuaZip::Mode::mdUnzip);
}

std::unique_ptr<IZip> Archive::CreateWriter(const QString & filename)
{
	return std::make_unique<QuaZipImpl>(filename, QuaZip::Mode::mdCreate);
}

}
