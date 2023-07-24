from kafka import KafkaConsumer
from typing import Dict
import json


bootstrap_servers = 'localhost:19092,localhost:29092,localhost:39092'
topic_name = 'trab-final'


def parse_message(payload: str) -> Dict:
    try:
        new_dict = json.loads(payload)
        return new_dict

    except Exception:
        return {}


def handle_message(payload: Dict) -> None:
    method = payload.get("method")
    if method is None:
        print("Erro ao receber mensagem!")
        print("Método de utilização não especificado.")

    pow_min = payload.get("powmin", 10)
    pow_max = payload.get("powmax", 20)

    if method == "omp-mpi":
        omp_mpi(pow_min, pow_max)
    
    elif method == "spark":
        spark(pow_min, pow_max)
    
    else:
        print("Tratando omp-mpi por default")
        omp_mpi(pow_min, pow_max)


def omp_mpi(powmin: int, powmax: int) -> None:
    print("Tratando um omp-mpi")
    print(powmin)
    print(powmax)


def spark(powmin: int, powmax: int) -> None:
    print("Tratando um spark")
    print(powmin)
    print(powmax)


def consume():
    consumer = KafkaConsumer(topic_name, bootstrap_servers=bootstrap_servers,
                             group_id='sample-tag')

    try:
        for message in consumer:
            message_payload = message.value.decode('utf-8')
            message_dict = parse_message(message_payload)
            handle_message(message_dict)
    
    except KeyboardInterrupt:
        print("\n\nEncerrando o consumidor...\n")
    
    finally:
        consumer.close()

if __name__ == "__main__":
    consume()
