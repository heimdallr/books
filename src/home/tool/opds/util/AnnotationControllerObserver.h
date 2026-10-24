#pragma once
#include <functional>

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Opds {

class AnnotationControllerObserver : public Flibrary::IAnnotationController::IObserver
{
public:
	using Functor = std::function<void(const Flibrary::IAnnotationController::IDataProvider& dataProvider)>;

public:
	static bool Request(Flibrary::IAnnotationController& controller, const QString& bookId, const Functor& onAnnotation);

public:
	explicit AnnotationControllerObserver(Functor f);

private: // IAnnotationController::IObserver
	void OnAnnotationRequested() override;
	void OnAnnotationChanged(const Flibrary::IAnnotationController::IDataProvider& dataProvider) override;
	void OnJokeErrorOccurred(const QString& api, const QString& error) override;
	void OnJokeTextChanged(const QString&) override;
	void OnJokeImageChanged(const QByteArray&) override;
	void OnArchiveParserProgress(int percents) override;

private:
	const Functor m_f;
};

} // namespace HomeCompa::Opds
