#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QTimer>

#include "fnd/algorithm.h"
#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "util/Settings.h"

#include "ModelController.h"
#include "ModelControllerObserver.h"

#include "models/RoleBase.h"

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
	: Observable<ModelControllerObserver>
{
private:
	ModelController & m_self;
	Settings & m_uiSettings;
	const char * const m_viewModeKey;
	const QVariant & m_viewModeDefaultValue;

	QTimer m_findTimer;
	QString m_viewModeText;

public:
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>() };
	int currentIndex { -1 };
	int pageSize { 10 };
	bool focused { false };

	Impl(ModelController & self, Settings & uiSettings, const char * viewModeKey, const QVariant & viewModeDefaultValue)
		: m_self(self)
		, m_uiSettings(uiSettings)
		, m_viewModeKey(viewModeKey)
		, m_viewModeDefaultValue(viewModeDefaultValue)
	{
		m_findTimer.setSingleShot(true);
		m_findTimer.setInterval(std::chrono::milliseconds(250));
		connect(&m_findTimer, &QTimer::timeout, [&]
		{
			assert(model);
			const auto viewModeRole = FindSecond(g_viewModes, GetViewMode().toUtf8().data(), PszComparer {});
			(void)model->setData({}, m_viewModeText, viewModeRole);
		});
	}

	void OnKeyPressed(const int key, const int modifiers)
	{
		auto index = currentIndex;
		model->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest { &index }), Role::TranslateIndexFromGlobal);

		const auto modelIndex = model->index(index, 0);
		if (!modelIndex.isValid())
			return;

		(void)model->setData(modelIndex, QVariant::fromValue(qMakePair(key, modifiers)), Role::KeyPressed);

		if (modifiers == Qt::ControlModifier)
		{
			switch (key)
			{
				case Qt::Key_Home:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(-model->rowCount()));

				case Qt::Key_End:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(model->rowCount()));

				default:
					return;
			}
		}

		if (modifiers == Qt::NoModifier)
		{
			switch (key)
			{
				case Qt::Key_Up:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(-1));

				case Qt::Key_Down:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(1));

				case Qt::Key_PageUp:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(-pageSize));

				case Qt::Key_PageDown:
					return (void)m_self.SetCurrentIndex(IncreaseNavigationIndex(pageSize));

				default:
					break;
			}
		}
	}

	QString GetViewMode() const
	{
		return m_uiSettings.Get(m_viewModeKey, m_viewModeDefaultValue).toString();
	}

	void SetViewMode(const QString & viewMode)
	{
		m_uiSettings.Set(m_viewModeKey, viewMode);
		m_findTimer.start();
	}

	void SetViewModeValue(const QString & text)
	{
		m_viewModeText = text;
		m_findTimer.start();
	}

	void UpdateCurrentIndex(const int globalIndex)
	{
		const auto index = globalIndex == -1 ? currentIndex : globalIndex;
		currentIndex = -1;
		m_self.HandleItemClicked(index);
	}

private:
	int IncreaseNavigationIndex(const int increment)
	{
		int result = currentIndex + increment;
		(void)model->setData({}, QVariant::fromValue(IncreaseLocalIndexRequest { currentIndex, &result }), Role::IncreaseLocalIndex);
		return result;
	}
};

ModelController::ModelController(Settings & uiSettings, const char * viewModeKey, const QVariant & viewModeDefaultValue, QObject * parent)
	: QObject(parent)
	, m_impl(*this, uiSettings, viewModeKey, viewModeDefaultValue)
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
	return m_impl->model.get();
}

QString ModelController::GetId(int index)
{
	if (!m_impl->model || index < 0)
		return {};

	m_impl->model->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest { &index }), Role::TranslateIndexFromGlobal);
	const auto localModelIndex = m_impl->model->index(index, 0);
	assert(localModelIndex.isValid());
	const auto authorIdVar = m_impl->model->data(localModelIndex, Role::Id);
	assert(authorIdVar.isValid());
	return authorIdVar.toString();
}

void ModelController::HandleModelItemFound(const int index)
{
	SetCurrentIndex(index);
}

void ModelController::HandleItemClicked(const int index)
{
	SetCurrentIndex(index);
	emit FocusedChanged();
	m_impl->Perform(&ModelControllerObserver::HandleClicked, this);
}

void ModelController::HandleInvalidated()
{
	auto toIndex = m_impl->currentIndex;
	(void)m_impl->model->setData({}, QVariant::fromValue(CheckIndexVisibleRequest { &toIndex }), Role::CheckIndexVisible);
	if (!SetCurrentIndex(toIndex))
		emit CurrentIndexChanged();

	emit CountChanged();
}

void ModelController::SetViewModeValue(const QString & text)
{
	m_impl->SetViewModeValue(text);
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
	assert(m_impl->model);
	int localIndex = m_impl->currentIndex;
	(void)m_impl->model->setData({}, QVariant::fromValue(TranslateIndexFromGlobalRequest { &localIndex }), Role::TranslateIndexFromGlobal);
	return localIndex;
}

QAbstractItemModel * ModelController::GetModel()
{
	if (m_impl->model)
		return m_impl->model.get();

	auto * model = CreateModel();
	QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
	m_impl->model.reset(model);
	return m_impl->model.get();
}

QString ModelController::GetViewMode() const
{
	return m_impl->GetViewMode();
}

int ModelController::GetCount() const
{
	return m_impl->model ? m_impl->model->data({}, Role::Count).toInt() : 0;
}

void ModelController::UpdateCurrentIndex(const int globalIndex)
{
	m_impl->UpdateCurrentIndex(globalIndex);
}

void ModelController::SetViewMode(const QString & viewMode)
{
	m_impl->SetViewMode(viewMode);
	emit ViewModeChanged();
}

bool ModelController::SetCurrentIndex(const int index)
{
	return true
		&& Util::Set(m_impl->currentIndex, index, *this, &ModelController::CurrentIndexChanged)
		&& (m_impl->Perform(&ModelControllerObserver::HandleCurrentIndexChanged, this, index), true)
		;
}

}
