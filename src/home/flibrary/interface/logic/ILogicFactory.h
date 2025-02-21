#pragma once

#include <memory>

#include <QList>
#include <QModelIndex>
#include <QString>

#include "fnd/Lockable.h"

#include "util/executor/factory.h"

#include "zip.h"

#include "export/flint.h"

class QAbstractItemModel;
class QTemporaryDir;

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::Util
{
class IExecutor;
}

namespace HomeCompa::DB::Factory
{
enum class Impl;
}

namespace HomeCompa::Flibrary
{

class ILogicFactory : public Lockable<ILogicFactory> // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct ExtractedBook
	{
		int id;
		QString folder;
		QString file;
		int64_t size;
		QString author;
		QString series;
		int seqNumber;
		QString title;
	};

	using ExtractedBooks = std::vector<ExtractedBook>;

public:
	virtual ~ILogicFactory() = default;

public:
	[[nodiscard]] virtual std::shared_ptr<class ITreeViewController> GetTreeViewController(enum class ItemType type) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ArchiveParser> CreateArchiveParser() const = 0;
	[[nodiscard]] virtual std::unique_ptr<Util::IExecutor> GetExecutor(Util::ExecutorInitializer initializer = {}) const = 0;
	[[nodiscard]] virtual std::shared_ptr<class GroupController> CreateGroupController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IBookSearchController> CreateSearchController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class BooksContextMenuProvider> CreateBooksContextMenuProvider() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IUserDataController> CreateUserDataController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class BooksExtractor> CreateBooksExtractor() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IInpxGenerator> CreateInpxGenerator() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IUpdateChecker> CreateUpdateChecker() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ICollectionCleaner> CreateCollectionCleaner() const = 0;
	[[nodiscard]] virtual std::shared_ptr<Zip::ProgressCallback> CreateZipProgressCallback(std::shared_ptr<class IProgressController> progressController) const = 0;
	[[nodiscard]] virtual std::shared_ptr<QTemporaryDir> CreateTemporaryDir() const = 0;

public: // special
	[[nodiscard]] virtual std::shared_ptr<IProgressController> GetProgressController() const = 0;

	FLINT_EXPORT static std::vector<std::vector<QString>> GetSelectedBookIds(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, const std::vector<int>& roles);
	FLINT_EXPORT static ExtractedBooks GetExtractedBooks(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList = {});
	FLINT_EXPORT static void FillScriptTemplate(QString& scriptTemplate, const ExtractedBook& book);
};

} // namespace HomeCompa::Flibrary
