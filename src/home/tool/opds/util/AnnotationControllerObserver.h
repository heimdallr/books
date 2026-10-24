#pragma once
#include <functional>

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Opds {

class AnnotationControllerObserver : public Flibrary::IAnnotationController::IObserver
{
public:
	using Functor = std::function<void(const Flibrary::IAnnotationController::IDataProvider& dataProvider)>;

	/// Requests the annotation of one book from the shared AnnotationController and blocks until it arrives or a timeout passes.
	/// The controller serves one book at a time and belongs to the main thread, so calls from the HTTP worker threads are
	/// serialised, and the observer is registered and removed on the main thread. Returns false on timeout.
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
