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
	using Callback = std::function<void(bool)>;
	using Extract = void(BooksExtractor::*)(QString, const QString &, Books &&, QString, Callback);

public:
	BooksExtractor(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
		, std::shared_ptr<class ILogicFactory> logicFactory
		, std::shared_ptr<const class IScriptController> scriptController
	);
	~BooksExtractor();

public:
	void ExtractAsArchives(QString folder, const QString & parameter, Books && books, QString outputFileNameTemplate, Callback callback);
	void ExtractAsIs(QString folder, const QString & parameter, Books && books, QString outputFileNameTemplate, Callback callback);
	void ExtractAsScript(QString folder, const QString & parameter, Books && books, QString outputFileNameTemplate, Callback callback);

	static void FillScriptTemplate(QString & scriptTemplate, const Book & book);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
