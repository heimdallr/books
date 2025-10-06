#include "AuthorAnnotationController.h"

#include <QCryptographicHash>

#include <ranges>

#include <QDir>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "interface/constants/Enums.h"

#include "inpx/constant.h"
#include "util/IExecutor.h"

#include "Constant.h"
#include "log.h"

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
		return m_authorsMode && !m_authorToArchive.empty();
	}

	void SetNavigationMode(const NavigationMode mode)
	{
		if (Util::Set(m_authorsMode, mode == NavigationMode::Authors))
			Perform(&IObserver::OnReadyChanged);
	}

	void SetAuthor(const long long id, QString name)
	{
		m_authorId = id;
		if (m_authorToArchive.empty())
			return;

		assert(m_executor);
		(*m_executor)(
			{ "Extract author's annotation",
		      [this, id, name = std::move(name)]() mutable {
				  auto annotation = GetAnnotation(name);
				  return [this, id, annotation = std::move(annotation)](size_t) {
					  if (id == m_authorId)
						  Perform(&IObserver::OnAuthorChanged, std::cref(annotation.first), std::cref(annotation.second));
				  };
			  } },
			1000
		);
	}

	bool CheckAuthor(const QString& name) const
	{
		return IsReady() ? Find(name).second >= 0 : false;
	}

	QString GetInfo(const QString& name) const
	{
		if (const auto [hashed, index] = Find(name); index >= 0)
		{
#ifndef NDEBUG
			PLOGV << name << ": " << index << ". " << hashed;
#endif
			return GetAnnotationText(hashed, index);
		}

		return {};
	}

	std::vector<QByteArray> GetImages(const QString& name) const
	{
		if (const auto [hashed, index] = Find(name); index >= 0)
			return GetAnnotationImages(hashed, index);

		return {};
	}

private:
	std::pair<QString, int> Find(const QString& name) const
	{
		QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
		hash.addData(name.split(' ', Qt::SkipEmptyParts).join(' ').toLower().simplified().toUtf8());
		auto       hashed = hash.result().toHex();
		const auto it     = m_authorToArchive.find(hashed);

		return std::make_pair(std::move(hashed), it != m_authorToArchive.end() ? it->second : -1);
	}

	void Init(const ILogicFactory& logicFactory, const ICollectionProvider& collectionProvider)
	{
		if (!collectionProvider.ActiveCollectionExists())
			return;

		m_authorsDir = QDir(collectionProvider.GetActiveCollection().folder + "/" + QString::fromStdWString(AUTHORS_FOLDER));
		if (!m_authorsDir.exists())
			return;

		m_executor = logicFactory.GetExecutor();
		(*m_executor)(
			{ "Create author's annotations map",
		      [this] {
				  std::unordered_map<QString, int> authorToArchive;
				  for (const auto& file : m_authorsDir.entryList(QDir::Files))
				  {
					  Zip zip(m_authorsDir.filePath(file));
					  std::ranges::transform(zip.GetFileNameList(), std::inserter(authorToArchive, authorToArchive.end()), [&](const auto& item) {
						  return std::make_pair(item, QFileInfo(file).baseName().toInt());
					  });
				  }
				  return [this, authorToArchive = std::move(authorToArchive)](size_t) mutable {
					  if (authorToArchive.empty())
						  return m_executor.reset();

					  m_authorToArchive = std::move(authorToArchive);
					  Perform(&IObserver::OnReadyChanged);
				  };
			  } },
			1000
		);
	}

	std::pair<QString, std::vector<QByteArray>> GetAnnotation(const QString& name) const
	{
		if (const auto [hashed, index] = Find(name); index >= 0)
			return std::make_pair(GetAnnotationText(hashed, index), GetAnnotationImages(hashed, index));

		return {};
	}

	QString GetAnnotationText(const QString& name, const int file) const
	{
		const auto annotationFileName = m_authorsDir.filePath(QString("%1.7z").arg(file));
		assert(QFile::exists(annotationFileName));
		Zip zip(annotationFileName);
		return QString::fromUtf8(zip.Read(name)->GetStream().readAll());
	}

	std::vector<QByteArray> GetAnnotationImages(const QString& name, const int file) const
	{
		const auto imagesFileName = m_authorsDir.filePath(QString("%1/%2.zip").arg(Global::PICTURES).arg(file));
		if (!QFile::exists(imagesFileName))
			return {};

		std::vector<QByteArray> result;
		Zip                     zip(imagesFileName);
		std::ranges::transform(
			zip.GetFileNameList() | std::views::filter([&](const auto& item) {
				return item.startsWith(name);
			}),
			std::back_inserter(result),
			[&](const auto& item) {
				return zip.Read(item)->GetStream().readAll();
			}
		);
		return result;
	}

private:
	bool m_authorsMode { false };

	QDir                             m_authorsDir;
	std::unique_ptr<Util::IExecutor> m_executor;

	std::unordered_map<QString, int> m_authorToArchive;

	long long m_authorId {};
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

void AuthorAnnotationController::SetAuthor(const long long id, QString name)
{
	m_impl->SetAuthor(id, std::move(name));
}

bool AuthorAnnotationController::CheckAuthor(const QString& name) const
{
	return m_impl->CheckAuthor(name);
}

QString AuthorAnnotationController::GetInfo(const QString& name) const
{
	return m_impl->GetInfo(name);
}

std::vector<QByteArray> AuthorAnnotationController::GetImages(const QString& name) const
{
	return m_impl->GetImages(name);
}

void AuthorAnnotationController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void AuthorAnnotationController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
