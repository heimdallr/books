#pragma once

#include "fnd/observer.h"

class QByteArray;
class QString;

namespace HomeCompa::Flibrary
{

enum class NavigationMode;

class IAuthorAnnotationController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnReadyChanged() = 0;
		virtual void OnAuthorChanged(const QString& text, const std::vector<QByteArray>& images) = 0;
	};

public:
	virtual ~IAuthorAnnotationController() = default;

public:
	virtual bool IsReady() const noexcept = 0;

	virtual void SetNavigationMode(NavigationMode mode) = 0;
	virtual void SetAuthor(long long id, QString name) = 0;

	virtual bool CheckAuthor(const QString& name) const = 0;
	virtual QString GetInfo(const QString& name) const = 0;
	virtual std::vector<QByteArray> GetImages(const QString& name) const = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
