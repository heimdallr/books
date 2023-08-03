// ReSharper disable CppClangTidyCppcoreguidelinesSpecialMemberFunctions
#pragma once

#include "fnd/observer.h"

class QString;

namespace HomeCompa::Flibrary {

class IAnnotationController
{
public:
	class IDataProvider
	{
	public:
		virtual ~IDataProvider() = default;
	};

	class IObserver : public Observer
	{
	public:
		virtual void OnAnnotationChanged(const IDataProvider & dataProvider) = 0;
	};

public:
	virtual ~IAnnotationController() = default;

public:
	virtual void SetCurrentBookId(QString bookId) = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}
// ReSharper enable CppClangTidyCppcoreguidelinesSpecialMemberFunctions
