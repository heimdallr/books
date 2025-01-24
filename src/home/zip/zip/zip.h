#pragma once

#include <memory>

#include <QStringList>

#include "export/zip.h"

#include "zip/interface/ProgressCallback.h"
#include "zip/interface/stream.h"

class QDateTime;

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
	[[nodiscard]] std::unique_ptr<Stream> Read(const QString & filename) const;
	[[nodiscard]] std::unique_ptr<Stream> Write(const QString & filename);
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
