#pragma once

#include <memory>

#include <QStringList>

#include "zip/interface/ProgressCallback.h"
#include "zip/interface/format.h"
#include "zip/interface/stream.h"
#include "zip/interface/types.h"

#include "export/zip.h"

class QDateTime;

namespace HomeCompa
{

class ZIP_EXPORT Zip
{
public:
	class ProgressCallback : public ZipDetails::ProgressCallback
	{
	};

	using Format = ZipDetails::Format;
	using PropertyId = ZipDetails::PropertyId;
	using CompressionLevel = ZipDetails::CompressionLevel;
	using CompressionMethod = ZipDetails::CompressionMethod;

	static Format FormatFromString(const QString& str);
	static QString FormatToString(Format format);

public:
	static bool IsArchive(const QString& filename);

public:
	explicit Zip(const QString& filename, std::shared_ptr<ProgressCallback> progress = {});
	explicit Zip(QIODevice& stream, std::shared_ptr<ProgressCallback> progress = {});
	Zip(const QString& filename, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	Zip(QIODevice& stream, Format format, bool appendMode = false, std::shared_ptr<ProgressCallback> progress = {});
	~Zip();

	[[nodiscard]] QStringList GetFileNameList() const;
	[[nodiscard]] size_t GetFileSize(const QString& filename) const;
	[[nodiscard]] const QDateTime& GetFileTime(const QString& filename) const;

	[[nodiscard]] std::unique_ptr<Stream> Read(const QString& filename) const;

	void SetProperty(PropertyId id, QVariant value);
	bool Write(const std::vector<QString>& fileNames, const ZipDetails::StreamGetter& streamGetter, const ZipDetails::SizeGetter& sizeGetter = {});
	bool Write(std::vector<std::pair<QString, QByteArray>> data);

	bool Remove(const std::vector<QString>& fileNames);

public:
	Zip(const Zip&) = delete;
	Zip(Zip&&) = default;
	Zip& operator=(const Zip&) = delete;
	Zip& operator=(Zip&&) = default;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa

ZIP_EXPORT std::ostream& operator<<(std::ostream& stream, HomeCompa::Zip::Format format);
