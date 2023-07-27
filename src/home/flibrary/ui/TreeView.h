#pragma once

#include <QWidget>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class TreeView final : public QWidget
{
    Q_OBJECT
    NON_COPY_MOVABLE(TreeView)

public:
    TreeView(std::shared_ptr<class ITreeViewController> controller
        , std::shared_ptr<ISettings> settings
        , QWidget * parent = nullptr);
    ~TreeView() override;

signals:
    void NavigationModeNameChanged(QString navigationModeName) const;

public:
    void SetNavigationModeName(QString navigationModeName);

private: // QWidget
    void resizeEvent(QResizeEvent* event) override;

private:
    class Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
