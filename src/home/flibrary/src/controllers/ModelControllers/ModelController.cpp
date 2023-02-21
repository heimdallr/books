#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QTimer>

#include <plog/Log.h>

#include "fnd/algorithm.h"
#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "models/RoleBase.h"

#include "ModelController.h"
#include "IModelControllerObserver.h"
#include "ModelControllerSettings.h"

namespace HomeCompa::Flibrary {

namespace {

using Role = RoleBase;

#define VIEW_MODE_ITEMS_MACRO  \
		VIEW_MODE_ITEM(Find)   \
		VIEW_MODE_ITEM(Filter)

constexpr std::pair<const char *, int> g_viewModes[]
{
#define VIEW_MODE_ITEM(NAME) { #NAME, Role::NAME },
		VIEW_MODE_ITEMS_MACRO
#undef  VIEW_MODE_ITEM
};

constexpr std::pair<ModelControllerType, const char *> g_typeNames[]
{
#define ITEM(NAME) { ModelControllerType::NAME, #NAME }
		ITEM(Navigation),
		ITEM(Books),
#undef	ITEM
};

}

struct ModelController::Impl
	: Observable<IModelControllerObserver>
	, IModelControllerSettingsObserver
{
	NON_COPY_MOVABLE(Impl)

private:
	ModelController & m_self;

public:
	PropagateConstPtr<QAbstractItemModel> model { std::unique_ptr<QAbstractItemModel>() };
	int currentIndex { -1 };
	int pageSize { 10 };
	bool focused { false };

	QTimer findTimer;

	std::unique_ptr<ModelControllerSettings> modelControllerSettings;

	const char * const typeName;
	const char * const viewSourceName;

	Impl(ModelController & self
		, std::unique_ptr<ModelControllerSettings> modelControllerSettings_
		, const char * typeName_
		, const char * sourceName_
	)
		: m_self(self)
		, modelControllerSettings(std::move(modelControllerSettings_))
		, typeName(typeName_)
		, viewSourceName(sourceName_)
	{
		modelControllerSettings->Register(this);

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

		PLOGD << typeName << " model controller created";
	}

	~Impl() override
	{
		modelControllerSettings->Unregister(this);

		PLOGD << typeName << " model controller destroyed";
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
			auto newIndex = currentIndex;
			switch (key)
			{
				case Qt::Key_Up:
					newIndex = IncreaseNavigationIndex(-1); break;

				case Qt::Key_Down:
					newIndex = IncreaseNavigationIndex(1); break;

				case Qt::Key_PageUp:
					newIndex = IncreaseNavigationIndex(-pageSize); break;

				case Qt::Key_PageDown:
					newIndex = IncreaseNavigationIndex(pageSize); break;

				default:
					break;
			}

			if (newIndex != currentIndex)
				(void)m_self.SetCurrentIndex(newIndex);
		}
	}

	void FindCurrentItem()
	{
		handle_idChanged(modelControllerSettings->id());
	}

private: // ModelControllerSettingsObserver
	void handle_viewModeChanged(const QVariant & /*value*/) override
	{
		if (modelControllerSettings->viewSource().toString() != viewSourceName)
			return;

		findTimer.start();
		emit m_self.ViewModeChanged();
	}

	void handle_viewModeValueChanged(const QVariant & /*value*/) override
	{
		if (modelControllerSettings->viewSource().toString() != viewSourceName)
			return;

		findTimer.start();
	}

	void handle_viewSourceChanged(const QVariant & /*value*/) override
	{
	}

	void handle_idChanged(const QVariant & value) override
	{
		if (modelControllerSettings->viewSource().toString() != viewSourceName)
			return;

		if (value.isNull() || !value.isValid())
			return;

		int itemIndex = -1;
		if (model->setData({}, QVariant::fromValue(FindItemRequest { value, &itemIndex }), Role::FindItem))
		{
			currentIndex = -1;
			(void)m_self.SetCurrentIndex(itemIndex);
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

ModelController::ModelController(std::unique_ptr<ModelControllerSettings> modelControllerSettings
	, const char * typeName
	, const char * sourceName
	, QObject * parent
)
	: QObject(parent)
	, m_impl(*this
		, std::move(modelControllerSettings)
		, typeName
		, sourceName
	)
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

QString ModelController::GetViewSource() const
{
	return m_impl->modelControllerSettings->viewSource().toString();
}

void ModelController::RegisterObserver(IModelControllerObserver * observer)
{
	m_impl->Register(observer);
}

void ModelController::UnregisterObserver(IModelControllerObserver * observer)
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
	if (m_impl->currentIndex != index)
		SetCurrentIndex(index);

	emit FocusedChanged();
	m_impl->Perform(&IModelControllerObserver::HandleClicked, this);
}

void ModelController::HandleItemDoubleClicked(const int)
{
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
	return m_impl->modelControllerSettings->viewMode().toString();
}

QString ModelController::GetViewModeValue() const
{
	return m_impl->modelControllerSettings->viewModeValue().toString();
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
	m_impl->modelControllerSettings->set_viewMode(viewMode);
}

void ModelController::SetViewModeValue(const QString & text)
{
	m_impl->modelControllerSettings->set_viewModeValue(text);
}

void ModelController::FindCurrentItem()
{
	m_impl->FindCurrentItem();
}

void ModelController::SaveCurrentItemId()
{
	if (m_impl->modelControllerSettings->viewSource().toString() != m_impl->viewSourceName)
		return;

	const int localIndex = GetCurrentLocalIndex();
	if (localIndex < 0)
		return;

	const auto modelIndex = m_impl->model->index(localIndex, 0);
	const auto id = m_impl->model->data(modelIndex, Role::Id);
	m_impl->modelControllerSettings->set_id(id);
}

const char * ModelController::GetTypeName(const ModelControllerType type)
{
	return FindSecond(g_typeNames, type);
}

bool ModelController::SetCurrentIndex(const int index)
{
	PLOGV << m_impl->typeName << ": set current index: " << index;
	return true
		&& Util::Set(m_impl->currentIndex, index, *this, &ModelController::CurrentIndexChanged)
		&& (m_impl->Perform(&IModelControllerObserver::HandleCurrentIndexChanged, this, index), true)
		;
}

}
