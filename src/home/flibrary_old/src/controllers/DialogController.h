#pragma once

#include <QMessageBox>
#include <QObject>

namespace HomeCompa::Flibrary {

class DialogController
	: public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool visible READ IsVisible WRITE SetVisible NOTIFY VisibleChanged)

signals:
	void VisibleChanged() const;

public:
	using Functor = std::function<bool(QMessageBox::StandardButton)>;

public:
	explicit DialogController(Functor functor = [] (QMessageBox::StandardButton) { return true; }, QObject * parent = nullptr);

	Q_INVOKABLE void OnButtonClicked(const QVariant & value);

public: // getters/setters
	bool IsVisible() const noexcept;
	void SetVisible(bool value);

private:
	const Functor m_functor;
	bool m_visible { false };
};

}
