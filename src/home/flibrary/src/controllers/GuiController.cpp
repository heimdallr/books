#pragma warning(push, 0)
#include <QAbstractItemModel>
#include <QApplication>
#include <QCommonStyle>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QTimer>
#pragma warning(pop)

#include <plog/Log.h>

#include "database/factory/Factory.h"
#include "database/interface/Database.h"
#include "database/interface/Query.h"
#include "database/interface/Transaction.h"

#include "models/RoleBase.h"
#include "models/BookRole.h"

#include "util/executor.h"
#include "util/executor/factory.h"

#include "util/Settings.h"
#include "util/SettingsObserver.h"

#include "ModelControllers/BooksModelController.h"
#include "ModelControllers/BooksViewType.h"
#include "ModelControllers/GroupsModelController.h"
#include "ModelControllers/ModelController.h"
#include "ModelControllers/ModelControllerObserver.h"
#include "ModelControllers/ModelControllerType.h"
#include "ModelControllers/NavigationModelController.h"
#include "ModelControllers/NavigationSource.h"

#include "constants/ProductConstant.h"

#include "AnnotationController.h"
#include "Collection.h"
#include "CollectionController.h"
#include "ComboBoxController.h"
#include "FileDialogProvider.h"
#include "LanguageController.h"
#include "LocaleController.h"
#include "LogController.h"
#include "Measure.h"
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
	PropagateConstPtr<DB::Database> db(Create(DB::Factory::Impl::Sqlite, connectionString));
	const auto transaction = db->CreateTransaction();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Books_User(BookID INTEGER NOT NULL PRIMARY KEY, IsDeleted INTEGER, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Groups_User(GroupID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE)")->Execute();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Groups_List_User(GroupID INTEGER NOT NULL, BookID INTEGER NOT NULL, PRIMARY KEY(GroupID, BookID), FOREIGN KEY(GroupID) REFERENCES Groups_User(GroupID) ON DELETE CASCADE, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction->Commit();
	return db;
}

auto CreateUiSettings()
{
	auto settings = std::make_unique<Settings>(Constant::COMPANY_ID, Constant::PRODUCT_ID);
	settings->BeginGroup(Constant::UI);
	return settings;
}

template<typename T>
SimpleModelItems GetViewSourceModelItems(const T & container)
{
	SimpleModelItems items;
	items.reserve(std::size(container));
	std::ranges::transform(container, std::back_inserter(items), [] (const auto & item)
	{
		return SimpleModelItem { item.second, item.second};
	});
	return items;
}

int UpdateSettings(Settings & settings, const char * settingsKey, const QVariant & defaultValue, const int key)
{
	const auto update = [&] (int increment)
	{
		const auto value = settings.Get(settingsKey, defaultValue).toInt();
		const auto result = value + increment;
		settings.Set(settingsKey, value + increment);
		return result;
	};
	switch (key)
	{
		case Qt::Key_Up:
			return update(1);
		case Qt::Key_Down:
			return update(-1);
		default:
			break;
	}
	return 0;
}

}

