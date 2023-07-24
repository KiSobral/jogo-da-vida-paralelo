from elasticsearch import Elasticsearch

def send_data_to_elasticsearch(data):
    # Conectando ao Elasticsearch local (pode ser substituído pelo endereço do servidor remoto se necessário)
    es = Elasticsearch("http://localhost:9200")

    # Substitua "my_index" e "my_type" pelo nome do índice e tipo que você deseja usar
    index_name = 'trab_final'
    doc_type = 'metrics_payload'

    try:
        # Enviando os dados para o Elasticsearch
        es.index(index=index_name, doc_type=doc_type, body=data)
        print("Dados enviados com sucesso para o Elasticsearch!")
    except Exception as e:
        print(f"Erro ao enviar dados para o Elasticsearch: {e}")

if __name__ == "__main__":
    data_to_send = {"powmin": 10, "powmax": 20, "method": "purple"}
    send_data_to_elasticsearch(data_to_send)
