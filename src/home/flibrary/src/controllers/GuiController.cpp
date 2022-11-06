#pragma warning(push, 0)
#include <QAbstractItemModel>
#include <QFileDialog>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QApplication>
#pragma warning(pop)

#include "fnd/algorithm.h"

#include "database/factory/Factory.h"
#include "database/interface/Database.h"

#include "models/RoleBase.h"
#include "models/BookRole.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "util/Settings.h"

#include "ModelControllers/BooksModelController.h"
#include "ModelControllers/BooksModelControllerObserver.h"
#include "ModelControllers/BooksViewType.h"
#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/NavigationSource.h"

#include "AnnotationController.h"
#include "Collection.h"
#include "NavigationSourceProvider.h"

#include "GuiController.h"

#include "Resources/flibrary.h"
#include "Settings/UiSettings.h"

#include "Configuration.h"

Q_DECLARE_METATYPE(QSystemTrayIcon::ActivationReason)

namespace HomeCompa::Flibrary {

namespace {

constexpr auto LANGUAGE = "Language";

PropagateConstPtr<DB::Database> CreateDatabase(const std::string & databaseName)
{
	const std::string connectionString = std::string("path=") + databaseName + ";extension=" + MHL_SQLITE_EXTENSION;
	return PropagateConstPtr<DB::Database>(Create(DB::Factory::Impl::Sqlite, connectionString));
}

}

class GuiController::Impl
	: virtual public ModelControllerObserver
	, virtual BooksModelControllerObserver
{
	NON_COPY_MOVABLE(Impl)
public:
	explicit Impl(GuiController & self)
		: m_self(self)
	{
		OpenCollection(m_settings.Get("database/current", {}).toString());
		QQmlEngine::setObjectOwnership(&m_annotationController, QQmlEngine::CppOwnership);
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
			m_booksModelController->UnregisterObserver(this);
		}
	}

	void Start()
	{
		m_translator.load(QString(":/resources/%1.qm").arg(GetLocale()));
		QCoreApplication::installTranslator(&m_translator);

		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qmlContext->setContextProperty("uiSettings", &m_uiSettings);
		qmlContext->setContextProperty("fieldsVisibilityProvider", &m_navigationSourceProvider);
		qmlContext->setContextProperty("iconTray", QIcon(":/icons/tray.png"));

		qmlRegisterType<QSystemTrayIcon>("QSystemTrayIcon", 1, 0, "QSystemTrayIcon");
		qRegisterMetaType<QSystemTrayIcon::ActivationReason>("ActivationReason");
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		qRegisterMetaType<ModelController *>("ModelController*");
		qRegisterMetaType<AnnotationController *>("AnnotationController*");

		m_qmlEngine.load("qrc:/Main.qml");
	}

	void OnKeyPressed(int key, int modifiers)
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			return (void)Util::Set(m_running, false, m_self, &GuiController::RunningChanged);

