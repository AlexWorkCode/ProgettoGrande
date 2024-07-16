#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "macros.h"
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/un.h>
#include <getopt.h>
#include <stdbool.h>



#define MSG_OK 'K'
#define MSG_ERR 'E'
#define MSG_REGISTRA_UTENTE 'R'
#define MSG_MATRICE 'M'
#define MSG_TEMPO_PARTITA 'T'
#define MSG_TEMPO_ATTESA 'A'
#define MSG_PAROLA 'W'
#define MSG_PUNTI_FINALI 'F'
#define MSG_PUNTI_PAROLA 'P'
#define MSG_CANCELLA_UTENTE 'D'
#define MSG_LOGIN_UTENTE 'L'
#define MSG_POST_BACHECA 'H'
#define MSG_SHOW_BACHECA 'S'
#define MSG_EXIT 'I'

#define LOG_FILE "log.txt"

#define MAX 32
#define N 4
#define BUFFER_SIZE 1024


typedef struct Messaggio{
    char type;
    int length;
    char *data;
}Messaggio;

typedef struct Partita {
    int matrice[N][N]; // Matrice 4x4
    int tempo_partita;  // Tempo di partita
    int t_finePartita;  // Tempo fine partita
    int t_nextPartita; // Tempo rimasto alla prossima partita
    int seed; // Seed
    int rand;
    char *file;
} Partita;

typedef struct Cliente{
    int socket;
    char *nome;
    int punti;
    int online; // se è 1 il cliente è attivo se è 0 no
    time_t ultimo_accesso;
    struct memCondivisa *memoria;
    // tempo di ultima attività
}Cliente;

typedef struct {
    char nome;
    int score;
} ScoreData;

typedef struct memCondivisa{
    Cliente Clienti[MAX];
    Partita partita;
    pthread_mutex_t mutexPers;
    pthread_mutex_t mutexFile;
    char *dizionario;
    int disconnessione;
}memCondivisa;

void generaMatrice(int s, memCondivisa *mem);
void generaMf(FILE *fp, int matr[N][N]);
int cercoPosto(memCondivisa *mem);
int confronta_punti(const void *a, const void *b);
int registra(Cliente *cliente, char *data);
bool esisteParola(int matrice[N][N], const char* parola);
void loginUtente(Cliente *cliente);
int parolaNelDiz(const char *str);
void inizio(Cliente *cliente);
int gestionePausa(Cliente *cliente);
const char* getTimestamp() {
    time_t now = time(NULL);
    return ctime(&now);
}


void *threadPart(void *arg){
    memCondivisa *mem = (memCondivisa*)arg;
    time_t start_time = time(NULL); // Tempo di inizio 
    int t_partita = mem->partita.tempo_partita * 60;  // Tempo durata partita

    generaMatrice(mem->partita.seed, mem);


    while(1){
        time_t c_time = time(NULL);
        double t_trasc = difftime(c_time, start_time); // Tempo trascorso nella partita

        mem->partita.t_finePartita = t_partita - t_trasc;

        if(mem->partita.t_finePartita <= 0){
            time_t ora = time(NULL);
            int t_pausa = 60;
        printf("partita in pausa\n");
        
            while(1){

                time_t f_ora = time(NULL);
                double t_rimasto = difftime(f_ora, ora); // Tempo trascorso nella pausa

                mem->partita.t_nextPartita = t_pausa - t_rimasto;
    printf("Tempo alla prossima partita %d\n", mem->partita.t_nextPartita);

                if (mem->partita.t_nextPartita <= 0){
                    generaMatrice(mem->partita.seed, mem);
                    start_time = time(NULL);  // Reset del timer
                    break;
                }
                sleep(1);
            }

        }

        sleep(1);
    }
    return NULL;
}

