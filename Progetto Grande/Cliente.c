#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "macros.h"
#include <time.h>

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
#define MSG_AIUTO 'A'
#define MSG_EXIT 'I'

#define SIZE 1024
#define N 4

void registrazione(int socket_fd, char *nome);
void loginUtente(int socket_fd, char *nome);
void stampaComandi();
int verificoParola(char *str);
void stampaMatrice(int matrice[N][N]);
int gestionePausa(int matrice[N][N], int tempo_partita, int tempo_attesa, int client_fd);

int main(int argc, char *argv[]){
    if (argc < 3) {
        fprintf(stderr, "Parametri insufficienti, minimo 2 parametri\n");
        exit(EXIT_FAILURE);
    }

    char *indirizzo = argv[1];
    int PORT = atoi(argv[2]);
    char *buffer = (char*)calloc(SIZE, sizeof(char));
    char *risposta;
    char comando[50];
    char data[50];
    char type;
    int length, punti, flag = 1;
    int matrice[N][N];
    int tempo_partita, tempo_attesa;

    int client_fd, retvalue;
    struct sockaddr_in server_addr;
    struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Forza l'uso di IPv4
    hints.ai_socktype = SOCK_STREAM; // Forza l'uso di TCP

    // Risolve l'indirizzo IP e assegna alla struttura server_addr
    if ((retvalue = getaddrinfo(indirizzo, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retvalue));
        exit(EXIT_FAILURE);
    }
    
    // Preparazione della struttura hints
    struct sockaddr_in *addr_in = (struct sockaddr_in *)res->ai_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr = addr_in->sin_addr;

    freeaddrinfo(res);


    client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) {
        perror("Errore nella creazione del socket");
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Errore nella connessione al server");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    while(1){
        // Invio dei commandi diponibili
        write(STDOUT_FILENO, "Comandi disponibili:\n", 21);
        write(STDOUT_FILENO, "1. -> registra_utente nome_utente\n", 35);
        write(STDOUT_FILENO, "2. -> login_utente nome_utente\n", 32);
        write(STDOUT_FILENO, "3. -> fine\n", 12);
        

        while (1) {
            read(STDIN_FILENO, buffer, SIZE);

            // Rimuovi il carattere newline
            buffer[strcspn(buffer, "\n")] = '\0';

            char *token = strtok(buffer, " ");

            if (token != NULL) {
                strncpy(comando, token, sizeof(comando) - 1); // Copia il token nella variabile comando
                comando[sizeof(comando) - 1] = '\0';

                if (strcmp(comando, "fine") != 0) {
                    token = strtok(NULL, " ");

                    if (token != NULL) {
                        strncpy(data, token, sizeof(data) - 1);
                        data[sizeof(data) - 1] = '\0';
                        break;
                    } else {
                        printf("Dato non trovato, riprova.\n");
                    }
                } else {
                    printf("Arrivederci!!\n");
                    type = MSG_EXIT;
                    write(client_fd, &type, sizeof(type));
                    close(client_fd);
                    exit(EXIT_SUCCESS);
                }
            } else {
                printf("Comando non trovato, riprova.\n");
            }
        }

        if (strcmp(comando, "registra_utente") == 0) {
            printf("Sono qui!! Perche?\n");
            registrazione(client_fd, data);
            break;
        } else if (strcmp(comando, "login_utente") == 0) {
            loginUtente(client_fd, data);
            break;
        } else if (strcmp(comando, "fine") == 0) {
            printf("Arrivederci!!\n");
            type = MSG_EXIT;
            write(client_fd, &type, sizeof(type));
            close(client_fd);
            return 0;
        }
        
    }

    stampaComandi();
    
    printf("\n");
    printf("Matrice:\n");
    read(client_fd, &length, sizeof(length));
    read(client_fd, matrice, length);
    stampaMatrice(matrice);
    printf("\n");
    read(client_fd, &type, sizeof(type));
    if(type == MSG_TEMPO_PARTITA){
        read(client_fd, &length, sizeof(length));
        read(client_fd, &tempo_partita, length);
        printf("Tempo rimasto per la fine della partita: %d\n", tempo_partita);
    }else if(type == MSG_TEMPO_ATTESA){
        read(client_fd, &length, sizeof(length));
        read(client_fd, &tempo_attesa, length);
        printf("Tempo rimasto per la fine della pausa: %d\n", tempo_attesa);
    }else{
        printf("C'è qualche problema");
    }


    while (1) {
        read(STDIN_FILENO, buffer, SIZE);

        // Controllo se la partita è finita
        type = MSG_TEMPO_PARTITA;
        write(client_fd, &type, sizeof(type));
        read(client_fd, &type, sizeof(type));

        if (type == MSG_OK){
            read(client_fd, &length, sizeof(length));
            read(client_fd, &tempo_partita, length);
            flag = 1;

        }else if(type == MSG_ERR){
            read(client_fd, &length, sizeof(length));

            if (risposta != NULL) {
                free(risposta);
            }

            risposta = (char*)calloc(length , sizeof(char));
            read(client_fd, risposta, length);
            printf("%s\n", risposta);
            if (flag){
                 //gestione scorer
                // Avvio Scorer
                flag = 0;
            }

            if(!gestionePausa(matrice, tempo_partita, tempo_attesa, client_fd)){
                
                free(buffer);
                free(risposta);
                close(client_fd);
                exit(EXIT_SUCCESS);
                    
            }
            free(risposta);

        }
        
    printf("Il mio socket: %d\n", client_fd);
        
        buffer[strcspn(buffer, "\n")] = '\0';
        char *token = strtok(buffer, " ");

        if (token != NULL) {
            strncpy(comando, token, sizeof(comando) - 1); // Copia il token nella variabile comando
            comando[sizeof(comando) - 1] = '\0';

            if (strcmp(comando, "aiuto") == 0) { // Aiuto
                stampaComandi();

            } else if (strcmp(comando, "matrice") == 0) { //Matrice
                type = MSG_MATRICE;

            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
            }
            read(client_fd, &type, sizeof(type));

            if (type == MSG_OK){

                if (read(client_fd, &length, sizeof(length)) == -1) {
                    perror("Errore lettura lunghezza matrice");
                    continue;
                }
                if (read(client_fd, matrice, length) == -1) {
                    perror("Errore lettura matrice");
                    continue;
                }
                stampaMatrice(matrice);
            }else if (type == MSG_ERR){

            }

        } else if (strcmp(comando, "tempo_partita") == 0) { // Tempo Partita
            type = MSG_TEMPO_PARTITA;
            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
            }
            read(client_fd, &type, sizeof(type));

            if (type == MSG_OK){
                read(client_fd, &length, sizeof(length));
                read(client_fd, &tempo_partita, length);
                printf("Tempo partita: %d\n", tempo_partita);
                flag = 1;

            }else if(type == MSG_ERR){
                read(client_fd, &length, sizeof(length));
                
                if (risposta != NULL) {
                    free(risposta);
                }

                risposta = (char*)calloc(length , sizeof(char));
                read(client_fd, risposta, length);
                printf("%s\n", risposta);
                if (flag){
                    //gestione scorer/pausa
                    gestionePausa(matrice, tempo_partita, tempo_attesa, client_fd);
                    flag = 0;
                }
            }
            
            printf("Tempo fine partita: %d\n", tempo_partita);

        } else if (strcmp(comando, "p") == 0) {
        printf("Parola presa\n");
            type = MSG_PAROLA;

            token = strtok(NULL, " ");
            if (token != NULL) {
                strncpy(data, token, sizeof(data) - 1);
                data[sizeof(data) - 1] = '\0';

                if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
                }
            printf("Parola: |%s|\n", token);

                length = strlen(data);

                if (write(client_fd, &length, sizeof(length)) == -1) {
                    printf("Errore invio lunghezza");
                    continue;
                }
        printf("Length inviata: %d\n", length);

                if (write(client_fd, data, length) == -1) {
                    printf("Errore invio parola");
                    continue;
                }
        printf("PArola inviata:|%s|\n", data);

                if (read(client_fd, &type, sizeof(type)) == -1) {
                    perror("Errore lettura risposta");
                    continue;
                }
                    printf("Tipo ricevuto: |%c|\n", type);

                if (type == MSG_OK) {
                    printf("Parola giusta, + %d punti\n", length);
                } else if (type == MSG_ERR) {
                    if (read(client_fd, &length, sizeof(length)) == -1) {
                        perror("Errore lettura buffer 1");
                        continue;
                    }
                    if (risposta != NULL) {
                        free(risposta);
                    }

                    risposta = (char *)calloc(length, sizeof(char));
                    if (read(client_fd, risposta, length) == -1) {
                        perror("Errore lettura buffer 2");
                        continue;
                    }
                    risposta[length - 1] = '\0';
                    printf("%s\n", risposta);
                }
            } else {
                printf("Dato non trovato, Riprova.\n");
            }

        } else if (strcmp(comando, "punti_finali") == 0) { //Punti Finali
            type = MSG_PUNTI_FINALI;
            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
            }
            if (read(client_fd, &length, sizeof(length)) == -1) {
                perror("Errore lettura lunghezza punteggio");
                continue;
            }

            if (read(client_fd, &punti, length) == -1) {
                perror("Errore lettura punteggio finale");
                continue;
            }
            printf("Punteggio finale: %d\n", punti);

        } else if (strcmp(comando, "cancella_registrazione") == 0) { // Cancella registrazione
            type = MSG_CANCELLA_UTENTE;
            printf("Arrivederci!!\n");

            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
            }

            close(client_fd);
            exit(EXIT_SUCCESS);

        } else if (strcmp(comando, "post_bacheca") == 0) {
            type = MSG_POST_BACHECA;
            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
            }

        } else if (strcmp(comando, "show_bacheca") == 0) { // Bacheca
            type = MSG_SHOW_BACHECA;
            if (write(client_fd, &type, sizeof(type)) == -1) {
                perror("Errore invio tipo messaggio");
                continue;
            }
            read(client_fd, &length, sizeof(length));
            risposta = (char*)calloc(length, sizeof(char));

            read(client_fd, buffer, sizeof(buffer));
               // "Errore lettura bacheca");
            
            printf("Bacheca:\n%s\n", buffer);


            } else if (strcmp(comando, "fine") == 0) {
                printf("Arrivederci!!\n");
                type = MSG_EXIT;
                write(client_fd, &type, sizeof(type));
                close(client_fd);
                exit(EXIT_SUCCESS);

            } else {
                printf("Comando non riconosciuto, Riprova.\n");
                stampaComandi();
            }
        }

        

    }


    // Chiusura del socket
    free(buffer);
    close(client_fd);
    return 0;
}


