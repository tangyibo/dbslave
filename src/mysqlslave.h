#include <string>
#include <map>
#include "logparser.h"
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>

class RabbitMQC;

class dbslave_daemon : public mysql::CLogParser
{
private:
    int do_terminate;
    MYSQL DB;
    pthread_t th_repl;
    boost::shared_ptr<RabbitMQC> rabbitmq;
    std::string routing_key;
    
    int on_insert ( const mysql::CTable& tbl, const mysql::CTable::TRows& rows );
    int on_update ( const mysql::CTable& tbl, const mysql::CTable::TRows& rows, const mysql::CTable::TRows& old_rows );
    int on_delete ( const mysql::CTable& tbl, const mysql::CTable::TRows& rows );

public:
    dbslave_daemon ( );
    virtual ~dbslave_daemon ( ) throw ( );

    void init ( const char *config_file );
    void run ( );

    void terminate ( );
};

