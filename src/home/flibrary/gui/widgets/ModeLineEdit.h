#pragma once

#include <QLineEdit>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa
{

class ISettings;

}

namespace HomeCompa::Flibrary
{

class ModeLineEdit final : public QLineEdit
{
	Q_OBJECT
	NON_COPY_MOVABLE(ModeLineEdit)

public:
	class IValueApplier // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		using ValueApplier = void (IValueApplier::*)();

	public:
		virtual ~IValueApplier() = default;
		virtual void Find()      = 0;
		virtual void Filter()    = 0;
	};

signals:
	void ValueApplierChanged(IValueApplier::ValueApplier) const;

public:
	explicit ModeLineEdit(QWidget* parent = nullptr);
	~ModeLineEdit() override;

public:
	IValueApplier::ValueApplier Setup(std::shared_ptr<ISettings> settings, QString settingsKey, bool isFindDefault = false);

private:
	void OnValueModeActionTriggered();

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
