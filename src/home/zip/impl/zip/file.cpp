#include "file.h"

#include <quazip/quazipfile.h>

#include "fnd/FindPair.h"
#include "zip/interface/error.h"
#include "zip/interface/file.h"

namespace HomeCompa::ZipDetails::Impl::Zip {

class QuaZipImpl final : virtual public IFile
{
public:
	enum class Mode
	{
		Read,
		Write,
	};

public:
	explicit QuaZipImpl(QuaZip & zip, const QString & filename, const Mode mode)
		: m_zipFile(&zip)
	{
		using Opener = void (QuaZipImpl::*)(const QString &);
		static constexpr std::pair<Mode, Opener> openers[]
		{
			{Mode::Read, &QuaZipImpl::OpenRead},
			{Mode::Write, &QuaZipImpl::OpenWrite},
		};
		std::invoke(FindSecond(openers, mode), this, filename);
	}

private: // IFile
	QIODevice & Read() override
	{
		return m_zipFile;
	}

	QIODevice & Write() override
	{
		return Read();
	}

private:
	void OpenRead(const QString & filename)
	{
		if (!m_zipFile.open(QIODevice::ReadOnly))
			Error::CannotExtractFileFromArchive(filename);
	}

	void OpenWrite(const QString & filename)
	{
		if (!m_zipFile.open(QIODevice::WriteOnly, filename, nullptr, 0, Z_DEFLATED, Z_BEST_COMPRESSION))
			Error::CannotAddFileToArchive(filename);
	}

private:
	QuaZipFile m_zipFile;
};

std::unique_ptr<IFile> File::Read(QuaZip & zip, const QString & filename)
{
	return std::make_unique<QuaZipImpl>(zip, filename, QuaZipImpl::Mode::Read);
}

std::unique_ptr<IFile> File::Write(QuaZip & zip, const QString & filename)
{
	return std::make_unique<QuaZipImpl>(zip, filename, QuaZipImpl::Mode::Write);
}

}
