//#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "fnd/memory.h"

#include "database/factory/Factory.h"

#include "database/interface/Database.h"

#include "models/AuthorsModel.h"

#include "GuiController.h"

namespace HomeCompa::Flibrary {

namespace {
enum class FocusState
{
	Navigation,
};

}

class GuiController::Impl
{
	using Signal = void(GuiController:: *)() const;
public:
	Impl(GuiController & self, const std::string & databaseName)
		: m_self(self)
		, m_db(Create(DB::Factory::Impl::Sqlite, databaseName))
		, m_authorsModel(std::unique_ptr<QAbstractItemModel>(CreateAuthorsModel(*m_db)))
	{
		QQmlEngine::setObjectOwnership(m_authorsModel.get(), QQmlEngine::CppOwnership);
	}

	~Impl()
	{
		m_qmlEngine.clearComponentCache();
	}

	void Start()
	{
		auto * const qmlContext = m_qmlEngine.rootContext();
		qmlContext->setContextProperty("guiController", &m_self);
		qRegisterMetaType<QAbstractItemModel *>("QAbstractItemModel*");
		m_qmlEngine.load("qrc:/qml/Main.qml");
	}

	QAbstractItemModel * GetAuthorsModel()
	{
		return m_authorsModel.get();
	}

	int GetCurrentNavigationIndex() const noexcept
	{
		return m_currentNavigationIndex;
	}

	void SetCurrentNavigationIndex(const int index)
	{
		m_focusState = FocusState::Navigation;
		Set(m_currentNavigationIndex, index, &GuiController::CurrentNavigationIndexChanged);
	}

	bool GetRunning() const noexcept
	{
		return m_running;
	}

	void OnKeyPressed(const int key, const int modifiers)
	{
		if (key == Qt::Key_X && modifiers == Qt::AltModifier)
			Set(m_running, false, &GuiController::RunningChanged);

		if (m_focusState == FocusState::Navigation)
		{
			if (modifiers == Qt::ControlModifier)
			{
				switch (key)
				{
					case Qt::Key_Home:
						return Set(m_currentNavigationIndex, IncreaseNavigationIndex(-m_authorsModel->rowCount()), &GuiController::CurrentNavigationIndexChanged);

					case Qt::Key_End:
						return Set(m_currentNavigationIndex, IncreaseNavigationIndex(m_authorsModel->rowCount()), &GuiController::CurrentNavigationIndexChanged);

					default:
						return;
				}
			}

			switch (key)
			{
				case Qt::Key_Up:
					return Set(m_currentNavigationIndex, IncreaseNavigationIndex(-1), &GuiController::CurrentNavigationIndexChanged);

				case Qt::Key_Down:
					return Set(m_currentNavigationIndex, IncreaseNavigationIndex(1), &GuiController::CurrentNavigationIndexChanged);

				case Qt::Key_PageUp:
					return Set(m_currentNavigationIndex, IncreaseNavigationIndex(-10), &GuiController::CurrentNavigationIndexChanged);

				case Qt::Key_PageDown:
					return Set(m_currentNavigationIndex, IncreaseNavigationIndex(10), &GuiController::CurrentNavigationIndexChanged);

				default:
					break;
			}
		}
	}

private:
	template<typename T>
	void Set(T & dst, const T value, const Signal signal)
	{
		if (dst == value)
			return;

		dst = value;
		emit (m_self.*signal)();
	}

	int IncreaseNavigationIndex(const int increment)
	{
		const int index = m_currentNavigationIndex + increment;
		return std::clamp(index, 0, m_authorsModel->rowCount() - 1);
	}

private:
	GuiController & m_self;
	QQmlApplicationEngine m_qmlEngine;
	PropagateConstPtr<DB::Database> m_db;
	PropagateConstPtr<QAbstractItemModel> m_authorsModel;
	int m_currentNavigationIndex { 0 };
	bool m_running { true };
	FocusState m_focusState { FocusState::Navigation };
};

GuiController::GuiController(const std::string & databaseName, QObject * parent)
	: QObject(parent)
	, m_impl(*this, databaseName)
{
}

GuiController::~GuiController() = default;

void GuiController::Start()
{
	m_impl->Start();
}

void GuiController::OnKeyPressed(const int key, const int modifiers)
{
	m_impl->OnKeyPressed(key, modifiers);
}

QAbstractItemModel * GuiController::GetAuthorsModel()
{
	return m_impl->GetAuthorsModel();
}

int GuiController::GetCurrentNavigationIndex() const
{
	return m_impl->GetCurrentNavigationIndex();
}

void GuiController::SetCurrentNavigationIndex(const int index)
{
	m_impl->SetCurrentNavigationIndex(index);
}

bool GuiController::GetRunning() const
{
	return m_impl->GetRunning();
}

}
