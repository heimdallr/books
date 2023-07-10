#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "interface/logic/ILogicFactory.h"
#include "interface/ISettings.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

namespace {
constexpr const auto GEOMETRY_KEY = "ui/MainWindow/Geometry";
constexpr const auto VERTICAL_SPLITTER_KEY = "ui/MainWindow/VSplitter";
constexpr const auto HORIZONTALL_SPLITTER_KEY = "ui/MainWindow/HSplitter";
}

struct MainWindow::Impl
{
    Ui::MainWindow ui;
    std::shared_ptr<ILogicFactory> logicFactory;
    std::shared_ptr<ISettings> settings;

    const ILogicFactory::Guard logicFactoryGuard;

    Impl(std::shared_ptr<ILogicFactory> logicFactory_
        , std::shared_ptr<ISettings> settings_
    )
        : logicFactory(std::move(logicFactory_))
        , settings(std::move(settings_))
        , logicFactoryGuard(*logicFactory)
    {
    }
};

MainWindow::MainWindow(std::shared_ptr<ILogicFactory> logicFactory
    , std::shared_ptr<ISettings> settings
    , QWidget *parent
)
    : QMainWindow(parent)
    , m_impl(std::move(logicFactory), std::move(settings))
{
    m_impl->ui.setupUi(this);
    connect(m_impl->ui.actionReload, &QAction::triggered, this, []
    {
        QCoreApplication::exit(1234);
    });

    if (const auto value = m_impl->settings->Get(GEOMETRY_KEY); value.isValid())
        setGeometry(value.toRect());

    if (const auto value = m_impl->settings->Get(VERTICAL_SPLITTER_KEY); value.isValid())
        m_impl->ui.verticalSplitter->restoreState(value.toByteArray());

    if (const auto value = m_impl->settings->Get(HORIZONTALL_SPLITTER_KEY); value.isValid())
        m_impl->ui.horizontalSplitter->restoreState(value.toByteArray());

    PLOGD << "MainWindow created";
}

MainWindow::~MainWindow()
{
    m_impl->settings->Set(GEOMETRY_KEY, geometry());
    m_impl->settings->Set(VERTICAL_SPLITTER_KEY, m_impl->ui.verticalSplitter->saveState());
    m_impl->settings->Set(HORIZONTALL_SPLITTER_KEY, m_impl->ui.horizontalSplitter->saveState());

    PLOGD << "MainWindow destroyed";
}
