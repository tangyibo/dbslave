#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fstream>
#include "jsoncpp/json.h"
#include "rabbitmq.h"
#define LOG4Z_FORMAT_INPUT_ENABLE
#include "log4z.h"
#include "mysqlslave.h"
#include "database.h"
#include <boost/lexical_cast.hpp>
#include <sstream>
#include <time.h>

static std::string get_now_time()
{
    time_t tmr = time(NULL);
    struct tm *t = localtime(&tmr);
    char buf[128];
    sprintf(buf, "%d-%d-%d %d:%d:%d",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec);

    return std::string(buf);
}

/////////////////////////////////////////////////

dbslave_daemon::dbslave_daemon()
: do_terminate(0)
, th_repl(0)
{

}

dbslave_daemon::~dbslave_daemon() throw ()
{
    if (th_repl > 0)
    {
        stop_event_loop();
        ::pthread_kill(th_repl, SIGTERM);

        pthread_join(th_repl, 0);
        //fprintf(stdout, "th_repl joined\n");
    }
}

void dbslave_daemon::terminate()
{
    do_terminate = 1;
}

static dbslave_daemon* test_daemon_ptr = 0;

void signal_handler(int sig)
{
    if (!test_daemon_ptr) return;

    switch (sig)
    {
        case SIGHUP:
        case SIGUSR1:
        case SIGINT:
        case SIGTERM:
        case SIGSTOP:
            LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "signal %d received, going to terminate", sig);
            test_daemon_ptr->terminate();
            break;
        case SIGSEGV:
        case SIGABRT:
            LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "signal %d received, fatal terminate", sig);
            exit(0);
        default:
            LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "signal %d received, ignoring", sig);
            break;
    }
}

void dbslave_daemon::init(const char *config_file)
{
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Init!!!!!");

    std::ifstream infile(config_file);
    Json::Reader reader;
    Json::Value json_root;
    if (!reader.parse(infile, json_root) || json_root.empty())
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "parse configure file content failed!");
        throw mysql::CException("parse configure file content failed!");
    }

    Json::Value mysql_root = json_root["mysql"];
    Json::Value rabbitmq_root = json_root["rabbitmq"];

    //connect rabbit
    const char *connectstr = rabbitmq_root["connectstr"].asCString();
    const char *exchange = rabbitmq_root["exchange"].asCString();
    const char *queue = rabbitmq_root["queue"].asCString();
    routing_key = rabbitmq_root["routing_key"].asString();

    rabbitmq.reset(new RabbitMQC(connectstr, exchange));
    rabbitmq->init();
    rabbitmq->add_routing_key(queue, routing_key);
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Connect to RabbitMQ:\n\t URI=%s,\n\t exchange=%s,\n\t queue=%s,\n\t routing_key=%s",
            connectstr, exchange, queue, routing_key.c_str());

    //connect mysql
    const char *host = mysql_root["host"].asCString();
    const char *user = mysql_root["username"].asCString();
    const char *passwd = mysql_root["password"].asCString();
    int port = mysql_root["port"].asInt();
    std::string dbname = mysql_root["dbname"].asString();

    set_connection_params(host, time(0), user, passwd, port);
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Connect to MySQL:\n\t host=%s,\n\t port=%d,\n\t username=%s,\n\t password=%s,\n\t dbname=%s",
            host, port, user, passwd, dbname.c_str());

    //set database name for watch
    watch(dbname);

    prepare();

    //set signals for program
    test_daemon_ptr = this;
    //    for (int sig = 1; sig < 32; sig++)
    //    {
    //        if (SIGKILL == sig || SIGSTOP == sig) continue;
    //        if (SIG_ERR == signal(sig, signal_handler))
    //        {
    //            fprintf(stderr, "can't set handler for %d signal\n", sig);
    //            LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "can't set handler for %d signal", sig);
    //            exit(0);
    //        }
    //    }

}

void dbslave_daemon::run()
{
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "slave start...");

    while (dispatch())
    {
        try
        {
            dispatch_events();
        }
        catch (const std::exception& e)
        {
            LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "catch exception: %s", e.what());
            sleep(1);
        }
    }

    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "slave finished...");
}

