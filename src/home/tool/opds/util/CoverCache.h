#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/ICoverCache.h"

namespace HomeCompa::Opds
{

class CoverCache final : virtual public ICoverCache
{
	NON_COPY_MOVABLE(CoverCache)

public:
	CoverCache();
	~CoverCache() override;

private: // ICoverCache
	void       Set(QString id, QByteArray data) const override;
	QByteArray Get(const QString& id) const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}