void *threadSc(void *arg) {
    memCondivisa *mem = (memCondivisa *)arg;
    pthread_mutex_lock(&mem->mutexPers);

        int i, j = 0;

        // Conta il numero di client online
        for (i = 0; i < MAX; i++) {
            if (mem->Clienti[i].online == 1) {
                j++;
            }
        }

        if (j != 0){
            // Alloca memoria per la lista di puntatori ai client online
            Cliente **lista = calloc(j, sizeof(Cliente *));
            if (lista == NULL) {
                perror("Errore nell'allocazione della memoria in threadSc");
                return NULL;
            }

            j = 0;
            // Riempie la lista con puntatori ai client online
            for (i = 0; i < MAX; i++) {
                if (mem->Clienti[i].online == 1) {
                    lista[j] = &mem->Clienti[i];
                    j++;
                }
            }

            // Ordina la lista in base ai punti
            qsort(lista, j, sizeof(Cliente *), confronta_punti);


            
            printf("Sta scrivendo il thread Scorer\n");

                // Invia la lista ordinata a ciascun client online se è finita la partite
                for (i = 0; i < j; i++) {
                    char type = MSG_PUNTI_FINALI;
                    if (write(lista[i]->socket, &type, sizeof(type)) < 0) {
                        perror("Errore durante l'invio del tipo di messaggio al socket");
                        continue;
                    }

                    int length = j * sizeof(Cliente *);
                    if (write(lista[i]->socket, &length, sizeof(length)) < 0) {
                        perror("Errore durante l'invio della lunghezza al socket");
                        continue;
                    }

                    if (write(lista[i]->socket, lista, length) < 0) {
                        perror("Errore durante l'invio della lista al socket");
                        continue;
                    }
                }

            // Libero la memoria allocata
            free(lista);
        };
    
    pthread_mutex_unlock(&mem->mutexPers);
    pthread_exit(NULL);
}


