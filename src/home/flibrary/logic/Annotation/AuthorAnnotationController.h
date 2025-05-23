#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAuthorAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary
{

class AuthorAnnotationController final : virtual public IAuthorAnnotationController
{
	NON_COPY_MOVABLE(AuthorAnnotationController)

public:
	AuthorAnnotationController(const std::shared_ptr<const ILogicFactory>& logicFactory, const std::shared_ptr<const ICollectionProvider>& collectionProvider);
	~AuthorAnnotationController() override;

private: // IAuthorAnnotationController
	bool IsReady() const noexcept override;

	void SetNavigationMode(NavigationMode mode) override;
	void SetAuthor(long long id, QString name) override;

	bool CheckAuthor(const QString& name) const override;
	QString GetInfo(const QString& name) const override;
	std::vector<QByteArray> GetImages(const QString& name) const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
