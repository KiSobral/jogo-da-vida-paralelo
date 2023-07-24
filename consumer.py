from kafka import KafkaConsumer

bootstrap_servers = 'localhost:19092,localhost:29092,localhost:39092'
topic_name = 'trab-final'

def consume():
    consumer = KafkaConsumer(topic_name, bootstrap_servers=bootstrap_servers,
                             group_id='sample-tag')

    try:
        for message in consumer:
            print(f"Recebido: {message.value.decode('utf-8')}")
    except KeyboardInterrupt:
        pass
    finally:
        consumer.close()

if __name__ == "__main__":
    consume()
