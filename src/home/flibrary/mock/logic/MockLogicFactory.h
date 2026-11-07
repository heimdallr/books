#include <gmock/gmock.h>

#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary::Test {

class MockLogicFactory : public ILogicFactory
{
	using QStringMap = std::unordered_map<QString, QVariant>;

public:
	MOCK_METHOD(std::shared_ptr<class ITreeViewController>, GetTreeViewController, (ItemType type), (const, override));
	MOCK_METHOD(std::shared_ptr<class ArchiveParser>, CreateArchiveParser, (), (const, override));
	MOCK_METHOD(std::unique_ptr<Util::IExecutor>, GetExecutor, (Util::ExecutorInitializer initializer), (const, override));
	MOCK_METHOD(std::shared_ptr<class GroupController>, CreateGroupController, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IBookSearchController>, CreateSearchController, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class BooksContextMenuProvider>, CreateBooksContextMenuProvider, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IUserDataController>, CreateUserDataController, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class BooksExtractor>, CreateBooksExtractor, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IInpxGenerator>, CreateInpxGenerator, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IUpdateChecker>, CreateUpdateChecker, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class ICollectionCleaner>, CreateCollectionCleaner, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IOpdsController>, CreateOpdsController, (), (const, override));
	MOCK_METHOD(std::shared_ptr<class IFillTemplateConverter>, CreateFillTemplateConverter, (bool needStub), (const, override));
	MOCK_METHOD(std::shared_ptr<ISettings>, CreateSettingsStub, (), (const, override));
	MOCK_METHOD(std::shared_ptr<ISettings>, CreateSettingsDecorator, (std::shared_ptr<ISettings> settings, QStringMap replacement), (const, override));
	MOCK_METHOD(std::shared_ptr<Zip::ProgressCallback>, CreateZipProgressCallback, (std::shared_ptr<class IProgressController> progressController), (const, override));
	MOCK_METHOD(std::shared_ptr<ITemporaryDir>, CreateTemporaryDir, (bool singleInstance), (const, override));
	MOCK_METHOD(std::shared_ptr<ITemporaryDir>, CreateTemporaryDir, (const QString& path), (const, override));
	MOCK_METHOD(Util::ExtractedBooks, GetExtractedBooks, (QAbstractItemModel * model, const QModelIndex& index, const QList<QModelIndex>& indexList), (const, override));
	MOCK_METHOD(Util::ExtractedBook, GetCurrentExtractedBook, (), (const, override));
	MOCK_METHOD(void, FindBook, (NavigationMode navigationMode, const QString& navigationId, long long bookId), (const, override));
	MOCK_METHOD(std::shared_ptr<IProgressController>, GetProgressController, (), (const, override));
};

static_assert(!std::is_abstract_v<MockLogicFactory>, "MockLogicFactory is abstract!");

} // namespace HomeCompa::Flibrary::Test
