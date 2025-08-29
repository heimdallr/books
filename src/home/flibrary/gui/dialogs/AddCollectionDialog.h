#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionController.h"
#include "interface/ui/IUiFactory.h"
#include "interface/ui/dialogs/IAddCollectionDialog.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AddCollectionDialog final
	: public QDialog
	, virtual public IAddCollectionDialog
{
	NON_COPY_MOVABLE(AddCollectionDialog)

public:
	AddCollectionDialog(const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
	                    std::shared_ptr<ISettings> settings,
	                    std::shared_ptr<ICollectionController> collectionController,
	                    std::shared_ptr<const IUiFactory> uiFactory);
	~AddCollectionDialog() override;

private: // IAddCollectionDialog
	[[nodiscard]] int Exec() override;
	[[nodiscard]] QString GetName() const override;
	[[nodiscard]] QString GetDatabaseFileName() const override;
	[[nodiscard]] QString GetArchiveFolder() const override;
	[[nodiscard]] QString GetDefaultArchiveType() const override;
	[[nodiscard]] bool AddUnIndexedBooks() const override;
	[[nodiscard]] bool ScanUnIndexedFolders() const override;
	[[nodiscard]] bool SkipLostBooks() const override;
	[[nodiscard]] bool MarkUnIndexedBooksAsDeleted() const override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
