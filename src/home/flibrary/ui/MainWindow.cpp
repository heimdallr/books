#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "interface/ISettings.h"
#include "interface/IUiFactory.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

namespace {
constexpr const auto GEOMETRY_KEY = "ui/MainWindow/Geometry";
constexpr const auto VERTICAL_SPLITTER_KEY = "ui/MainWindow/VSplitter";
constexpr const auto HORIZONTALL_SPLITTER_KEY = "ui/MainWindow/HSplitter";
}

struct MainWindow::Impl
{
    MainWindow & self;
    Ui::MainWindow ui;
    std::shared_ptr<ILogicFactory> logicFactory;
    std::shared_ptr<ISettings> settings;
    std::shared_ptr<IUiFactory> uiFactory;

    const ILogicFactory::Guard logicFactoryGuard;

    std::shared_ptr<QWidget> booksWidget;
    std::shared_ptr<QWidget> navigationWidget;

    Impl(MainWindow & self_
        , std::shared_ptr<ILogicFactory> logicFactory_
        , std::shared_ptr<IUiFactory> uiFactory_
        , std::shared_ptr<ISettings> settings_
    )
        : self(self_)
        , logicFactory(std::move(logicFactory_))
        , settings(std::move(settings_))
        , uiFactory(std::move(uiFactory_))
        , logicFactoryGuard(*logicFactory)
        , booksWidget(uiFactory->CreateTreeViewWidget(TreeViewControllerType::Books))
        , navigationWidget(uiFactory->CreateTreeViewWidget(TreeViewControllerType::Navigation))
    {
        ui.setupUi(&self_);

        ui.booksWidget->layout()->addWidget(booksWidget.get());
        ui.navigationWidget->layout()->addWidget(navigationWidget.get());
    }
};

MainWindow::MainWindow(std::shared_ptr<ILogicFactory> logicFactory
    , std::shared_ptr<IUiFactory> uiFactory
    , std::shared_ptr<ISettings> settings
    , QWidget *parent
)
    : QMainWindow(parent)
    , m_impl(*this
        , std::move(logicFactory)
        , std::move(uiFactory)
        , std::move(settings)
    )
{
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
