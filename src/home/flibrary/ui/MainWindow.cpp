#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "interface/ICollectionController.h"
#include "interface/IUiFactory.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/ITreeViewController.h"
#include "util/ISettings.h"

#include <plog/Log.h>

using namespace HomeCompa::Flibrary;

namespace {
constexpr auto GEOMETRY_KEY = "ui/MainWindow/Geometry";
constexpr auto VERTICAL_SPLITTER_KEY = "ui/MainWindow/VSplitter";
constexpr auto HORIZONTAL_SPLITTER_KEY = "ui/MainWindow/HSplitter";
constexpr auto FONT_SIZE_KEY = "ui/Font/Size";

constexpr auto FONT_SIZE_DEFAULT = 9;
}

class MainWindow::Impl
{
    NON_COPY_MOVABLE(Impl)

public:
    Impl(MainWindow & self
        , std::shared_ptr<ILogicFactory> logicFactory
        , std::shared_ptr<IUiFactory> uiFactory
        , std::shared_ptr<ISettings> settings
        , std::shared_ptr<ICollectionController> collectionController
    )
        : m_self(self)
        , m_logicFactory(std::move(logicFactory))
        , m_uiFactory(std::move(uiFactory))
        , m_settings(std::move(settings))
		, m_collectionController(std::move(collectionController))
        , m_logicFactoryGuard(*logicFactory)
        , m_booksWidget(this->m_uiFactory->CreateTreeViewWidget(TreeViewControllerType::Books))
        , m_navigationWidget(this->m_uiFactory->CreateTreeViewWidget(TreeViewControllerType::Navigation))
    {
        Init();
    }

	~Impl()
    {
        m_settings->Set(GEOMETRY_KEY, m_self.geometry());
        m_settings->Set(VERTICAL_SPLITTER_KEY, m_ui.verticalSplitter->saveState());
        m_settings->Set(HORIZONTAL_SPLITTER_KEY, m_ui.horizontalSplitter->saveState());
        m_settings->Set(FONT_SIZE_KEY, m_fontSize);
    }

private:
    void Init()
    {
        m_ui.setupUi(&m_self);

        m_ui.booksWidget->layout()->addWidget(m_booksWidget.get());
        m_ui.navigationWidget->layout()->addWidget(m_navigationWidget.get());

        if (const auto value = m_settings->Get(GEOMETRY_KEY); value.isValid())
            m_self.setGeometry(value.toRect());

        if (const auto value = m_settings->Get(VERTICAL_SPLITTER_KEY); value.isValid())
            m_ui.verticalSplitter->restoreState(value.toByteArray());

        if (const auto value = m_settings->Get(HORIZONTAL_SPLITTER_KEY); value.isValid())
            m_ui.horizontalSplitter->restoreState(value.toByteArray());

        const auto setFontSize = [&](const int fontSize)
        {
            EnumerateWidgets(m_self, [&] (QWidget & widget)
            {
                auto font = widget.font();
                font.setPointSize(fontSize);
                widget.setFont(font);
            });
        };
        setFontSize(m_fontSize);

        connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [&, setFontSize] { setFontSize(++m_fontSize); });
        connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [&, setFontSize] { setFontSize(--m_fontSize); });
        connect(m_ui.actionE_xit, &QAction::triggered, &m_self, [] { QCoreApplication::exit(); });
        connect(m_ui.actionReload, &QAction::triggered, &m_self, [] { QCoreApplication::exit(1234); });
    }

    static void EnumerateWidgets(const QWidget& parent, const std::function<void(QWidget&)> & f)
    {
        for (auto * widget : parent.findChildren<QWidget *>())
            f(*widget);
    }

private:
    MainWindow & m_self;
    Ui::MainWindow m_ui {};
    std::shared_ptr<ILogicFactory> m_logicFactory;
    std::shared_ptr<IUiFactory> m_uiFactory;
    std::shared_ptr<ISettings> m_settings;
    std::shared_ptr<ICollectionController> m_collectionController;

    const ILogicFactory::Guard m_logicFactoryGuard;

    std::shared_ptr<QWidget> m_booksWidget;
    std::shared_ptr<QWidget> m_navigationWidget;
    int m_fontSize { m_settings->Get(FONT_SIZE_KEY, FONT_SIZE_DEFAULT).toInt() };
};

MainWindow::MainWindow(std::shared_ptr<ILogicFactory> logicFactory
    , std::shared_ptr<IUiFactory> uiFactory
    , std::shared_ptr<ISettings> settings
    , std::shared_ptr<ICollectionController> collectionController
    , QWidget * parent
)
    : QMainWindow(parent)
    , m_impl(*this
        , std::move(logicFactory)
        , std::move(uiFactory)
        , std::move(settings)
        , std::move(collectionController)
    )
{
    PLOGD << "MainWindow created";
}

MainWindow::~MainWindow()
{
    PLOGD << "MainWindow destroyed";
}
