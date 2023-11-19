#include "zip.h"

#include "fnd/FindPair.h"
#include "fnd/memory.h"

#include "interface/file.h"
#include "interface/zip.h"
#include "factory/factory.h"

using namespace HomeCompa::Zip;

namespace {

constexpr std::pair<Zip::Format, Factory::Format> FORMATS[]
{
	{Zip::Format::Zip, Factory::Format::Zip},
};

}

class Zip::Impl
{
public:
	explicit Impl(const QString & filename)
		: m_zip(Factory::Create(filename))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	Impl(const QString & filename, const Format format)
		: m_zip(Factory::Create(filename, FindSecond(FORMATS, format)))
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

	QIODevice & Write(const QString & filename)
	{
		m_file.reset(m_zip->Write(filename));
		return m_file->Write();
	}

private:
	PropagateConstPtr<IZip> m_zip;
	PropagateConstPtr<IFile> m_file;
};

Zip::Zip(const QString & filename)
	: m_impl(std::make_unique<Impl>(filename))
{
}

Zip::Zip(const QString & filename, const Format format)
	: m_impl(std::make_unique<Impl>(filename, format))
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

QIODevice & Zip::Write(const QString & filename)
{
	return m_impl->Write(filename);
}
