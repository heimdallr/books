#pragma once

#include <memory>

#include <QList>
#include <QModelIndex>
#include <QString>

#include "util/executor/factory.h"

#include "export/flint.h"

class QAbstractItemModel;

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Util {
class IExecutor;
}

namespace HomeCompa::DB::Factory {
enum class Impl;
}

namespace HomeCompa::Flibrary {

class ILogicFactory  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	struct ExtractedBook
	{
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
	[[nodiscard]] virtual std::shared_ptr<class SearchController> CreateSearchController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class BooksContextMenuProvider> CreateBooksContextMenuProvider() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class ReaderController> CreateReaderController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IUserDataController> CreateUserDataController() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class BooksExtractor> CreateBooksExtractor() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class InpxCollectionExtractor> CreateInpxCollectionExtractor() const = 0;
	[[nodiscard]] virtual std::shared_ptr<class IUpdateChecker> CreateUpdateChecker() const = 0;
	[[nodiscard]] virtual std::shared_ptr<QTemporaryDir> CreateTemporaryDir() const = 0;

	FLINT_EXPORT static std::vector<std::vector<QString>> GetSelectedBookIds(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, const std::vector<int> & roles);
	FLINT_EXPORT static ExtractedBooks GetExtractedBooks(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList = {});
	FLINT_EXPORT static void FillScriptTemplate(QString & scriptTemplate, const ExtractedBook & book);
};

}
