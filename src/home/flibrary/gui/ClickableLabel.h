#pragma once

#include <QLabel>

namespace HomeCompa::Flibrary {

class ClickableLabel final : public QLabel
{
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget * parent = nullptr);

signals:
    void clicked(const QPoint & pos);

private:
    void mousePressEvent(QMouseEvent * event) override;
};

}
