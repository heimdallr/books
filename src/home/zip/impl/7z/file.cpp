#include <atlcomcli.h>

#include "file.h"

#include <QBuffer>

#include "zip/interface/file.h"
#include "zip/interface/ProgressCallback.h"
#include "zip/interface/stream.h"

#include "MemExtractCallback.h"
#include "PropVariant.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

class StreamImpl : public Stream
{
public:
	explicit StreamImpl(QByteArray bytes)
		: m_bytes(std::move(bytes))
		, m_buffer(&m_bytes)
	{
	}

private: // Stream
	QIODevice & GetStream() override
	{
		m_buffer.open(QIODevice::ReadOnly);
		return m_buffer;
	}

private:
	QByteArray m_bytes;
	QBuffer m_buffer;
};

class FileReader : virtual public IFile
{
public:
	FileReader(CComPtr<IInArchive> zip, const uint32_t index, std::shared_ptr<ProgressCallback> progress)
		: m_zip(std::move(zip))
		, m_index(index)
		, m_progress(std::move(progress))
	{
	}

private:
	std::unique_ptr<Stream> Read() override
	{
		const UInt32 indices[] = { m_index };
		QByteArray bytes;
		const auto callback = MemExtractCallback::Create(m_zip, bytes, m_progress);

		CPropVariant prop;
		m_zip->GetProperty(m_index, kpidSize, &prop);
		m_progress->OnStartWithTotal(static_cast<int64_t>(prop.uhVal.QuadPart));

		m_zip->Extract(indices, 1, 0, callback);
		m_progress->OnDone();

		return std::make_unique<StreamImpl>(std::move(bytes));
	}

	std::unique_ptr<Stream> Write() override
	{
		throw std::runtime_error("Cannot write with reader");
	}

private:
	CComPtr<IInArchive> m_zip;
	const uint32_t m_index;
	std::shared_ptr<ProgressCallback> m_progress;
};

}

std::unique_ptr<IFile> File::Read(CComPtr<IInArchive> zip, const uint32_t index, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<FileReader>(std::move(zip), index, std::move(progress));
}

}
