#include "zip.h"

#include <quazip>

#include "interface/error.h"
#include "interface/zip.h"
#include "interface/file.h"
#include "file.h"

namespace HomeCompa::Zip::Impl {

namespace {

class ZipReader final : virtual public IZip
{
public:
	explicit ZipReader(const QString & filename)
		: m_zip(std::make_unique<QuaZip>(filename))
	{
		if (!m_zip->open(QuaZip::Mode::mdUnzip))
			Error::CannotOpenFile(filename);
	}

private: // IZip
	QStringList GetFileNameList() const override
	{
		return m_zip->getFileNameList();
	}

	std::unique_ptr<IFile> Read(const QString & filename) const override
	{
		return File::Read(*m_zip, filename);
	}

private:
	std::unique_ptr<QuaZip> m_zip;
};

}

std::unique_ptr<IZip> Zip::Create(const QString & filename)
{
	return std::make_unique<ZipReader>(filename);
}

}
