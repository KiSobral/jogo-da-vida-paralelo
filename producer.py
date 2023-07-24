import json
from kafka import KafkaProducer

bootstrap_servers = 'localhost:19092,localhost:29092,localhost:39092'
topic_name = 'trab-final'


def enviar_mensagem(msg):
    producer = KafkaProducer(bootstrap_servers=bootstrap_servers)
    producer.send(topic_name, value=msg.encode('utf-8'))
    producer.flush()
    print("\nMensagem enviada com sucesso!")


if __name__ == "__main__":
    try:
        powmin = int(input("Defina qual o valor de powmin: "))
    except Exception:
        powmin = 10
    
    try:
        powmax = int(input("Defina qual o valor de powmax: "))
    except Exception:
        powmax = 20
    
    try:
        metodo = input("Defina qual a engine a ser usada (omp-mpi/spark): ")
        metodo = metodo.lower()
        if metodo != "omp-mpi" and metodo != "spark":
            metodo = "omp-mpi"
    except Exception:
        metodo = "omp-mpi"
    

    print("----------------")
    print("Payload definido como:")
    print(f"\tPOWMIN: {powmin}")
    print(f"\tPOWMAX: {powmax}")
    print(f"\tMETODO: {metodo}")
    print("----------------")
    
    payload = {
        "powmin": powmin,
        "powmax": powmax,
        "method": metodo
    }

    enviar_mensagem(json.dumps(payload))
