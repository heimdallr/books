#pragma once

#include <filesystem>

#include "fnd/ConvertableT.h"
#include "fnd/memory.h"

#include "models/BookModelObserver.h"

#include "ModelController.h"

namespace HomeCompa::Util {
class Executor;
}

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::Flibrary {

class BooksModelControllerObserver;
class ProgressController;
enum class BooksViewType;
enum class NavigationSource;

class BooksModelController final
	: public ModelController
	, public BookModelObserver
	, public ConvertibleT<BooksModelController>
{
	NON_COPY_MOVABLE(BooksModelController)
	Q_OBJECT

public:
	Q_INVOKABLE bool RemoveAvailable(long long id);
	Q_INVOKABLE bool RestoreAvailable(long long id);
	Q_INVOKABLE bool AllSelected();
	Q_INVOKABLE bool HasSelected();
	Q_INVOKABLE bool SelectAll();
	Q_INVOKABLE bool DeselectAll();
	Q_INVOKABLE bool InvertSelection();
	Q_INVOKABLE void Remove(long long id);
	Q_INVOKABLE void Restore(long long id);
	Q_INVOKABLE void WriteToArchive(QString path, long long id);
	Q_INVOKABLE void WriteToFile(QString path, long long id);
	Q_INVOKABLE void Read(long long id);

public:
	BooksModelController(Util::Executor & executor
		, DB::Database & db
		, ProgressController & progressController
		, BooksViewType booksViewType
		, std::filesystem::path archiveFolder
		, Settings & uiSettings
	);
	~BooksModelController() override;

	void SetNavigationState(NavigationSource navigationSource, const QString & navigationId);
	void RegisterObserver(BooksModelControllerObserver * observer);
	void UnregisterObserver(BooksModelControllerObserver * observer);

private: // ModelController
	ModelControllerType GetType() const noexcept override;
	void UpdateModelData() override;
	QAbstractItemModel * CreateModel() override;
	bool SetCurrentIndex(int index) override;

private: // BookModelObserver
	void HandleBookRemoved(const std::vector<std::reference_wrapper<const Book>> & books) override;
	void HandleItemDoubleClicked(int index) override;

private slots:
	void OnGetCheckedBooksRequest(std::vector<Book> & books);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
