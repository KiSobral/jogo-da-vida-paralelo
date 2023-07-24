#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <curl/curl.h>
#include <string.h>

#define normaliza(i,j) (i)*(tam+2)+j

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

char * MontarJson(int tam, double tempo0, double tempo1, double tempo2, double tempo3) {
    char * json_data = (char *) malloc(300 * sizeof(char));

    sprintf(
        json_data, "{\"tamanho\": %4d, \"inicializacao\": %7.7lf,"
        "\"vida\": %7.7lf, \"corretude\": %7.7lf, \"total\": %7.7lf}",
        tam, tempo0, tempo1, tempo2, tempo3
    );
    printf("JsonData (%d): %s\n", strlen(json_data), json_data);

    return json_data;
}

int main(int argc, char * argv[]) {
    // Definir a URL do Elasticsearch para indexar o documento
    const char *elastic_url = "http://localhost:9200/trab_final/metrics_payload/";
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
        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "Erro ao inicializar o libcurl\n");
            return 1;
        }
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

            char *json_data = MontarJson(
                tam, tempo1-tempo0,
                tempo2-tempo1, tempo3-tempo2,
                tempo3-tempo0
            );
            printf("Json montado\n");
            curl_easy_setopt(curl, CURLOPT_URL, elastic_url);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                fprintf(stderr, "Erro ao realizar a requisição: %s\n", curl_easy_strerror(res));
            }
            
            free(tabuleiro_entrada);
            free(tabuleiro_saida);
            free(json_data);

            if(pow + 1 <= powmax){
                for(int j=1; j < group_size; ++j)                 
                    MPI_Send(&keep_running, 1, MPI_INT, j, tag, MPI_COMM_WORLD);
            }
        }
        keep_running = 0;
        curl_easy_cleanup(curl);
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
