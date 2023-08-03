#pragma once

#include "fnd/observer.h"

class QString;

namespace HomeCompa::Flibrary {
class IDataItem;

class IAnnotationController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IDataProvider  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IDataProvider() = default;

	public:
		[[nodiscard]] virtual const IDataItem & GatBook() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GatSeries() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GatAuthors() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GatGenres() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GatGroups() const noexcept = 0;

		[[nodiscard]] virtual const QString & GetAnnotation() const noexcept = 0;
		[[nodiscard]] virtual const std::vector<QString> & GetKeywords() const noexcept = 0;
		[[nodiscard]] virtual const std::vector<QString> & GetCovers() const noexcept = 0;
		[[nodiscard]] virtual int GetCoverIndex() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GetContent() const noexcept = 0;

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
