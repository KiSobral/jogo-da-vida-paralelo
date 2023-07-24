from elasticsearch import Elasticsearch
import random, datetime

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
        
        # result = es.indices.delete(index=index_name, ignore=[400, 404])

        # # Verificar o resultado da exclusão do índice
        # if "acknowledged" in result and result["acknowledged"]:
        #     print("Índice excluído com sucesso!")
        # else:
        #     print("Falha ao excluir o índice.")
    except Exception as e:
        print(f"Erro ao enviar dados para o Elasticsearch: {e}")

def delete_index():
    # Conectando ao Elasticsearch local (pode ser substituído pelo endereço do servidor remoto se necessário)
    es = Elasticsearch("http://localhost:9200")

    # Substitua "my_index" e "my_type" pelo nome do índice e tipo que você deseja usar
    index_name = 'trab_final'
    doc_type = 'metrics_payload'

    try:      
        result = es.indices.delete(index=index_name, ignore=[400, 404])

        # Verificar o resultado da exclusão do índice
        if "acknowledged" in result and result["acknowledged"]:
            print("Índice excluído com sucesso!")
        else:
            print("Falha ao excluir o índice.")
    except Exception as e:
        print(f"Erro ao enviar dados para o Elasticsearch: {e}")

def create_index_with_mapping():
    # Configurar o cliente Elasticsearch
    es = Elasticsearch("http://localhost:9200")

    index_name = "trab_final"

    # JSON para definir o mapeamento dos campos
    mapping_json = {
        "mappings": {
            "properties": {
                "tamanho": {"type": "long"},
                "inicializacao": {"type": "float"},
                "vida": {"type": "float"},
                "corretude": {"type": "float"},
                "total": {"type": "float"}
            }
        }
    }

    # Realizar a solicitação para criar o índice com o mapeamento
    response = es.indices.create(index=index_name, body=mapping_json)

    if response["acknowledged"]:
        print("Índice criado com sucesso!")
    else:
        print(f"Erro ao criar o índice: {response}")

if __name__ == "__main__":
    delete_index()
    # create_index_with_mapping()
    # data_to_send = {"tamanho":    8, "inicializacao": 0.0000029,"vida": 0.0001650, "corretude": 0.0000112, "total": 0.0001791}
    # send_data_to_elasticsearch(data_to_send)
