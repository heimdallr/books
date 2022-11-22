#pragma warning(push, 0)
#include <QAbstractItemModel>
#include <QApplication>
#include <QCommonStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#pragma warning(pop)

#include <plog/Log.h>

#include "database/factory/Factory.h"
#include "database/interface/Database.h"

#include "models/RoleBase.h"
#include "models/BookRole.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "util/Settings.h"
#include "util/SettingsObserver.h"

#include "ModelControllers/BooksModelController.h"
#include "ModelControllers/BooksViewType.h"
#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/NavigationSource.h"

#include "constants/ProductConstant.h"

#include "AnnotationController.h"
#include "Collection.h"
#include "CollectionController.h"
#include "FileDialogProvider.h"
#include "LocaleController.h"
#include "LogController.h"
#include "NavigationSourceProvider.h"
#include "ProgressController.h"
#include "ViewSourceController.h"

#include "GuiController.h"

#include "Settings/UiSettings.h"
#include "Settings/UiSettings_keys.h"
#include "Settings/UiSettings_values.h"

#include "Configuration.h"

Q_DECLARE_METATYPE(QSystemTrayIcon::ActivationReason)

namespace HomeCompa::Flibrary {

namespace {

PropagateConstPtr<DB::Database> CreateDatabase(const std::string & databaseName)
{
	const std::string connectionString = std::string("path=") + databaseName + ";extension=" + MHL_SQLITE_EXTENSION;
	return PropagateConstPtr<DB::Database>(Create(DB::Factory::Impl::Sqlite, connectionString));
}

auto CreateUiSettings()
{
	auto settings = std::make_unique<Settings>(Constant::COMPANY_ID, Constant::PRODUCT_ID);
	settings->BeginGroup(Constant::UI);
	return settings;
}

SimpleModelItems GetViewSourceNavigationModelItems()
{
	return SimpleModelItems
	{
		{ "Authors", QT_TRANSLATE_NOOP("ViewSource", "Authors") },
		{ "Series", QT_TRANSLATE_NOOP("ViewSource", "Series") },
		{ "Genres", QT_TRANSLATE_NOOP("ViewSource", "Genres") },
	};
}

SimpleModelItems GetViewSourceBooksModelItems()
{
	return SimpleModelItems
	{
		{ "BooksListView", QT_TRANSLATE_NOOP("ViewSource", "List") },
		{ "BooksTreeView", QT_TRANSLATE_NOOP("ViewSource", "Tree") },
	};
}

}

class GuiController::Impl
	: virtual ModelControllerObserver
	, LocaleController::LanguageProvider
	, CollectionController::Observer
	, SettingsObserver
{
	NON_COPY_MOVABLE(Impl)
public:
	explicit Impl(GuiController & self)
		: m_self(self)
	{
		QQmlEngine::setObjectOwnership(m_viewSourceNavigationController.get(), QQmlEngine::CppOwnership);
		QQmlEngine::setObjectOwnership(m_viewSourceBooksController.get(), QQmlEngine::CppOwnership);

		m_settings.RegisterObserver(this);
		m_uiSettingsSrc->RegisterObserver(this);
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();

		m_settings.UnregisterObserver(this);
		m_uiSettingsSrc->UnregisterObserver(this);

		for (auto & [_, controller] : m_navigationModelControllers)
			controller->UnregisterObserver(this);

		if (m_booksModelController)
		{
			static_cast<ModelController *>(m_booksModelController.get())->UnregisterObserver(this);
			m_booksModelController->UnregisterObserver(m_annotationController.GetBooksModelControllerObserver());
			m_booksModelController->UnregisterObserver(m_localeController.GetBooksModelControllerObserver());
		}
	}

	void Start()
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qmlContext->setContextProperty("uiSettings", &m_uiSettings);
		qmlContext->setContextProperty("fieldsVisibilityProvider", &m_navigationSourceProvider);
		qmlContext->setContextProperty("localeController", &m_localeController);
		qmlContext->setContextProperty("annotationController", &m_annotationController);
		qmlContext->setContextProperty("fileDialog", new FileDialogProvider(&m_self));
		qmlContext->setContextProperty("collectionController", &m_collectionController);
		qmlContext->setContextProperty("log", &m_logController);
		qmlContext->setContextProperty("progressController", &m_progressController);
		qmlContext->setContextProperty("iconTray", QIcon(":/icons/tray.png"));

		qmlRegisterType<QSystemTrayIcon>("QSystemTrayIcon", 1, 0, "QSystemTrayIcon");
		qRegisterMetaType<QSystemTrayIcon::ActivationReason>("ActivationReason");
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
		qRegisterMetaType<BooksModelController *>("BooksModelController*");
		qRegisterMetaType<AnnotationController *>("AnnotationController*");
		qRegisterMetaType<ViewSourceController *>("ViewSourceController*");

		qmlRegisterType<QCommonStyle>("Style", 1, 0, "Style");

		m_qmlEngine.load("qrc:/Main.qml");
	}

