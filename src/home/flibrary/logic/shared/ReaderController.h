#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IReaderController.h"
#include "interface/ui/IUiFactory.h"

#include "settings/ISettings.h"

class QString;

namespace HomeCompa::Flibrary {

struct SetForIdsArg;

class ReaderController final
    : public QObject
    , public IReaderController
{
	Q_OBJECT
	NON_COPY_MOVABLE(ReaderController)

signals:
	void SetUserRate(const SetForIdsArg& args) const;

public:
	ReaderController(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<ISettings>                  settings,
		std::shared_ptr<ICollectionController>      collectionController,
		std::shared_ptr<IUiFactory>                 uiFactory,
		std::shared_ptr<const IDatabaseProvider>    databaseProvider
	);
	~ReaderController() override;

public: // IReaderController
	void Read(long long id) const override;
	void ReadRandomBook(QString lang) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
