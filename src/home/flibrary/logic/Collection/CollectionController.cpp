#include "CollectionController.h"

#include <QDir>
#include <QFile>

#include <plog/Log.h>

#include "fnd/observable.h"

#include "interface/constants/Localization.h"

#include "interface/ui/dialogs/IAddCollectionDialog.h"
#include "interface/ui/IUiFactory.h"

#include "CollectionImpl.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr int MAX_OVERWRITE_CONFIRM_COUNT = 10;
const Collection EMPTY_COLLECTION;

constexpr auto CONTEXT = "CollectionController";
constexpr auto CONFIRM_OVERWRITE_DATABASE = QT_TRANSLATE_NOOP("CollectionController", "The existing database file will be overwritten. Continue?");

QString Tr(const char * str)
{
	return Loc::Tr(CONTEXT, str);
}

QString GetInpx(const QString & folder)
{
	const auto inpxList = QDir(folder).entryList({ "*.inpx" });
	auto result = inpxList.isEmpty() ? QString() : QString("%1/%2").arg(folder, inpxList.front());
	if (result.isEmpty())
		PLOGE << "Cannot find inpx in " << folder;

	return result;
}

const Collection & FindCollectionById(const Collections & collections, const QString & id) noexcept
{
	const auto it = std::ranges::find_if(collections, [&] (const auto & item)
	{
		return item->id == id;
	});

	return it != std::cend(collections) ? **it : EMPTY_COLLECTION;

}

}

class CollectionController::Impl final
	: public Observable<IObserver>
{
public:
	explicit Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
	{
		if (std::ranges::none_of(m_collections, [id = CollectionImpl::GetActive(*m_settings)] (const auto & item) { return item->id == id; }))
			SetActiveCollection(m_collections.empty() ? EMPTY_COLLECTION : *m_collections.front());
	}

	const Collections & GetCollections() const noexcept
	{
		return m_collections;
	}

	const ISettings& GetSettings() const noexcept
	{
		return *m_settings;
	}

	ISettings & GetSettings() noexcept
	{
		return *m_settings;
	}

	void AddCollection()
	{
		switch (const auto dialog = m_uiFactory->CreateAddCollectionDialog(); dialog->Exec())
		{
			case IAddCollectionDialog::Result::CreateNew:
				CreateNew(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
				break;

			case IAddCollectionDialog::Result::Add:
				Add(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
				break;

			default:
				break;
		}

		m_overwriteConfirmCount = 0;
	}

	void SetActiveCollection(const Collection & collection)
	{
		CollectionImpl::SetActive(GetSettings(), collection.id);
		Perform(&IObserver::OnActiveCollectionChanged, std::cref(collection));
	}

private:
	void CreateNew(const QString & /*name*/, const QString & db, const QString & /*folder*/)
	{
		if (QFile(db).exists())
		{
			if (m_uiFactory->ShowWarning(Tr(CONFIRM_OVERWRITE_DATABASE), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
				return ++m_overwriteConfirmCount < MAX_OVERWRITE_CONFIRM_COUNT
					? AddCollection()
					: Perform(&IObserver::OnActiveCollectionChanged, std::cref(EMPTY_COLLECTION));

			PLOGW << db << " will be rewritten";
		}
	}

	void Add(QString name, QString db, QString folder)
	{
		const CollectionImpl collection(std::move(name), std::move(db), std::move(folder));
		CollectionImpl::Serialize(collection, *m_settings);
		SetActiveCollection(collection);
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
	PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
	Collections m_collections { CollectionImpl::Deserialize(*m_settings) };
	int m_overwriteConfirmCount { 0 };
};

CollectionController::CollectionController(std::shared_ptr<ISettings> settings
	, std::shared_ptr<IUiFactory> uiFactory
)
	: m_impl(std::move(settings)
		, std::move(uiFactory)
	)
{
	PLOGD << "CollectionController created";
}

CollectionController::~CollectionController()
{
	PLOGD << "CollectionController destroyed";
}

void CollectionController::AddCollection()
{
	m_impl->AddCollection();
}

bool CollectionController::IsEmpty() const noexcept
{
	return GetCollections().empty();
}

bool CollectionController::IsCollectionNameExists(const QString & name) const
{
	return std::ranges::any_of(GetCollections(), [&] (const auto & item)
	{
		return item->name == name;
	});
}

QString CollectionController::GetCollectionDatabaseName(const QString & databaseFileName) const
{
	return FindCollectionById(GetCollections(), CollectionImpl::GenerateId(databaseFileName)).name;
}

bool CollectionController::IsCollectionFolderHasInpx(const QString & folder) const
{
	return !GetInpx(folder).isEmpty();
}

const Collections & CollectionController::GetCollections() const noexcept
{
	return m_impl->GetCollections();
}

const Collection & CollectionController::GetActiveCollection() const noexcept
{
	return FindCollectionById(GetCollections(), CollectionImpl::GetActive(m_impl->GetSettings()));
}

void CollectionController::SetActiveCollection(const QString & id)
{
	m_impl->SetActiveCollection(FindCollectionById(m_impl->GetCollections(), id));
}

void CollectionController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void CollectionController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
