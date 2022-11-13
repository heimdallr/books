#pragma warning(push, 0)
#include <QAbstractItemModel>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QApplication>
#pragma warning(pop)

#include "database/factory/Factory.h"
#include "database/interface/Database.h"

#include "models/RoleBase.h"
#include "models/BookRole.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "util/Settings.h"

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
#include "NavigationSourceProvider.h"
#include "ProgressController.h"

#include "GuiController.h"

#include "Settings/UiSettings.h"

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
	settings->BeginGroup("ui");
	return settings;
}

}

class GuiController::Impl
	: virtual ModelControllerObserver
	, LocaleController::LanguageProvider
	, CollectionController::Observer
{
	NON_COPY_MOVABLE(Impl)
public:
	explicit Impl(GuiController & self)
		: m_self(self)
	{
	}

	~Impl() override
	{
		m_qmlEngine.clearComponentCache();
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
		qmlContext->setContextProperty("collectionController", new CollectionController(*this, &m_self));
		qmlContext->setContextProperty("progressController", &m_progressController);
		qmlContext->setContextProperty("iconTray", QIcon(":/icons/tray.png"));

		qmlRegisterType<QSystemTrayIcon>("QSystemTrayIcon", 1, 0, "QSystemTrayIcon");
		qRegisterMetaType<QSystemTrayIcon::ActivationReason>("ActivationReason");
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
		qRegisterMetaType<BooksModelController *>("BooksModelController*");
		qRegisterMetaType<AnnotationController *>("AnnotationController*");

		m_qmlEngine.load("qrc:/Main.qml");
	}

	void OnKeyPressed(int key, int modifiers) const
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			return QCoreApplication::exit(0);

		if (m_activeModelController)
			m_activeModelController->OnKeyPressed(key, modifiers);
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

		auto & controller = m_navigationModelControllers.emplace(navigationSource, std::make_unique<NavigationModelController>(*m_executor, *m_db, navigationSource)).first->second;
		QQmlEngine::setObjectOwnership(controller.get(), QQmlEngine::CppOwnership);
		controller->RegisterObserver(this);

		return controller.get();
	}

	BooksModelController * GetBooksModelController() noexcept
	{
		assert(m_booksModelController);
		return m_booksModelController.get();
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
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db, m_progressController, type, m_currentCollection.folder.toStdWString())).swap(m_booksModelController);
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
	UiSettings m_uiSettings { CreateUiSettings() };
	Collection m_currentCollection;

	NavigationSourceProvider m_navigationSourceProvider;
	LocaleController m_localeController { *this };
	ProgressController m_progressController;

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

bool GuiController::GetOpened() const noexcept
{
	return m_impl->GetOpened();
}

QString GuiController::GetTitle() const
{
	return m_impl->GetTitle();
}

}
