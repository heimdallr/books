#include "ImageViewerController.h"

#include <QAbstractItemModel>

#include "database/interface/IDatabase.h"

#include "interface/constants/ImageModelRole.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

class ImageViewerController::Impl
{
public:
	explicit Impl(std::shared_ptr<const IModelProvider> modelProvider)
		: m_modelProvider { std::move(modelProvider) }
	{
	}

	QAbstractItemModel* GetImagesModel() noexcept
	{
		return m_imageModel.get();
	}

	void SetImageSize(const int value)
	{
		m_imageModel->setData({}, value, ImageModelRole::ImageSize);
	}

private:

private:
	std::shared_ptr<const IModelProvider> m_modelProvider;

	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_imageModel { m_modelProvider->CreateImageModel() };
};

ImageViewerController::ImageViewerController(std::shared_ptr<const IModelProvider> modelProvider)
	: m_impl { std::move(modelProvider) }
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
