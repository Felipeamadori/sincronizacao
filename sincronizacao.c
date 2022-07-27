#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>



int rodadas = 0, maxPedidos = 0, barAberto = 1, atendidos = 0, clientesBar = 0, semClientes = 0, novaRodada = 0;
sem_t* flagPedido;
sem_t acessoPedidos, acessoProntos, acessoCont, garDisp, qtdPedido;

//srand(time);


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
    
    sem_wait(&acessoCont);
    atendidos++;
    sem_post(&acessoCont);
    
}

void esperarPedido(int id, int r){
    // esperar o garcom dizer que o pedido do cliente X esta pronto
    printf("Rodada %d: Cliente (%d) vai esperar por seu pedido\n", r, id);
    sem_wait(&flagPedido[id]);
}

void recebePedido(int id, int r){
    // retirar pedido da lista de pedidos prontos
    while(getPrimeiro(pedidosProntos) != id); // busy waiting
    sem_wait(&acessoProntos);
    int ped = retiraFila(pedidosProntos);
    sem_post(&acessoProntos);

    printf("Rodada %d: Cliente (%d) recebeu seu pedido\n", r, ped);

}


void consomePedido(int id, int r){
    // demora entre 1 a 5 segundos para consumir a bebida
    printf("Rodada %d: Cliente (%d) comecou a consumir sua bebida\n", r, id);
    sleep((rand() % 5) + 1);
    printf("Rodada %d: Cliente (%d) terminou de consumir sua bebida\n", r, id);
}

int anotaPedido(int *p, int cont, int idGar){
    
    sem_wait(&acessoPedidos);
    p[cont] = retiraFila(listaPedidos);
    sem_post(&acessoPedidos);
    printf("Garcom %d atendeu o cliente %d\n", idGar, p[cont]);

    return cont++;
}

void registraPedido(int* p){
    
    sem_wait(&acessoProntos);
    for (int i = 0; i < maxPedidos; i++){
       adicionaFila(pedidosProntos, p[i]);
    }
    sem_post(&acessoProntos);    
    
}

void entregaPedido(int* p){
    int a;
    for (int i = 0; i < maxPedidos; i++){
        a = p[i];
        sem_post(&flagPedido[a]);
    }
    
}


void* cliente(void* args){
    int idCliente = *(int*) args;
    int clienteRodada = 0;
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
            printf("Rodada %d: Cliente (%d) nao ira fazer pedido\n", clienteRodada, idCliente);
            sem_wait(&acessoCont);
            atendidos++;
            sem_post(&acessoCont);
        }

        clienteRodada++;
        printf("Cliente (%d) esperando nova rodada\n", idCliente);
        while(novaRodada); 


        if(clienteRodada >= rodadas) barAberto = 0;    
    }

    sem_post(&qtdPedido);
    free(args);

    return (void*) 1;

}

void* garcom(void* args){
    int idGar = *(int*) args;
    int meusPedido[maxPedidos];
    int cont = 0;
   
    while (!semClientes){
        printf("Garcom %d indo atender clientes\n", idGar);
        sem_wait(&qtdPedido);
        if (!filaVazia(listaPedidos)){
            cont = anotaPedido(meusPedido, cont, idGar);
            if(cont < maxPedidos){ 
                registraPedido(meusPedido);
                entregaPedido(meusPedido);
                cont = 0; 
            }

            if(atendidos == clientesBar){
                sem_wait(&acessoCont); 
                atendidos = 0;
                sem_post(&acessoCont);
                novaRodada = 1;
            }
        }    
   }

   free(args);

   return (void*) 1;
}

int main(int argc, char const *argv[])
{
    int nCliente = 0 , nGarcom = 0, nMaxpedidos = 0, nrodadas = 0;
    printf("Digite a quantidade de clientes, garcom, limite por garcom e rodadas gratuitas:\n");
    scanf("%d %d %d %d", &nCliente, &nGarcom, &nMaxpedidos, &nrodadas);

    if(nCliente == 0){
        semClientes = 1;
    }

    listaPedidos = criaFila();
    pedidosProntos = criaFila();
    rodadas = nrodadas;
    maxPedidos = nMaxpedidos;
    clientesBar = nCliente;
    
    sem_init(&garDisp, 0, nGarcom);
    sem_init(&acessoPedidos, 0, 1);
    sem_init(&acessoProntos, 0, 1);
    sem_init(&qtdPedido, 0, 0);
    sem_init(&acessoCont, 0, 1);

    flagPedido = (sem_t*) malloc(sizeof(sem_t)*nCliente);
    for (int i = 0; i < nCliente; i++){
        sem_init(&flagPedido[i],0,0);
    }  
    

    pthread_t clientes[nCliente];
    pthread_t garcons[nGarcom];


    for (int i = 0; i < nCliente; i++){
        int *aux = malloc(sizeof(int));
        *aux = i;
        if(pthread_create(&clientes[i], NULL, &cliente, aux) != 0){
            perror("Falha ao criar thread");
        }
    }

    for (int i = 0; i < nGarcom; i++){
        int *aux = malloc(sizeof(int));
        *aux = i;
        if(pthread_create(&garcons[i], NULL, &garcom, aux) != 0){
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
    sem_destroy(&acessoCont);

    for (int i = 0; i < nGarcom; i++){
        if(pthread_join(garcons[i], NULL) != 0){
            perror("Falha join thread");
        }
    }
    
    
    filaLibera(listaPedidos);
    filaLibera(pedidosProntos);

   
    
    
    for (int i = 0; i < nCliente; i++) {
        sem_destroy(&flagPedido[i]);
    }


    return 0;
}
