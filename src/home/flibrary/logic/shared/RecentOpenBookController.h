#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBookInteractor.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IRecentOpenBookController.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class RecentOpenBookController final : public IRecentOpenBookController
{
	NON_COPY_MOVABLE(RecentOpenBookController)

public:
	RecentOpenBookController(const std::shared_ptr<const ISettings>& settings, std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const IBookInteractor> bookInteractor);
	~RecentOpenBookController() override;

private: // IRecentOpenBookController
	void SetMenu(QMenu* menu) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
