#include "Initializer.h"

#include <atomic>
#include <mutex>

#include <xercesc/util/PlatformUtils.hpp>

using namespace HomeCompa;
using namespace Util;
namespace xercesc = xercesc_3_3;

namespace
{
std::mutex g_xercescGuard;
int64_t    g_counter { 0 };
}

XMLPlatformInitializer::XMLPlatformInitializer()
{
	std::lock_guard lock(g_xercescGuard);
	if (g_counter++ == 0)
		xercesc::XMLPlatformUtils::Initialize();
}

XMLPlatformInitializer::~XMLPlatformInitializer()
{
	std::lock_guard lock(g_xercescGuard);
	if (--g_counter == 0)
		xercesc::XMLPlatformUtils::Terminate();
}
