#pragma once

#include <memory>

#include <QStringList>

#include "export/zip.h"

#include "zip/interface/ProgressCallback.h"

class QDateTime;
class QIODevice;

namespace HomeCompa {

class ZIP_EXPORT Zip
{
public:
	enum class Format
	{
		Zip,
	};

	class ProgressCallback : public ZipDetails::ProgressCallback
	{
	};

public:
	explicit Zip(const QString & filename, std::shared_ptr<ProgressCallback> progress = {});
	Zip(const QString & filename, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	Zip(QIODevice & stream, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	~Zip();

	[[nodiscard]] QStringList GetFileNameList() const;
	[[nodiscard]] QIODevice & Read(const QString & filename) const;
	[[nodiscard]] QIODevice & Write(const QString & filename);
	[[nodiscard]] size_t GetFileSize(const QString & filename) const;
	[[nodiscard]] const QDateTime & GetFileTime(const QString & filename) const;

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
