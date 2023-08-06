#include "LogController.h"

#include <QAbstractItemModel>

#include "model/LogModel.h"

using namespace HomeCompa::Flibrary;

struct LogController::Impl
{
	std::unique_ptr<QAbstractItemModel> model { CreateLogModel() };
};

LogController::LogController() = default;
LogController::~LogController() = default;

QAbstractItemModel * LogController::GetModel() const noexcept
{
	assert(m_impl->model);
	return m_impl->model.get();
}

void LogController::Clear()
{
	assert(m_impl->model);
	m_impl->model->setData({}, {}, LogModelRole::Clear);
}
