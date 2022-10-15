#include <QAbstractItemModel>
#include <QTimer>

#include "fnd/algorithm.h"
#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

#include "models/BaseRole.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = BaseRole;

#define VIEW_MODE_ITEMS_MACRO \
		VIEW_MODE_ITEM(Find) \
		VIEW_MODE_ITEM(Filter)

constexpr std::pair<const char *, int> g_viewModes[]
{
#define VIEW_MODE_ITEM(NAME) { #NAME, Role::NAME },
		VIEW_MODE_ITEMS_MACRO
#undef  VIEW_MODE_ITEM
};

}

struct ModelController::Impl
	: Observable<ModelControllerObserver>
{
	QAbstractItemModel * model { nullptr };
	int currentIndex { 0 };
	int viewModeRole { Role::Find };

	explicit Impl(ModelController & self)
		: m_self(self)
	{
		m_findTimer.setSingleShot(true);
		m_findTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_findTimer, &QTimer::timeout, [&]
		{
			(void)model->setData({}, m_viewModeText, viewModeRole);
		});
	}

	void OnKeyPressed(int key, int modifiers) const
	{
		if (modifiers == Qt::ControlModifier)
		{
			switch (key)
			{
				case Qt::Key_Home:
					return m_self.SetCurrentIndex(IncreaseNavigationIndex(-model->rowCount()));

				case Qt::Key_End:
					return m_self.SetCurrentIndex(IncreaseNavigationIndex(model->rowCount()));

				default:
					return;
			}
		}

		switch (key)
		{
			case Qt::Key_Up:
				return m_self.SetCurrentIndex(IncreaseNavigationIndex(-1));

			case Qt::Key_Down:
				return m_self.SetCurrentIndex(IncreaseNavigationIndex(1));

			case Qt::Key_PageUp:
				return m_self.SetCurrentIndex(IncreaseNavigationIndex(-10));

			case Qt::Key_PageDown:
				return m_self.SetCurrentIndex(IncreaseNavigationIndex(10));

			default:
				break;
		}
	}

	void SetViewMode(const QString & mode, const QString & text)
	{
		viewModeRole = FindSecond(g_viewModes, mode.toStdString().data(), PszComparer{});
		m_viewModeText = text;
		m_findTimer.start();
	}

private:
	int IncreaseNavigationIndex(const int increment) const
	{
		const int index = currentIndex + increment;
		return std::clamp(index, 0, model->rowCount() - 1);
	}

private:
	ModelController & m_self;
	QTimer m_findTimer;

	QString m_viewModeText;
};

ModelController::ModelController(QObject * parent)
	: QObject(parent)
	, m_impl(*this)
{
}

ModelController::~ModelController() = default;

void ModelController::OnKeyPressed(const int key, const int modifiers)
{
	m_impl->OnKeyPressed(key, modifiers);
}

void ModelController::RegisterObserver(ModelControllerObserver * observer)
{
	m_impl->Register(observer);
}

void ModelController::UnregisterObserver(ModelControllerObserver * observer)
{
	m_impl->Unregister(observer);
}

void ModelController::SetModel(QAbstractItemModel * const model)
{
	m_impl->model = model;
}

void ModelController::SetCurrentIndex(int index)
{
	if (Util::Set(m_impl->currentIndex, index, *this, &ModelController::CurrentIndexChanged))
		m_impl->Perform(&ModelControllerObserver::HandleCurrentIndexChanged, this, index);
}

void ModelController::SetViewMode(const QString & mode, const QString & text)
{
	m_impl->SetViewMode(mode, text);
}

int ModelController::GetCurrentIndex() const
{
	return m_impl->currentIndex;
}

QAbstractItemModel * ModelController::GetModel()
{
	assert(m_impl->model);
	return m_impl->model;
}

QString ModelController::GetViewMode() const
{
	return FindFirst(g_viewModes, m_impl->viewModeRole);
}

void ModelController::OnClicked(const int index)
{
	m_impl->Perform(&ModelControllerObserver::HandleClicked, this, index);
	SetCurrentIndex(index);
}

}
