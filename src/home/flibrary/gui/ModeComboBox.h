#pragma once

#include <QComboBox>

#include "Fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class ModeComboBox : public QComboBox
{
	NON_COPY_MOVABLE(ModeComboBox)

public:
	class IValueApplier  // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IValueApplier() = default;
		virtual void Find() = 0;
		virtual void Filter() = 0;
	};

	using ApplyValue = void(IValueApplier:: *)();

	static constexpr std::pair<const char *, ApplyValue> VALUE_MODES[]
	{
		{ QT_TRANSLATE_NOOP("TreeView", "Find"), &IValueApplier::Find },
		{ QT_TRANSLATE_NOOP("TreeView", "Filter"), &IValueApplier::Filter },
	};

public:
	explicit ModeComboBox(QWidget * parent);
	~ModeComboBox() override;

private:
	void paintEvent(QPaintEvent * event) override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}