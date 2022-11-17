#include <QAbstractListModel>
#include <QColor>

#include <plog/Appenders/IAppender.h>

#include "plog/LogAppender.h"
#include "util/FunctorExecutionForwarder.h"

#include "LogModel.h"

namespace HomeCompa::Flibrary {

namespace {

struct Item
{
	QString message;
	plog::Severity severity;
};

std::vector<Item> s_items;
const QColor s_colors[]
{
	QColorConstants::Svg::black,
	QColorConstants::Svg::darkred,
	QColorConstants::Svg::red,
	QColorConstants::Svg::yellow,
	QColorConstants::Svg::black,
	QColorConstants::Svg::darkgrey,
	QColorConstants::Svg::gray,
};

static_assert(plog::Severity::none == 0);
static_assert(plog::Severity::fatal == 1);
static_assert(plog::Severity::error == 2);
static_assert(plog::Severity::warning == 3);
static_assert(plog::Severity::info == 4);
static_assert(plog::Severity::debug == 5);
static_assert(plog::Severity::verbose == 6);

struct Role
{
	enum Value
	{
		Message = Qt::UserRole + 1,
		Color,
	};
};

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
	QHash<int, QByteArray> roleNames() const override
	{
		return QHash<int, QByteArray>
		{
#define		ITEM(NAME) { Role::NAME, #NAME }
			ITEM(Message),
			ITEM(Color)
#undef		ITEM
		};
	}

	int rowCount(const QModelIndex & parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(std::size(s_items));
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		assert(index.isValid());
		const auto & item = s_items[index.row()];
		switch (role)
		{
			case Qt::DisplayRole:
			case Role::Message:
				return item.message;

			case Role::Color:
				return s_colors[item.severity];

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

		m_forwarder.Forward([&, message = QString::fromStdWString(record.getMessage()), severity = record.getSeverity()] () mutable
		{
			const auto pos = rowCount();
			emit beginInsertRows({}, pos, pos);
			s_items.emplace_back(std::move(message), severity);
			emit endInsertRows();
		});
	}

private:
	Util::FunctorExecutionForwarder m_forwarder;
	[[maybe_unused]] const Log::LogAppender m_logAppender {this};
};

}

QAbstractItemModel * CreateLogModel(QObject * parent)
{
	return new Model(parent);
}

}
