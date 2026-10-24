#include "AnnotationControllerObserver.h"

#include <mutex>

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include "log.h"

using namespace HomeCompa::Opds;

namespace {

// A book whose archive is missing or unreadable never produces an annotation; it must not block a request forever
constexpr auto ANNOTATION_TIMEOUT = std::chrono::seconds(30);

template <typename F>
void RunOnMainThread(F&& f)
{
	auto* app = QCoreApplication::instance();
	if (QThread::currentThread() == app->thread())
		return f();

	[[maybe_unused]] const auto ok = QMetaObject::invokeMethod(app, std::forward<F>(f), Qt::BlockingQueuedConnection);
	assert(ok);
}

} // namespace

bool AnnotationControllerObserver::Request(Flibrary::IAnnotationController& controller, const QString& bookId, const Functor& onAnnotation)
{
	static std::mutex requestGuard;
	std::lock_guard   lock(requestGuard);

	QEventLoop                   eventLoop;
	bool                         received = false;
	AnnotationControllerObserver observer([&](const Flibrary::IAnnotationController::IDataProvider& dataProvider) {
		if (received)
			return;

		received = true;
		onAnnotation(dataProvider);
		eventLoop.exit();
	});
	QTimer::singleShot(ANNOTATION_TIMEOUT, &eventLoop, [&] {
		eventLoop.exit();
	});

	RunOnMainThread([&] {
		controller.RegisterObserver(&observer);
		controller.SetCurrentBookId(bookId, true);
	});
	eventLoop.exec();
	RunOnMainThread([&] {
		controller.UnregisterObserver(&observer);
	});

	if (!received)
	{
		PLOGW << "Annotation of book " << bookId << " was not received in time";
	}
	return received;
}

AnnotationControllerObserver::AnnotationControllerObserver(Functor f)
	: m_f { std::move(f) }
{
}

void AnnotationControllerObserver::OnAnnotationRequested()
{
}

void AnnotationControllerObserver::OnAnnotationChanged(const Flibrary::IAnnotationController::IDataProvider& dataProvider)
{
	m_f(dataProvider);
}

void AnnotationControllerObserver::OnJokeErrorOccurred(const QString&, const QString&)
{
}

void AnnotationControllerObserver::OnJokeTextChanged(const QString&)
{
}

void AnnotationControllerObserver::OnJokeImageChanged(const QByteArray&)
{
}

void AnnotationControllerObserver::OnArchiveParserProgress(int /*percents*/)
{
}
