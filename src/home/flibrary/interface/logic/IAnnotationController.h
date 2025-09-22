#pragma once

#include <QDateTime>
#include <QString>

#include "fnd/observer.h"

#include "interface/logic/IDataItem.h"

#include "IJokeRequesterFactory.h"

#include "export/flint.h"

namespace HomeCompa::Flibrary::ExportStat
{
enum class Type;
}

namespace HomeCompa::Flibrary
{
enum class NavigationMode;

class IAnnotationController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IDataProvider // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		struct Cover
		{
			QString name;
			QByteArray bytes;
		};

		struct Review
		{
			QDateTime time;
			QString name;
			QString text;
		};

		using Covers = std::vector<Cover>;
		using ExportStatistics = std::vector<std::pair<ExportStat::Type, std::vector<QDateTime>>>;
		using Reviews = std::vector<Review>;

	public:
		virtual ~IDataProvider() = default;

	public:
		[[nodiscard]] virtual const IDataItem& GetBook() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetSeries() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetAuthors() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetGenres() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetGroups() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetKeywords() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetFolder() const noexcept = 0;
		[[nodiscard]] virtual const IDataItem& GetUpdate() const noexcept = 0;

		[[nodiscard]] virtual const QString& GetError() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetAnnotation() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetEpigraph() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetEpigraphAuthor() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetLanguage() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetSourceLanguage() const noexcept = 0;
		[[nodiscard]] virtual const std::vector<QString>& GetFb2Keywords() const noexcept = 0;
		[[nodiscard]] virtual const Covers& GetCovers() const noexcept = 0;
		[[nodiscard]] virtual size_t GetTextSize() const noexcept = 0;
		[[nodiscard]] virtual size_t GetWordCount() const noexcept = 0;
		[[nodiscard]] virtual IDataItem::Ptr GetContent() const noexcept = 0;
		[[nodiscard]] virtual IDataItem::Ptr GetTranslators() const noexcept = 0;

		[[nodiscard]] virtual const QString& GetPublisher() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetPublishCity() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetPublishYear() const noexcept = 0;
		[[nodiscard]] virtual const QString& GetPublishIsbn() const noexcept = 0;
		[[nodiscard]] virtual const ExportStatistics& GetExportStatistics() const noexcept = 0;
		[[nodiscard]] virtual const Reviews& GetReviews() const noexcept = 0;

		[[nodiscard]] virtual std::vector<IDataItem::Flags> GetFlags(NavigationMode navigationMode, const std::vector<QString>& ids) const = 0;
	};

	class IObserver : public Observer
	{
	public:
		virtual void OnAnnotationRequested() = 0;
		virtual void OnAnnotationChanged(const IDataProvider& dataProvider) = 0;
		virtual void OnJokeTextChanged(const QString& value) = 0;
		virtual void OnJokeImageChanged(const QByteArray& value) = 0;
		virtual void OnArchiveParserProgress(int percents) = 0;
	};

	class IStrategy // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		enum class Section
		{
			Title,
			Epigraph,
			EpigraphAuthor,
			Annotation,
			Keywords,
			UrlTable,
			Translators,
			BookInfo,
			PublishInfo,
			ParseError,
			ExportStatistics,
			Reviews,
		};

	public:
		virtual ~IStrategy() = default;
		virtual QString GenerateUrl(const char* type, const QString& id, const QString& str, bool textMode = false) const = 0;
		virtual QString GenerateStars(int rate) const = 0;

		virtual void Add(const Section section, QString& text, const QString& str, const char* pattern = "%1") const
		{
			AddImpl(section, text, str, pattern);
		}

		virtual QString AddTableRow(const char* name, const QString& value) const
		{
			return AddTableRowImpl(name, value);
		}

		virtual QString AddTableRow(const QStringList& values) const
		{
			return AddTableRowImpl(values);
		}

		virtual QString TableRowsToString(const QStringList& values) const
		{
			return TableRowsToStringImpl(values);
		}

		virtual QString GetReviewsDelimiter() const
		{
			return {};
		}

	private:
		FLINT_EXPORT static void AddImpl(Section section, QString& text, const QString& str, const char* pattern);
		FLINT_EXPORT static QString AddTableRowImpl(const char* name, const QString& value);
		FLINT_EXPORT static QString AddTableRowImpl(const QStringList& values);
		FLINT_EXPORT static QString TableRowsToStringImpl(const QStringList& values);
	};

public:
	virtual ~IAnnotationController() = default;

public:
	virtual void SetCurrentBookId(QString bookId, bool extractNow = false) = 0;
	virtual QString CreateAnnotation(const IDataProvider& dataProvider, const IStrategy& strategy) const = 0;
	virtual void ShowJokes(IJokeRequesterFactory::Implementation impl, bool value) = 0;
	virtual void ShowReviews(bool value) = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
