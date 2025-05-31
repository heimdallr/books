#include "NoSqlRequester.h"

#include <QByteArray>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>

#include "fnd/ScopedCall.h"

#include "util/AnnotationControllerObserver.h"

using namespace HomeCompa::Opds;

struct NoSqlRequester::Impl
{
	std::shared_ptr<const ICoverCache> coverCache;
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
					if (const auto coverIndex = dataProvider.GetCoverIndex())
					{
						result = covers[*coverIndex].bytes;
						return;
					}

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
};

NoSqlRequester::NoSqlRequester(std::shared_ptr<const ICoverCache> coverCache, std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl { std::move(coverCache), std::move(annotationController) }
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
