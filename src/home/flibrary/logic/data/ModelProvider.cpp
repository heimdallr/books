#include "ModelProvider.h"

#include <Hypodermic/Container.h>

#include "Model.h"

using namespace HomeCompa::Flibrary;

struct ModelProvider::Impl
{
	Hypodermic::Container & container;
	mutable DataItem::Ptr data { std::unique_ptr<DataItem>() };
	mutable IModelObserver * observer { nullptr };

	explicit Impl(Hypodermic::Container & container)
		: container(container)
	{
	}
};

ModelProvider::ModelProvider(Hypodermic::Container & container)
	: m_impl(container)
{
}

ModelProvider::~ModelProvider() = default;

std::shared_ptr<QAbstractItemModel> ModelProvider::CreateModel(DataItem::Ptr data, IModelObserver & observer) const
{
	m_impl->data = std::move(data);
	m_impl->observer = &observer;
	return m_impl->container.resolve<TreeModel>();
}

DataItem::Ptr ModelProvider::GetData() const noexcept
{
	assert(m_impl->data);
	return std::move(m_impl->data);
}

IModelObserver & ModelProvider::GetObserver() const noexcept
{
	assert(m_impl->observer);
	return *m_impl->observer;
}