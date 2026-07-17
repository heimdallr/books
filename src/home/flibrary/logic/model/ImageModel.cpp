#include "ImageModel.h"

#include <QFileInfo>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QTimer>

#include "fnd/ScopedCall.h"
#include "fnd/try.h"

#include "interface/constants/ImageModelRole.h"
#include "interface/constants/ModelRole.h"
#include "interface/localization.h"

#include "data/DataItem.h"
#include "settings/UiTimer.h"
#include "util/FunctorExecutionForwarder.h"
#include "util/ImageUtil.h"
#include "util/executor/ThreadPool.h"

#include "Constant.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT = "ImageModel";
constexpr auto COVER   = QT_TRANSLATE_NOOP("ImageModel", "Cover");
constexpr auto IMAGE   = QT_TRANSLATE_NOOP("ImageModel", "Image #%1");

TR_DEF

struct Item
{
	int            zipId;
	QString        fileName;
	IDataItem::Ptr book;
	bool           isCover;
	int            ordNum { -1 };
	QPixmap        pixmap;
};

using Items = std::vector<Item>;

class Extractor
{
public:
	class IObserver // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IObserver() = default;

		virtual void OnItemsCreated(Items items)                  = 0;
		virtual void OnExtractFinished(int row, QByteArray bytes) = 0;
		virtual void OnRequestFinished(int row, QByteArray bytes) = 0;
	};

public:
	Extractor(int n, QString filePath, IDataItem::Items books, IObserver& observer)
		: m_observer { observer }
	{
		m_threadPool.enqueue([this, n, filePath = std::move(filePath), books = std::move(books)](size_t&) mutable {
			m_zip = TRY(QString("open %1").arg(filePath), [&] {
				return std::make_unique<Zip>(filePath);
			});
			if (!m_zip)
				return;

			std::unordered_map<QString, IDataItem::Ptr> bookFiles;
			for (auto&& book : books)
			{
				auto fileName = QFileInfo(book->GetRawData(BookItem::Column::FileName)).completeBaseName();
				bookFiles.try_emplace(std::move(fileName), std::move(book));
			}

			Items items;

			for (auto&& fileName : m_zip->GetFileNameList())
			{
				const auto split = fileName.split('/', Qt::SkipEmptyParts);
				if (const auto it = bookFiles.find(split.front()); it != bookFiles.end())
				{
					if (auto& item = items.emplace_back(n, std::move(fileName), it->second, split.size() == 1); !item.isCover)
						item.ordNum = split.back().toInt();
				}
			}

			std::ranges::sort(items, {}, [](const Item& item) {
				return std::tuple<const QString&, const QString, const QString, int>(
					item.book->GetRawData(BookItem::Column::AuthorFull),
					item.book->GetRawData(BookItem::Column::Title),
					item.book->GetId(),
					item.ordNum
				);
			});

			m_observer.OnItemsCreated(std::move(items));
		});
	}

	void ExtractImage(const int row, QString fileName)
	{
		Enqueue(row, std::move(fileName), &IObserver::OnExtractFinished);
	}

	void RequestImage(const int row, QString fileName)
	{
		Enqueue(row, std::move(fileName), &IObserver::OnRequestFinished);
	}

private:
	void Enqueue(const int row, QString fileName, void(IObserver::*onFinished)(int, QByteArray))
	{
		if (!m_zip)
			return;

		m_threadPool.enqueue([this, row, fileName = std::move(fileName), onFinished](size_t&) {
			auto bytes = m_zip->Read(fileName)->GetStream().readAll();
			std::invoke(onFinished, std::ref(m_observer), row, std::move(bytes));
		});
	}

