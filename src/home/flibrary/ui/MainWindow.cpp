#include "ui_MainWindow.h"
#include "MainWindow.h"

#include <QTimer>
#include <plog/Log.h>

#include "GeometryRestorable.h"
#include "interface/constants/Enums.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/logic/ICollectionController.h"
#include "interface/ui/IUiFactory.h"
#include "ParentWidgetProvider.h"
#include "util/ISettings.h"
#include "TreeView.h"

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto VERTICAL_SPLITTER_KEY = "ui/MainWindow/VSplitter";
constexpr auto HORIZONTAL_SPLITTER_KEY = "ui/MainWindow/HSplitter";
}

class MainWindow::Impl final
    : GeometryRestorable
	, GeometryRestorable::IObserver
	, ICollectionController::IObserver
{
    NON_COPY_MOVABLE(Impl)

public:
    Impl(MainWindow & self
        , const std::shared_ptr<IUiFactory> & uiFactory
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<ICollectionController> collectionController
        , std::shared_ptr<ParentWidgetProvider> parentWidgetProvider
    )
	    : GeometryRestorable(*this, settings, "MainWindow")
        , m_self(self)
        , m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
		, m_parentWidgetProvider(std::move(parentWidgetProvider))
        , m_booksWidget(uiFactory->CreateTreeViewWidget(ItemType::Books))
        , m_navigationWidget(uiFactory->CreateTreeViewWidget(ItemType::Navigation))
    {
        Setup();
        ConnectActions();
        CreateCollectionsMenu();
        RestoreWidgetsState();
    }

	~Impl() override
    {
        m_collectionController->UnregisterObserver(this);

        m_settings->Set(VERTICAL_SPLITTER_KEY, m_ui.verticalSplitter->saveState());
        m_settings->Set(HORIZONTAL_SPLITTER_KEY, m_ui.horizontalSplitter->saveState());
    }

private: // GeometryRestorable::IObserver
    QWidget & GetWidget() noexcept override
    {
        return m_self;
    }

private: // ICollectionController::IObserver
    void OnActiveCollectionChanged(const Collection& collection) override
    {
        if (collection.id.isEmpty())
            throw std::runtime_error("No collections found");

        Reboot();
    }

private:
    void Setup()
    {
        m_ui.setupUi(&m_self);

        m_parentWidgetProvider->SetWidget(&m_self);

        m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
        m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());
    }

    void RestoreWidgetsState()
    {
        if (const auto value = m_settings->Get(VERTICAL_SPLITTER_KEY); value.isValid())
            m_ui.verticalSplitter->restoreState(value.toByteArray());

        if (const auto value = m_settings->Get(HORIZONTAL_SPLITTER_KEY); value.isValid())
            m_ui.horizontalSplitter->restoreState(value.toByteArray());

        Init();
    }

    void ConnectActions()
    {
        const auto incrementFontSize = [&](const int value)
        {
            bool ok = false;
            const auto fontSize = m_settings->Get(Constant::Settings::FONT_SIZE_KEY, Constant::Settings::FONT_SIZE_DEFAULT).toInt(&ok);
            m_settings->Set(Constant::Settings::FONT_SIZE_KEY, (ok ? fontSize : Constant::Settings::FONT_SIZE_DEFAULT) + value);
        };

        connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [incrementFontSize] { incrementFontSize(1); });
        connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [incrementFontSize] { incrementFontSize(-1); });
        connect(m_ui.actionExit, &QAction::triggered, &m_self, [] { QCoreApplication::exit(); });
        connect(m_ui.actionAddNewCollection, &QAction::triggered, &m_self, [&] { m_collectionController->AddCollection(); });
        connect(m_navigationWidget.get(), &TreeView::NavigationModeNameChanged, m_booksWidget.get(), &TreeView::SetNavigationModeName);
    }

    void CreateCollectionsMenu()
    {
        if (m_collectionController->IsEmpty())
            m_collectionController->AddCollection();

        m_collectionController->RegisterObserver(this);

        m_ui.menuSelectCollection->clear();

        const auto & activeCollection = m_collectionController->GetActiveCollection();
        for (const auto & collection : m_collectionController->GetCollections())
        {
            const auto active = collection->id == activeCollection.id;
            auto * action = m_ui.menuSelectCollection->addAction(collection->name);
            connect(action, &QAction::triggered, &m_self, [&, id = collection->id] { m_collectionController->SetActiveCollection(id); });
            action->setCheckable(true);
            action->setChecked(active);
            action->setEnabled(!active);
        }

        const auto enabled = !m_ui.menuSelectCollection->isEmpty();
        m_ui.actionRemoveCollection->setEnabled(enabled);
        m_ui.menuSelectCollection->setEnabled(enabled);
    }

    static void Reboot()
    {
        QTimer::singleShot(0, [] { QCoreApplication::exit(1234); });
    }

private:
    MainWindow & m_self;
    Ui::MainWindow m_ui {};
    PropagateConstPtr<ISettings, std::shared_ptr> m_settings;
    PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
    PropagateConstPtr<ParentWidgetProvider, std::shared_ptr> m_parentWidgetProvider;

    PropagateConstPtr<TreeView, std::shared_ptr> m_booksWidget;
    PropagateConstPtr<TreeView, std::shared_ptr> m_navigationWidget;
};

MainWindow::MainWindow(const std::shared_ptr<IUiFactory> & uiFactory
    , std::shared_ptr<ISettings> settings
    , std::shared_ptr<ICollectionController> collectionController
    , std::shared_ptr<ParentWidgetProvider> parentWidgetProvider
    , QWidget * parent
)
    : QMainWindow(parent)
    , m_impl(*this
        , uiFactory
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