void registrazione(int socket_fd, char *nome){
    char type = MSG_REGISTRA_UTENTE;
    int length;
    char response;
    char buffer[256];

    // invio solo una volta il tipo del messaggio
    if (write(socket_fd, &type, sizeof(type)) == -1) {
            perror("Errore invio tipo messaggio");
            exit(EXIT_FAILURE);
        }

    printf("Nella Registrazione\n");

    while (1) {
        length = strlen(nome);

        // Invio del nome

        if (write(socket_fd, &length, sizeof(length)) == -1) {
            perror("Errore invio lunghezza nome");
            exit(EXIT_FAILURE);
        }

        if (write(socket_fd, nome, length) == -1) {
            perror("Errore invio nome");
            exit(EXIT_FAILURE);
        }

        // Leggo la risposta dal server
        if (read(socket_fd, &response, sizeof(response)) == -1) {
            perror("Errore lettura risposta");
            exit(EXIT_FAILURE);
        }

        if (response == MSG_OK) {
            printf("Registrazione avvenuta con successo.\n");
            printf("\n");
            break;
        } else if (response == MSG_ERR) {
            printf("Nome non disponibile, Riprovare.\n");
            printf("Mettere solo nome:\n");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';  // Rimuove il carattere di nuova riga
            strcpy(nome, buffer);
        }
        
    }
    return;
}

