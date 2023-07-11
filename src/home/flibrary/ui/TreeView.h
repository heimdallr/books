#pragma once

#include <QWidget>

#include "fnd/memory.h"

namespace HomeCompa {
class ITreeViewController;
}

namespace HomeCompa::Flibrary {

class TreeView : public QWidget
{
public:
    explicit TreeView(std::shared_ptr<ITreeViewController> controller
        , QWidget * parent = nullptr);
    ~TreeView() override;

private:
    struct Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
