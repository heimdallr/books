#pragma once

class QString;

namespace HomeCompa::Flibrary::UserData {
using Callback = std::function<void(const QString &)>;
}
