#pragma once

#include <iostream>

class QIODevice;

#include "export/Util.h"

namespace HomeCompa
{

class QStdIStream : public std::istream
{
public:
	UTIL_EXPORT static std::unique_ptr<std::istream> create(QIODevice& dev);

private:
	explicit QStdIStream(std::unique_ptr<class QStdIStreamBuf> buf);
	~QStdIStream() override;

private:
	std::unique_ptr<QStdIStreamBuf> m_buf;
};

class QStdOStream : public std::ostream
{
public:
	UTIL_EXPORT static std::unique_ptr<std::ostream> create(QIODevice& dev);

private:
	explicit QStdOStream(std::unique_ptr<class QStdOStreamBuf> buf);
	~QStdOStream() override;

private:
	std::unique_ptr<QStdOStreamBuf> m_buf;
};

} // namespace HomeCompa
