#pragma once

#include <QMainWindow>

#include "fnd/memory.h"

namespace HomeCompa {
class ILogicFactory;
class ISettings;
}

namespace HomeCompa::Flibrary {

class MainWindow : public QMainWindow
{
public:
    MainWindow(std::shared_ptr<ILogicFactory> logicFactory
        , std::shared_ptr<ISettings> settings
        , QWidget * parent = nullptr);
    ~MainWindow() override;

private:
    struct Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
