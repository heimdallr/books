#include "LogModel.h"

#include <chrono>
#include <vector>

#include <QSortFilterProxyModel>
#include <QString>
#include <QTimer>

#include <plog/Severity.h>
#include <plog/Appenders/IAppender.h>

#include "fnd/observer.h"
#include "fnd/observable.h"
#include "interface/constants/ProductConstant.h"
#include "logging/LogAppender.h"
#include "util/FunctorExecutionForwarder.h"

namespace HomeCompa::Flibrary {

namespace {

struct Item
{
	QString message;
	plog::Severity severity;
};

std::vector<Item> * s_items { nullptr };

class ILogAppenderObserver
	: public Observer
{
public:
	virtual void OnInsertBegin(size_t begin, size_t end) = 0;
	virtual void OnInsertEnd() = 0;
	virtual void OnRemoveBegin(size_t begin, size_t end) = 0;
	virtual void OnRemoveEnd() = 0;
};

class LogAppenderImpl final
	: virtual public plog::IAppender
	, public Observable<ILogAppenderObserver>
{
public:
	LogAppenderImpl()
	{
		m_timer.setSingleShot(true);
		m_timer.setInterval(std::chrono::milliseconds(100));
		QObject::connect(&m_timer, &QTimer::timeout, [&]
		{
			OnLogUpdated();
		});
	}

private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		std::lock_guard lock(m_itemsGuard);
		auto splitted = QString::fromStdString(record.getMessage()).split('\n', Qt::SplitBehaviorFlags::SkipEmptyParts);
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

		m_forwarder.Forward([&]
		{
			m_timer.start();
		});
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

		assert(s_items);
		Perform(&ILogAppenderObserver::OnInsertBegin, s_items->size(), s_items->size() + std::size(items));
		std::copy(std::make_move_iterator(std::begin(items)), std::make_move_iterator(std::end(items)), std::back_inserter(*s_items));
		Perform(&ILogAppenderObserver::OnInsertEnd);

		if (s_items->size() < Constant::MAX_LOG_SIZE)
			return;

		Perform(&ILogAppenderObserver::OnRemoveBegin, 0, Constant::MAX_LOG_SIZE / 2);
		s_items->erase(s_items->cbegin(), std::next(s_items->cbegin(), static_cast<ptrdiff_t>(Constant::MAX_LOG_SIZE) / 2));
		Perform(&ILogAppenderObserver::OnRemoveEnd);
	}

private:
	std::mutex m_itemsGuard;
	Util::FunctorExecutionForwarder m_forwarder{};
	QTimer m_timer;
	std::vector<Item> m_items;
};

using Role = LogModelRole;
LogAppenderImpl * s_logAppenderImpl { nullptr };

class Model final
	: public QAbstractListModel
	, virtual ILogAppenderObserver
{
	NON_COPY_MOVABLE(Model)

public:
	Model()
	{
		assert(s_logAppenderImpl);
		s_logAppenderImpl->Register(this);
	}

	~Model() override
	{
		assert(s_logAppenderImpl);
		s_logAppenderImpl->Unregister(this);
	}

private: // QAbstractListModel
	int rowCount(const QModelIndex & parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(s_items->size());
	}

	QVariant data(const QModelIndex & index, const int role) const override
	{
		assert(index.isValid());
		const auto & item = (*s_items)[index.row()];
		switch (role)
		{
			case Qt::DisplayRole:
			case Role::Message:
				return item.message;

			case Role::Color:
//				return m_controller.GetColor(item.severity);

			default:
				break;
		}

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
				s_items->clear();
				emit endRemoveRows();
				return true;

			default:
				break;
		}

		assert(false && "unexpected role");
		return false;
	}

private: // ILogAppenderObserver
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
//	ILogModelController & m_controller;
};

class FilterProxyModel final
	: public QSortFilterProxyModel
{
public:
	explicit FilterProxyModel(QObject * parent)
		: QSortFilterProxyModel(parent)
	{
		QSortFilterProxyModel::setSourceModel(&m_model);
	}

private:
	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		if (role == Role::Severity)
		{
			m_logLevel = value.toInt();
			invalidateFilter();
			return true;
		}

		return QSortFilterProxyModel::setData(index, value, role);
	}

private: // QSortFilterProxyModel
	bool filterAcceptsRow(const int row, const QModelIndex & /*parent*/) const override
	{
		assert(row < static_cast<const QAbstractItemModel &>(m_model).rowCount());
		return static_cast<int>((*s_items)[row].severity) <= m_logLevel;
	}

private:
	Model m_model;
	int m_logLevel { plog::Severity::warning };
};

}

QAbstractItemModel * CreateLogModel(QObject * parent)
{
	return new FilterProxyModel(parent);
}

class LogModelAppender::Impl
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl()
	{
		s_logAppenderImpl = &m_logAppenderImpl;
		s_items = &m_items;
	}

	~Impl()
	{
		s_items = nullptr;
		s_logAppenderImpl = nullptr;
	}

private:
	LogAppenderImpl m_logAppenderImpl;
	[[maybe_unused]] const Log::LogAppender m_logAppender { &m_logAppenderImpl };
	std::vector<Item> m_items;
};

LogModelAppender::LogModelAppender() = default;
LogModelAppender::~LogModelAppender() = default;

}
