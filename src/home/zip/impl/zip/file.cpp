#include "file.h"

#include <quazip>

#include "interface/error.h"
#include "interface/file.h"

namespace HomeCompa::Zip::Impl {

class Reader final : virtual public IFile
{
public:
	explicit Reader(QuaZip & zip, const QString & filename)
		: m_zipFile(&zip)
	{
		if (!zip.setCurrentFile(filename))
			Error::CannotFindFileInArchive(filename);

		if (!m_zipFile.open(QIODevice::ReadOnly))
			Error::CannotExtractFileFromArchive(filename);
	}

private: // IFile
	QIODevice & Read() override
	{
		return m_zipFile;
	}

private:
	QuaZipFile m_zipFile;
};

std::unique_ptr<IFile> File::Read(QuaZip & zip, const QString & filename)
{
	return std::make_unique<Reader>(zip, filename);
}

}
