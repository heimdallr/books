#pragma once

#include <QObject>
#include <QPointer>

class QAbstractScrollArea;
class QTimer;

namespace HomeCompa::Flibrary
{

class ScrollBarController final : public QObject
{
public:
	explicit ScrollBarController(QObject* parent = nullptr);
	void SetScrollArea(QAbstractScrollArea* area);

private: // QObject
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	QTimer* CreateTimer(void (ScrollBarController::*)() const);
	void OnTimeoutV() const;
	void OnTimeoutH() const;

private:
	QTimer* m_timerV;
	QTimer* m_timerH;
	QPointer<QAbstractScrollArea> m_area;
};

}