void *threadCl(void *arg){
    Cliente *cliente = (Cliente*)arg;
    Messaggio msg;
    


    while(1){
        pthread_mutex_lock(&cliente->memoria->mutexPers);
        printf("2 nel thread prima di while\n");
        cliente->ultimo_accesso = time(NULL);
        read(cliente->socket, &msg.type, sizeof(msg.type));
    printf("RIcevuto read: |%c|\n", msg.type);

        // Controllo il tipo del messaggio
        
        switch (msg.type){
        
            case MSG_REGISTRA_UTENTE: //Registra
                printf("Regstazione\n");
                read(cliente->socket, &msg.length, sizeof(msg.length));
                msg.data = (char *)calloc(msg.length + 1, sizeof(char));
                read(cliente->socket, msg.data, msg.length);
                
                    if(registra(cliente, msg.data)){

                        printf("Registrato\n");
                        cliente->nome = msg.data;
                        cliente->punti = 0;
                        cliente->online = 1;

                        msg.type = MSG_OK;
                        write(cliente->socket, &msg.type, sizeof(msg.type));
                        inizio(cliente);

                        // Scrivo nel file log la registrazione
                        pthread_mutex_lock(&cliente->memoria->mutexFile);
                        FILE *logFile = fopen(LOG_FILE, "a");
                        fprintf(logFile, "[%s] Registrazione utente: %s\n", getTimestamp(), cliente->nome);
                        fclose(logFile);

                        pthread_mutex_unlock(&cliente->memoria->mutexFile);

                    }else{
                        free(msg.data);
                        close(cliente->socket);
                        pthread_exit(NULL);
                    }
                    

                break;
            case MSG_MATRICE:  //Matrice
                if (cliente->memoria->partita.t_finePartita > 0){   
                    msg.type = MSG_OK;
                    write(cliente->socket, &msg.type, sizeof(msg.type));

                    msg.length = N * N * sizeof(int);
                    write(cliente->socket, &msg.length, sizeof(msg.length));
                    write(cliente->socket, cliente->memoria->partita.matrice, msg.length);
                }else{
                    // commmunicare tempo attesa partita
                    msg.type = MSG_ERR;
                    write(cliente->socket, &msg.type, sizeof(msg.type));
                    msg.length = cliente->memoria->partita.t_nextPartita;

                    write(cliente->socket, &msg.length, sizeof(msg.length));
                    write(cliente->socket, &cliente->memoria->partita.t_nextPartita, msg.length);
                }
                break;
            
            case MSG_PAROLA:  //Parola
                if (read(cliente->socket, &msg.length, sizeof(msg.length)) == -1) {
                    perror("Errore lettura lunghezza parola");
                    break;
                }

                msg.data = (char *)calloc(msg.length + 1, sizeof(char));
                if (read(cliente->socket, msg.data, msg.length) == -1) {
                    perror("Errore lettura parola");
                    free(msg.data);
                    break;
                }


                if(parolaNelDiz(msg.data)){

            printf("Parola approvata dal Dizionario\n");
                    
                    if (esisteParola(cliente->memoria->partita.matrice, msg.data)) {
            printf("La parola esiste\n");

                        msg.type = MSG_OK;
                        
                        write(cliente->socket, &msg.type, sizeof(msg.type));
            printf("Type inviato1: |%c|\n", msg.type);
                        cliente->punti += msg.length;
            printf("Punti: %d\n", msg.length);
            printf("Punti Totali: %d\n", cliente->punti);

                        pthread_mutex_lock(&cliente->memoria->mutexFile);
                        FILE *logFile = fopen(LOG_FILE, "a");
                        fprintf(logFile, "[%s] Parola inviata da %s: %s\n", getTimestamp(), cliente->nome, msg.data);
                        fclose(logFile);
                        pthread_mutex_unlock(&cliente->memoria->mutexFile);
                        break;
                         
                    } else {
            printf("La parola non esiste\n");

                        char buf[35] = "Parola non trovata nella matrice!!";
                        msg.type = MSG_ERR;
                        write(cliente->socket, &msg.type, sizeof(msg.type));
            printf("Type inviato2: |%c|\n", msg.type);

                        msg.length = strlen(buf);
                        write(cliente->socket, &msg.length, sizeof(msg.length));
                        write(cliente->socket, buf, sizeof(buf));
                    
                    
                    break;
                    };

                }else{
            printf("Parola non approvata dal Dizionario\n");

                    char buf[28] = "Parola non nel Dizionario";
                    msg.type = MSG_ERR;
                    write(cliente->socket, &msg.type, sizeof(msg.type));
            printf("Type inviato3: |%c|\n", msg.type);
                    msg.length = strlen(buf);
                    write(cliente->socket, &msg.length, sizeof(msg.length));
                    write(cliente->socket, buf, sizeof(buf));
                }
                
            printf("Finito PAROLA\n");
                break;

            case MSG_CANCELLA_UTENTE:   //Cancalla Utente
                    cliente->nome = NULL;
                    cliente->punti = 0;
                    cliente->online = 0;
                    cliente->socket = 0;

                    pthread_mutex_lock(&cliente->memoria->mutexFile);
                    FILE *logFile = fopen(LOG_FILE, "a");
                    fprintf(logFile, "[%s] Cancellazione utente: %s\n", getTimestamp(), cliente->nome);
                    fclose(logFile);
                    pthread_mutex_unlock(&cliente->memoria->mutexFile);

                    
                    pthread_mutex_unlock(&cliente->memoria->mutexPers);
                    close(cliente->socket);
                    pthread_exit(NULL);

                break;

            case MSG_LOGIN_UTENTE:   //Login Utente
                loginUtente(cliente);
                inizio(cliente);
                break;
            case MSG_POST_BACHECA:   //Post Bacheca
                
                break;
            case MSG_SHOW_BACHECA:   // Show Bacheca
                
                break;
            case MSG_EXIT:   //Exit
    printf("Sono Uscito in teoria!\n");

                cliente->online = 0;
                pthread_mutex_unlock(&cliente->memoria->mutexPers);
                close(cliente->socket);
                pthread_exit(NULL);
            
            case MSG_TEMPO_PARTITA: // Tempo Partita
                if (cliente->memoria->partita.t_finePartita > 0){ //Sono ancora nella partita
                    msg.type = MSG_OK;
                    msg.length = sizeof(cliente->memoria->partita.t_finePartita);

                    write(cliente->socket, &msg.type, sizeof(msg.type));
                    write(cliente->socket, &msg.length, sizeof(msg.length));
                    write(cliente->socket, &cliente->memoria->partita.t_finePartita, msg.length);
                }else{ // Partita finita
                    msg.type = MSG_ERR;
                    char buf[16] = "Partita finita!";
                    msg.length = strlen(buf);

                    write(cliente->socket, &msg.type, sizeof(msg.type));
                    write(cliente->socket, &msg.length, sizeof(msg.length));
                    write(cliente->socket, buf, sizeof(buf));

                    

                }
                break;
            default:
                printf("Errore\n");
                printf("Ricevuto:  |%c|\n", msg.type);
                close(cliente->socket);
                pthread_exit(NULL);
                break;
        }

        cliente->ultimo_accesso = time(NULL);
        printf("Fine ciclo while\n");
        pthread_mutex_unlock(&cliente->memoria->mutexPers);


        if (cliente->memoria->partita.t_finePartita <= 0){
            /*pthread_t tempSc;
            pthread_create(&tempSc, NULL, threadSc, &cliente->memoria);
            pthread_join(tempSc, NULL);*/

            if(gestionePausa(cliente)){
                break;
            };
            cliente->punti = 0;
        }
    };
    
    
    if (msg.data != NULL){
        free(msg.data);
    }
    close(cliente->socket);
    pthread_exit(NULL);
}


