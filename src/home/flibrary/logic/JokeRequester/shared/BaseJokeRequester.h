#pragma once

#include <QBuffer>
#include <QString>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IJokeRequester.h"

class QJsonValue;

namespace HomeCompa::Flibrary
{

class BaseJokeRequester : virtual public IJokeRequester
{
	NON_COPY_MOVABLE(BaseJokeRequester)

protected:
	struct Item
	{
		std::weak_ptr<IClient> client;
		QByteArray data;
		QBuffer stream { &data };

		explicit Item(std::weak_ptr<IClient> client);
	};

public:
	explicit BaseJokeRequester(QString uri);
	~BaseJokeRequester() override;

private: // IJokeRequester
	void Request(std::weak_ptr<IClient> client) override;

private:
	void OnResponse(size_t id, int code, const QString& message);
	virtual bool Process(const QJsonValue& value, std::weak_ptr<IClient> client) = 0;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
