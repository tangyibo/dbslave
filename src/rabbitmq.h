#ifndef __RABBITMQ_LINUX_CXX_H__
#define __RABBITMQ_LINUX_CXX_H__

#include <string>
#include <vector>
#include <cstdlib>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signal.hpp>
#include "AMQPcpp.h"

using namespace std;

class RabbitMQC
{
public:
    RabbitMQC ( string uri, string exchange );
    ~RabbitMQC ( );
    void publish ( string msg, string routing_key );
    AMQPQueue* add_routing_key ( string queue_name, string routing_key = "", string m_consumer_tag = "" );
    void consume ( string queue_name );
    void init ( string mod = "exchange" );

    boost::signal< int (char * msg, uint32_t length ) > on_recv_msg;
    boost::signal< int (char * msg ) > on_cancel_msg;
    boost::function<int(AMQPMessage* ) > onmsg;

private:
    int on_message ( AMQPMessage * message );
    int on_cancel ( AMQPMessage * message );

    string m_uri;
    string m_exchange;
    AMQP* mp_amqp;
    AMQPExchange* mp_ex;
    AMQPQueue* mp_que;
    map<string, AMQPQueue*> m_que_map;
};


/*
usage
RabbitMQC* mq = new RabbitMQC("test:123456@localhost:5672/private","ex_name");

consumer:
mq->on_recv_msg.connect(boost::bind(******));
mq->add_routing_key("queue_name","routing_key","consume tag");
while(1)
   mq->consume("queue_name");

producer:
mq.init();
mq->add_routing_key("queue_name","routing_key");
mq->publish("msg","routing key");
 */

#endif