void *threadD(void *arg) {
    
    memCondivisa *mem = (memCondivisa *)arg;
    int disconnetti_dopo = mem->disconnessione * 60;  // Tempo di inattività in secondi prima della disconnessione
printf("Disconessione attiva per: %d\n", mem->disconnessione);


    while (1) {
        time_t current_time = time(NULL);
        sleep(1);
        for (int i = 0; i < MAX; i++) {
            if (mem->Clienti[i].socket != 0 && difftime(current_time, mem->Clienti[i].ultimo_accesso) > disconnetti_dopo) {
                close(mem->Clienti[i].socket);
                printf("%d\n", mem->Clienti[i].socket);
                mem->Clienti[i].socket = 0;
                mem->Clienti[i].online = 0;
                printf("Cliente %s disconnesso per inattività\n", mem->Clienti[i].nome);
            }
        }

    }
    return NULL;
}

int main(int argc, char *argv[]){
    int f = 0, PORT = 0;
    int opt;
    int option_index = 0;
    memCondivisa memoria;
    memoria.partita.tempo_partita = 3;
    memoria.partita.seed = time(0);


    pthread_mutex_init(&memoria.mutexPers, NULL);
    pthread_mutex_init(&memoria.mutexFile, NULL);
    pthread_t client[MAX];

    int server_fd, retvalue;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    char *indirizzo = NULL;
    pthread_t tempPar;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM; 

    indirizzo = argv[1];
    PORT = atoi(argv[2]);
    
    

    // Controllo gli argomenti con getopt_long()
    struct option long_options[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'z'},
        {"disconnetti-dopo", required_argument, 0, 'x'},
        {0, 0, 0, 0}
    };


    while ((opt = getopt_long(argc, argv, "m:d::s::z:x:", long_options, &option_index)) != -1) {
        switch (opt) {
            
            case 'm':
                memoria.partita.file = optarg;
                break;

            case 'd':
                    memoria.partita.tempo_partita = atoi(optarg);
                break;

            case 's':
                    memoria.partita.seed = atoi(optarg);
                    memoria.partita.rand = 0;
                break;

            case 'z':
                memoria.dizionario = optarg;
                break;

            case 'x':
                memoria.disconnessione = atoi(optarg);
                pthread_t tempD;
                pthread_create(&tempD, NULL, threadD, &memoria);
                break;

            case '?':
                write(STDOUT_FILENO,"Parametro non valido", 21);
                break;
        }
    }
    

    // Controllo la presenza dell'indirizzo e della PORT
    if (indirizzo == NULL || PORT == 0) {
        fprintf(stderr, "Indirizzo IP e porta devono essere specificati.\n");
        exit(EXIT_FAILURE);
    }
    
    // inizializzazione della str.typeuttura server
    if ((retvalue = getaddrinfo(indirizzo, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retvalue));
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)res->ai_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr = addr_in->sin_addr;

    freeaddrinfo(res);

    SYSC(server_fd, socket(AF_INET, SOCK_STREAM, 0), "nella socket");


    // Binding
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));


    // Listen
    SYSC(retvalue, listen(server_fd, 10), "nella listen");

    for(int g = 0; g < MAX; g++){
        memoria.Clienti[g].nome = "";
        memoria.Clienti[g].socket = 0;
        memoria.Clienti[g].punti = 0;
        memoria.Clienti[g].online = 0;
    };

    // Creazione dei thread Partita e SCore
    pthread_create(&tempPar, NULL, threadPart, &memoria);

    
    //Accept
    while(1){
    f = cercoPosto(&memoria); // cerca posto libero
    if (f == -1) {
            printf("Nessun posto libero disponibile\n");
            sleep(1); // Attende prima di riprovare
            continue;
        }

    memoria.Clienti[f].socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    memoria.Clienti[f].memoria = &memoria;
    memoria.Clienti[f].online = 0;
    pthread_create(&client[f], NULL, threadCl, &memoria.Clienti[f]);
    sleep(1);
printf("-------------------------Fine while nuovi thread\n");
    };


    // Chiusura del socket
    for (f = 0; f < MAX; f++){
        SYSC(retvalue, close(client[f]), "nella close");
    };

    SYSC(retvalue, close(server_fd), "nella close");

}


