#include <QColor>
#include <QSortFilterProxyModel>
#include <QTimer>

#include <mutex>

#include <plog/Appenders/IAppender.h>

#include "fnd/NonCopyMovable.h"
#include "fnd/observable.h"

#include "plog/LogAppender.h"

#include "util/FunctorExecutionForwarder.h"
#include "util/Settings.h"
#include "util/ISettingsObserver.h"

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

class ILogAppenderObserver
	: public Observer
{
public:
	virtual void OnInsertBegin(size_t begin, size_t end) = 0;
	virtual void OnInsertEnd() = 0;
	virtual void OnRemoveBegin(size_t begin, size_t end) = 0;
	virtual void OnRemoveEnd() = 0;
};

class LogAppenderImpl
	: virtual public plog::IAppender
	, public Observable<ILogAppenderObserver>
{
public:
	LogAppenderImpl()
	{
		m_timer.setSingleShot(true);
		m_timer.setInterval(std::chrono::milliseconds(100));
		QObject::connect(&m_timer, &QTimer::timeout, [&]{ OnLogUpdated(); });
	}

	void SetLogMaximumSize(const size_t sizeLogMaximum)
	{
		m_sizeLogMaximum = sizeLogMaximum;
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		std::lock_guard lock(m_itemsGuard);

		auto splitted = QString::fromStdWString(record.getMessage()).split('\n', Qt::SplitBehaviorFlags::SkipEmptyParts);
		if (splitted.isEmpty())
			return;

		const auto severity = record.getSeverity();
		const auto time = record.getTime().time;
		const auto millieTime = record.getTime().millitm;
		const auto tId = record.getTid();

		tm t {};
		plog::util::localtime_s(&t, &time);
		{
			auto str = QString("%1:%2:%3.%4 [%5] %6")
				.arg(t.tm_hour, 2, 10, QChar('0'))
				.arg(t.tm_min, 2, 10, QChar('0'))
				.arg(t.tm_sec, 2, 10, QChar('0'))
				.arg(millieTime, 3, 10, QChar('0'))
				.arg(tId, 6, 10, QChar('0'))
				.arg(splitted.front())
				;

			m_items.emplace_back(std::move(str), severity);
		}
		splitted.erase(splitted.begin());
		for (auto && str : splitted)
			m_items.emplace_back(std::move(str), severity);

		m_forwarder.Forward([&] { m_timer.start(); });
	}

private:
	void OnLogUpdated()
	{
		std::vector<Item> items;
		{
			std::lock_guard lock(m_itemsGuard);
			items.swap(m_items);
		}

		if (items.empty())
			return;

		Perform(&ILogAppenderObserver::OnInsertBegin, std::size(s_items), std::size(s_items) + std::size(items));
		std::copy(std::make_move_iterator(std::begin(items)), std::make_move_iterator(std::end(items)), std::back_inserter(s_items));
		Perform(&ILogAppenderObserver::OnInsertEnd);

		if (std::size(s_items) < m_sizeLogMaximum)
			return;

		Perform(&ILogAppenderObserver::OnRemoveBegin, 0, m_sizeLogMaximum / 2);
		s_items.erase(std::begin(s_items), std::next(std::begin(s_items), m_sizeLogMaximum / 2));
		Perform(&ILogAppenderObserver::OnRemoveEnd);
	}

private:
	std::mutex m_itemsGuard;
	Util::FunctorExecutionForwarder m_forwarder;
	QTimer m_timer;
	size_t m_sizeLogMaximum { Constant::UiSettings_ns::sizeLogMaximum_default.toULongLong() };
	std::vector<Item> m_items;
};

LogAppenderImpl s_logAppenderImpl;
[[maybe_unused]] const Log::LogAppender m_logAppender { &s_logAppenderImpl };

using Role = LogModelRole;

class Model final
	: public QAbstractListModel
	, virtual ILogAppenderObserver
{
	NON_COPY_MOVABLE(Model)

public:
	explicit Model(ILogModelController & controller)
		: m_controller(controller)
	{
		s_logAppenderImpl.SetLogMaximumSize(m_controller.GetUiSettings().Get(Constant::UiSettings_ns::sizeLogMaximum, Constant::UiSettings_ns::sizeLogMaximum_default).toULongLong());
		s_logAppenderImpl.Register(this);
	}

	~Model() override
	{
		s_logAppenderImpl.Unregister(this);
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
private:
	void OnInsertBegin(const size_t begin, const size_t end) override
	{
		emit beginInsertRows({}, static_cast<int>(begin), static_cast<int>(end) - 1);
	}

	void OnInsertEnd() override
	{
		emit endInsertRows();
	}

	void OnRemoveBegin(const size_t begin, const size_t end) override
	{
		emit beginRemoveRows({}, static_cast<int>(begin), static_cast<int>(end) - 1);
	}

	void OnRemoveEnd() override
	{
		emit endRemoveRows();
	}

private:
	ILogModelController & m_controller;
};

class FilterProxyModel final
	: public QSortFilterProxyModel
	, virtual ISettingsObserver
{
	NON_COPY_MOVABLE(FilterProxyModel)

public:
	FilterProxyModel(ILogModelController & controller, QObject * parent)
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

QAbstractItemModel * CreateLogModel(ILogModelController & controller, QObject * parent)
{
	return new FilterProxyModel(controller, parent);
}

}
