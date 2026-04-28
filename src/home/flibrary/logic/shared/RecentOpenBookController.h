#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IRecentOpenBookController.h"
#include "interface/ui/IUiFactory.h"

namespace HomeCompa::Flibrary
{

class RecentOpenBookController final : public IRecentOpenBookController
{
	NON_COPY_MOVABLE(RecentOpenBookController)

public:
	RecentOpenBookController(std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<const IDatabaseUser> databaseUser);
	~RecentOpenBookController() override;

private: // IRecentOpenBookController
	void SetMenu(QMenu* menu) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
