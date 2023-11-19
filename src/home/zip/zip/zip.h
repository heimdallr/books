#pragma once

#include <memory>

#include <QStringList>

#include "export/zip.h"

class QIODevice;

namespace HomeCompa::Zip {

class ZIP_EXPORT Zip
{
public:
	enum class Format
	{
		Zip,
	};

public:
	explicit Zip(const QString & filename);
	Zip(const QString & filename, Format format);
	~Zip();

	[[nodiscard]] QStringList GetFileNameList() const;
	[[nodiscard]] QIODevice & Read(const QString & filename) const;
	[[nodiscard]] QIODevice & Write(const QString & filename);

public:
	Zip(const Zip &) = delete;
	Zip(Zip &&) = default;
	Zip & operator=(const Zip &) = delete;
	Zip & operator=(Zip &&) = default;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

}
