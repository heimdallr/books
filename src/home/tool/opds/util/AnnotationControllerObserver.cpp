#include "AnnotationControllerObserver.h"

using namespace HomeCompa::Opds;

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

void AnnotationControllerObserver::OnJokeErrorOccured(const QString&, const QString&)
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
