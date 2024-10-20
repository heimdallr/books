#include "Initializer.h"

#include <xercesc/util/PlatformUtils.hpp>


using namespace HomeCompa;
using namespace Util;
namespace xercesc = xercesc_3_2;

class XMLPlatformInitializer::Impl
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl()
	{
		xercesc::XMLPlatformUtils::Initialize();
	}

	~Impl()
	{
		xercesc::XMLPlatformUtils::Terminate();
	}
};

namespace {
std::shared_ptr<XMLPlatformInitializer::Impl> g_initializer;
}

XMLPlatformInitializer::XMLPlatformInitializer()
	: m_impl(g_initializer ? g_initializer : std::make_shared<Impl>())
{
}

XMLPlatformInitializer::~XMLPlatformInitializer() = default;
