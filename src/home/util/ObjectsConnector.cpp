#include "ObjectsConnector.h"

#include <list>
#include <string>
#include <unordered_map>
#include <utility>

namespace HomeCompa::Util
{

namespace
{
using MetaPair    = std::pair<const QObject*, std::string>;
using MetaObjects = std::unordered_map<QString, std::list<MetaPair>>;
MetaObjects g_signals, g_slots; // NOLINT(clang-diagnostic-exit-time-destructors)

#ifndef NDEBUG
ObjectsConnector OBJECTS_CONNECTOR;
#endif

void registerMetaType(MetaObjects& metaObjects, QString id, MetaPair metaPair)
{
	metaObjects[id].emplace_back(metaPair);
	auto* object = metaPair.first;
	QObject::connect(object, &QObject::destroyed, [&metaObjects, id = std::move(id), metaPair = std::move(metaPair)] {
		metaObjects[id].remove(metaPair);
	});
}

std::string GetMetaName(const QString& name, const QChar prefix)
{
	return ((name.startsWith(prefix)) ? name : prefix + name).toStdString();
}

} // namespace

ObjectsConnector::ObjectsConnector(QObject* parent)
	: QObject(parent)
{
}

ObjectsConnector::~ObjectsConnector() = default;

void ObjectsConnector::registerEmitter(QString ID, QObject* sender, const QString& signal_, const bool queued /* = false*/)
{
	auto signal = GetMetaName(signal_, '2');

#ifndef NDEBUG
	connect(sender, signal_.toStdString().data(), &OBJECTS_CONNECTOR, SLOT(Log()));
#endif

	for (const auto& [receiver, slot] : g_slots[ID])
	{
		assert(sender != receiver);
		[[maybe_unused]] const auto result =
			connect(sender, signal.data(), receiver, slot.data(), queued ? static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection) : Qt::UniqueConnection);
		assert(result && "bad emitter");
	}

	registerMetaType(g_signals, std::move(ID), std::make_pair(sender, std::move(signal)));
}

void ObjectsConnector::registerReceiver(QString ID, QObject* receiver, const QString& slot_, const bool queued /* = false*/)
{
	auto slot = GetMetaName(slot_, '1');

	for (const auto& [sender, signal] : g_signals[ID])
	{
		assert(sender != receiver);
		[[maybe_unused]] const auto result =
			connect(sender, signal.data(), receiver, slot.data(), queued ? static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection) : Qt::UniqueConnection);
		assert(result && "bad receiver");
	}

	registerMetaType(g_slots, std::move(ID), std::make_pair(receiver, std::move(slot)));
}

void ObjectsConnector::unregisterEmitter(const QString& ID, QObject* sender, const QString& signal_)
{
	auto signal = GetMetaName(signal_, '2');

	for (const auto& [receiver, slot] : g_slots[ID])
	{
		assert(sender != receiver);
		disconnect(sender, signal.data(), receiver, slot.data());
	}

	g_signals[ID].remove(std::make_pair(sender, std::move(signal)));
}

void ObjectsConnector::unregisterReceiver(const QString& ID, QObject* receiver, const QString& slot_)
{
	auto slot = GetMetaName(slot_, '1');

	for (const auto& [sender, signal] : g_signals[ID])
	{
		assert(sender != receiver);
		disconnect(sender, signal.data(), receiver, slot.data());
	}

	g_slots[ID].remove(std::make_pair(receiver, std::move(slot)));
}

void ObjectsConnector::Log()
{
}

} // namespace HomeCompa::Util
