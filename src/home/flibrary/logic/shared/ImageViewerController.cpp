#include "ImageViewerController.h"

#include <QAbstractItemModel>

#include "database/interface/IDatabase.h"

#include "interface/constants/ImageModelRole.h"
#include "interface/logic/IDataProvider.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

class ImageViewerController::Impl final : public IBookInfoProvider::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<const IModelProvider> modelProvider, std::shared_ptr<IBookInfoProvider> bookInfoProvider)
		: m_modelProvider { std::move(modelProvider) }
		, m_bookInfoProvider { std::move(bookInfoProvider) }
	{
		m_bookInfoProvider->RegisterObserver(this);
		m_bookInfoProvider->RequestRoot();
	}

	~Impl() override
	{
		m_bookInfoProvider->UnregisterObserver(this);
	}

	QAbstractItemModel* GetImagesModel() noexcept
	{
		return m_imageModel.get();
	}

	void SetImageSize(const int value)
	{
		m_imageModel->setData({}, value, ImageModelRole::ImageSize);
	}

private: // IBookInfoProvider::IObserver
	void OnBooksSelected(const NavigationMode /*navigationMode*/, IDataItem::Ptr root) override
	{
		m_imageModel->setData({}, QVariant::fromValue(std::move(root)), ImageModelRole::BooksRoot);
	}

private:
	std::shared_ptr<const IModelProvider>                  m_modelProvider;
	PropagateConstPtr<IBookInfoProvider, std::shared_ptr>  m_bookInfoProvider;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_imageModel { m_modelProvider->CreateImageModel() };
};

ImageViewerController::ImageViewerController(std::shared_ptr<const IModelProvider> modelProvider, std::shared_ptr<IBookInfoProvider> bookInfoProvider)
	: m_impl { std::move(modelProvider), std::move(bookInfoProvider) }
{
}

ImageViewerController::~ImageViewerController() = default;

QAbstractItemModel* ImageViewerController::GetImageModel() noexcept
{
	return m_impl->GetImagesModel();
}

void ImageViewerController::SetImageSize(const int value)
{
	m_impl->SetImageSize(value);
}