void generaMatrice(int s, memCondivisa *mem) {
    int i, j;
    if (mem->partita.rand){
        s = time(0);
    }
    srand(s);

    if (mem->partita.file == NULL) {
        // Genero la matrice
        for (i = 0; i < N; i++) {
            for (j = 0; j < N; j++) {
                mem->partita.matrice[i][j] = (rand() % 26) + 'A';
            }
        }
    } else {
        FILE *file = fopen(mem->partita.file, "r");
        generaMf(file, mem->partita.matrice);
        fclose(file);
    }
}


void generaMf(FILE *fp, int matr[N][N]){
    int i = 0, j = 0;
    while( 1){
        int n = fgetc(fp);
        if (n == EOF || n == '\n') { 
            break;
        }

        if (n != ' ') { 
            matr[i][j] = n;
             j++;
            if (j == N) { 
                j = 0;
                i++;
                if (i == N) { 
                    break;
                }
            }
        }
    }
}

int cercoPosto(memCondivisa *mem){
    int i = 0;
    while(1){
        if(strcmp(mem->Clienti[i].nome, "") == 0 || mem->Clienti[i].socket == 0){
            return i;
        }
        i++;

        if (i >= MAX){
            i = 0;
        };
        sleep(1);
    };
}

int confronta_punti(const void *a, const void *b) {
    Cliente *giocatoreA = *(Cliente **)a;
    Cliente *giocatoreB = *(Cliente **)b;
    return (giocatoreB->punti - giocatoreA->punti);
}

int registra(Cliente *giocatore, char *name){
    int i;

    while(1){
        if (strcmp(name, "fine") == 0){
            return 0;
        }
        
        for (i = 0; i < MAX; i++){
            if (strcmp(giocatore->memoria->Clienti[i].nome, name) == 0){
                printf("Gia esiste\n");
                char c = MSG_ERR;
                write(giocatore->socket, &c, sizeof(c));

                int length;
                read(giocatore->socket, &length, sizeof(length));
                char *new_name = (char *)calloc(length + 1, sizeof(char));
                read(giocatore->socket, new_name, length);
                name = new_name;
                break;

            } else if(i == (MAX -1)){
                return 1;
            }
        }
    }

    return 0;
}

bool cercaParola(int matrice[N][N], const char* parola, int r, int c, int pos, bool visitato[N][N]) {
    if (r < 0 || r >= N || c < 0 || c >= N || visitato[r][c]) {
        return false;
    }

    char currentChar = matrice[r][c];
    if (parola[pos] == 'Q' && (pos + 1) < strlen(parola) && parola[pos + 1] == 'u') {
        if (currentChar != 'Q') {
            return false;
        }
        pos += 2;
    } else {
        if (currentChar != parola[pos]) {
            return false;
        }
        pos++;
    }

    if (pos == strlen(parola)) {
        return true;
    }

    visitato[r][c] = true;

    if (cercaParola(matrice, parola, r - 1, c, pos, visitato) ||
        cercaParola(matrice, parola, r + 1, c, pos, visitato) ||
        cercaParola(matrice, parola, r, c - 1, pos, visitato) ||
        cercaParola(matrice, parola, r, c + 1, pos, visitato)) {
        return true;
    }

    visitato[r][c] = false;

    return false;
}

void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 'a' - 'A';
        }
    }
}

int parolaNelDiz(const char *parola) {
    char *str = calloc(strlen(parola) + 1, sizeof(char));
    strcpy(str, parola);

    FILE *file = fopen("dictionary_ita.txt", "r");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        free(str);
        return 0;
    }

    to_lowercase(str);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        to_lowercase(line);
        if (strcmp(line, str) == 0) {
            fclose(file);
            free(str);
            return 1;
        }
    }
    fclose(file);
    free(str);
    return 0;
}

bool esisteParola(int matrice[N][N], const char* parola) {
    if (strlen(parola) > N * N) {
        return false;
    }

    bool visitato[N][N];
    memset(visitato, 0, sizeof(visitato));

    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (matrice[r][c] == parola[0] && cercaParola(matrice, parola, r, c, 0, visitato)) {
                return true;
            }
        }
    }

    return false;
}

