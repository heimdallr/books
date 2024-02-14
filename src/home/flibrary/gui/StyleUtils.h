#pragma once

#include "export/gui.h"

class QWidget;

namespace HomeCompa::Flibrary::StyleUtils {

void SetHeaderViewStyle(QWidget & widget);
void SetLineEditWithErrorStyle(QWidget & widget);

GUI_EXPORT void EnableStyleUtils(bool enabled);

}
