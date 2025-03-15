#pragma once

#include <QObject>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

class QMenu;

namespace HomeCompa::Flibrary
{

class IBooksModelControllerObserver;

class LocaleController final : public QObject
{
	NON_COPY_MOVABLE(LocaleController)
	Q_OBJECT

signals:
	void LocaleChanged() const;

public:
	LocaleController(std::shared_ptr<ISettings> settings, std::shared_ptr<IUiFactory> uiFactory, QObject* parent = nullptr);
	~LocaleController() override;

public:
	void Setup(QMenu& menu);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
