#pragma once

#include <QString>

#include "fnd/observer.h"

#include "interface/logic/IDataItem.h"

namespace HomeCompa::Flibrary {

class IAnnotationController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IDataProvider  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		struct Cover
		{
			QString name;
			QByteArray bytes;
		};
		using Covers = std::vector<Cover>;

	public:
		virtual ~IDataProvider() = default;

	public:
		[[nodiscard]] virtual const IDataItem & GetBook() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GetSeries() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GetAuthors() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GetGenres() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem & GetGroups() const noexcept = 0;

		[[nodiscard]] virtual const QString & GetError() const noexcept = 0;
		[[nodiscard]] virtual const QString & GetAnnotation() const noexcept = 0;
		[[nodiscard]] virtual const QString & GetEpigraph() const noexcept = 0;
		[[nodiscard]] virtual const QString & GetEpigraphAuthor() const noexcept = 0;
		[[nodiscard]] virtual const std::vector<QString> & GetKeywords() const noexcept = 0;
		[[nodiscard]] virtual const Covers & GetCovers() const noexcept = 0;
		[[nodiscard]] virtual int GetCoverIndex() const noexcept = 0;
		[[nodiscard]] virtual IDataItem::Ptr GetContent() const noexcept = 0;
		[[nodiscard]] virtual IDataItem::Ptr GetTranslators() const noexcept = 0;

	};

	class IObserver : public Observer
	{
	public:
		virtual void OnAnnotationRequested() = 0;
		virtual void OnAnnotationChanged(const IDataProvider & dataProvider) = 0;
		virtual void OnArchiveParserProgress(int percents) = 0;
	};

public:
	virtual ~IAnnotationController() = default;

public:
	virtual void SetCurrentBookId(QString bookId) = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}
