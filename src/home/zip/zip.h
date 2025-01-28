#pragma once

#include <memory>

#include <QStringList>

#include "zip/interface/format.h"
#include "zip/interface/ProgressCallback.h"
#include "zip/interface/stream.h"

#include "export/zip.h"

class QDateTime;

namespace HomeCompa {

class ZIP_EXPORT Zip
{
public:
	class ProgressCallback : public ZipDetails::ProgressCallback
	{
	};

	using Format = ZipDetails::Format;
	static Format FindFormat(const QString & str);

public:
	explicit Zip(const QString & filename, std::shared_ptr<ProgressCallback> progress = {});
	Zip(const QString & filename, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	Zip(QIODevice & stream, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	~Zip();

	[[nodiscard]] QStringList GetFileNameList() const;
	[[nodiscard]] std::unique_ptr<Stream> Read(const QString & filename) const;
	bool Write(const std::vector<QString> & fileNames, const ZipDetails::StreamGetter & streamGetter);
	bool Write(std::vector<std::pair<QString, QByteArray>> data);
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

ZIP_EXPORT std::ostream & operator<<(std::ostream & stream, HomeCompa::Zip::Format format);