void loginUtente(int socket_fd, char *nome){
    char type = MSG_LOGIN_UTENTE;
    int length = strlen(nome);

    if (write(socket_fd, &type, sizeof(type)) == -1) {
            perror("Errore invio tipo messaggio");
            return;
        };

    while(1){
        
        if (write(socket_fd, &length, sizeof(length)) == -1) {
            perror("Errore invio lunghezza nome");
            return;
        }
        if (write(socket_fd, nome, length) == -1) {
            perror("Errore invio nome");
            return;
        }

        if (read(socket_fd, &type, sizeof(type)) == -1) {
            perror("Errore lettura risposta");
            return;
        }
        
        if (type == MSG_OK ) {
            printf("Login effetuato con successo.\n");
            return;

        } else if (type == MSG_ERR) {
            read(socket_fd, &length, sizeof(length));
            char *errorMsg = (char *)calloc(length, sizeof(char));
            read(socket_fd, errorMsg, length);
            printf("%s\n", errorMsg);

            free(errorMsg);
            printf("Riprovare o registerati con quel nome.\nIserisci il nome o scrivi: registrati\n");
            fgets(nome, 100, stdin);

            nome[strcspn(nome, "\n")] = '\0';
            length = strlen(nome); 

            if (strcmp(nome, "registrati") == 0) {
                write(socket_fd, &length, sizeof(length));
                write(socket_fd, nome, length);
                printf("Va nella registrazione\n");
                registrazione(socket_fd, nome);
                return;
            }
            length = strlen(nome);
        }
    }
printf("Finito Login\n");
}

