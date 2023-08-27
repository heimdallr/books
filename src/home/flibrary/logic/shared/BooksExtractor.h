#pragma once

#include <functional>
#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class BooksExtractor
{
	NON_COPY_MOVABLE(BooksExtractor)

public:
	struct Book
	{
		QString folder;
		QString file;
		int64_t size;
		QString author;
		QString series;
		int seqNumber;
		QString title;
	};
	using Books = std::vector<Book>;
	using Callback = std::function<void()>;
	using Extract = void(BooksExtractor::*)(const QString &, Books &&, Callback);

public:
	BooksExtractor(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IProgressController> progressController
		, std::shared_ptr<class ILogicFactory> logicFactory
	);
	~BooksExtractor();

public:
	void ExtractAsArchives(const QString & folder, Books && books, Callback callback);
	void ExtractAsIs(const QString & folder, Books && books, Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}