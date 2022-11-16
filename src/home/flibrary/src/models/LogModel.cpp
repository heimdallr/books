#include <QAbstractListModel>

#include <plog/Appenders/IAppender.h>

#include "plog/LogAppender.h"
#include "util/FunctorExecutionForwarder.h"

#include "LogModel.h"

namespace HomeCompa::Flibrary {

namespace {

std::vector<QString> s_items;

class Model final
	: public QAbstractListModel
	, virtual public plog::IAppender
{
public:
	explicit Model(QObject * parent)
		: QAbstractListModel(parent)
	{
	}

private: // QAbstractListModel
	int rowCount(const QModelIndex & parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(s_items));
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		switch (role)
		{
			case Qt::DisplayRole:
				return s_items[index.row()];

			default:
				break;
		}
		assert(false && "unexpected role");
		return {};
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		if (record.getSeverity() > plog::Severity::info)
			return;

		m_forwarder.Forward([&, message = QString::fromStdWString(record.getMessage())] () mutable
		{
			const auto pos = rowCount();
			emit beginInsertRows({}, pos, pos);
			s_items.push_back(std::move(message));
			emit endInsertRows();
		});
	}

private:
	Util::FunctorExecutionForwarder m_forwarder;
	const Log::LogAppender m_logAppender {this};
};

}

QAbstractItemModel * CreateLogModel(QObject * parent)
{
	return new Model(parent);
}

}