private:
	IObserver&                 m_observer;
	std::unique_ptr<const Zip> m_zip;
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

		virtual void OnDecodeScaledFinished(int row, QPixmap pixmap) = 0;
		virtual void OnDecodeFinished(int row, QPixmap pixmap)       = 0;
	};

	Decoder(const int imageSize, IObserver& observer)
		: m_imageSize { imageSize }
		, m_observer { observer }
	{
	}

	void DecodeScaled(const int row, QByteArray bytes)
	{
		m_threadPool.enqueue([this, row, bytes = std::move(bytes)](size_t&) {
			auto pixmap = Util::Decode(bytes);
			if (std::max(pixmap.width(), pixmap.height()) > m_imageSize)
				pixmap = pixmap.scaled(m_imageSize, m_imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			m_observer.OnDecodeScaledFinished(row, std::move(pixmap));
		});
	}

	void Decode(const int row, QByteArray bytes)
	{
		m_threadPool.enqueue([this, row, bytes = std::move(bytes)](size_t&) {
			auto pixmap = Util::Decode(bytes);
			m_observer.OnDecodeFinished(row, std::move(pixmap));
		}, true);
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
	void OnItemsCreated(Items items) override
	{
		if (items.empty())
			return;

		m_forwarder.Forward([this, items = std::move(items)]() mutable {
			const ScopedCall insertGuard(
				[&] {
					beginInsertRows({}, static_cast<int>(m_items.size()), static_cast<int>(m_items.size() + items.size()) - 1);
				},
				[this] {
					endInsertRows();
				}
			);
			std::ranges::move(items, std::back_inserter(m_items));
		});
	}

	void OnExtractFinished(const int row, QByteArray bytes) override
	{
		m_decoder->DecodeScaled(row, std::move(bytes));
	}

	void OnRequestFinished(const int row, QByteArray bytes) override
	{
		m_decoder->Decode(row, std::move(bytes));
	}

private: // Decoder::IObserver
	void OnDecodeScaledFinished(const int row, QPixmap pixmap) override
	{
		m_forwarder.Forward([this, row, pixmap = std::move(pixmap)]() mutable {
			auto& item       = m_items[row];
			item.pixmap      = std::move(pixmap);
			const auto index = this->index(row, 0);
			emit       dataChanged(index, index, { Qt::DecorationRole });
		});
	}

	void OnDecodeFinished(const int row, QPixmap pixmap) override
	{
		m_forwarder.Forward([this, row, pixmap = std::move(pixmap)]() mutable {
			m_imageRequested = std::move(pixmap);
			m_imageRequestedFileName = m_items[row].fileName;
			const auto index = this->index(row, 0);
			emit       dataChanged(index, index, { ImageModelRole::Image });
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

			case Qt::ToolTipRole:
				return QString("%1. %2\n%3")
				    .arg(
						item.book->GetRawData(BookItem::Column::AuthorFull),
						item.book->GetRawData(BookItem::Column::Title),
						item.isCover ? Tr(COVER) : Tr(IMAGE).arg(item.fileName.split('/', Qt::SkipEmptyParts).back())
					);

			case ImageModelRole::Ready:
				return !item.pixmap.isNull();

			case ImageModelRole::Image:
				return item.fileName == m_imageRequestedFileName ? m_imageRequested : QVariant {};

			default:
				break;
		}
		return {};
	}

	bool SetData(const QVariant& value, const int role)
	{
		switch (role)
		{
			case ImageModelRole::BooksRoot:
				return Reset(value.value<IDataItem::Ptr>()), true;

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
		const auto& item = m_items[index.row()];
		switch (role)
		{
			case ImageModelRole::Prepare:
				return Prepare(index.row()), true;

			case ImageModelRole::Image:
				return m_extractors[item.zipId]->RequestImage(index.row(), item.fileName), true;

			default:
				break;
		}
		return assert(false && "unexpected role"), QAbstractListModel::setData(index, value, role);
	}

private:
	void Reset(IDataItem::Ptr root)
	{
		m_bookRoot = std::move(root);
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
		m_items.clear();

		std::unordered_map<QString, IDataItem::Items> folders;

		static constexpr const char* FOLDERS[] { Global::COVERS, Global::IMAGES };

		const auto rootFolder = m_collectionProvider->GetActiveCollection().GetFolder();
		for (size_t i = 0, sz = m_bookRoot->GetChildCount(); i < sz; ++i)
		{
			auto       book       = m_bookRoot->GetChild(i);
			const auto folderName = QFileInfo(book->GetRawData(BookItem::Column::Folder)).completeBaseName();
			for (const auto* folder : FOLDERS)
			{
				const auto filePath = QString("%1/%2/%3.zip").arg(rootFolder, folder, folderName);
				if (QFile::exists(filePath))
					folders[filePath].emplace_back(book);
			}
		}

		m_decoder = std::make_unique<Decoder>(m_imageSize, *this);
		for (int n = 0; auto&& [filePath, books] : folders)
			m_extractors.emplace_back(std::make_unique<Extractor>(n++, filePath, std::move(books), *this));
	}

	void Prepare(const int row)
	{
		auto& item  = m_items[row];
		item.pixmap = m_imagePlaceholderScaled;
		m_extractors[item.zipId]->ExtractImage(row, item.fileName);
	}

private:
	Util::FunctorExecutionForwarder            m_forwarder;
	std::shared_ptr<const ICollectionProvider> m_collectionProvider;
	std::shared_ptr<const IDatabaseUser>       m_databaseUser;

	int                     m_imageSize { -1 };
	IDataItem::Ptr          m_bookRoot;
	std::unique_ptr<QTimer> m_booksTimer { Util::CreateUiTimer([this] {
		ResetImpl();
	}) };

	std::unique_ptr<Decoder>                m_decoder;
	std::vector<std::unique_ptr<Extractor>> m_extractors;

	Items   m_items;
	QPixmap m_imagePlaceholder, m_imagePlaceholderScaled, m_imageRequested;
	QString m_imageRequestedFileName;
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
