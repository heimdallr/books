#include <QQmlEngine>
#include <QTimer>
#include <QVariant>

#include "DialogController.h"

namespace HomeCompa::Flibrary {

DialogController::DialogController(Functor functor, QObject * parent)
	: QObject(parent)
	, m_functor(std::move(functor))
{
	QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

void DialogController::OnButtonClicked(const QVariant & value)
{
	const auto button = static_cast<QMessageBox::StandardButton>(value.toInt());
	QTimer::singleShot(0, [visible = !m_functor(button), obj = QPointer(this)]
	{
		if (obj)
			obj->SetVisible(visible);
	});
}

bool DialogController::IsVisible() const noexcept
{
	return m_visible;
}

void DialogController::SetVisible(bool value)
{
	m_visible = value;
	emit VisibleChanged();
}

}
