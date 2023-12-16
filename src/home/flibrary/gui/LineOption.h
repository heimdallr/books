#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/ui/ILineOption.h"

class QLineEdit;
class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class LineOption final
	: virtual public ILineOption
{
	NON_COPY_MOVABLE(LineOption)

public:
	explicit LineOption(std::shared_ptr<ISettings> settings);
	~LineOption() override;

private: // ILineOption
	void SetLineEdit(QLineEdit * lineEdit) noexcept override;
	void SetSettingsKey(QString key, const QString & defaultValue) noexcept override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
