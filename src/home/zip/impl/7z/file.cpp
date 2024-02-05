#include <atlcomcli.h>

#include "file.h"

#include <QBuffer>

#include "zip/interface/file.h"
#include "zip/interface/ProgressCallback.h"

#include "MemExtractCallback.h"
#include "PropVariant.h"

namespace HomeCompa::ZipDetails::Impl::SevenZip {

namespace {

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
	QIODevice & Read() override
	{
		const UInt32 indices[] = { m_index };
		const auto callback = MemExtractCallback::Create(m_zip, m_bytes, m_progress);

		CPropVariant prop;
		m_zip->GetProperty(m_index, kpidSize, &prop);
		m_progress->OnStartWithTotal(prop.uhVal.QuadPart);

		m_zip->Extract(indices, 1, 0, callback);
		m_progress->OnDone();

		m_buffer = std::make_unique<QBuffer>(&m_bytes);
		m_buffer->open(QIODevice::ReadOnly);
		return *m_buffer;
	}

	QIODevice & Write() override
	{
		throw std::runtime_error("Cannot write with reader");
	}

private:
	CComPtr<IInArchive> m_zip;
	const uint32_t m_index;
	std::shared_ptr<ProgressCallback> m_progress;
	QByteArray m_bytes;
	std::unique_ptr<QBuffer> m_buffer;
};

}

std::unique_ptr<IFile> File::Read(CComPtr<IInArchive> zip, const uint32_t index, std::shared_ptr<ProgressCallback> progress)
{
	return std::make_unique<FileReader>(std::move(zip), index, std::move(progress));
}

}
