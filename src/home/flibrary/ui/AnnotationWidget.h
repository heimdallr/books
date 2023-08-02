#pragma once

#include <QWidget>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class AnnotationWidget final : public QWidget
{
	NON_COPY_MOVABLE(AnnotationWidget)

public:
	explicit AnnotationWidget(std::shared_ptr<ISettings> settings
		, QWidget * parent = nullptr
	);
	~AnnotationWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
