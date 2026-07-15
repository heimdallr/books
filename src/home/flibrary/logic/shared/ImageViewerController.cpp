#include "ImageViewerController.h"

#include <QAbstractItemModel>

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/ImageModelRole.h"

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

	QAbstractItemModel* GetImagesModel() noexcept
	{
		return m_imageModel.get();
	}

	void SetFolder(const QModelIndex& index)
	{
		m_imageModel->setData({}, QVariant::fromValue(index), ImageModelRole::Folder);
	}

	void SetImageSize(const int value)
	{
		m_imageModel->setData({}, value, ImageModelRole::ImageSize);
	}

private:
	void RequestNavigation() const
	{
		auto        executor    = m_databaseUser->Executor();
		auto        db          = m_databaseUser->Database();
		const auto& executorRef = *executor;
		executorRef({ "Get navigation", [this, executor = std::move(executor), db = std::move(db)] {
						 auto       root  = NavigationItem::Create();
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

	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_imageModel { m_modelProvider->CreateImageModel() };
};

ImageViewerController::ImageViewerController(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IModelProvider> modelProvider)
	: m_impl { std::move(databaseUser), std::move(modelProvider) }
{
}

ImageViewerController::~ImageViewerController() = default;

QAbstractItemModel* ImageViewerController::GetImageModel() noexcept
{
	return m_impl->GetImagesModel();
}

void ImageViewerController::SetFolder(const QModelIndex& index)
{
	m_impl->SetFolder(index);
}

void ImageViewerController::SetImageSize(const int value)
{
	m_impl->SetImageSize(value);
}

void ImageViewerController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void ImageViewerController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