class GuiController::Impl
	: virtual ModelControllerObserver
	, LanguageController::LanguageProvider
	, CollectionController::Observer
	, SettingsObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(GuiController & self)
		: m_self(self)
	{
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
			m_booksModelController->UnregisterObserver(m_languageController.GetBooksModelControllerObserver());
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
		qmlContext->setContextProperty("measure", new Measure(&m_self));
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
		qRegisterMetaType<ComboBoxController *>("ComboBoxController*");
		qRegisterMetaType<GroupsModelController *>("GroupsModelController*");

		qmlRegisterType<QCommonStyle>("Style", 1, 0, "Style");

		qRegisterMetaType<ModelControllerType>("ModelControllerType");
		qmlRegisterUncreatableType<ModelControllerTypeClass>("HomeCompa.Flibrary.ModelControllerType", 1, 0, "ModelControllerType", "Not creatable as it is an enum type");

		PLOGD << "Loading UI";
		m_qmlEngine.load("qrc:/Main.qml");
		PLOGD << "UI loaded";
	}

	void OnKeyEvent(const QKeyEvent & keyEvent)
	{
		if (!keyEvent.spontaneous())
			return;

		const auto key = keyEvent.key();
		const auto modifiers = keyEvent.modifiers();

		if (keyEvent.nativeVirtualKey() == Qt::Key_X && modifiers == Qt::AltModifier)
			return QCoreApplication::exit(0);

		if (!(modifiers & Qt::AltModifier))
		{
			if (modifiers & Qt::ControlModifier)
				if (const auto value = UpdateSettings(*m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::heightRow, HomeCompa::Constant::UiSettings_ns::heightRow_default, key))
					m_uiSettingsSrc->Set(HomeCompa::Constant::UiSettings_ns::sizeFont, 9 + (value - 27) * (64 - 9) / (150 - 27));

			if (modifiers & Qt::ShiftModifier)
				(void)UpdateSettings(*m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::sizeFont, HomeCompa::Constant::UiSettings_ns::sizeFont_default, key);
		}

		if (modifiers == Qt::NoModifier && key == Qt::Key_Tab)
		{
			if (m_activeModelController != m_booksModelController.get())
				HandleClicked(m_booksModelController.get());
			else
				if (const auto it = m_navigationModelControllers.find(m_navigationSourceProvider.GetSource()); it != m_navigationModelControllers.end())
					HandleClicked(it->second.get());
		}

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

	GroupsModelController * GetGroupsModelController()
	{
		if (m_groupsModelController)
			return m_groupsModelController.get();

		PropagateConstPtr<GroupsModelController>(std::make_unique<GroupsModelController>(*m_executor, *m_db)).swap(m_groupsModelController);
		QQmlEngine::setObjectOwnership(m_groupsModelController.get(), QQmlEngine::CppOwnership);

		return m_groupsModelController.get();

	}

	ModelController * GetNavigationModelController()
	{
		const auto navigationSource = FindFirst(g_viewSourceNavigationModelItems, m_uiSettings.viewSourceNavigation().toByteArray().data(), NavigationSource::Authors, PszComparer {});
		if (m_navigationSourceProvider.GetSource() != navigationSource)
			m_currentNavigationIndex = -1;

		m_navigationSourceProvider.SetSource(navigationSource);

		const auto it = m_navigationModelControllers.find(navigationSource);
		if (it != m_navigationModelControllers.end())
			return it->second.get();

		auto & controller = m_navigationModelControllers.emplace(navigationSource, std::make_unique<NavigationModelController>(*m_executor, *m_db, navigationSource, *m_uiSettingsSrc)).first->second;
		QQmlEngine::setObjectOwnership(controller.get(), QQmlEngine::CppOwnership);
		controller->RegisterObserver(this);
		HandleClicked(controller.get());

		return controller.get();
	}

	BooksModelController * GetBooksModelController()
	{
		const BooksViewType type = FindFirst(g_viewSourceBooksModelItems, m_uiSettings.viewSourceBooks().toString().toUtf8().data(), BooksViewType::List, PszComparer {});
		if (m_navigationSourceProvider.GetBookViewType() == type)
			return m_booksModelController.get();

		if (m_booksModelController)
		{
			static_cast<ModelController *>(m_booksModelController.get())->UnregisterObserver(this);
			m_booksModelController->UnregisterObserver(m_annotationController.GetBooksModelControllerObserver());
			m_booksModelController->UnregisterObserver(m_languageController.GetBooksModelControllerObserver());
		}

		m_navigationSourceProvider.SetBookViewType(type);
		PropagateConstPtr<BooksModelController>(std::make_unique<BooksModelController>(*m_executor, *m_db, m_progressController, type, m_currentCollection.folder.toStdWString(), *m_uiSettingsSrc)).swap(m_booksModelController);
		QQmlEngine::setObjectOwnership(m_booksModelController.get(), QQmlEngine::CppOwnership);
		static_cast<ModelController *>(m_booksModelController.get())->RegisterObserver(this);
		m_booksModelController->RegisterObserver(m_languageController.GetBooksModelControllerObserver());
		m_booksModelController->RegisterObserver(m_annotationController.GetBooksModelControllerObserver());
		m_booksModelController->GetModel()->setData({}, m_uiSettings.showDeleted(), BookRole::ShowDeleted);

		if (m_navigationSourceProvider.GetSource() != NavigationSource::Undefined && m_currentNavigationIndex != -1)
		{
			if (const auto it = m_navigationModelControllers.find(m_navigationSourceProvider.GetSource()); it != m_navigationModelControllers.end())
				m_booksModelController->SetNavigationState(m_navigationSourceProvider.GetSource(), it->second->GetId(m_currentNavigationIndex));
		}

		return m_booksModelController.get();
	}

	BooksModelController * GetCurrentBooksModelController() noexcept
	{
		assert(m_booksModelController);
		return m_booksModelController.get();
	}

	ComboBoxController * GetViewSourceComboBoxNavigationController() noexcept
	{
		return m_viewSourceNavigationController.GetComboBoxController();
	}

	ComboBoxController * GetViewSourceComboBoxBooksController() noexcept
	{
		return m_viewSourceBooksController.GetComboBoxController();
	}

	ComboBoxController * GetLanguageComboBoxBooksController() noexcept
	{
		return m_languageController.GetComboBoxController();
	}

	void LogCollectionStatistics()
	{
		(*m_executor)({ "Get collection statistics", [&]
		{
			static constexpr auto dbStatQueryText =
				"select '%1', count(42) from Authors union all "
				"select '%2', count(42) from Series union all "
				"select '%3', count(42) from Books union all "
				"select '%4', count(42) from Books b left join Books_User bu on bu.BookID = b.BookID where coalesce(bu.IsDeleted, b.IsDeleted, 0) != 0"
				;

			QStringList stats;
			stats << QCoreApplication::translate("CollectionStatistics", "Collection statistics:");
			auto bookQuery = m_db->CreateQuery(QString(dbStatQueryText).arg
			(
				  QT_TRANSLATE_NOOP("CollectionStatistics", "Authors:")
				, QT_TRANSLATE_NOOP("CollectionStatistics", "Series:")
				, QT_TRANSLATE_NOOP("CollectionStatistics", "Books:")
				, QT_TRANSLATE_NOOP("CollectionStatistics", "Deleted books:")
			).toStdString());
			for (bookQuery->Execute(); !bookQuery->Eof(); bookQuery->Next())
			{
				[[maybe_unused]] const auto * name = bookQuery->Get<const char *>(0);
				[[maybe_unused]] const auto translated = QCoreApplication::translate("CollectionStatistics", bookQuery->Get<const char *>(0));
				stats << QString("%1 %2").arg(translated).arg(bookQuery->Get<long long>(1));
			}

			return[stats = stats.join("\n")]
			{
				PLOGI << std::endl << stats;
			};
		} });
	}

	void HandleLink(const QString & link, const long long bookId) const
	{
		const QUrl url(link);
		if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty())
		{
			QDesktopServices::openUrl(url);
			return;
		}

		const auto navigationPair = link.split('|');
		assert(navigationPair.size() == 2);
		m_uiSettingsSrc->Set(HomeCompa::Constant::UiSettings_ns::idBooks, bookId);
		m_uiSettingsSrc->Set(HomeCompa::Constant::UiSettings_ns::viewSourceNavigation, navigationPair.front());
		m_uiSettingsSrc->Set(HomeCompa::Constant::UiSettings_ns::idNavigation, navigationPair.back());
	}

