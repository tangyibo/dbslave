#include "mysqlslave.h"
#define LOG4Z_FORMAT_INPUT_ENABLE
#include "log4z.h"
#include <iostream>

using namespace zsummer::log4z;

int main(int argc, char** argv)
{
    ILog4zManager::getRef().config("./logger.conf");
    ILog4zManager::getRef().start();
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Start server ...");

    try
    {
        dbslave_daemon d;
        d.init("./config.json");
        d.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Exception: %s ", e.what());
    }

    return 0;
}
