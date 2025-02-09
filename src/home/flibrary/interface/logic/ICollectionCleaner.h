#pragma once

#include <QString>

namespace HomeCompa::Flibrary {

class ICollectionCleaner  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto CONTEXT = "CollectionCleaner";
	static constexpr auto REMOVE_PERMANENTLY_INFO = QT_TRANSLATE_NOOP("CollectionCleaner", "%1 book(s) deleted permanently");

	struct Book
	{
		long long id;
		QString folder;
		QString file;
	};
	using Books = std::vector<Book>;

	class IAnalyzeObserver  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IAnalyzeObserver() = default;
		virtual void AnalyzeFinished(Books books) = 0;
	};

	using Callback = std::function<void(bool result)>;

public:
	virtual ~ICollectionCleaner() = default;
	virtual void Remove(Books books, Callback callback) const = 0;
	virtual void Analyze(IAnalyzeObserver& callback) const = 0;
	virtual void AnalyzeCancel() const = 0;
};

}
