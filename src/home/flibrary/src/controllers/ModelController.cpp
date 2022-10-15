#include <QAbstractItemModel>
#include <QTimer>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

#include "models/BaseRole.h"

namespace HomeCompa::Flibrary {

namespace {
using Role = BaseRole;
}

struct ModelController::Impl
	: Observable<ModelControllerObserver>
{
	QAbstractItemModel * model { nullptr };
	int currentIndex { 0 };

	explicit Impl(ModelController & self)
		: m_self(self)
	{
		m_findTimer.setSingleShot(true);
		m_findTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_findTimer, &QTimer::timeout, [&]
		{
			(void)model->setData({}, m_findText, Role::Find);
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

	void Find(const QString & findText)
	{
		m_findText = findText;
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
	QString m_findText;
	QTimer m_findTimer;
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

void ModelController::Find(const QString & findText)
{
	m_impl->Find(findText);
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

void ModelController::OnClicked(const int index)
{
	m_impl->Perform(&ModelControllerObserver::HandleClicked, this, index);
	SetCurrentIndex(index);
}

}
