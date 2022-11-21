#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QTimer>

#include <plog/Log.h>

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

constexpr std::pair<ModelController::Type, const char *> g_typeNames[]
{
#define ITEM(NAME) { ModelController::Type::NAME, #NAME }
		ITEM(Navigation),
		ITEM(Books),
#undef	ITEM
};

}

struct ModelController::Impl
	: Observable<ModelControllerObserver>
{
private:
	ModelController & m_self;

public:
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>() };
	int currentIndex { -1 };
	int pageSize { 10 };
	bool focused { false };

	QTimer findTimer;

	Settings & uiSettings;

	const char * const typeName;
	const char * const viewModeKey;
	const QVariant & viewModeDefaultValue;
	const char * const viewModeValueKey;

	Impl(ModelController & self, Settings & uiSettings_, const char * typeName_, const char * viewModeKey_, const QVariant & viewModeDefaultValue_, const char * viewModeValueKey_)
		: m_self(self)
		, uiSettings(uiSettings_)
		, typeName(typeName_)
		, viewModeKey(viewModeKey_)
		, viewModeDefaultValue(viewModeDefaultValue_)
		, viewModeValueKey(viewModeValueKey_)
	{
		findTimer.setSingleShot(true);
		findTimer.setInterval(std::chrono::milliseconds(250));
		connect(&findTimer, &QTimer::timeout, [&]
		{
			const auto viewMode = m_self.GetViewMode();
			const auto viewModeValue = m_self.GetViewModeValue();
			PLOGV << typeName << ": view mode changed: " << viewMode << " '" << viewModeValue << "'";

			assert(model);
			const auto viewModeRole = FindSecond(g_viewModes, viewMode.toUtf8().data(), PszComparer {});
			(void)model->setData({}, viewModeValue, viewModeRole);
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

private:
	int IncreaseNavigationIndex(const int increment)
	{
		int result = currentIndex + increment;
		(void)model->setData({}, QVariant::fromValue(IncreaseLocalIndexRequest { currentIndex, &result }), Role::IncreaseLocalIndex);
		return result;
	}
};

ModelController::ModelController(Settings & uiSettings
	, const char * typeName
	, const char * viewModeKey
	, const QVariant & viewModeDefaultValue
	, const char * viewModeValueKey
	, QObject * parent
)
	: QObject(parent)
	, m_impl(*this, uiSettings, typeName, viewModeKey, viewModeDefaultValue, viewModeValueKey)
{
	if (!GetViewModeValue().isEmpty())
		m_impl->findTimer.start();
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
	return m_impl->uiSettings.Get(m_impl->viewModeKey, m_impl->viewModeDefaultValue).toString();
}

QString ModelController::GetViewModeValue() const
{
	return m_impl->uiSettings.Get(m_impl->viewModeValueKey, {}).toString();
}

int ModelController::GetCount() const
{
	return m_impl->model ? m_impl->model->data({}, Role::Count).toInt() : 0;
}

void ModelController::UpdateCurrentIndex(const int globalIndex)
{
	const auto index = globalIndex == -1 ? m_impl->currentIndex : globalIndex;
	m_impl->currentIndex = -1;
	HandleItemClicked(index);
}

void ModelController::SetViewMode(const QString & viewMode)
{
	m_impl->uiSettings.Set(m_impl->viewModeKey, viewMode);
	m_impl->findTimer.start();
	emit ViewModeChanged();
}

void ModelController::SetViewModeValue(const QString & text)
{
	m_impl->uiSettings.Set(m_impl->viewModeValueKey, text);
	m_impl->findTimer.start();
}

void ModelController::UpdateViewMode()
{
	m_impl->findTimer.start();
}

const char * ModelController::GetTypeName(const Type type)
{
	return FindSecond(g_typeNames, type);
}

bool ModelController::SetCurrentIndex(const int index)
{
	PLOGV << m_impl->typeName << ": set current index: " << index;
	return true
		&& Util::Set(m_impl->currentIndex, index, *this, &ModelController::CurrentIndexChanged)
		&& (m_impl->Perform(&ModelControllerObserver::HandleCurrentIndexChanged, this, index), true)
		;
}

}
