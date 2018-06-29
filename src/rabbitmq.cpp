#include "log4z.h"
#include "rabbitmq.h"

using namespace std;

RabbitMQC::RabbitMQC(string uri, string exchange)
{
    m_uri = uri;
    m_exchange = exchange;
    mp_amqp = NULL;
    mp_ex = NULL;
}

void RabbitMQC::init(string mod)
{
    bool connect = false;

    while (!connect)
    {
        try
        {
            if (NULL != mp_amqp)
            {
                delete mp_amqp;
                mp_amqp = NULL;
            }

            mp_amqp = new AMQP(m_uri);

            connect = true;
        }
        catch (AMQPException e)
        {
            LOGE("Connect RabbitMQ Error:" << e.getMessage());
            sleep(2);
        }
    }

    if (mod.compare("exchange") == 0)
    {
        mp_ex = mp_amqp->createExchange(m_exchange);
        mp_ex->Declare(m_exchange, "direct", AMQP_DURABLE | AMQP_MANDATORY);
    }
}

RabbitMQC::~RabbitMQC()
{
    if (mp_amqp)
    {
        delete mp_amqp;
        mp_amqp = NULL;
    }
}

void RabbitMQC::publish(string msg, string routing_key)
{
    //LOGD("publish routing_key:"<<routing_key<<" msg:"<<msg);
    try
    {
        mp_ex->Publish(msg, routing_key);
    }
    catch (AMQPException e)
    {
        LOGD("publish routing_key:" << routing_key << " error:" << e.getMessage());
        if (mp_amqp)
        {
            delete mp_amqp;
            mp_amqp = NULL;
        }
        sleep(2);
        init();
        m_que_map.clear();
        publish(msg, routing_key);
    }
}

void RabbitMQC::consume(string queue_name)
{
    //LOGD("consume queue_name:" << queue_name);

    try
    {
        map<string, AMQPQueue*>::iterator it = m_que_map.find(queue_name);
        if (it != m_que_map.end())
        {
            it->second->Consume();
        }
        else
        {
            AMQPQueue* p_que = add_routing_key(queue_name);
            p_que->Consume();
        }
    }
    catch (AMQPException e)
    {
        LOGD("consume queue_name:" << queue_name << " error:" << e.getMessage());
        if (mp_amqp)
        {
            delete mp_amqp;
            mp_amqp = NULL;
        }

        sleep(2);
        init();
        m_que_map.clear();
        consume(queue_name);
    }
}

int RabbitMQC::on_cancel(AMQPMessage * message)
{
    LOGD("cancel tag=" << message->getDeliveryTag());
    return 0;
}

int RabbitMQC::on_message(AMQPMessage * message)
{
    uint32_t len = 0;
    char * data = message->getMessage(&len);
    if (data)
    {
        on_recv_msg(data, len);
    }

    AMQPQueue * q = message->getQueue();
    q->Ack(message->getDeliveryTag());

    return 0;
}

AMQPQueue* RabbitMQC::add_routing_key(string queue_name, string routing_key, string m_consumer_tag)
{
    AMQPQueue* p_que = NULL;
    map<string, AMQPQueue*>::iterator it = m_que_map.find(queue_name);
    try
    {
        if (it == m_que_map.end())
        {
            p_que = mp_amqp->createQueue(queue_name);
            p_que->Declare(queue_name, AMQP_DURABLE);
            p_que->Qos(0, 1, false);
            if (routing_key != "")
                p_que->Bind(m_exchange, routing_key);
            if (m_consumer_tag != "")
            {
                p_que->setConsumerTag(m_consumer_tag);
                p_que->addEvent(AMQP_MESSAGE, boost::bind(&RabbitMQC::on_message, this, _1));
                p_que->addEvent(AMQP_CANCEL, boost::bind(&RabbitMQC::on_cancel, this, _1));
            }
            m_que_map[queue_name] = p_que;

        }
        else if (routing_key != "")
        {
            it->second->Bind(m_exchange, routing_key);
            p_que = it->second;
        }
    }
    catch (AMQPException e)
    {
        std::cout << e.getMessage() << std::endl;
        return p_que;
    }

    return p_que;
}
