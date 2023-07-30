#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QMenu;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class IBooksModelControllerObserver;

class LocaleController final
	: public QObject
{
	NON_COPY_MOVABLE(LocaleController)
	Q_OBJECT

signals:
	void LocaleChanged() const;

public:
	LocaleController(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IUiFactory> uiFactory
		, QObject * parent = nullptr);
	~LocaleController() override;

public:
	void Setup(QMenu & menu);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
