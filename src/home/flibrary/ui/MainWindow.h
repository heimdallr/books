#pragma once

#include <QMainWindow>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class MainWindow final
	: public QMainWindow
{
    NON_COPY_MOVABLE(MainWindow)

public:
    MainWindow(const std::shared_ptr<class IUiFactory> & uiFactory
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<class ICollectionController> collectionController
        , std::shared_ptr<class ParentWidgetProvider> parentWidgetProvider
        , QWidget * parent = nullptr);
    ~MainWindow() override;

private:
    class Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