private: // ModelControllerObserver
	void HandleCurrentIndexChanged(ModelController * const controller, const int index) override
	{
		if (controller->GetType() == ModelControllerType::Navigation)
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

		LogCollectionStatistics();
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

	Settings m_settings { Constant::COMPANY_ID, Constant::PRODUCT_ID };
	std::shared_ptr<Settings> m_uiSettingsSrc { CreateUiSettings() };
	UiSettings m_uiSettings { m_uiSettingsSrc };

	int m_currentNavigationIndex { -1 };
	AnnotationController m_annotationController;
	Collection m_currentCollection;

	std::map<NavigationSource, PropagateConstPtr<NavigationModelController>> m_navigationModelControllers;
	PropagateConstPtr<BooksModelController> m_booksModelController { std::unique_ptr<BooksModelController>() };
	PropagateConstPtr<GroupsModelController> m_groupsModelController { std::unique_ptr<GroupsModelController>() };
	ModelController * m_activeModelController { nullptr };

	NavigationSourceProvider m_navigationSourceProvider;
	LocaleController m_localeController { *m_uiSettingsSrc, &m_self };
	LanguageController m_languageController { *this };
	ProgressController m_progressController;
	LogController m_logController { *this };
	CollectionController m_collectionController { *this };

	ViewSourceController m_viewSourceNavigationController { *m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::viewSourceNavigation, HomeCompa::Constant::UiSettings_ns::viewSourceNavigation_default, GetViewSourceModelItems(g_viewSourceNavigationModelItems) };
	ViewSourceController m_viewSourceBooksController { *m_uiSettingsSrc, HomeCompa::Constant::UiSettings_ns::viewSourceBooks, HomeCompa::Constant::UiSettings_ns::viewSourceBooks_default, GetViewSourceModelItems(g_viewSourceBooksModelItems) };

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

GroupsModelController * GuiController::GetGroupsModelController()
{
	return m_impl->GetGroupsModelController();
}

ModelController * GuiController::GetNavigationModelController()
{
	return m_impl->GetNavigationModelController();
}

BooksModelController * GuiController::GetBooksModelController()
{
	return m_impl->GetBooksModelController();
}

BooksModelController * GuiController::GetCurrentBooksModelController()
{
	return m_impl->GetCurrentBooksModelController();
}

int GuiController::GetPixelMetric(const QVariant & metric)
{
	const auto value = QApplication::style()->pixelMetric(metric.value<QStyle::PixelMetric>());
	return value;
}

ComboBoxController * GuiController::GetViewSourceComboBoxNavigationController() noexcept
{
	return m_impl->GetViewSourceComboBoxNavigationController();
}

ComboBoxController * GuiController::GetViewSourceComboBoxBooksController() noexcept
{
	return m_impl->GetViewSourceComboBoxBooksController();
}

ComboBoxController * GuiController::GetLanguageComboBoxBooksController() noexcept
{
	return m_impl->GetLanguageComboBoxBooksController();
}

void GuiController::LogCollectionStatistics()
{
	m_impl->LogCollectionStatistics();
}

void GuiController::HandleLink(const QString & link, const long long bookId)
{
	m_impl->HandleLink(link, bookId);
}

bool GuiController::eventFilter(QObject * obj, QEvent * event)
{
	if (event->type() == QEvent::KeyPress)
		m_impl->OnKeyEvent(*static_cast<QKeyEvent *>(event));

	return QObject::eventFilter(obj, event);
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
