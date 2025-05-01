#include "AuthorAnnotationController.h"

#include <QCryptographicHash>

#include <ranges>

#include <QDir>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "interface/constants/Enums.h"

#include "inpx/src/util/constant.h"
#include "util/IExecutor.h"

#include "Constant.h"

using namespace HomeCompa::Flibrary;

class AuthorAnnotationController::Impl final : public Observable<IObserver>
{
public:
	Impl(const ILogicFactory& logicFactory, const ICollectionProvider& collectionProvider)
	{
		Init(logicFactory, collectionProvider);
	}

	bool IsReady() const noexcept
	{
		return m_ready && m_authorsMode;
	}

	void SetNavigationMode(const NavigationMode mode)
	{
		if (Util::Set(m_authorsMode, mode == NavigationMode::Authors))
			Perform(&IObserver::OnReadyChanged);
	}

	void SetAuthor(const long long id, QString lastName, QString firstName, QString middleName)
	{
		m_authorId = id;
		if (!m_executor)
			return;

		(*m_executor)({ "Extract author's annotation",
		                [this, id, lastName = std::move(lastName), firstName = std::move(firstName), middleName = std::move(middleName)]() mutable
		                {
							auto annotation = GetAnnotation(std::move(lastName), firstName, middleName);
							return [this, id, annotation = std::move(annotation)](size_t)
							{
								if (id == m_authorId)
									Perform(&IObserver::OnAuthorChanged, std::cref(annotation.first), std::cref(annotation.second));
							};
						} },
		              1000);
	}

private:
	void Init(const ILogicFactory& logicFactory, const ICollectionProvider& collectionProvider)
	{
		if (!collectionProvider.ActiveCollectionExists())
			return;

		m_authorsDir = QDir(collectionProvider.GetActiveCollection().folder + "/" + QString::fromStdWString(AUTHORS_FOLDER));
		if (!m_authorsDir.exists())
			return;

		m_executor = logicFactory.GetExecutor();
		(*m_executor)({ "Create author's annotations map",
		                [this]
		                {
							std::unordered_map<QString, int> authorToArchive;
							for (const auto& file : m_authorsDir.entryList(QDir::Files))
							{
								Zip zip(m_authorsDir.filePath(file));
								std::ranges::transform(zip.GetFileNameList(),
				                                       std::inserter(authorToArchive, authorToArchive.end()),
				                                       [&](const auto& item) { return std::make_pair(item, QFileInfo(file).baseName().toInt()); });
							}
							return [this, authorToArchive = std::move(authorToArchive)](size_t) mutable
							{
								if (authorToArchive.empty())
									return m_executor.reset();

								m_authorToArchive = std::move(authorToArchive);
								m_ready = true;
								Perform(&IObserver::OnReadyChanged);
							};
						} },
		              1000);
	}

	std::pair<QString, std::vector<QByteArray>> GetAnnotation(QString lastName, const QString& firstName, const QString& middleName) const
	{
		QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
		hash.addData(lastName.append(' ').append(firstName).append(' ').append(middleName).split(' ', Qt::SkipEmptyParts).join(' ').toLower().simplified().toUtf8());
		const auto hashed = hash.result().toHex();

		if (const auto it = m_authorToArchive.find(hashed); it != m_authorToArchive.end())
			return std::make_pair(GetAnnotationText(hashed, it->second), GetAnnotationImages(hashed, it->second));

		return {};
	}

	QString GetAnnotationText(const QString& name, const int file) const
	{
		const auto annotationFileName = m_authorsDir.filePath(QString("%1.7z").arg(file));
		assert(QFile::exists(annotationFileName));
		Zip zip(annotationFileName);
		auto result = QString::fromUtf8(zip.Read(name)->GetStream().readAll());
		result.replace("[b]", "<b>").replace("[/b]", "</b>");
		return result;
	}

	std::vector<QByteArray> GetAnnotationImages(const QString& name, const int file) const
	{
		const auto imagesFileName = m_authorsDir.filePath(QString("%1/%2.zip").arg(Global::IMAGES).arg(file));
		if (!QFile::exists(imagesFileName))
			return {};

		std::vector<QByteArray> result;
		Zip zip(imagesFileName);
		std::ranges::transform(zip.GetFileNameList() | std::views::filter([&](const auto& item) { return item.startsWith(name); }),
		                       std::back_inserter(result),
		                       [&](const auto& item) { return zip.Read(item)->GetStream().readAll(); });
		return result;
	}

private:
	bool m_ready { false };
	bool m_authorsMode { false };

	QDir m_authorsDir;
	std::unique_ptr<Util::IExecutor> m_executor;

	std::unordered_map<QString, int> m_authorToArchive;

	long long m_authorId;
};

AuthorAnnotationController::AuthorAnnotationController(const std::shared_ptr<const ILogicFactory>& logicFactory, const std::shared_ptr<const ICollectionProvider>& collectionProvider)
	: m_impl(*logicFactory, *collectionProvider)
{
}

AuthorAnnotationController::~AuthorAnnotationController() = default;

bool AuthorAnnotationController::IsReady() const noexcept
{
	return m_impl->IsReady();
}

void AuthorAnnotationController::SetNavigationMode(const NavigationMode mode)
{
	m_impl->SetNavigationMode(mode);
}

void AuthorAnnotationController::SetAuthor(const long long id, QString lastName, QString firstName, QString middleName)
{
	m_impl->SetAuthor(id, std::move(lastName), std::move(firstName), std::move(middleName));
}

void AuthorAnnotationController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void AuthorAnnotationController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
