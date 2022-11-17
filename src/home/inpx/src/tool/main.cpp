#include <iostream>

#include <plog/Init.h>
#include <plog/Appenders/IAppender.h>


#include "util/constant.h"
#include "util/inpx.h"

namespace {

class Appender : public plog::IAppender
{
private: // plog::IAppender
	void write(const plog::Record & record) override
	{
		(record.getSeverity() > plog::Severity::warning ? std::wcout : std::wcerr) << record.getMessage() << std::endl;
	}
};

}

int main(int argc, char* argv[])
{
	Appender appender;
	init(plog::verbose, &appender);
	return !HomeCompa::Inpx::CreateNewCollection(argc < 2 ? std::filesystem::path(argv[0]).replace_extension(INI_EXT) : std::filesystem::path(argv[1]));
}
