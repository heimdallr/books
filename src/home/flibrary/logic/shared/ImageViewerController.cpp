#include "ImageViewerController.h"

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"

#include "data/DataItem.h"
#include "database/DatabaseUtil.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

class ImageViewerController::Impl final : public Observable<IObserver>
{
public:
	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IModelProvider> modelProvider)
		: m_databaseUser { std::move(databaseUser) }
		, m_modelProvider { std::move(modelProvider) }
	{
		RequestNavigation();
	}

private:
	void RequestNavigation() const
	{
		auto        executor    = m_databaseUser->Executor();
		auto        db          = m_databaseUser->Database();
		const auto& executorRef = *executor;
		executorRef({ "Get navigation", [this, executor = std::move(executor), db = std::move(db)] {
						 auto root = NavigationItem::Create();
						 const auto query = db->CreateQuery("select FolderID, FolderTitle, IsDeleted from Folders");
						 for (query->Execute(); !query->Eof(); query->Next())
							 root->AppendChild(DatabaseUtil::CreateSimpleListItem(*query));

						 return [this, root = std::move(root)](size_t) mutable {
							 Perform(&IObserver::OnNavigationModelChanged, m_modelProvider->CreateListModel(std::move(root)));
						 };
					 } });
	}

private:
	std::shared_ptr<const IDatabaseUser>  m_databaseUser;
	std::shared_ptr<const IModelProvider> m_modelProvider;
};

ImageViewerController::ImageViewerController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IModelProvider> modelProvider)
	: m_impl { std::move(databaseUser), std::move(modelProvider) }
{
}

ImageViewerController::~ImageViewerController() = default;

void ImageViewerController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void ImageViewerController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
