#pragma once

#include <QMainWindow>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class ICollectionController;
class ILogicFactory;
class ISettings;
class IUiFactory;
}

namespace HomeCompa::Flibrary {

class MainWindow final
	: public QMainWindow
{
    NON_COPY_MOVABLE(MainWindow)

public:
    MainWindow(std::shared_ptr<ILogicFactory> logicFactory
        , std::shared_ptr<IUiFactory> uiFactory
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<ICollectionController> collectionController
        , QWidget * parent = nullptr);
    ~MainWindow() override;

private:
    class Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
