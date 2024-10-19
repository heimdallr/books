#pragma once

#include <functional>
#include <QString>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary {

class BooksExtractor
{
	NON_COPY_MOVABLE(BooksExtractor)

public:
	using Callback = std::function<void(bool)>;
	using Extract = void(BooksExtractor::*)(QString, const QString &, ILogicFactory::ExtractedBooks &&, QString, Callback);

public:
	BooksExtractor(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
		, const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<const class IScriptController> scriptController
		, std::shared_ptr<class DatabaseUser> databaseUser
	);
	~BooksExtractor();

public:
	void ExtractAsArchives(QString folder, const QString & parameter, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback);
	void ExtractAsIs(QString folder, const QString & parameter, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback);
	void ExtractAsScript(QString folder, const QString & parameter, ILogicFactory::ExtractedBooks && books, QString outputFileNameTemplate, Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
