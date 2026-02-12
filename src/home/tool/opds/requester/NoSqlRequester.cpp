#include "NoSqlRequester.h"

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QIODevice>
#include <QStandardPaths>
#include <QUuid>

#include "fnd/ScopedCall.h"

#include "interface/localization.h"

#include "util/AnnotationControllerObserver.h"
#include "util/ImageRestore.h"
#include "util/ImageUtil.h"
#include "util/ProcessWrapper.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa::Opds;
using namespace HomeCompa;

namespace
{

QByteArray Decompress(const QString& path, const QString& archive, const QString& fileName, const bool restoreImages)
{
	const Zip  unzip(path + "/" + archive);
	const auto stream = unzip.Read(fileName);
	if (!restoreImages)
		return stream->GetStream().readAll();

	QByteArray data;
	{
		QBuffer          buffer(&data);
		const ScopedCall bufferGuard(
			[&] {
				buffer.open(QIODevice::WriteOnly);
			},
			[&] {
				buffer.close();
			}
		);
		buffer.write(Util::PrepareToExport(stream->GetStream(), path + "/" + archive, fileName));
	}
	return data;
}

QByteArray Compress(QByteArray data, QString fileName)
{
	QByteArray zippedData;
	{
		QBuffer          buffer(&zippedData);
		const ScopedCall bufferGuard(
			[&] {
				buffer.open(QIODevice::WriteOnly);
			},
			[&] {
				buffer.close();
			}
		);
		buffer.open(QIODevice::WriteOnly);
		Zip  zip(buffer, Zip::Format::Zip);
		auto zipFiles = Zip::CreateZipFileController();
		zipFiles->AddFile(std::move(fileName), std::move(data));
		zip.Write(std::move(zipFiles));
	}
	return zippedData;
}

} // namespace

struct NoSqlRequester::Impl
{
	std::shared_ptr<const ISettings>                     settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
	std::shared_ptr<const Flibrary::IBookExtractor>      bookExtractor;
	std::shared_ptr<const ICoverCache>                   coverCache;
	std::shared_ptr<Flibrary::IAnnotationController>     annotationController;

	QByteArray GetCoverThumbnail(const QString& bookId) const
	{
		if (auto result = coverCache->Get(bookId); !result.isEmpty())
			return result;

		QEventLoop eventLoop;
		QByteArray result;

		AnnotationControllerObserver observer([&](const Flibrary::IAnnotationController::IDataProvider& dataProvider) {
			ScopedCall eventLoopGuard([&] {
				eventLoop.exit();
			});
			if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
				return (void)(result = std::move(Util::Recode(covers.front().bytes).first));

			QFile                       file(":/images/book.png");
			[[maybe_unused]] const auto ok = file.open(QIODevice::ReadOnly);
			assert(ok);
			result = file.readAll();
		});

		annotationController->RegisterObserver(&observer);
		annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return result;
	}

	std::tuple<QString, QString, QByteArray> GetBook(const QString& bookId, const bool restoreImages) const
	{
		auto book           = bookExtractor->GetExtractedBook(bookId);
		auto outputFileName = bookExtractor->GetFileName(book);
		auto data           = Decompress(collectionProvider->GetActiveCollection().GetFolder(), book.folder, book.file, restoreImages);

		Convert(outputFileName, data);

		return std::make_tuple(std::move(book.file), QFileInfo(outputFileName).fileName(), std::move(data));
	}

private:
	void Convert(QString& outputFileName, QByteArray& data) const
	{
		const auto command = settings->Get(CONVERTER_COMMAND).toString();
		if (command.isEmpty())
			return;

		const QFileInfo fileInfo(outputFileName);
		const auto      src = QString("%1/%2.%3").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), QUuid::createUuid().toString(QUuid::WithoutBraces), fileInfo.suffix());
		ScopedCall      srcGuard([&] {
            QFile::remove(src);
        });
		{
			QFile file(src);
			if (!file.open(QIODevice::WriteOnly))
			{
				PLOGE << "Cannot write to " << src;
				return;
			}
			file.write(data);
		}
		const auto ext = settings->Get(CONVERTER_EXT, fileInfo.suffix());
		const auto dst = QString("%1/%2.%3").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), QUuid::createUuid().toString(QUuid::WithoutBraces), ext);
		ScopedCall dstGuard([&] {
			QFile::remove(dst);
		});

		auto arguments = settings->Get(CONVERTER_ARGUMENTS, QString(R"("%1" "%2")").arg(src, dst));
		arguments.replace("%src%", src);
		arguments.replace("%dst%", dst);

		if (!Util::RunProcess(command, arguments, settings->Get(CONVERTER_CWD).toString()))
			return;

		QFile file(dst);
		if (!file.open(QIODevice::ReadOnly))
		{
			PLOGE << "Cannot read from " << dst;
			return;
		}
		data = file.readAll();
		file.close();

		outputFileName = fileInfo.dir().filePath(QString("%1.%2").arg(fileInfo.completeBaseName(), ext));
	}
};

NoSqlRequester::NoSqlRequester(
	std::shared_ptr<const ISettings>                     settings,
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	std::shared_ptr<const Flibrary::IBookExtractor>      bookExtractor,
	std::shared_ptr<const ICoverCache>                   coverCache,
	std::shared_ptr<Flibrary::IAnnotationController>     annotationController
)
	: m_impl { std::move(settings), std::move(collectionProvider), std::move(bookExtractor), std::move(coverCache), std::move(annotationController) }
{
}

NoSqlRequester::~NoSqlRequester() = default;

QByteArray NoSqlRequester::GetCover(const QString& bookId) const
{
	return m_impl->GetCoverThumbnail(bookId);
}

QByteArray NoSqlRequester::GetCoverThumbnail(const QString& bookId) const
{
	return GetCover(bookId);
}

std::pair<QString, QByteArray> NoSqlRequester::GetBook(const QString& bookId, const bool restoreImages) const
{
	auto [_, title, data] = m_impl->GetBook(bookId, restoreImages);
	return std::make_pair(title, std::move(data));
}

std::pair<QString, QByteArray> NoSqlRequester::GetBookZip(const QString& bookId, const bool restoreImages) const
{
	auto [fileName, title, data] = m_impl->GetBook(bookId, restoreImages);
	data                         = Compress(std::move(data), std::move(fileName));
	return std::make_pair(QFileInfo(title).completeBaseName() + ".zip", std::move(data));
}