	void OnKeyPressed(const int key, const int modifiers)
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			return QCoreApplication::exit(0);

		if (m_activeModelController)
			m_activeModelController->OnKeyPressed(key, modifiers);

		m_logController.OnKeyPressed(key, modifiers);
	}

	bool GetOpened() const noexcept
	{
		return !!m_db;
	}

	QString GetTitle() const
	{
		return QString("Flibrary - %1").arg(m_currentCollection.name);
	}

	ModelController * GetNavigationModelController(const NavigationSource navigationSource)
	{
		if (m_navigationSourceProvider.GetSource() != navigationSource)
			m_currentNavigationIndex = -1;

		m_navigationSourceProvider.SetSource(navigationSource);

		const auto it = m_navigationModelControllers.find(navigationSource);
		if (it != m_navigationModelControllers.end())
			return it->second.get();

		auto & controller = m_navigationModelControllers.emplace(navigationSource, std::make_unique<NavigationModelController>(*m_executor, *m_db, navigationSource, *m_uiSettingsSrc)).first->second;
		QQmlEngine::setObjectOwnership(controller.get(), QQmlEngine::CppOwnership);
		controller->RegisterObserver(this);

		return controller.get();
	}

	BooksModelController * GetBooksModelController() noexcept
	{
		assert(m_booksModelController);
		return m_booksModelController.get();
	}

	ViewSourceController * GetViewSourceNavigationController() noexcept
	{
		assert(m_viewSourceNavigationController);
		return m_viewSourceNavigationController.get();
	}

	ViewSourceController * GetViewSourceBooksController() noexcept
	{
		assert(m_viewSourceBooksController);
		return m_viewSourceBooksController.get();
	}

	BooksModelController * GetBooksModelController(const BooksViewType type)
	{
		if (m_navigationSourceProvider.GetBookViewType() == type)
			return m_booksModelController.get();

		if (m_booksModelController)
		{
			static_cast<ModelController *>(m_booksModelController.get())->UnregisterObserver(this);
			m_booksModelController->UnregisterObserver(m_annotationController.GetBooksModelControllerObserver());
			m_booksModelController->UnregisterObserver(m_localeController.GetBooksModelControllerObserver());
		}

		m_navigationSourceProvider.SetBookViewType(type);
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db, m_progressController, type, m_currentCollection.folder.toStdWString(), *m_uiSettingsSrc)).swap(m_booksModelController);
		QQmlEngine::setObjectOwnership(m_booksModelController.get(), QQmlEngine::CppOwnership);
		static_cast<ModelController *>(m_booksModelController.get())->RegisterObserver(this);
		m_booksModelController->RegisterObserver(m_localeController.GetBooksModelControllerObserver());
		m_booksModelController->RegisterObserver(m_annotationController.GetBooksModelControllerObserver());
		m_booksModelController->GetModel()->setData({}, m_uiSettings.showDeleted(), BookRole::ShowDeleted);

		if (m_navigationSourceProvider.GetSource() != NavigationSource::Undefined && m_currentNavigationIndex != -1)
		{
			if (const auto it = m_navigationModelControllers.find(m_navigationSourceProvider.GetSource()); it != m_navigationModelControllers.end())
				m_booksModelController->SetNavigationState(m_navigationSourceProvider.GetSource(), it->second->GetId(m_currentNavigationIndex));
		}

		return m_booksModelController.get();
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, const int index) override
	{
		if (controller->GetType() == ModelController::Type::Navigation)
		{
			m_currentNavigationIndex = index;
			if (m_booksModelController)
				m_booksModelController->SetNavigationState(m_navigationSourceProvider.GetSource(), controller->GetId(index));
		}
	}

	void HandleClicked(ModelController * controller) override
	{
		m_activeModelController = controller;

		const auto setFocus = [&] (ModelController * modelController)
		{
			modelController->SetFocused(modelController == m_activeModelController);
		};

		for (auto & [_, navigationModelController] : m_navigationModelControllers)
			setFocus(navigationModelController.get());

		if (m_booksModelController)
			setFocus(m_booksModelController.get());
	}

private: // SettingsProvider
	Settings & GetSettings() override
	{
		return m_settings;
	}

	Settings & GetUiSettings() override
	{
		return *m_uiSettingsSrc;
	}

private: // LocaleController::LanguageProvider
	QStringList GetLanguages() override
	{
		return m_booksModelController ? m_booksModelController->GetModel()->data({}, BookRole::Languages).toStringList() : QStringList {};
	}

	QString GetLanguage() override
	{
		return m_booksModelController ? m_booksModelController->GetModel()->data({}, BookRole::Language).toString() : QString {};
	}

	void SetLanguage(const QString & language) override
	{
		if (m_booksModelController)
			m_booksModelController->GetModel()->setData({}, language, BookRole::Language);
	}