void stampaComandi(){
    printf("Comandi disponibili:\n");
    printf("1. -> aiuto\n");
    printf("2. -> matrice\n");
    printf("3. -> tempo_partita\n");
    printf("4. -> p parola_indicata\n");
    printf("5. -> punti_finali\n");
    printf("6. -> cancella_registrazione\n");
    printf("7. -> msg testo_messaggio -> per scrivere sulla bacheca\n");
    printf("8. -> show-msg -> per visualizzare la bacheca\n");
    printf("9. -> fine\n");
}

void stampaMatrice(int matrice[N][N]){
    int i, j;
    for (i=0; i<N; i++){
        for(j = 0; j < N; j++){
            if (matrice[i][j] == 81){
                //printf("Qu");
                fprintf(stdout, "Qu ");
            }else{
                //printf("%c", matrice[i][j]);
                fprintf(stdout, "%c ", matrice[i][j]);
            };
            //printf(" ");
        }
        printf("\n");
    }
}

int verificoParola(char *str){ // Verifico che la parola abbia solo lettere e non altro
    for (int i = 0; str[i]; i++) {
        if (!((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z'))) {
            return 0; // Falso: contiene caratteri non alfabetici
        }
    }
    return 1; // Vero: contiene solo lettere
}

int gestionePausa(int matrice[N][N], int tempo_partita, int tempo_attesa, int client_fd){
    char type = 'J';
    int punti, length = sizeof(tempo_attesa);
    char *buffer = (char*)calloc(SIZE, sizeof(char));
    char comando[50];
    char data[50];

    printf("Comandi disponibili:\n");
    printf("1. -> aiuto -> per visualizzare \n");
    printf("2. -> matrice\n");
    printf("3. -> tempo_attesa\n");
    printf("4. -> punti_finali\n");
    printf("5. -> cancella_registrazione\n");
    printf("6. -> msg testo_messaggio -> per scrivere sulla bacheca\n");
    printf("7. -> show-msg -> per visualizzare la bacheca\n");
    printf("8. -> fine -> per uscire dal gioco\n");

    printf("\n");
    printf("Il vincitori sono:\n");
    read(client_fd, &type, sizeof(type));
/*
    while (1){
        if(type == MSG_PUNTI_FINALI){
            read(client_fd, &length, sizeof(length));

        }
    }
    */
    
    


    while(1){

        
        read(client_fd, &length, sizeof(length));
        read(client_fd, &tempo_attesa, length);
        printf("Tempo rimanente: %d\n",tempo_attesa);

        
        if (tempo_attesa < 0) {
            printf("Inizio nuova partita\n");
            printf("Matrice:\n");
            read(client_fd, &length, sizeof(length));
            read(client_fd, matrice, length);
            stampaMatrice(matrice);
            printf("\n");
            read(client_fd, &type, sizeof(type));
            if(type == MSG_TEMPO_PARTITA){
                read(client_fd, &length, sizeof(length));
                read(client_fd, &tempo_partita, length);
                printf("Tempo rimasto per la fine della partita: %d\n", tempo_partita);
            }
            return 1;
        }

        printf("Tempo rimasto fine pausa: %d\n", tempo_attesa);

        // gestione comandi solo durante la pausa

        read(STDIN_FILENO, buffer, SIZE);
        
        buffer[strcspn(buffer, "\n")] = '\0';
        char *token = strtok(buffer, " ");

        if(token != NULL){
            strncpy(comando, token, sizeof(comando) - 1); // Copia il token nella variabile comando
            comando[sizeof(comando) - 1] = '\0';

            if (strcmp(comando, "matrice") == 0){
                type = MSG_MATRICE;
                write(client_fd, &type, sizeof(type));
                read(client_fd, &length, sizeof(length));
                read(client_fd, &tempo_attesa, length);

            }else if (strcmp(comando, "tempo_attesa") == 0){
                printf("Tempo rimasto alla prossima partita %d\n", tempo_attesa);

            }else if (strcmp(comando, "punti_finali") == 0){
                type = MSG_PUNTI_FINALI;
                write(client_fd, &type, sizeof(type));
                read(client_fd, &length, sizeof(length));
                read(client_fd, &punti, length);
                printf("Punteggio finale: %d\n", punti);

            }else if(strcmp(comando, "cancella_registrazione") == 0){
                type = MSG_CANCELLA_UTENTE;
                printf("Arrivederci!!\n");
                write(client_fd, &type, sizeof(type));
                return 0;

            }else if(strcmp(comando, "msg") == 0){
                type = MSG_POST_BACHECA;
                token = strtok(NULL, " ");
                if (token != NULL){
                    strncpy(data, token, sizeof(data) - 1);
                    data[sizeof(data) - 1] = '\0';

                    write(client_fd, &type, sizeof(type));

                    length = strlen(data);
                    write(client_fd, &length, sizeof(length));
                    write(client_fd, data, length);

                }else{
                    printf("Manca il messaggio, Riprovare.\n");
                }
            }else if(strcmp(comando, "show-msg") == 0){
                // Vedere la bacheca

            }else if(strcmp(comando, "fine") == 0){
                printf("Arrivederci!!\n");
                type = MSG_EXIT;
                write(client_fd, &type, sizeof(type));
                return 0;

            }else if (strcmp(comando, "aiuto" ) == 0){
                printf("Comandi disponibili:\n");
                printf("1. -> aiuto -> per visualizzare \n");
                printf("2. -> matrice\n");
                printf("3. -> tempo_attesa\n");
                printf("4. -> punti_finali\n");
                printf("5. -> cancella_registrazione\n");
                printf("6. -> msg testo_messaggio -> per scrivere sulla bacheca\n");
                printf("7. -> show-msg -> per visualizzare la bacheca\n");
                printf("8. -> fine -> per uscire dal gioco\n");

            }else{
                printf("Comando non riconosciuto\n");
            }
        }else{
            printf("Riprova!\n");
        }
    }
}
