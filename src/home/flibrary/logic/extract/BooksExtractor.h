#pragma once

#include <functional>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBookExtractor.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IScriptController.h"

#include "settings/ISettings.h"

namespace HomeCompa::Flibrary {

struct SetForIdsArg;

class BooksExtractor final : public QObject

{
	Q_OBJECT
	NON_COPY_MOVABLE(BooksExtractor)

signals:
	void SetUserRate(const SetForIdsArg& args) const;

public:
	using Callback = std::function<void(bool)>;
	using Extract  = void (BooksExtractor::*)(QString, const QString&, Util::ExtractedBooks&&, Callback);

public:
	BooksExtractor(
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<ICollectionController>      collectionController,
		std::shared_ptr<IMainProgressController>    progressController,
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IScriptController>    scriptController,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IDatabaseProvider>    databaseUser
	);
	~BooksExtractor() override;

public:
	void ExtractAsArchives(QString folder, const QString& parameter, Util::ExtractedBooks&& books, Callback callback);
	void ExtractAsIs(QString folder, const QString& parameter, Util::ExtractedBooks&& books, Callback callback);
	void ExtractUnpack(QString folder, const QString& parameter, Util::ExtractedBooks&& books, Callback callback);
	void ExtractAsScript(QString folder, const QString& parameter, Util::ExtractedBooks&& books, Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
