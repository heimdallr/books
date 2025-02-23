#pragma once

#include <QLabel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa::Flibrary
{

class ClickableLabel final : public QLabel
{
	Q_OBJECT
	NON_COPY_MOVABLE(ClickableLabel)

public:
	explicit ClickableLabel(QWidget* parent = nullptr);
	~ClickableLabel() override;

signals:
	void clicked(const QPoint& pos) const;
	void doubleClicked(const QPoint& pos) const;
	void mouseEnter(QEnterEvent* event) const;
	void mouseLeave(QEvent* event) const;

private: // QWidget
	void mousePressEvent(QMouseEvent* event) override;
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
