#pragma once

#include <QWidget>

#include "fnd/memory.h"

namespace HomeCompa::Flibrary {

class TreeView : public QWidget
{
public:
    TreeView(QWidget * parent = nullptr);
    ~TreeView() override;

private:
    struct Impl;
    PropagateConstPtr<Impl> m_impl;
};

}
