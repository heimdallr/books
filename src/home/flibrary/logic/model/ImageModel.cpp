#include "ImageModel.h"

#include <Constant.h>

#include <ranges>

#include <QFileInfo>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/try.h"

#include "interface/constants/ImageModelRole.h"
#include "interface/constants/ModelRole.h"

#include "settings/UiTimer.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ImageUtil.h"
#include "util/executor/ThreadPool.h"

#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

struct Item
{
	int             zipId;
	QString         fileName;
	mutable QPixmap pixmap;
};

using Items = std::vector<Item>;

class Extractor
{
public:
	class IObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;

		virtual void OnExtractFinished(int row, QByteArray bytes) = 0;
	};

public:
	Extractor(std::unique_ptr<const Zip> zip, IObserver& observer)
		: m_zip { std::move(zip) }
		, m_observer { observer }
	{
	}

	void Enqueue(const int row, QString fileName)
	{
		m_threadPool.enqueue([this, row, fileName = std::move(fileName)](size_t&) {
			auto bytes = m_zip->Read(fileName)->GetStream().readAll();
			m_observer.OnExtractFinished(row, std::move(bytes));
		});
	}

private:
	std::unique_ptr<const Zip> m_zip;
	IObserver&                 m_observer;
	Util::ThreadPool<>         m_threadPool { Util::ThreadPool<>::Initializer { .threadCount = 1 } };
};

class Decoder
{
	static unsigned int GetThreadCount()
	{
		const auto threadCount = std::thread::hardware_concurrency();
		return threadCount > 4 ? threadCount - 4 : 1;
	}

public:
	class IObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;

		virtual void OnDecodeFinished(int row, QPixmap bytes) = 0;
	};

	Decoder(const int imageSize, IObserver& observer)
		: m_imageSize { imageSize }
		, m_observer { observer }
	{
	}

	void Enqueue(const int row, QByteArray bytes)
	{
		m_threadPool.enqueue([this, row, bytes = std::move(bytes)](size_t&) {
			auto pixmap = Util::Decode(bytes);
			if (std::max(pixmap.width(), pixmap.height()) > m_imageSize)
				pixmap = pixmap.scaled(m_imageSize, m_imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			m_observer.OnDecodeFinished(row, std::move(pixmap));
		});
	}

private:
	const int          m_imageSize;
	IObserver&         m_observer;
	Util::ThreadPool<> m_threadPool { Util::ThreadPool<>::Initializer { .threadCount = GetThreadCount() } };
};

class Model final
	: public QAbstractListModel
	, public Extractor::IObserver
	, public Decoder::IObserver
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	{
		return std::make_unique<Model>(std::move(collectionProvider), std::move(databaseUser));
	}

