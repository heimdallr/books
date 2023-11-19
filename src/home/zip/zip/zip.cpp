#include "zip.h"

#include "fnd/memory.h"

#include "interface/file.h"
#include "interface/zip.h"
#include "factory/factory.h"

using namespace HomeCompa::Zip;

class Zip::Impl
{
public:
	explicit Impl(const QString & filename)
		: m_zip(Factory::Create(filename))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	QStringList GetFileNameList() const
	{
		return m_zip->GetFileNameList();
	}

	QIODevice & Read(const QString & filename)
	{
		m_file.reset(m_zip->Read(filename));
		return m_file->Read();
	}

private:
	PropagateConstPtr<IZip> m_zip;
	PropagateConstPtr<IFile> m_file;
};

Zip::Zip(const QString & filename)
	: m_impl(std::make_unique<Impl>(filename))
{
}

Zip::~Zip() = default;

QIODevice & Zip::Read(const QString & filename) const
{
	return m_impl->Read(filename);
}

QStringList Zip::GetFileNameList() const
{
	return m_impl->GetFileNameList();
}
