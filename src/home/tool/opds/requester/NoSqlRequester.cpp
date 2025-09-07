#include "NoSqlRequester.h"

#include <QBuffer>
#include <QByteArray>
#include <QEventLoop>
#include <QFileInfo>
#include <QIODevice>

#include "fnd/ScopedCall.h"

#include "logic/shared/ImageRestore.h"
#include "util/AnnotationControllerObserver.h"
#include "util/localization.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{
const auto CONTEXT = "Http";
const auto LOGIN = QT_TRANSLATE_NOOP("Http", "Login");
const auto PASSWORD = QT_TRANSLATE_NOOP("Http", "Password");
const auto ENTER_LOGIN = QT_TRANSLATE_NOOP("Http", "Enter Username");
const auto ENTER_PASSWORD = QT_TRANSLATE_NOOP("Http", "Enter Password");
TR_DEF

QByteArray Decompress(const QString& path, const QString& archive, const QString& fileName, const bool restoreImages)
{
	const Zip unzip(path + "/" + archive);
	const auto stream = unzip.Read(fileName);
	if (!restoreImages)
		return stream->GetStream().readAll();

	QByteArray data;
	{
		QBuffer buffer(&data);
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		buffer.write(Flibrary::RestoreImages(stream->GetStream(), path + "/" + archive, fileName));
	}
	return data;
}

QByteArray Compress(QByteArray data, QString fileName)
{
	QByteArray zippedData;
	{
		QBuffer buffer(&zippedData);
		const ScopedCall bufferGuard([&] { buffer.open(QIODevice::WriteOnly); }, [&] { buffer.close(); });
		buffer.open(QIODevice::WriteOnly);
		Zip zip(buffer, Zip::Format::Zip);
		auto zipFiles = Zip::CreateZipFileController();
		zipFiles->AddFile(std::move(fileName), std::move(data));
		zip.Write(std::move(zipFiles));
	}
	return zippedData;
}

} // namespace

struct NoSqlRequester::Impl
{
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
	std::shared_ptr<const ICoverCache> coverCache;
	std::shared_ptr<const IBookExtractor> bookExtractor;
	std::shared_ptr<Flibrary::IAnnotationController> annotationController;

	QByteArray GetCoverThumbnail(const QString& bookId) const
	{
		if (auto result = coverCache->Get(bookId); !result.isEmpty())
			return result;

		QEventLoop eventLoop;
		QByteArray result;

		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });
				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
					return (void)(result = std::move(Flibrary::Recode(covers.front().bytes).first));

				QFile file(":/images/book.png");
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
		auto book = bookExtractor->GetExtractedBook(bookId);
		auto outputFileName = bookExtractor->GetFileName(book);
		auto data = Decompress(collectionProvider->GetActiveCollection().folder, book.folder, book.file, restoreImages);

		return std::make_tuple(std::move(book.file), QFileInfo(outputFileName).fileName(), std::move(data));
	}
};

NoSqlRequester::NoSqlRequester(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                               std::shared_ptr<const ICoverCache> coverCache,
                               std::shared_ptr<const IBookExtractor> bookExtractor,
                               std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl { std::move(collectionProvider), std::move(coverCache), std::move(bookExtractor), std::move(annotationController) }
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
	data = Compress(std::move(data), std::move(fileName));
	return std::make_pair(QFileInfo(title).completeBaseName() + ".zip", std::move(data));
}

QByteArray NoSqlRequester::RequestAuth(const QString& title, const QString& url) const
{
	return QString(R"(
<html>
<body>
<h1>%1</h1>
<form action="%2" method="post">
  <div class="container">
    <label for="user"><b>%3</b></label>
    <input type="text" placeholder="%4" id="user" name="user" required>

    <label for="password"><b>%5</b></label>
    <input type="password" placeholder="%6" id="password" name="password" required>

    <button type="submit">Login</button>
  </div>
</form>
</body>
</html>
)")
	    .arg(title, url, Tr(LOGIN), Tr(ENTER_LOGIN), Tr(PASSWORD), Tr(ENTER_PASSWORD))
	    .toUtf8();
}
