#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa::Flibrary
{

class ProgressBar final : public QWidget
{
	NON_COPY_MOVABLE(ProgressBar)

public:
	explicit ProgressBar(std::shared_ptr<class IBooksExtractorProgressController> progressController, QWidget* parent = nullptr);
	~ProgressBar() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
