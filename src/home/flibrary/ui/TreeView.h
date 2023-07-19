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
    NON_COPY_MOVABLE(TreeView)

public:
    TreeView(std::shared_ptr<class ITreeViewController> controller
        , std::shared_ptr<ISettings> settings
        , QWidget * parent = nullptr);
    ~TreeView() override;

private:
    class Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
