#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <glib.h>
#include <librdkafka/rdkafka.h>

#define normaliza(i,j) (i)*(tam+2)+j

static void load_config_group(rd_kafka_conf_t *conf, GKeyFile *key_file, const char *group);
/* Optional per-message delivery callback (triggered by poll() or flush())
 * when a message has been successfully delivered or permanently
 * failed delivery (after retries).
 */
static void dr_msg_cb (rd_kafka_t *kafka_handle, const rd_kafka_message_t *rkmessage, void *opaque);
int ConfigureProducer(const char *config_file, rd_kafka_t **producer);
int SendMessage(const char *topic, char *key, char *value, rd_kafka_t *producer);
void SendTime(
    int tam, double tempo0, double tempo1, 
    double tempo2, double tempo3, 
    const char *topic, rd_kafka_t *producer
);
double WallTime();
void UmaVida(int* tabuleiro_entrada, int* tabuleiro_saida, int linhas, int tam);
void PrintaSeparador(int first, int last);
void DumpTabuleiro(int * tabuleiro, int tam, int primeiro, int ultimo, char* msg);
void InitTabuleiro(int* tabuleiro_entrada, int* tabuleiro_saida, int tam);
int Correto(int* tabuleiro, int tam);


int main(int argc, char * argv[]) {
    int pow,
        powmin,
        powmax,
        i,
        tam,
        *tabuleiro_entrada,
        *tabuleiro_entrada_dev,
        *tabuleiro_saida,
        *tabuleiro_saida_dev;
    
    int rank, group_size, segment_size, segment_mod, tag=99;

    char msg[9], hostname[100];

    double  tempo0,
            tempo1,
            tempo2,
            tempo3;
    
    rd_kafka_t *producer;
    const char *config_file = "kafkaConfig.ini", *topic = "MPIOMP";

    if(argc < 3) {
        fprintf(stderr, "Usage: %s <POWMIN> <POWMAX>\n", argv[0]);
		exit(EXIT_FAILURE);
    }


    powmin = atoi(argv[1]);
    powmax = atoi(argv[2]);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &group_size);

    int hostname_len = 0;
    MPI_Get_processor_name(hostname, &hostname_len);

    if(rank == 0) {
        ConfigureProducer(config_file, &producer);      

        int keep_running = 1;
        for (pow=powmin; pow<=powmax; pow++) {
            tam = 1 << pow;

            tempo0 = WallTime();

            // Inicializa tabuleiros
            tabuleiro_entrada   = (int *) malloc ((tam+2)*(tam+2) * sizeof(int));
            tabuleiro_saida     = (int *) malloc ((tam+2)*(tam+2) * sizeof(int));
            InitTabuleiro(tabuleiro_entrada, tabuleiro_saida, tam);

            tempo1 = WallTime();

            // Computa vidas

            segment_size = (tam + 2) / (group_size-1);
            segment_mod = (tam + 2) % (group_size-1);

            for (i=0; i<4*(tam-3); i++) {
                int linhas_colunas[2];
                linhas_colunas[1] = tam;
                // send to others
                for(int j=1; j < group_size; ++j){
                    int inicio = (j-1) * segment_size;
                    int fim = inicio + segment_size + (j+1 == group_size? segment_mod - 1 : 1);
                    linhas_colunas[0] = (fim - inicio + 1);
                   
                    MPI_Send(linhas_colunas, 2, 
                        MPI_INT, j, tag, MPI_COMM_WORLD);

                    MPI_Send(&tabuleiro_entrada[inicio * (tam+2)], 
                        linhas_colunas[0] * (tam+2), 
                        MPI_INT, j, tag, MPI_COMM_WORLD);
                }
                // receive from others
                for(int j=1; j < group_size; ++j){
                    int inicio = (j-1) * segment_size;
                    int fim = inicio + segment_size + (j+1 == group_size? segment_mod - 1 : 1);
                    
                    MPI_Recv(&tabuleiro_entrada[(inicio * (tam+2)) + tam + 2],
                        ((fim - inicio + 1) * (tam+2)) - tam - 2, 
                        MPI_INT, j, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }

                if(i + 1 < 4*(tam-3)){
                    for(int j=1; j < group_size; ++j)                 
                        MPI_Send(&keep_running, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
                }
            }

            tempo2 = WallTime();

            // DumpTabuleiro(tabuleiro_entrada, tam, tam-5, tam, "");
            // printf("CONTROLLER 0\n");
            // DumpTabuleiro(tabuleiro_entrada, tam, tam-5, tam, "");

            // Verifica corretude
            if (Correto(tabuleiro_entrada, tam)) printf("**RESULTADO CORRETO**\n");
            else                                 printf("**RESULTADO ERRADO**\n");

            tempo3 = WallTime();

            printf("** Dados de execução **\n");
            printf("tamanho = %d Blocos\n", tam);
            printf("Tempos:\n");
            printf("    Inicialização:      %7.7f Segundos\n",   tempo1-tempo0);
            printf("    Computa vida:       %7.7f Segundos\n",   tempo2-tempo1);
            printf("    Verifica corretude: %7.7f Segundos\n",   tempo3-tempo2);
            printf("Tempo total: %7.7f Segundos\n\n\n",          tempo3-tempo0);

            SendTime(
                tam, tempo1-tempo0, tempo2-tempo1, 
                tempo3-tempo2, tempo3-tempo0, 
                topic, producer
            );

            // Block until the messages are all sent.
            g_message("Flushing final messages..");
            rd_kafka_flush(producer, 10 * 1000);

            if (rd_kafka_outq_len(producer) > 0) {
                g_error("%d message(s) were not delivered", rd_kafka_outq_len(producer));
            }
            g_message("Events were produced to topic %s.", topic);

            free(tabuleiro_entrada);
            free(tabuleiro_saida);

            if(pow + 1 <= powmax){
                for(int j=1; j < group_size; ++j)                 
                    MPI_Send(&keep_running, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
            }
        }
        rd_kafka_destroy(producer);
        keep_running = 0;
        for(int j=1; j < group_size; ++j)                 
            MPI_Send(&keep_running, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
    } else {
        int keep_running = 1;
        while(keep_running){
            int linhas_colunas[2];
            // recebe inicio e fim
            MPI_Recv(linhas_colunas, 2, MPI_INT, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          
            int tam_matriz = linhas_colunas[0] * (linhas_colunas[1]+2);
            
            tabuleiro_entrada   = (int *) malloc (tam_matriz * sizeof(int));
            tabuleiro_saida   = (int *) malloc (tam_matriz * sizeof(int));
            
            for(int i=0; i < tam_matriz; ++i)
                tabuleiro_saida[i] = 0;

            MPI_Recv(tabuleiro_entrada, tam_matriz, 
                MPI_INT, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            UmaVida(tabuleiro_entrada, tabuleiro_saida, linhas_colunas[0] - 2, linhas_colunas[1]);
            MPI_Send(&tabuleiro_saida[linhas_colunas[1] + 2], 
                tam_matriz - linhas_colunas[1] - 2, 
                MPI_INT, 0, tag, MPI_COMM_WORLD);

            free(tabuleiro_entrada);
            free(tabuleiro_saida);

            MPI_Recv(&keep_running, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    MPI_Finalize();
    return 0;
}

static void load_config_group(rd_kafka_conf_t *conf,
                              GKeyFile *key_file,
                              const char *group
                              ) {
    char errstr[512];
    g_autoptr(GError) error = NULL;

    gchar **ptr = g_key_file_get_keys(key_file, group, NULL, &error);
    if (error) {
        g_error("%s", error->message);
        exit(1);
    }

    while (*ptr) {
        const char *key = *ptr;
        g_autofree gchar *value = g_key_file_get_string(key_file, group, key, &error);

        if (error) {
            g_error("Reading key: %s", error->message);
            exit(1);
        }

        if (rd_kafka_conf_set(conf, key, value, errstr, sizeof(errstr))
            != RD_KAFKA_CONF_OK
            ) {
            g_error("%s", errstr);
            exit(1);
        }

        ptr++;
    }
}

/* Optional per-message delivery callback (triggered by poll() or flush())
 * when a message has been successfully delivered or permanently
 * failed delivery (after retries).
 */
static void dr_msg_cb (rd_kafka_t *kafka_handle,
                       const rd_kafka_message_t *rkmessage,
                       void *opaque) {
    if (rkmessage->err) {
        g_error("Message delivery failed: %s", rd_kafka_err2str(rkmessage->err));
    }
}

int ConfigureProducer(const char *config_file, rd_kafka_t **producer) {
    char errstr[512];
    rd_kafka_conf_t *conf;
    g_autoptr(GError) error = NULL;
    g_autoptr(GKeyFile) key_file = g_key_file_new();
    if (!g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_NONE, &error)) {
        g_error ("Error loading config file: %s", error->message);
        return 1;
    }

    conf = rd_kafka_conf_new();
    load_config_group(conf, key_file, "default");
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    *(producer) = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!*(producer)) {
        g_error("Failed to create new producer: %s", errstr);
        return 1;
    }
    conf = NULL;
    return 0;
}

int SendMessage(const char *topic, char *key, char *value, rd_kafka_t *producer) {
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);

    rd_kafka_resp_err_t err;
    err = rd_kafka_producev(producer,
                            RD_KAFKA_V_TOPIC(topic),
                            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                            RD_KAFKA_V_KEY((void*)key, key_len),
                            RD_KAFKA_V_VALUE((void*)value, value_len),
                            RD_KAFKA_V_OPAQUE(NULL),
                            RD_KAFKA_V_END);
    if (err) {
        g_error("Failed to produce to topic %s: %s", topic, rd_kafka_err2str(err));
        return 1;
    } else {
        g_message("Produced event to topic %s: key = %12s value = %12s", topic, key, value);
    }

    rd_kafka_poll(producer, 0);
    return 0;
}

void SendTime(
    int tam, double tempo0, double tempo1, 
    double tempo2, double tempo3, 
    const char *topic, rd_kafka_t *producer
) {
    char key[16], value[16];

    sprintf(key, "tamanho");
    sprintf(value, "%d", tam);
    SendMessage(topic, key, value, producer);

    sprintf(key, "inicializacao");
    sprintf(value, "%7.7lf", tempo0);
    SendMessage(topic, key, value, producer);

    sprintf(key, "vida");
    sprintf(value, "%7.7lf", tempo1);
    SendMessage(topic, key, value, producer);

    sprintf(key, "corretude");
    sprintf(value, "%7.7lf", tempo2);
    SendMessage(topic, key, value, producer);
    
    sprintf(key, "total");
    sprintf(value, "%7.7lf", tempo3);
    SendMessage(topic, key, value, producer);
}

double WallTime() {
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    return(tv.tv_sec + tv.tv_usec/1000000.0);
}

void UmaVida(int* tabuleiro_entrada, int* tabuleiro_saida, int linhas, int tam) {
    int i, j, vizinhos_vivos;

    #pragma omp parallel for shared(tabuleiro_saida) private(i, j, vizinhos_vivos)
    for (i=1; i<=linhas; i++) {
        for (j= 1; j<=tam; j++) {
            vizinhos_vivos = tabuleiro_entrada[normaliza(i-1, j-1)] + tabuleiro_entrada[normaliza(i-1, j  )] +
                             tabuleiro_entrada[normaliza(i-1, j+1)] + tabuleiro_entrada[normaliza(i  , j-1)] +
                             tabuleiro_entrada[normaliza(i  , j+1)] + tabuleiro_entrada[normaliza(i+1, j-1)] +
                             tabuleiro_entrada[normaliza(i+1, j  )] + tabuleiro_entrada[normaliza(i+1, j+1)];

            if (tabuleiro_entrada[normaliza(i, j)] && vizinhos_vivos < 2)          tabuleiro_saida[normaliza(i, j)] = 0;
            else if (tabuleiro_entrada[normaliza(i, j)] && vizinhos_vivos > 3)     tabuleiro_saida[normaliza(i, j)] = 0;
            else if (!tabuleiro_entrada[normaliza(i, j)] && vizinhos_vivos == 3)   tabuleiro_saida[normaliza(i, j)] = 1;
            else                                                                   tabuleiro_saida[normaliza(i, j)] = tabuleiro_entrada[normaliza(i, j)];
        }
    }
}

void PrintaSeparador(int first, int last) {
    for (int i=first; i<=last; i++) printf("=");
    printf("=\n");
}

void DumpTabuleiro(int * tabuleiro, int tam, int primeiro, int ultimo, char* msg){
    printf("%s; Dump posicoes [%d:%d, %d:%d] de tabuleiro %d x %d\n", msg, primeiro, ultimo, primeiro, ultimo, tam, tam);

    PrintaSeparador(primeiro, ultimo);

    for (int i=normaliza(primeiro, 0); i<=normaliza(ultimo, 0); i+=normaliza(1, 0)) {
        for (int j=i+primeiro; j<=i+ultimo; j++) printf("%c", tabuleiro[j]? 'X' : '.');

        printf("\n");
    }

    PrintaSeparador(primeiro, ultimo);
}


void InitTabuleiro(int* tabuleiro_entrada, int* tabuleiro_saida, int tam){
    for (int i=0; i<(tam+2)*(tam+2); i++) {
        tabuleiro_entrada[i] = 0;
        tabuleiro_saida[i] = 0;
    }

    tabuleiro_entrada[normaliza(1, 2)] = 1;
    tabuleiro_entrada[normaliza(2, 3)] = 1;
    tabuleiro_entrada[normaliza(3, 1)] = 1;
    tabuleiro_entrada[normaliza(3, 2)] = 1;
    tabuleiro_entrada[normaliza(3, 3)] = 1;
}


int Correto(int* tabuleiro, int tam){
    int cnt = 0;

    for (int i=0; i<(tam+2)*(tam+2); i++) cnt = cnt + tabuleiro[i];

    return (
        cnt == 5
        && tabuleiro[normaliza(tam-2, tam-1)]
        && tabuleiro[normaliza(tam-1, tam  )]
        && tabuleiro[normaliza(tam  , tam-2)]
        && tabuleiro[normaliza(tam  , tam-1)]
        && tabuleiro[normaliza(tam  , tam  )]
    );
}