		if (m_activeModelController)
			m_activeModelController->OnKeyPressed(key, modifiers);
	}

	bool GetRunning() const noexcept
	{
		return m_running;
	}

	bool GetOpened() const noexcept
	{
		return !!m_db;
	}

	QStringList GetLanguages()
	{
		return m_booksModelController ? m_booksModelController->GetModel()->data({}, BookRole::Languages).toStringList() : QStringList{};
	}

	QString GetLanguage()
	{
		return m_booksModelController ? m_booksModelController->GetModel()->data({}, BookRole::Language).toString() : QString {};
	}

	QString GetLocale() const
	{
		auto locale = m_settings.Get(LANGUAGE, "").toString();
		if (!locale.isEmpty())
			return locale;

		if (const auto it = std::ranges::find_if(LOCALES, [sysLocale = QLocale::system().name()](const char * item){ return sysLocale.startsWith(item); }); it != std::cend(LOCALES))
			return *it;

		assert(!std::empty(LOCALES));
		return LOCALES[0];
	}

	const QString & GetTitle() const noexcept
	{
		return m_title;
	}

	AnnotationController * GetAnnotationController()
	{
		return &m_annotationController;
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

	ModelController * GetBooksModelController(const BooksViewType type) noexcept
	{
		if (m_booksViewType == type)
			return m_booksModelController.get();

		if (m_booksModelController)
		{
			static_cast<ModelController *>(m_booksModelController.get())->UnregisterObserver(this);
			m_booksModelController->UnregisterObserver(m_annotationController.GetBooksModelControllerObserver());
			m_booksModelController->UnregisterObserver(this);
		}

		m_booksViewType = type;
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db, type)).swap(m_booksModelController);
		QQmlEngine::setObjectOwnership(m_booksModelController.get(), QQmlEngine::CppOwnership);
		static_cast<ModelController *>(m_booksModelController.get())->RegisterObserver(this);
		m_booksModelController->RegisterObserver(this);
		m_booksModelController->RegisterObserver(m_annotationController.GetBooksModelControllerObserver());
		m_booksModelController->GetModel()->setData({}, m_uiSettings.showDeleted(), BookRole::ShowDeleted);

		if (m_navigationSourceProvider.GetSource() != NavigationSource::Undefined && m_currentNavigationIndex != -1)
		{
			if (const auto it = m_navigationModelControllers.find(m_navigationSourceProvider.GetSource()); it != m_navigationModelControllers.end())
				m_booksModelController->SetNavigationState(m_navigationSourceProvider.GetSource(), it->second->GetId(m_currentNavigationIndex));
		}

		return m_booksModelController.get();
	}

	void AddCollection(QString name, QString db, QString folder)
	{
		const Collection collection(std::move(name), std::move(db), std::move(folder));
		collection.Serialize(m_settings);
		collection.SetActive(m_settings);
		QApplication::exit(1234);
	}

	void SetLanguage(const QString & language)
	{
		if (m_booksModelController && !m_preventSetLanguageFilter)
			m_booksModelController->GetModel()->setData({}, language, BookRole::Language);
	}

	void SetLocale(const QString & locale)
	{
		m_settings.Set(LANGUAGE, locale);
		emit m_self.LocaleChanged();
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

private: //BooksModelControllerObserver
	void HandleBookChanged(const std::string & /*folder*/, const std::string & /*file*/) override
	{
	}

	void HandleModelReset() override
	{
		m_preventSetLanguageFilter = true;
		emit m_self.LanguagesChanged();
		m_preventSetLanguageFilter = false;
	}

private:
	void OpenCollection(QString collectionId)
	{
		const auto collection = Collection::Deserialize(m_settings, std::move(collectionId));
		if (collection.id.isEmpty())
			return;

		Util::Set(m_title, QString("Flibrary - %1").arg(collection.name), m_self, &GuiController::TitleChanged);
		m_annotationController.SetRootFolder(std::filesystem::path(collection.folder.toUtf8().data()));

		CreateExecutor(collection.database.toStdString());

		connect(&m_uiSettings, &UiSettings::showDeletedChanged, [&]
		{
			if (m_booksModelController)
				m_booksModelController->GetModel()->setData({}, m_uiSettings.showDeleted(), BookRole::ShowDeleted);
		});
	}

	void CreateExecutor(const std::string & databaseName)
	{
		auto executor = Util::ExecutorFactory::Create(Util::ExecutorImpl::Async, {
			  [databaseName, &db = m_db] { CreateDatabase(databaseName).swap(db); }
			, []{ QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, []{ QGuiApplication::restoreOverrideCursor(); }
		});

		PropagateConstPtr<Util::Executor>(std::move(executor)).swap(m_executor);
		emit m_self.OpenedChanged();
	}

private:
	GuiController & m_self;
	QTranslator m_translator;

	PropagateConstPtr<DB::Database> m_db { std::unique_ptr<DB::Database>() };
	PropagateConstPtr<Util::Executor> m_executor { std::unique_ptr<Util::Executor>() };

	AnnotationController m_annotationController;
	std::map<NavigationSource, PropagateConstPtr<NavigationModelController>> m_navigationModelControllers;
	PropagateConstPtr<BooksModelController> m_booksModelController { std::unique_ptr<BooksModelController>() };
	ModelController * m_activeModelController { nullptr };

	NavigationSourceProvider m_navigationSourceProvider;

	bool m_running { true };
	BooksViewType m_booksViewType { BooksViewType::Undefined };
	int m_currentNavigationIndex { -1 };
	bool m_preventSetLanguageFilter { false };

	Settings m_settings { "HomeCompa", "Flibrary" };
	UiSettings m_uiSettings { std::make_unique<Settings>("HomeCompa", "Flibrary\\ui") };
	QString m_title;

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

AnnotationController * GuiController::GetAnnotationController()
{
	return m_impl->GetAnnotationController();
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

ModelController * GuiController::GetBooksModelControllerList()
{
	return m_impl->GetBooksModelController(BooksViewType::List);
}

ModelController * GuiController::GetBooksModelControllerTree()
{
	return m_impl->GetBooksModelController(BooksViewType::Tree);
}

void GuiController::AddCollection(QString name, QString db, QString folder)
{
	m_impl->AddCollection(std::move(name), std::move(db), std::move(folder));
}

QString GuiController::SelectFile(const QString & fileName) const
{
	return QFileDialog::getOpenFileName(nullptr, QCoreApplication::translate("FileDialog", "Select database file"), fileName);
}

QString GuiController::SelectFolder(const QString & folderName) const
{
	return QFileDialog::getExistingDirectory(nullptr, QCoreApplication::translate("FileDialog", "Select archives folder"), folderName);
}

void GuiController::Restart()
{
	QCoreApplication::exit(1234);
}

bool GuiController::GetOpened() const noexcept
{
	return m_impl->GetOpened();
}

bool GuiController::GetRunning() const noexcept
{
	return m_impl->GetRunning();
}

QStringList GuiController::GetLanguages()
{
	return m_impl->GetLanguages();
}

QStringList GuiController::GetLocales() const
{
	QStringList result;
	result.reserve(static_cast<int>(std::size(LOCALES)));
	std::ranges::copy(LOCALES, std::back_inserter(result));
	return result;
}

QString GuiController::GetLanguage()
{
	return m_impl->GetLanguage();
}

QString GuiController::GetLocale() const
{
	return m_impl->GetLocale();
}

const QString & GuiController::GetTitle() const noexcept
{
	return m_impl->GetTitle();
}

void GuiController::SetLanguage(const QString & language)
{
	m_impl->SetLanguage(language);
}

void GuiController::SetLocale(const QString & locale)
{
	m_impl->SetLocale(locale);
}

}
