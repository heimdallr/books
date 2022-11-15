#include <QAbstractListModel>

#include <plog/Appenders/IAppender.h>

#include "plog/LogAppender.h"

#include "LogModel.h"

namespace HomeCompa::Flibrary {

namespace {

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
		return parent.isValid() ? 0 : static_cast<int>(std::size(m_items));
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		switch (role)
		{
			case Qt::DisplayRole:
				return m_items[index.row()];

			default:
				break;
		}
		assert(false && "unexpected role");
		return {};
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		emit beginInsertRows({}, rowCount(), rowCount());
		m_items.push_back(QString::fromStdWString(record.getMessage()));
		emit endInsertRows();
	}

private:
	const Log::LogAppender m_logAppender {this};
	std::vector<QString> m_items;
};

}

QAbstractItemModel * CreateLogModel(QObject * parent)
{
	return new Model(parent);
}

}
