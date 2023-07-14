#pragma once

#include <QWidget>

#include "fnd/memory.h"

namespace HomeCompa {
class ISettings;
class ITreeViewController;
}

namespace HomeCompa::Flibrary {

class TreeView : public QWidget
{
public:
    TreeView(std::shared_ptr<ITreeViewController> controller
        , std::shared_ptr<ISettings> settings
        , QWidget * parent = nullptr);
    ~TreeView() override;

private:
    struct Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
