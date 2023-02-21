#pragma once

#include <QObject>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class ProgressController
	: public QObject
{
	NON_COPY_MOVABLE(ProgressController)
	Q_OBJECT

	Q_PROPERTY(bool started READ GetStarted WRITE SetStarted NOTIFY StartedChanged)
	Q_PROPERTY(double progress READ GetProgress NOTIFY ProgressChanged)

signals:
	void StartedChanged() const;
	void ProgressChanged() const;

public:
	class IProgress
	{
	public:
		virtual ~IProgress() = default;
		virtual size_t GetTotal() const = 0;
		virtual size_t GetProgress() const = 0;
		virtual void Stop() = 0;
	};

public:
	explicit ProgressController(QObject * parent = nullptr);
	~ProgressController() override;

	void Add(std::shared_ptr<IProgress> progress);

private: // property getters
	bool GetStarted();
	double GetProgress();

private: // property setters
	void SetStarted(bool value);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
