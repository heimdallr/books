#include "ui_MainWindow.h"
#include "MainWindow.h"

#include <plog/Log.h>

#include "GeometryRestorable.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "interface/ui/IUiFactory.h"
#include "ParentWidgetProvider.h"
#include "util/ISettings.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto VERTICAL_SPLITTER_KEY = "ui/MainWindow/VSplitter";
constexpr auto HORIZONTAL_SPLITTER_KEY = "ui/MainWindow/HSplitter";
}

class MainWindow::Impl
    : GeometryRestorable
	, GeometryRestorable::IObserver
{
    NON_COPY_MOVABLE(Impl)

public:
    Impl(MainWindow & self
        , std::shared_ptr<ILogicFactory> logicFactory
        , std::shared_ptr<IUiFactory> uiFactory
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<ICollectionController> collectionController
        , std::shared_ptr<ParentWidgetProvider> parentWidgetProvider
    )
	    : GeometryRestorable(*this, settings, "MainWindow")
        , m_self(self)
        , m_logicFactory(std::move(logicFactory))
        , m_uiFactory(std::move(uiFactory))
        , m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
		, m_parentWidgetProvider(std::move(parentWidgetProvider))
        , m_logicFactoryGuard(*logicFactory)
        , m_booksWidget(this->m_uiFactory->CreateTreeViewWidget(TreeViewControllerType::Books))
        , m_navigationWidget(this->m_uiFactory->CreateTreeViewWidget(TreeViewControllerType::Navigation))
    {
        Init();
    }

	~Impl() override
    {
        m_settings->Set(VERTICAL_SPLITTER_KEY, m_ui.verticalSplitter->saveState());
        m_settings->Set(HORIZONTAL_SPLITTER_KEY, m_ui.horizontalSplitter->saveState());
    }

private: // GeometryRestorable::IObserver
    QWidget & GetWidget() noexcept override
    {
        return m_self;
    }

private:
    void Init()
    {
        m_ui.setupUi(&m_self);

        m_parentWidgetProvider->SetWidget(&m_self);

        m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
        m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());

        if (const auto value = m_settings->Get(VERTICAL_SPLITTER_KEY); value.isValid())
            m_ui.verticalSplitter->restoreState(value.toByteArray());

        if (const auto value = m_settings->Get(HORIZONTAL_SPLITTER_KEY); value.isValid())
            m_ui.horizontalSplitter->restoreState(value.toByteArray());

        const auto incrementFontSize = [&](const int value)
        {
            bool ok = false;
            const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT).toInt(&ok);
            m_settings->Set(Constant::Settings::FONT_SIZE_KEY, (ok ? fontSize : Constant::Settings::FONT_SIZE_DEFAULT) + value);
        };

        connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [incrementFontSize] { incrementFontSize(1); });
        connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [incrementFontSize] { incrementFontSize(-1); });
        connect(m_ui.actionExit, &QAction::triggered, &m_self, [] { QCoreApplication::exit(); });
        connect(m_ui.actionReload, &QAction::triggered, &m_self, [] { QCoreApplication::exit(1234); });
        connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [&]
        {
            if (m_collectionController->AddCollection())
                QCoreApplication::exit(1234);
        });

        GeometryRestorable::Init();

        if (m_collectionController->IsEmpty() && !m_collectionController->AddCollection())
            throw std::runtime_error("No collections found");
    }

private:
    MainWindow & m_self;
    Ui::MainWindow m_ui {};
    PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
    PropagateConstPtr<IUiFactory, std::shared_ptr> m_uiFactory;
    PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
    PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
    PropagateConstPtr<ParentWidgetProvider, std::shared_ptr> m_parentWidgetProvider;

    const ILogicFactory::Guard m_logicFactoryGuard;

    PropagateConstPtr<QWidget, std::shared_ptr> m_booksWidget;
    PropagateConstPtr<QWidget, std::shared_ptr> m_navigationWidget;
};

MainWindow::MainWindow(std::shared_ptr<ILogicFactory> logicFactory
    , std::shared_ptr<IUiFactory> uiFactory
    , std::shared_ptr<ISettings> settings
    , std::shared_ptr<ICollectionController> collectionController
    , std::shared_ptr<ParentWidgetProvider> parentWidgetProvider
    , QWidget * parent
)
    : QMainWindow(parent)
    , m_impl(*this
        , std::move(logicFactory)
        , std::move(uiFactory)
        , std::move(settings)
        , std::move(collectionController)
        , std::move(parentWidgetProvider)
    )
{
    PLOGD << "MainWindow created";
}

MainWindow::~MainWindow()
{
    PLOGD << "MainWindow destroyed";
}
