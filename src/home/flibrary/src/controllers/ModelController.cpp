#include <QAbstractItemModel>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

namespace HomeCompa::Flibrary {

struct ModelController::Impl
	: Observable<ModelControllerObserver>
{
	QAbstractItemModel * model { nullptr };
	int currentIndex { 0 };

	explicit Impl(ModelController & self)
		: m_self(self)
	{
	}

	void OnKeyPressed(int key, int modifiers)
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

private:
	int IncreaseNavigationIndex(const int increment) const
	{
		const int index = currentIndex + increment;
		return std::clamp(index, 0, model->rowCount() - 1);
	}

private:
	ModelController & m_self;
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

int ModelController::GetCurrentIndex() const
{
	return m_impl->currentIndex;
}

QAbstractItemModel * ModelController::GetModel()
{
	assert(m_impl->model);
	return m_impl->model;
}

void ModelController::SetCurrentIndex(const int index)
{
	if (Util::Set(m_impl->currentIndex, index, *this, &ModelController::CurrentIndexChanged))
		m_impl->Perform(&ModelControllerObserver::HandleCurrentIndexChanged, this, index);
}

}
