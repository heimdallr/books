#include "ImageViewerController.h"

#include <QAbstractItemModel>
#include <QTimer>

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/ImageModelRole.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IImageViewerController.h"

#include "TreeViewController/AbstractTreeViewController.h"
#include "settings/UiTimer.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ImageUtil.h"
#include "util/executor/ThreadPool.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

class ImageViewerController::Impl final
	: public QObject
	, public IBookInfoProvider::IObserver
	, public Observable<IObserver>
{
	NON_COPY_MOVABLE(Impl)

	struct SaveProgressItem
	{
		std::unique_ptr<IProgressController::IProgressItem> progressItem;
		QDir                                                saveDir;
		std::unordered_set<int>                             rows;
	};

public:
	Impl(std::shared_ptr<const IModelProvider> modelProvider, std::shared_ptr<IBookInfoProvider> bookInfoProvider, std::shared_ptr<IProgressController> progressController)
		: m_modelProvider { std::move(modelProvider) }
		, m_bookInfoProvider { std::move(bookInfoProvider) }
		, m_progressController { std::move(progressController) }
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
		const auto applyNow = m_imageSize < 0;
		m_imageSize         = value;
		applyNow ? ApplyImageSize() : m_imageSizeTimer->start();
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

		m_requestImage = index.row();
		m_requestImageTimer->start();
	}

	void Filter(QString filter)
	{
		if (filter.length() < 3)
			filter.clear();

		m_filter = std::move(filter);
		m_filterTimer->start();
	}

	void Save(const QString& folder, const QList<QModelIndex>& indices)
	{
		auto rows = indices | std::views::transform([](const auto& item) {
						return item.row();
					})
		          | std::ranges::to<std::unordered_set>();
		{
			std::lock_guard lock(m_saveProgressItemsGuard);
			m_saveProgressItems.emplace_back(m_progressController->Add(indices.size()), QDir(folder), std::move(rows));
		}

		for (const auto& index : indices)
			m_imageModel->setData(index, {}, ImageModelRole::Save);
	}

private: // IBookInfoProvider::IObserver
	void OnBooksSelected(const NavigationMode /*navigationMode*/, IDataItem::Ptr root) override
	{
		m_imageModel->setData({}, QVariant::fromValue(std::move(root)), ImageModelRole::BooksRoot);
	}

private:
	void OnModelReset()
	{
		Perform(&IImageViewerController::IObserver::OnImageReceived, QPixmap {});
	}

	void OnModelDataChanged(const QModelIndex& topLeft, const QModelIndex& /*bottomRight*/, const QList<int>& roles)
	{
		if (roles.contains(ImageModelRole::Image))
			RequestImage(topLeft);

		if (roles.contains(ImageModelRole::Save))
			SaveImage(topLeft);
	}

	void OnModelRowCountChanged()
	{
		Perform(&IImageViewerController::IObserver::OnCountChanges, m_imageModel->rowCount());
	}

	void RequestImage()
	{
		if (const auto index = m_imageModel->index(m_requestImage, 0); index.isValid())
			m_imageModel->setData(index, {}, ImageModelRole::Image);
	}

	void ApplyFilter()
	{
		m_imageModel->setData({}, m_filter, ImageModelRole::Filter);
		Perform(&IImageViewerController::IObserver::OnCountChanges, m_imageModel->rowCount());
	}

	void ApplyImageSize()
	{
		m_imageModel->setData({}, m_imageSize, ImageModelRole::ImageSize);
	}

	void SaveImage(const QModelIndex& index)
	{
		m_threadPool.enqueue([this,
		                      pixmap   = m_imageModel->data(index, ImageModelRole::Save).value<QPixmap>(),
		                      fileName = m_imageModel->data(index, ImageModelRole::FileName).toString(),
		                      row      = index.row()](size_t&, const std::stop_token&) mutable {
			fileName.replace('/', '_');
			auto       image  = Util::HasAlpha(pixmap.toImage());
			const auto format = image.pixelFormat().channelCount() > 3 ? Util::PNG : Util::JPEG;

			const auto fileNames = [&] {
				std::lock_guard lock(m_saveProgressItemsGuard);
				return m_saveProgressItems | std::views::filter([row](const SaveProgressItem& item) {
						   return item.rows.contains(row);
					   })
				     | std::views::transform([&](const SaveProgressItem& item) {
						   return item.saveDir.filePath(fileName + "." + format);
					   })
				     | std::ranges::to<std::vector>();
			}();

			for (const auto& file : fileNames)
				image.save(file);

			m_forwarder.Forward([this, row] {
				std::lock_guard lock(m_saveProgressItemsGuard);
				for (auto& progress : m_saveProgressItems)
				{
					if (progress.progressItem->IsStopped())
						m_imageModel->setData({}, {}, ImageModelRole::SaveStop);

					if (progress.rows.erase(row))
						progress.progressItem->Increment(1);
				}

				std::erase_if(m_saveProgressItems, [](const auto& item) {
					return item.rows.empty() || item.progressItem->IsStopped();
				});
			});
		});
	}

private:
	Util::FunctorExecutionForwarder m_forwarder;

	std::shared_ptr<const IModelProvider>                   m_modelProvider;
	PropagateConstPtr<IBookInfoProvider, std::shared_ptr>   m_bookInfoProvider;
	PropagateConstPtr<IProgressController, std::shared_ptr> m_progressController;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr>  m_imageModel { m_modelProvider->CreateImageModel() };

	int                     m_requestImage { -1 };
	std::unique_ptr<QTimer> m_requestImageTimer { Util::CreateUiTimer([this] {
		RequestImage();
	}) };

	QString                 m_filter;
	std::unique_ptr<QTimer> m_filterTimer { Util::CreateUiTimer([this] {
		ApplyFilter();
	}) };

	int                     m_imageSize { -1 };
	std::unique_ptr<QTimer> m_imageSizeTimer { Util::CreateUiTimer([this] {
		ApplyImageSize();
	}) };

	Util::ThreadPool<>          m_threadPool { Util::ThreadPool<>::Initializer { .threadCount = 1 } };
	std::mutex                  m_saveProgressItemsGuard;
	std::list<SaveProgressItem> m_saveProgressItems;
};

ImageViewerController::ImageViewerController(
	std::shared_ptr<const IModelProvider>    modelProvider,
	std::shared_ptr<IBookInfoProvider>       bookInfoProvider,
	std::shared_ptr<IMainProgressController> progressController
)
	: m_impl { std::move(modelProvider), std::move(bookInfoProvider), std::move(progressController) }
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

void ImageViewerController::Filter(QString filter)
{
	m_impl->Filter(std::move(filter));
}

void ImageViewerController::Save(const QString& folder, const QList<QModelIndex>& indices)
{
	m_impl->Save(folder, indices);
}

void ImageViewerController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void ImageViewerController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
