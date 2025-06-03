#pragma once
#include <functional>

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Opds
{

class AnnotationControllerObserver : public Flibrary::IAnnotationController::IObserver
{
	using Functor = std::function<void(const Flibrary::IAnnotationController::IDataProvider& dataProvider)>;

public:
	explicit AnnotationControllerObserver(Functor f);

private: // IAnnotationController::IObserver
	void OnAnnotationRequested() override;
	void OnAnnotationChanged(const Flibrary::IAnnotationController::IDataProvider& dataProvider) override;
	void OnJokeTextChanged(const QString&) override;
	void OnJokeImageChanged(const QByteArray&) override;
	void OnArchiveParserProgress(int percents) override;

private:
	const Functor m_f;
};

} // namespace HomeCompa::Opds
