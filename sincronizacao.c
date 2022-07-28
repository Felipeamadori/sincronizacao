#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>



int rodadas = 0, maxPedidos = 0, semClientes = 0;
sem_t* flagPedido;
sem_t acessoPedidos, acessoProntos, garDisp, qtdPedido;

pthread_barrier_t novaRodada;


struct node {
    int num;
    struct node* prox;
};
typedef struct node Node;

struct fila {
    struct node* ini;
    struct node* fim;
};
typedef struct fila Fila;

struct fila* listaPedidos = NULL;
struct fila* pedidosProntos = NULL;

Fila* criaFila(){
    Fila* f = (Fila*)malloc(sizeof(Fila));
    f->ini = f->fim = NULL;
    return f;
}

int filaVazia(Fila* f){
    return (f->ini == NULL);
}

void filaLibera(Fila* f){
    Node* n = f->ini;
    
    while (n != NULL){
        Node* temp = n->prox;
        free(n);
        n = temp;
    }

    free(f);
}

void adicionaFila(Fila* f, int k){
    
    Node* n = (Node*)malloc(sizeof(Node));
    n->num = k;
    n->prox = NULL; 
  
    if (f->fim != NULL) {
        f->fim->prox = n;
    } 
    else {
        f->ini = n;
    }
    
    f->fim = n;
}

int retiraFila(Fila* f){
    Node* temp;
    int v;
    if (filaVazia(f)){
        return -1;
    }
    
    temp = f->ini;
    v = temp->num;
    f->ini = temp->prox;

    if(f->ini == NULL){
        f->fim = NULL;
    }
    
    free(temp);
    
    return v;
}

int filaTamanho(Fila* f){
    Node* n;
    int cont = 0;
    for (n = f->ini; n != NULL; n = n->prox){
        cont++;
    }

    return cont;
}

int getPrimeiro(Fila* f){
    return f->ini->num;
}

void fazPedido(int id, int r){
    
    sem_wait(&acessoPedidos);
    adicionaFila(listaPedidos, id);
    sem_post(&acessoPedidos);
    
    sem_post(&qtdPedido);

    printf("Rodada %d: Cliente (%d) fez seu pedido\n", r, id);
    
}

void esperarPedido(int id, int r){

    printf("Rodada %d: Cliente (%d) vai esperar por seu pedido\n", r, id);
    sem_wait(&flagPedido[id]);
}

void recebePedido(int id, int r){

    while(getPrimeiro(pedidosProntos) != id); 
    sem_wait(&acessoProntos);
    int ped = retiraFila(pedidosProntos);
    sem_post(&acessoProntos);

    printf("Rodada %d: Cliente (%d) recebeu seu pedido\n", r, ped);

}


void consomePedido(int id, int r){

    printf("Rodada %d: Cliente (%d) comecou a consumir sua bebida\n", r, id);
    sleep((rand() % 5) + 1);
    printf("Rodada %d: Cliente (%d) terminou de consumir sua bebida\n", r, id);
}

void anotaPedido(int *p, int idGar){
    
    sem_wait(&acessoPedidos);   
    *p = retiraFila(listaPedidos);
    sem_post(&acessoPedidos);
    printf("Garcom (%d) atendeu o cliente (%d)\n", idGar, *p);
}

void registraPedido(int* p, int idGar){
    
    sem_wait(&acessoProntos);
    
    adicionaFila(pedidosProntos, *p);
    printf("Garçom (%d) registrou o pedido do cliente (%d)\n", idGar, *p);
    
    sem_post(&acessoProntos);    
    
}

void entregaPedido(int* p, int idGar){
    int a = *p;
    
    sem_post(&flagPedido[a]);
    printf("Garçom (%d) entregou o pedido do cliente (%d)\n", idGar, a);
    
}


