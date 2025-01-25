#include "zip.h"

#include "fnd/memory.h"

#include "zip/interface/file.h"
#include "zip/interface/zip.h"
#include "zip/factory/factory.h"

using namespace HomeCompa;
using namespace ZipDetails;

namespace {

class ProgressCallbackStub final
	: public Zip::ProgressCallback
{
public:
	void OnStartWithTotal(int64_t) override
	{
	}
	void OnIncrement(int64_t) override
	{
	}
	void OnDone() override
	{
	}
	void OnFileDone(const QString &) override
	{
	}
	bool OnCheckBreak() override
	{
		return false;
	}
};

}

class Zip::Impl
{
public:
	Impl(const QString & filename, std::shared_ptr<ProgressCallback> progress)
		: m_zip(Factory::Create(filename, progress ? std::move(progress) : std::make_shared<ProgressCallbackStub>()))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	Impl(const QString & filename, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(Factory::Create(filename, progress ? std::move(progress) : std::make_shared<ProgressCallbackStub>(), format, appendMode))
		, m_file(std::unique_ptr<IFile>{})
	{
	}

	Impl(QIODevice & stream, const Format format, const bool appendMode, std::shared_ptr<ProgressCallback> progress)
		: m_zip(Factory::Create(stream, progress ? std::move(progress) : std::make_shared<ProgressCallbackStub>(), format, appendMode))
		, m_file(std::unique_ptr<IFile>{})
	{
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

	std::unique_ptr<Stream> Write(const QString & filename)
	{
		m_file.reset();
		m_file.reset(m_zip->Write(filename));
		return m_file->Write();
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

std::unique_ptr<Stream> Zip::Read(const QString & filename) const
{
	return m_impl->Read(filename);
}

QStringList Zip::GetFileNameList() const
{
	return m_impl->GetFileNameList();
}

std::unique_ptr<Stream> Zip::Write(const QString & filename)
{
	return m_impl->Write(filename);
}

size_t Zip::GetFileSize(const QString & filename) const
{
	return m_impl->GetFileSize(filename);
}

const QDateTime & Zip::GetFileTime(const QString & filename) const
{
	return m_impl->GetFileTime(filename);
}