public:
	explicit Model(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_databaseUser { std::move(databaseUser) }
	{
		m_imagePlaceholder.load(":/images/image-placeholder.png");
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_items.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		return index.isValid() ? GetData(index, role) : GetData(role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		return index.isValid() ? SetData(index, value, role) : SetData(value, role);
	}

private: // Extractor::IObserver
	void OnExtractFinished(const int row, QByteArray bytes) override
	{
		m_decoder->Enqueue(row, std::move(bytes));
	}

private: // Decoder::IObserver
	void OnDecodeFinished(const int row, QPixmap pixmap) override
	{
		m_forwarder.Forward([this, row, pixmap = std::move(pixmap)]() mutable {
			auto& item       = m_items[row];
			item.pixmap      = std::move(pixmap);
			const auto index = this->index(row, 0);
			emit       dataChanged(index, index, { Qt::DecorationRole });
		});
	}

private:
	QVariant GetData(const int /*role*/) const
	{
		assert(false && "unexpected role");
		return {};
	}

	QVariant GetData(const QModelIndex& index, const int role) const
	{
		const auto& item = m_items[index.row()];
		switch (role)
		{
			case Qt::DecorationRole:
				return item.pixmap.isNull() ? m_imagePlaceholderScaled : item.pixmap;

			case ImageModelRole::Ready:
				return !item.pixmap.isNull();

			default:
				break;
		}
		return {};
	}

	bool SetData(const QVariant& value, const int role)
	{
		switch (role)
		{
			case ImageModelRole::Folder:
				return Reset(value.value<QModelIndex>()), true;

			case ImageModelRole::ImageSize:
				return Util::Set(m_imageSize, value.toInt(), [&] {
					m_decoder.reset();
					for (auto& item : m_items)
						item.pixmap = {};

					m_decoder                = std::make_unique<Decoder>(m_imageSize, *this);
					m_imagePlaceholderScaled = m_imagePlaceholder.scaled(value.toInt(), value.toInt(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
					emit dataChanged(index(0, 0), index(rowCount({}) - 1, 0), { Qt::DecorationRole });
				});

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	bool SetData(const QModelIndex& index, const QVariant& value, const int role)
	{
		switch (role)
		{
			case ImageModelRole::Prepare:
				return Prepare(index.row()), true;

			default:
				break;
		}
		return assert(false && "unexpected role"), QAbstractListModel::setData(index, value, role);
	}

private:
	void Reset(const QModelIndex& index)
	{
		m_requestedFolderTitle = QFileInfo(index.data().toString()).completeBaseName();
		m_booksTimer->start();
	}

	void ResetImpl()
	{
		const ScopedCall resetGuard(
			[this] {
				beginResetModel();
			},
			[this] {
				endResetModel();
			}
		);

		m_extractors.clear();
		m_decoder.reset();

		static constexpr const char* FOLDERS[] { Global::COVERS, Global::IMAGES };

		const auto folder = m_collectionProvider->GetActiveCollection().GetFolder();
		auto       zips   = FOLDERS | std::views::transform([&](const char* item) {
						const auto filePath = QString("%1/%2/%3.zip").arg(folder, item, m_requestedFolderTitle);
						if (!QFile::exists(filePath))
							return std::unique_ptr<const Zip> {};
						auto zip = TRY(QString("open %1").arg(m_requestedFolderTitle), [&] {
							return std::make_unique<const Zip>(filePath);
						});
						return zip;
							})
		                  | std::views::filter([](const auto& item) {
						return !!item;
							})
		                  | std::ranges::to<std::vector>();

		m_items.clear();
		for (auto&& [zipId, zip] : std::views::zip(std::views::iota(0), zips))
			std::ranges::transform(zip->GetFileNameList(), std::back_inserter(m_items), [&](const auto& item) {
				return Item { .zipId = zipId, .fileName = item };
			});

		m_decoder = std::make_unique<Decoder>(m_imageSize, *this);
		for (auto&& zip : zips)
			m_extractors.emplace_back(std::make_unique<Extractor>(std::move(zip), *this));
	}

	void Prepare(const int row)
	{
		auto& item  = m_items[row];
		item.pixmap = m_imagePlaceholderScaled;
		m_extractors[item.zipId]->Enqueue(row, item.fileName);
	}

private:
	Util::FunctorExecutionForwarder            m_forwarder;
	std::shared_ptr<const ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const IDatabaseUser>       m_databaseUser;

	int                     m_imageSize { -1 };
	QString                 m_requestedFolderTitle;
	std::unique_ptr<QTimer> m_booksTimer { Util::CreateUiTimer([this] {
		ResetImpl();
	}) };

	std::unique_ptr<Decoder>                m_decoder;
	std::vector<std::unique_ptr<Extractor>> m_extractors;

	Items   m_items;
	QPixmap m_imagePlaceholder, m_imagePlaceholderScaled;
};

class ModelFiltered final : public QSortFilterProxyModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser, QObject* parent = nullptr)
	{
		return std::make_unique<ModelFiltered>(std::move(collectionProvider), std::move(databaseUser), parent);
	}

public:
	ModelFiltered(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser, QObject* parent)
		: QSortFilterProxyModel(parent)
		, m_sourceModel { Model::Create(std::move(collectionProvider), std::move(databaseUser)) }
	{
		QSortFilterProxyModel::setSourceModel(m_sourceModel.get());
	}

private: // QAbstractItemModel
	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		//		if (index.isValid())
		//		{
		//			assert(role == Qt::CheckStateRole);
		//			SetChecked(index, value.value<Qt::CheckState>());
		//			return true;
		//		}
		//
		//		switch (role)
		//		{
		//			case Role::VisibleGenreCodes:
		//				return Util::Set(
		//					m_visibleGenres,
		//					value.value<std::unordered_set<QString>>(),
		//					[this] {
		//						BEGIN_FILTER_CHANGE;
		//					},
		//					[this] {
		//						END_FILTER_CHANGE;
		//					}
		//				);
		//
		//			default:
		//				break;
		//		}

		return QSortFilterProxyModel::setData(index, value, role);
	}

private: // QSortFilterProxyModel
	bool filterAcceptsRow(const int /*sourceRow*/, const QModelIndex& /*sourceParent*/) const override
	{
		return true;
	}

private:
	PropagateConstPtr<QAbstractItemModel> m_sourceModel;
};

} // namespace

ImageModel::ImageModel(std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	: m_model { ModelFiltered::Create(std::move(collectionProvider), std::move(databaseUser)) }
{
	QIdentityProxyModel::setSourceModel(m_model.get());
}

ImageModel::~ImageModel() = default;
