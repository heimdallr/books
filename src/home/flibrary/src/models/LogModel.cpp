#include <QSortFilterProxyModel>
#include <QColor>

#include <plog/Appenders/IAppender.h>

#include "fnd/NonCopyMovable.h"

#include "plog/LogAppender.h"

#include "util/FunctorExecutionForwarder.h"
#include "util/Settings.h"
#include "util/SettingsObserver.h"

#include "LogModel.h"

#include "Settings/UiSettings_keys.h"
#include "Settings/UiSettings_values.h"

namespace HomeCompa::Flibrary {

namespace {

struct Item
{
	QString message;
	plog::Severity severity;
};

std::vector<Item> s_items;

using Role = LogModelRole;

class Model final
	: public QAbstractListModel
	, virtual public plog::IAppender
{
public:
	explicit Model(LogModelController & controller)
		: m_controller(controller)
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
				return m_controller.GetColor(item.severity);

			default:
				break;
		}
		assert(false && "unexpected role");
		return {};
	}

	bool setData(const QModelIndex & index, const QVariant & /*value*/, const int role) override
	{
		if (index.isValid())
			return false;

		switch (role)
		{
			case Role::Clear:
				emit beginRemoveRows({}, 0, rowCount() - 1);
				s_items.clear();
				emit endRemoveRows();
				return true;

			default:
				break;
		}

		assert(false && "unexpected role");
		return false;
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		m_forwarder.Forward([&
			, message = QString::fromStdWString(record.getMessage())
			, severity = record.getSeverity()
			, time = record.getTime().time
			, millieTime = record.getTime().millitm
			, tId = record.getTid()
		] () mutable
		{
			auto splitted = message.split('\n', Qt::SplitBehaviorFlags::SkipEmptyParts);
			if (splitted.isEmpty())
				return;

			if (static_cast<int>(std::size(s_items)) > m_sizeLogMaximum)
			{
				emit beginRemoveRows({}, 0, m_sizeLogMaximum / 2 - 1);
				s_items.erase(std::begin(s_items), std::next(std::begin(s_items), m_sizeLogMaximum / 2));
				emit endRemoveRows();
			}

			tm t{};
			plog::util::localtime_s(&t, &time);
			const auto pos = rowCount();
			emit beginInsertRows({}, pos, pos + splitted.size() - 1);

			{
				auto str = QString("%1:%2:%3.%4 [%5] %6")
					.arg(t.tm_hour, 2, 10, QChar('0'))
					.arg(t.tm_min, 2, 10, QChar('0'))
					.arg(t.tm_sec, 2, 10, QChar('0'))
					.arg(millieTime, 3, 10, QChar('0'))
					.arg(tId, 6, 10, QChar('0'))
					.arg(splitted.front());
				;

				s_items.emplace_back(std::move(str), severity);
			}
			splitted.erase(splitted.begin());
			for (auto && str : splitted)
				s_items.emplace_back(std::move(str), severity);

			emit endInsertRows();
		});
	}

private:
	LogModelController & m_controller;
	Util::FunctorExecutionForwarder m_forwarder;
	[[maybe_unused]] const Log::LogAppender m_logAppender { this };
	int m_sizeLogMaximum { m_controller.GetUiSettings().Get(Constant::UiSettings_ns::sizeLogMaximum, Constant::UiSettings_ns::sizeLogMaximum_default).toInt() };
};

class FilterProxyModel final
	: public QSortFilterProxyModel
	, virtual SettingsObserver
{
	NON_COPY_MOVABLE(FilterProxyModel)

public:
	FilterProxyModel(LogModelController & controller, QObject * parent)
		: QSortFilterProxyModel(parent)
		, m_settings(controller.GetUiSettings())
		, m_model(controller)
	{
		QSortFilterProxyModel::setSourceModel(&m_model);
		m_settings.RegisterObserver(this);
	}

	~FilterProxyModel() override
	{
		m_settings.UnregisterObserver(this);
	}

private: // QSortFilterProxyModel
	bool filterAcceptsRow(const int row, const QModelIndex & /*parent*/) const override
	{
		assert(row < static_cast<const QAbstractItemModel&>(m_model).rowCount());
		return static_cast<int>(s_items[row].severity) <= m_logLevel;
	}

private: // SettingsObserver
	void HandleValueChanged(const QString & key, const QVariant & value) override
	{
		if (key != Constant::UiSettings_ns::logLevel)
			return;

		assert(value.isValid());
		bool ok = false;
		m_logLevel = value.toInt(&ok);
		assert(ok);
		invalidateFilter();
	}

private:
	Settings & m_settings;
	Model m_model;
	int m_logLevel { m_settings.Get(Constant::UiSettings_ns::logLevel, Constant::UiSettings_ns::logLevel_default).toInt() };
};

}

QAbstractItemModel * CreateLogModel(LogModelController & controller, QObject * parent)
{
	return new FilterProxyModel(controller, parent);
}

}
