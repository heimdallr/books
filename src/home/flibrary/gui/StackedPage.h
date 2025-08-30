#pragma once

#include <QWidget>
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary
{

class StackedPage
	: public QWidget
	, public std::enable_shared_from_this<StackedPage>
{
	Q_OBJECT
	NON_COPY_MOVABLE(StackedPage)

public:
	struct State
	{
		enum
		{
			Created,
			Started,
			Canceled,
			Finished,
		};
	};

signals:
	void StateChanged(std::shared_ptr<QWidget> widget, int state) const;

public:
	explicit StackedPage(QWidget* parent = nullptr);
	~StackedPage() override;

public:
	void StateChanged(int state);
};

}
