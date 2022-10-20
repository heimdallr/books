#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QTimer>

#include "fnd/algorithm.h"
#include "fnd/ConvertableT.h"
#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

#include "models/RoleBase.h"
#include "models/ModelObserver.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

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
	: ConvertibleT<Impl>
	, Observable<ModelControllerObserver>
	, ModelObserver
{
	NON_COPY_MOVABLE(Impl)

private:
	ModelController & m_self;
	QTimer m_findTimer;

	QString m_viewModeText;

public:
	PropagateConstPtr<QAbstractItemModel> model;
	int currentIndex { -1 };
	int viewModeRole { Role::Find };
	int pageSize { 10 };
	bool focused { false };
	QString currentModelType;

	explicit Impl(ModelController & self)
		: m_self(self)
	{
		m_findTimer.setSingleShot(true);
		m_findTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_findTimer, &QTimer::timeout, [&]
		{
			assert(model);
			(void)model->setData({}, m_viewModeText, viewModeRole);
		});
	}

	~Impl() override
	{
		model->setData({}, QVariant::fromValue(To<ModelObserver>()), RoleBase::ObserverUnregister);
	}

	bool OnKeyPressed(int key, int modifiers)
	{
		if (modifiers == Qt::ControlModifier)
		{
			switch (key)
			{
				case Qt::Key_Home:
					return SetCurrentIndex(IncreaseNavigationIndex(-model->rowCount()));

				case Qt::Key_End:
					return SetCurrentIndex(IncreaseNavigationIndex(model->rowCount()));

				default:
					return false;
			}
		}

		switch (key)
		{
			case Qt::Key_Up:
				return SetCurrentIndex(IncreaseNavigationIndex(-1));

			case Qt::Key_Down:
				return SetCurrentIndex(IncreaseNavigationIndex(1));

			case Qt::Key_PageUp:
				return SetCurrentIndex(IncreaseNavigationIndex(-pageSize));

			case Qt::Key_PageDown:
				return SetCurrentIndex(IncreaseNavigationIndex(pageSize));

			default:
				break;
		}

		return false;
	}

	void SetViewMode(const QString & mode, const QString & text)
	{
		viewModeRole = FindSecond(g_viewModes, mode.toStdString().data(), PszComparer{});
		m_viewModeText = text;
		m_findTimer.start();
	}

private: // ModelObserver
	void HandleModelItemFound(const int index) override
	{
		SetCurrentIndex(index);
	}

	void HandleItemClicked(const int index) override
	{
		SetCurrentIndex(index);
		emit m_self.FocusedChanged();
		Perform(&ModelControllerObserver::HandleClicked, &m_self);
	}

	void HandleInvalidated() override
	{
		auto toIndex = currentIndex;
		(void)model->setData({}, QVariant::fromValue(CheckIndexVisibleRequest { &toIndex }), Role::CheckIndexVisible);
		if (!SetCurrentIndex(toIndex))
			emit m_self.CurrentIndexChanged();
	}

private:
	bool SetCurrentIndex(const int index)
	{
		return true
			&& Util::Set(currentIndex, index, m_self, &ModelController::CurrentIndexChanged)
			&& (Perform(&ModelControllerObserver::HandleCurrentIndexChanged, &m_self, index), true)
			;
	}

	int IncreaseNavigationIndex(const int increment)
	{
		int result = currentIndex + increment;
		(void)model->setData({}, QVariant::fromValue(IncreaseLocalIndexRequest { currentIndex, &result }), Role::IncreaseLocalIndex);
		return result;
	}
};

ModelController::ModelController(QObject * parent)
	: QObject(parent)
	, m_impl(*this)
{
}

ModelController::~ModelController() = default;

void ModelController::OnKeyPressed(const int key, const int modifiers)
{
	QTimer::singleShot(0, [=, &impl = *m_impl]
	{
		impl.OnKeyPressed(key, modifiers);
	});
}

void ModelController::RegisterObserver(ModelControllerObserver * observer)
{
	m_impl->Register(observer);
}

void ModelController::UnregisterObserver(ModelControllerObserver * observer)
{
	m_impl->Unregister(observer);
}

void ModelController::SetFocused(const bool value)
{
	Util::Set(m_impl->focused, value, *this, &ModelController::FocusedChanged);
}

QAbstractItemModel * ModelController::GetCurrentModel()
{
	assert(m_impl->model);
	return m_impl->model.get();
}

void ModelController::SetViewMode(const QString & mode, const QString & text)
{
	m_impl->SetViewMode(mode, text);
}

void ModelController::SetPageSize(const int pageSize)
{
	m_impl->pageSize = pageSize;
}

bool ModelController::GetFocused() const noexcept
{
	return m_impl->focused;
}

int ModelController::GetCurrentLocalIndex()
{
	int localIndex = m_impl->currentIndex;
	(void)m_impl->model->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest { &localIndex }), Role::TranslateIndexFromGlobal);
	return localIndex;
}

QAbstractItemModel * ModelController::GetModel(const QString & modelType)
{
	if (m_impl->currentModelType == modelType)
		return m_impl->model.get();

	if (m_impl->model)
		(void)m_impl->model->setData({}, QVariant::fromValue(m_impl->To<ModelObserver>()), RoleBase::ObserverUnregister);

	auto * model = GetModelImpl(modelType);
	QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
	m_impl->model.reset(model);
	(void)model->setData({}, QVariant::fromValue(m_impl->To<ModelObserver>()), RoleBase::ObserverRegister);
	m_impl->currentModelType = modelType;
	return m_impl->model.get();
}

QString ModelController::GetViewMode() const
{
	return FindFirst(g_viewModes, m_impl->viewModeRole);
}

}