int dbslave_daemon::on_insert(const mysql::CTable& tbl, const mysql::CTable::TRows& rows)
{
    std::string db_name = tbl.get_database_name();
    std::string table_name = tbl.get_table_name();
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "INSERT(table:%s)....", table_name.c_str());

    if (rows.size() > 0)
    {
        for (mysql::CTable::TRows::const_iterator it = rows.begin(); it != rows.end(); ++it)
        {
            Json::Value text_json;
            text_json["dbname"] = db_name;
            text_json["operator"] = "insert";
            text_json["table"] = table_name;
            text_json["time"] = get_now_time();

            const mysql::CTable::data_type* col = tbl.get_columns();
            mysql::CTable::data_type::const_iterator iter;
            Json::Value test_row_json;
            for (iter = col->begin(); iter != col->end(); ++iter)
            {
                const mysql::CValue &val = (*it)[iter->first];
                std::stringstream s;
                s << val;

                test_row_json[iter->first] = s.str();
            }

            text_json["data"] = test_row_json;
            std::string strmsg = Json::FastWriter().write(text_json);
            LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "INSERT:%s", strmsg.c_str());
            rabbitmq->publish(Json::FastWriter().write(text_json), routing_key);
        }
    }
    else
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "No data for INSERT(table:%s)....", table_name.c_str());
    }


    return 0;
}

int dbslave_daemon::on_update(const mysql::CTable& tbl, const mysql::CTable::TRows& rows, const mysql::CTable::TRows& old_rows)
{
    std::string db_name = tbl.get_database_name();
    std::string table_name = tbl.get_table_name();
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "UPDATE(table:%s)....", table_name.c_str());

    if (rows.size()>0 || old_rows.size() > 0)
    {
        mysql::CTable::TRows::const_iterator jt = old_rows.begin();
        for (mysql::CTable::TRows::const_iterator it = rows.begin(); it != rows.end(); ++it, ++jt)
        {
            Json::Value text_json;
            text_json["dbname"] = db_name;
            text_json["operator"] = "update";
            text_json["table"] = table_name;
            text_json["time"] = get_now_time();

            const mysql::CTable::data_type* col = tbl.get_columns();
            mysql::CTable::data_type::const_iterator iter;
            Json::Value test_row_json;
            for (iter = col->begin(); iter != col->end(); ++iter)
            {
                const mysql::CValue &val_org = (*it)[iter->first];
                const mysql::CValue &val_new = (*jt)[iter->first];
                std::stringstream s_org, s_new;
                s_org << val_org;
                s_new << val_new;

                test_row_json["befor"][iter->first] = s_org.str();
                test_row_json["after"][iter->first] = s_new.str();
            }

            text_json["data"] = test_row_json;
            std::string strmsg = Json::FastWriter().write(text_json);
            LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "UPDATE:%s", strmsg.c_str());
            rabbitmq->publish(Json::FastWriter().write(text_json), routing_key);
        }
    }
    else
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "No data for UPDATE(table:%s)....", table_name.c_str());
    }

    return 0;
}

int dbslave_daemon::on_delete(const mysql::CTable& tbl, const mysql::CTable::TRows& rows)
{
    std::string db_name = tbl.get_database_name();
    std::string table_name = tbl.get_table_name();
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "DELETE(table:%s)....", table_name.c_str());

    if (rows.size() > 0)
    {
        for (mysql::CTable::TRows::const_iterator it = rows.begin(); it != rows.end(); ++it)
        {
            Json::Value text_json;
            text_json["dbname"] = db_name;
            text_json["operator"] = "delete";
            text_json["table"] = table_name;
            text_json["time"] = get_now_time();

            const mysql::CTable::data_type* col = tbl.get_columns();
            mysql::CTable::data_type::const_iterator iter;
            Json::Value test_row_json;
            for (iter = col->begin(); iter != col->end(); ++iter)
            {
                const mysql::CValue &val = (*it)[iter->first];
                std::stringstream s;
                s << val;

                test_row_json[iter->first] = s.str();
            }

            text_json["data"] = test_row_json;
            std::string strmsg = Json::FastWriter().write(text_json);
            LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "DELETE:%s", strmsg.c_str());
            rabbitmq->publish(Json::FastWriter().write(text_json), routing_key);
        }
    }
    else
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "No data for DELETE(table:%s)....", table_name.c_str());
    }

    return 0;
}

