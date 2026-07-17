#include "ImageViewerController.h"

#include <QAbstractItemModel>
#include <QTimer>

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/ImageModelRole.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IImageViewerController.h"

#include "settings/UiTimer.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

class ImageViewerController::Impl final
	: public QObject
	, public IBookInfoProvider::IObserver
	, public Observable<IObserver>
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(std::shared_ptr<const IModelProvider> modelProvider, std::shared_ptr<IBookInfoProvider> bookInfoProvider)
		: m_modelProvider { std::move(modelProvider) }
		, m_bookInfoProvider { std::move(bookInfoProvider) }
	{
		m_bookInfoProvider->RegisterObserver(this);
		m_bookInfoProvider->RequestRoot();

		connect(m_imageModel.get(), &QAbstractItemModel::dataChanged, this, &Impl::OnModelDataChanged);
		connect(m_imageModel.get(), &QAbstractItemModel::modelReset, this, &Impl::OnModelReset);
		connect(m_imageModel.get(), &QAbstractItemModel::modelReset, this, &Impl::OnModelRowCountChanged);
		connect(m_imageModel.get(), &QAbstractItemModel::rowsInserted, this, &Impl::OnModelRowCountChanged);
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

	void PrepareImage(const QModelIndex& index)
	{
		if (!index.data(ImageModelRole::Ready).toBool())
			m_imageModel->setData(index, {}, ImageModelRole::Prepare);
	}

	void RequestImage(const QModelIndex& index)
	{
		if (auto pixmap = m_imageModel->data(index, ImageModelRole::Image).value<QPixmap>(); !pixmap.isNull())
			return Perform(&IImageViewerController::IObserver::OnImageReceived, std::move(pixmap));

		m_requestedImageRow = index.row();
		m_requestImageTimer->start();
	}

private: // IBookInfoProvider::IObserver
	void OnBooksSelected(const NavigationMode /*navigationMode*/, IDataItem::Ptr root) override
	{
		m_imageModel->setData({}, QVariant::fromValue(std::move(root)), ImageModelRole::BooksRoot);
	}

private:
	void OnModelReset()
	{
		Perform(&IImageViewerController::IObserver::OnImageReceived, QPixmap{});
	}

	void OnModelDataChanged(const QModelIndex& topLeft, const QModelIndex& /*bottomRight*/, const QList<int>& roles)
	{
		if (roles.contains(ImageModelRole::Image))
			RequestImage(topLeft);
	}

	void OnModelRowCountChanged()
	{
		Perform(&IImageViewerController::IObserver::OnCountChanges, m_imageModel->rowCount());
	}

private:
	std::shared_ptr<const IModelProvider>                  m_modelProvider;
	PropagateConstPtr<IBookInfoProvider, std::shared_ptr>  m_bookInfoProvider;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_imageModel { m_modelProvider->CreateImageModel() };
	int                                                    m_requestedImageRow { -1 };
	std::unique_ptr<QTimer>                                m_requestImageTimer { Util::CreateUiTimer([this] {
		m_imageModel->setData(m_imageModel->index(m_requestedImageRow, 0), {}, ImageModelRole::Image);
	}) };
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

void ImageViewerController::PrepareImage(const QModelIndex& index)
{
	m_impl->PrepareImage(index);
}

void ImageViewerController::RequestImage(const QModelIndex& index)
{
	m_impl->RequestImage(index);
}

void ImageViewerController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void ImageViewerController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