private: // CollectionController::Observer
	void HandleCurrentCollectionChanged(const Collection & collection) override
	{
		if (collection.id.isEmpty())
		{
			m_currentCollection = Collection {};
			PropagateConstPtr<Util::Executor>(std::unique_ptr<Util::Executor>()).swap(m_executor);
			emit m_self.OpenedChanged();
			return;
		}

		m_currentCollection = collection;
		CreateExecutor(collection.database.toStdString());

		m_annotationController.SetRootFolder(std::filesystem::path(collection.folder.toStdWString()));

		connect(&m_uiSettings, &UiSettings::showDeletedChanged, [&]
		{
			if (m_booksModelController)
				m_booksModelController->GetModel()->setData({}, m_uiSettings.showDeleted(), BookRole::ShowDeleted);
		});

		emit m_self.TitleChanged();

		m_collectionController.CheckForUpdate(collection);
	}

private: // SettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override
	{
		PLOGV << "Set " << key << " = " << value.toString();
	}

private:
	void CreateExecutor(const std::string & databaseName)
	{
		auto executor = Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, {
			  [databaseName, &db = m_db] { CreateDatabase(databaseName).swap(db); }
			, []{ QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, []{ QGuiApplication::restoreOverrideCursor(); }
			, [&db = m_db] { PropagateConstPtr<DB::Database>(std::unique_ptr<DB::Database>()).swap(db); }
		});

		PropagateConstPtr<Util::Executor>(std::move(executor)).swap(m_executor);
		emit m_self.OpenedChanged();
	}

private:
	GuiController & m_self;

	PropagateConstPtr<DB::Database> m_db { std::unique_ptr<DB::Database>() };
	PropagateConstPtr<Util::Executor> m_executor { std::unique_ptr<Util::Executor>() };

	AnnotationController m_annotationController;
	std::map<NavigationSource, PropagateConstPtr<NavigationModelController>> m_navigationModelControllers;
	PropagateConstPtr<BooksModelController> m_booksModelController { std::unique_ptr<BooksModelController>() };
	ModelController * m_activeModelController { nullptr };

	int m_currentNavigationIndex { -1 };

	Settings m_settings { Constant::COMPANY_ID, Constant::PRODUCT_ID };
	std::shared_ptr<Settings> m_uiSettingsSrc { CreateUiSettings() };
	UiSettings m_uiSettings { m_uiSettingsSrc };
	Collection m_currentCollection;

	NavigationSourceProvider m_navigationSourceProvider;
	LocaleController m_localeController { *this };
	ProgressController m_progressController;
	LogController m_logController { *this };
	CollectionController m_collectionController { *this };

	PropagateConstPtr<ViewSourceController> m_viewSourceNavigationController { std::make_unique<ViewSourceController>(*m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::viewSourceNavigation, HomeCompa::Constant::UiSettings_ns::viewSourceNavigation_default, GetViewSourceNavigationModelItems(), &m_self) };
	PropagateConstPtr<ViewSourceController> m_viewSourceBooksController { std::make_unique<ViewSourceController>(*m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::viewSourceBooks, HomeCompa::Constant::UiSettings_ns::viewSourceBooks_default, GetViewSourceBooksModelItems(), &m_self) };

	QQmlApplicationEngine m_qmlEngine;
};

GuiController::GuiController(QObject * parent)
	: QObject(parent)
	, m_impl(*this)
{
}

GuiController::~GuiController() = default;

void GuiController::Start()
{
	m_impl->Start();
}

void GuiController::OnKeyPressed(int key, int modifiers)
{
	m_impl->OnKeyPressed(key, modifiers);
}

ModelController * GuiController::GetNavigationModelControllerAuthors()
{
	return m_impl->GetNavigationModelController(NavigationSource::Authors);
}

ModelController * GuiController::GetNavigationModelControllerSeries()
{
	return m_impl->GetNavigationModelController(NavigationSource::Series);
}

ModelController * GuiController::GetNavigationModelControllerGenres()
{
	return m_impl->GetNavigationModelController(NavigationSource::Genres);
}

BooksModelController * GuiController::GetBooksModelControllerList()
{
	return m_impl->GetBooksModelController(BooksViewType::List);
}

BooksModelController * GuiController::GetBooksModelControllerTree()
{
	return m_impl->GetBooksModelController(BooksViewType::Tree);
}

BooksModelController * GuiController::GetBooksModelController()
{
	return m_impl->GetBooksModelController();
}

int GuiController::GetPixelMetric(const QVariant & metric)
{
	const auto value = QApplication::style()->pixelMetric(metric.value<QStyle::PixelMetric>());
	return value;
}

ViewSourceController * GuiController::GetViewSourceNavigationController() noexcept
{
	return m_impl->GetViewSourceNavigationController();
}

ViewSourceController * GuiController::GetViewSourceBooksController() noexcept
{
	return m_impl->GetViewSourceBooksController();
}

bool GuiController::GetOpened() const noexcept
{
	return m_impl->GetOpened();
}

QString GuiController::GetTitle() const
{
	return m_impl->GetTitle();
}

}
