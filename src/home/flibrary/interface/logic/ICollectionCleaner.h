#pragma once

#include <QString>

namespace HomeCompa::Flibrary
{

class ICollectionCleaner // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto CONTEXT                 = "CollectionCleaner";
	static constexpr auto REMOVE_PERMANENTLY_INFO = QT_TRANSLATE_NOOP("CollectionCleaner", "%1 book(s) deleted permanently");

	struct Book
	{
		long long id;
		QString   folder;
		QString   file;
	};

	using Books = std::vector<Book>;

	enum class CleanGenreMode
	{
		None = -1,
		Full,
		Partial,
	};

	class IAnalyzeObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IAnalyzeObserver()               = default;
		virtual void AnalyzeFinished(Books books) = 0;

		virtual bool                  IsPermanently() const                              = 0;
		virtual bool                  NeedDeleteMarkedAsDeleted() const                  = 0;
		virtual bool                  NeedDeleteDuplicates() const                       = 0;
		virtual bool                  NeedDeleteUnrated() const                          = 0;
		virtual QStringList           GetLanguages() const                               = 0;
		virtual QStringList           GetGenres() const                                  = 0;
		virtual CleanGenreMode        GetCleanGenreMode() const                          = 0;
		virtual std::optional<size_t> GetMinimumBookSize() const                         = 0;
		virtual std::optional<size_t> GetMaximumBookSize() const                         = 0;
		virtual std::optional<double> GetMinimumLibRate() const                          = 0;
		virtual bool                  NeedDeleteCompletelyDuplicatedCompilations() const = 0;
		virtual bool                  NeedDeleteBooksDuplicatedByCompilations() const    = 0;
		virtual void                  CompilationInfoExistsResponse(bool value) const    = 0;
	};

	using Callback = std::function<void(bool result)>;

public:
	virtual ~ICollectionCleaner()                                               = default;
	virtual void Remove(Books books, Callback callback) const                   = 0;
	virtual void RemovePermanently(Books books, Callback callback) const        = 0;
	virtual void Analyze(IAnalyzeObserver& callback) const                      = 0;
	virtual void AnalyzeCancel() const                                          = 0;
	virtual void CompilationInfoExistsRequest(IAnalyzeObserver& callback) const = 0;
};

} // namespace HomeCompa::Flibrary
