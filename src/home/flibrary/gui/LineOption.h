#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/ui/ILineOption.h"

#include "util/ISettings.h"

class QLineEdit;
class QString;

namespace HomeCompa::Flibrary
{

class LineOption final : virtual public ILineOption
{
	NON_COPY_MOVABLE(LineOption)

public:
	explicit LineOption(std::shared_ptr<ISettings> settings);
	~LineOption() override;

private: // ILineOption
	void SetLineEdit(QLineEdit* lineEdit) noexcept override;
	void SetSettingsKey(QString key, const QString& defaultValue) noexcept override;

	void Register(IObserver* observer) override;
	void Unregister(IObserver* observer) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