void* cliente(void* args){
    int idCliente = *(int*) args;
    int clienteRodada = 0;
    int barAberto = 1;
    
    while (barAberto) {
        if(rand() % 3 != 0) {
                printf("Rodada %d: Cliente (%d) esta esperando por um garcom\n", clienteRodada, idCliente);
                sem_wait(&garDisp);
                fazPedido(idCliente, clienteRodada);
                sem_post(&garDisp);
                
                esperarPedido(idCliente, clienteRodada);
                recebePedido(idCliente, clienteRodada);
                consomePedido(idCliente, clienteRodada);
        } else {
            printf("Rodada %d: Cliente (%d) nao ira fazer pedido esta rodada\n", clienteRodada, idCliente);
        }

        printf("Cliente (%d) esperando nova rodada\n", idCliente);
        pthread_barrier_wait(&novaRodada);

        clienteRodada++;

        if(clienteRodada >= rodadas) barAberto = 0;    
    }
    
    sem_post(&qtdPedido);

    free(args);

    return (void*) 1;

}

void* garcom(void* args){
    int idGar = *(int*) args;
    int meusPedido;
   
    while (!semClientes){
        sem_wait(&qtdPedido);
        if (filaVazia(listaPedidos)){
            free(args);
            return (void*) 1;
        }

        printf("Garcom (%d) indo atender clientes\n", idGar);
        anotaPedido(&meusPedido,idGar);       
        registraPedido(&meusPedido, idGar);
        entregaPedido(&meusPedido, idGar);
    }         
    
    
   free(args);

   return (void*) 1;
}

int main(int argc, char const *argv[])
{
    int nCliente = 0 , nGarcom = 0, nMaxpedidos = 0, nrodadas = 0;
    printf("Digite a quantidade de clientes, garcom, limite por garcom e rodadas gratuitas:\n");
    scanf("%d %d %d %d", &nCliente, &nGarcom, &nMaxpedidos, &nrodadas);

    if(nCliente == 0 || nGarcom == 0 || nrodadas == 0 || nMaxpedidos == 0){
        printf("Bar sem condiçoes de abrir no momento\n");
        exit(1);
    }

    listaPedidos = criaFila();
    pedidosProntos = criaFila();
    rodadas = nrodadas;
    maxPedidos = nMaxpedidos;
    
    sem_init(&garDisp, 0, nGarcom);
    sem_init(&acessoPedidos, 0, 1);
    sem_init(&acessoProntos, 0, 1);
    sem_init(&qtdPedido, 0, 0);

    pthread_barrier_init(&novaRodada, NULL, nCliente);

    flagPedido = (sem_t*) malloc(sizeof(sem_t)*nCliente);
    for (int i = 0; i < nCliente; i++){
        sem_init(&flagPedido[i],0,0);
    }  
    
    pthread_t clientes[nCliente];
    pthread_t garcons[nGarcom];

    for (int i = 0; i < nGarcom; i++){
        int *aux = malloc(sizeof(int));
        *aux = i;
        if(pthread_create(&garcons[i], NULL, &garcom, aux) != 0){
            perror("Falha ao criar thread");
        }
    }

    for (int i = 0; i < nCliente; i++){
        int *aux = malloc(sizeof(int));
        *aux = i;
        if(pthread_create(&clientes[i], NULL, &cliente, aux) != 0){
            perror("Falha ao criar thread");
        }
    }

    
    for (int i = 0; i < nCliente; i++){
        if(pthread_join(clientes[i], NULL) != 0){
            perror("Falha join thread");
        }
    }

    printf("Todos os clientes sairam do bar\n");   
    semClientes = 1;
    

    sem_destroy(&qtdPedido);
    sem_destroy(&garDisp);
    sem_destroy(&acessoPedidos);
    sem_destroy(&acessoProntos);

    pthread_barrier_destroy(&novaRodada);

    for (int i = 0; i < nGarcom; i++){
        if(pthread_join(garcons[i], NULL) != 0){
            perror("Falha join thread");
        }
    }
    
    printf("Todos os garçons sairam do bar\n");
    
    
    filaLibera(listaPedidos);
    filaLibera(pedidosProntos);   
    
    for (int i = 0; i < nCliente; i++) {
        sem_destroy(&flagPedido[i]);
    }


    return 0;
}