#include "CollectionController.h"

#include <QDir>
#include <QFile>

#include <plog/Log.h>

#include "interface/ui/dialogs/IAddCollectionDialog.h"
#include "interface/ui/IUiFactory.h"

#include "Collection.h"

using namespace HomeCompa::Flibrary;

namespace {

constexpr int MAX_OVERWRITE_CONFIRM_COUNT = 10;

QString GetInpx(const QString & folder)
{
	const auto inpxList = QDir(folder).entryList({ "*.inpx" });
	auto result = inpxList.isEmpty() ? QString() : QString("%1/%2").arg(folder, inpxList.front());
	if (result.isEmpty())
		PLOGE << "Cannot find inpx in " << folder;

	return result;
}

}

class CollectionController::Impl
{
public:
	explicit Impl(std::shared_ptr<ISettings> settings
		, std::shared_ptr<IUiFactory> uiFactory
	)
		: m_settings(std::move(settings))
		, m_uiFactory(std::move(uiFactory))
	{
	}

	bool AddCollection()
	{
		auto added = false;
		switch (const auto dialog = m_uiFactory->CreateAddCollectionDialog(); dialog->Exec())
		{
			case IAddCollectionDialog::Result::CreateNew:
				added = CreateNew(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
				break;

			case IAddCollectionDialog::Result::Add:
				added = Add(dialog->GetName(), dialog->GetDatabaseFileName(), dialog->GetArchiveFolder());
				break;

			default:
				break;
		}

		m_overwriteConfirmCount = 0;
		return added;
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return m_collections.empty();
	}

	[[nodiscard]] bool IsCollectionNameExists(const QString & name) const
	{
		return std::ranges::any_of(m_collections, [&](const auto & item){ return item.name == name; });
	}

	[[nodiscard]] QString GetCollectionDatabaseName(const QString & databaseFileName) const
	{
		const auto it = std::ranges::find_if(m_collections, [id = Collection::GenerateId(databaseFileName)] (const auto & item)
		{
			return item.id == id;
		});

		return it != std::cend(m_collections) ? it->name : QString();
	}

	[[nodiscard]] static bool IsCollectionFolderHasInpx(const QString & folder)
	{
		return !GetInpx(folder).isEmpty();
	}

private:
	[[nodiscard]] bool CreateNew(const QString & /*name*/, const QString & db, const QString & /*folder*/)
	{
		if (QFile(db).exists())
		{
			if (m_uiFactory->ShowWarning("Warning", "The existing database file will be overwritten. Continue?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
				return ++m_overwriteConfirmCount < MAX_OVERWRITE_CONFIRM_COUNT
					? AddCollection()
					: false;

			PLOGW << db << " will be rewritten";
		}

		return true;
	}

	[[nodiscard]] bool Add(QString name, QString db, QString folder) const
	{
		const Collection collection(std::move(name), std::move(db), std::move(folder));
		collection.Serialize(*m_settings);
		return true;
	}

private:
	std::shared_ptr<ISettings> m_settings;
	std::shared_ptr<IUiFactory> m_uiFactory;
	Collections m_collections { Collection::Deserialize(*m_settings) };
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

bool CollectionController::AddCollection()
{
	return m_impl->AddCollection();
}

bool CollectionController::IsEmpty() const noexcept
{
	return m_impl->IsEmpty();
}

bool CollectionController::IsCollectionNameExists(const QString & name) const
{
	return m_impl->IsCollectionNameExists(name);
}

QString CollectionController::GetCollectionDatabaseName(const QString & databaseFileName) const
{
	return m_impl->GetCollectionDatabaseName(databaseFileName);
}

bool CollectionController::IsCollectionFolderHasInpx(const QString & folder) const
{
	return m_impl->IsCollectionFolderHasInpx(folder);
}
