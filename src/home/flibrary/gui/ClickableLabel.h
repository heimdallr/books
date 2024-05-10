#pragma once

#include <QLabel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class ClickableLabel final : public QLabel
{
    Q_OBJECT
	NON_COPY_MOVABLE(ClickableLabel)

public:
    explicit ClickableLabel(QWidget * parent = nullptr);
	~ClickableLabel() override;

signals:
    void clicked(const QPoint & pos);
	void doubleClicked(const QPoint & pos);

private:
    void mousePressEvent(QMouseEvent * event) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
