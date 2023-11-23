#include <atlcomcli.h>

#include "file.h"


#include <QBuffer>

#include "interface/error.h"
#include "interface/file.h"
#include "MemExtractCallback.h"

#include "7z/CPP/7zip/Archive/IArchive.h"

namespace HomeCompa::Zip::Impl::SevenZip {

namespace {

class FileReader : virtual public IFile
{
public:
	FileReader(CComPtr<IInArchive> zip, const uint32_t index)
		: m_zip(std::move(zip))
		, m_index(index)
	{
	}

private:
	QIODevice & Read() override
	{
		const UInt32 indices[] = { m_index };
		const auto callback = MemExtractCallback::Create(m_zip, m_bytes);
		m_zip->Extract(indices, 1, false, callback);
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
	QByteArray m_bytes;
	std::unique_ptr<QBuffer> m_buffer;
};

}

std::unique_ptr<IFile> File::Read(CComPtr<IInArchive> zip, const uint32_t index)
{
	return std::make_unique<FileReader>(std::move(zip), index);
}

}