#pragma once

#include <QDialog>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/ui/dialogs/IAddCollectionDialog.h"

namespace HomeCompa {
class IParentWidgetProvider;
class ISettings;
}

namespace HomeCompa::Flibrary {

class AddCollectionDialog final
	: public QDialog
	, virtual public IAddCollectionDialog
{
	NON_COPY_MOVABLE(AddCollectionDialog)

public:
    AddCollectionDialog(const std::shared_ptr<IParentWidgetProvider> & parentWidgetProvider
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IUiFactory> uiFactory
	);
    ~AddCollectionDialog() override;

private: // IAddCollectionDialog
	[[nodiscard]] int Exec() override;
	[[nodiscard]] QString GetName() const override;
	[[nodiscard]] QString GetDatabaseFileName() const override;
	[[nodiscard]] QString GetArchiveFolder() const override;
	[[nodiscard]] bool AddUnIndexedBooks() const override;
	[[nodiscard]] bool ScanUnIndexedFolders() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
