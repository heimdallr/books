#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IJokeRequesterFactory.h"

namespace Hypodermic
{

class Container;

}

namespace HomeCompa::Flibrary
{

class JokeRequesterFactory final : public IJokeRequesterFactory
{
	NON_COPY_MOVABLE(JokeRequesterFactory)

public:
	explicit JokeRequesterFactory(Hypodermic::Container& container);
	~JokeRequesterFactory() override;

private: // IJokeRequesterFactory
	[[nodiscard]] std::vector<ImplementationDescription> GetImplementations() const override;
	[[nodiscard]] std::shared_ptr<IJokeRequester>        Create(Implementation impl) const override;
	[[nodiscard]] std::shared_ptr<Network::Downloader>   GetDownloader() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