void loginUtente(Cliente *cliente) {
    int i, length;
    char *data;
    char type;

    while(1) {
        if (read(cliente->socket, &length, sizeof(length)) != sizeof(length)) {
            perror("Failed to read message length");
            return;
        }

        data = (char *)calloc(length + 1, sizeof(char));
        read(cliente->socket, data, length);
        if (data == NULL) {
            perror("Failed to allocate memory for message data");
            return;
        }
        printf("Login data: |%s|\n", data);

        if (strcmp(data, "registrati") == 0) {
            printf("Sono qui\n");
            free(data);
            return;
        }

        printf("Login data: %s\n", data);
        for (i = 0; i < MAX; i++) {
            if (strcmp(cliente->memoria->Clienti[i].nome, data) == 0) {
                if (cliente->memoria->Clienti[i].online == 1) {
                    const char *errorMsg = "Giocatore già online";
                    int errorMsgLength = strlen(errorMsg);
                    type = MSG_ERR;
                    write(cliente->socket, &type, sizeof(type));
                    write(cliente->socket, &errorMsgLength, sizeof(errorMsgLength)); 
                    write(cliente->socket, errorMsg, errorMsgLength);
                } else {
                    type = MSG_OK;
                    write(cliente->socket, &type, sizeof(type));
                    cliente->memoria->Clienti[i].socket = cliente->socket;
                    cliente->memoria->Clienti[i].online = 1;
                    free(data);
                    return;
                }
            }
        }

        const char *errorMsg = "Utente non trovato";
        int errorMsgLength = strlen(errorMsg);

        type = MSG_ERR;
        write(cliente->socket, &type, sizeof(type));
        write(cliente->socket, &errorMsgLength, sizeof(errorMsgLength));
        write(cliente->socket, errorMsg, errorMsgLength);

        free(data);
    }
}

void inizio(Cliente *cliente) {
    int length = N * N * sizeof(int);
    char type;
    write(cliente->socket, &length, sizeof(length));
    write(cliente->socket, cliente->memoria->partita.matrice, length);
    if (cliente->memoria->partita.t_finePartita <= 0) {
        type = MSG_TEMPO_ATTESA;
        int tempoPartita = cliente->memoria->partita.t_nextPartita;
        length = sizeof(tempoPartita);
        write(cliente->socket, &type, sizeof(type));
        write(cliente->socket, &length, sizeof(length));
        write(cliente->socket, &tempoPartita, sizeof(cliente->memoria->partita.t_nextPartita));
    } else {
        type = MSG_TEMPO_PARTITA;
        write(cliente->socket, &type, sizeof(type));
        int tempoPartita = cliente->memoria->partita.t_finePartita;
        length = sizeof(tempoPartita);
        write(cliente->socket, &length, sizeof(length));
        write(cliente->socket, &tempoPartita, length);
    }
}


int gestionePausa(Cliente *cliente) {

printf("Nella pausa funzione\n");
    int length;
    char type;
    int tempo_attesa = cliente->memoria->partita.t_nextPartita;

    while (1) {
        cliente->ultimo_accesso = time(NULL);

        length = sizeof(tempo_attesa);
        write(cliente->socket, &length, sizeof(length));
        write(cliente->socket, &cliente->memoria->partita.t_nextPartita, sizeof(cliente->memoria->partita.t_nextPartita));
        
        tempo_attesa = cliente->memoria->partita.t_nextPartita;
        if (tempo_attesa < 0) {
            printf("Pausa finita\n");
            return 0;
        }

        read(cliente->socket, &type, sizeof(type));
        switch (type) {
            case MSG_CANCELLA_UTENTE:
                    cliente->nome = NULL;
                    cliente->punti = 0;
                    cliente->online = 0;
                    cliente->socket = 0;

                close(cliente->socket);
                return 1;
            case MSG_EXIT:
                return 1;
            case MSG_REGISTRA_UTENTE:
                read(cliente->socket, &length, sizeof(length));
                char *data = (char*)calloc(length, sizeof(char));
                read(cliente->socket, data, length);
                registra(cliente, data);
                free(data);
                break;
            case MSG_MATRICE:
                length = sizeof(cliente->memoria->partita.t_nextPartita);
                write(cliente->socket, &length, sizeof(length));
                write(cliente->socket, &cliente->memoria->partita.t_nextPartita, sizeof(cliente->memoria->partita.t_nextPartita));
                break;
            default:
                printf("Comando non riconosciuto %c\n", type);
                break;
        }
    }
printf("Finita la pausa\n");
}

// sistemeare la pausa
// makefile  OK forse
// file dei log
// sistemare lo scorer cosa manda
// rileggere il file del progetto 
