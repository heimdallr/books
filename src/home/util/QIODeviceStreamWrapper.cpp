#include "QIODeviceStreamWrapper.h"

#include <QIODevice>

namespace HomeCompa
{

class QStdStreamBuf : public std::streambuf
{
public:
	explicit QStdStreamBuf(QIODevice& dev);

protected: // std::streambuf
	std::streamsize          xsgetn(std::streambuf::char_type* str, std::streamsize n) override;
	std::streamsize          xsputn(const std::streambuf::char_type* str, std::streamsize n) override;
	std::streambuf::pos_type seekoff(std::streambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode __mode) override;
	std::streambuf::pos_type seekpos(std::streambuf::pos_type off, std::ios_base::openmode /*__mode*/) override;
	std::streambuf::int_type underflow() override;

private:
	static constexpr std::streamsize BUFFER_SIZE = 1024;
	std::streambuf::char_type        m_inBuf[BUFFER_SIZE] {};
	QIODevice&                       m_dev;
};

QStdStreamBuf::QStdStreamBuf(QIODevice& dev)
	: m_dev { dev }
{
	// Initialize get pointer.  This should be zero so that underflow is called upon first read.
	this->setg(nullptr, nullptr, nullptr);
}

std::streamsize QStdStreamBuf::xsgetn(std::streambuf::char_type* str, const std::streamsize n)
{
	[[maybe_unused]] const auto isOpen = m_dev.isOpen();
	[[maybe_unused]] const auto pos    = m_dev.pos();

	return m_dev.read(str, n);
}

std::streamsize QStdStreamBuf::xsputn(const std::streambuf::char_type* str, const std::streamsize n)
{
	return m_dev.write(str, n);
}

std::streambuf::pos_type QStdStreamBuf::seekoff(std::streambuf::off_type off, const std::ios_base::seekdir dir, const std::ios_base::openmode mode)
{
	switch (dir)
	{
		case std::ios_base::beg:
			break;
		case std::ios_base::end:
			off = m_dev.size() - off;
			break;
		case std::ios_base::cur:
			off = m_dev.pos() + off;
			break;
	}

	const auto result = seekpos(off, mode);
	return result;
}

std::streambuf::pos_type QStdStreamBuf::seekpos(const std::streambuf::pos_type off, std::ios_base::openmode /*__mode*/) //-V801
{
	return std::streambuf::pos_type(m_dev.seek(off) ? m_dev.pos() : std::streambuf::off_type(-1));
}

std::streambuf::int_type QStdStreamBuf::underflow()
{
	// Read enough bytes to fill the buffer.
	const auto len = sgetn(m_inBuf, BUFFER_SIZE);

	// Since the input buffer content is now valid (or is new)
	// the get pointer should be initialized (or reset).
	setg(m_inBuf, m_inBuf, m_inBuf + len);

	// If nothing was read, then the end is here.
	if (len == 0)
		return traits_type::eof();

	// Return the first character.
	return traits_type::not_eof(m_inBuf[0]);
}

class QStdIStreamBuf : public QStdStreamBuf
{
public:
	QStdIStreamBuf(QIODevice& dev)
		: QStdStreamBuf(dev)
	{
	}
};

class QStdOStreamBuf : public QStdStreamBuf
{
public:
	QStdOStreamBuf(QIODevice& dev)
		: QStdStreamBuf(dev)
	{
	}
};

QStdIStream::QStdIStream(std::unique_ptr<QStdIStreamBuf> buf)
	: std::istream(buf.get())
	, m_buf(std::move(buf))
{
}

QStdIStream::~QStdIStream()
{
	rdbuf(nullptr);
}

std::unique_ptr<std::istream> QStdIStream::create(QIODevice& dev)
{
	auto buf = std::make_unique<QStdIStreamBuf>(dev);
	return std::unique_ptr<std::istream>(new QStdIStream(std::move(buf)));
}

QStdOStream::QStdOStream(std::unique_ptr<QStdOStreamBuf> buf)
	: std::ostream(buf.get())
	, m_buf(std::move(buf))
{
}

QStdOStream::~QStdOStream()
{
	rdbuf(nullptr);
}

std::unique_ptr<std::ostream> QStdOStream::create(QIODevice& dev)
{
	auto buf = std::make_unique<QStdOStreamBuf>(dev);
	return std::unique_ptr<std::ostream>(new QStdOStream(std::move(buf)));
}

} // namespace HomeCompa
