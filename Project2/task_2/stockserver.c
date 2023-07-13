/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
}item;

typedef struct node{
    item contents;
    struct node* left;
    struct node* right;
}node;

node* root;
int byte_cnt = 0;
int client_cnt = 0;
int readcnt = 0;
sem_t mutex, glob;
char client_print[50000];
void echo(int connfd);

void print_stock(node* cur)
{
    if(cur == NULL) return;
    char tmp[50];
    sprintf(tmp, "%d %d %d\n", (cur->contents).ID, (cur->contents).left_stock, (cur->contents).price);
    strcat(client_print, tmp);
    print_stock(cur->left);
    print_stock(cur->right);
}
node* search_stock(node* cur, int ID)
{
    if(cur == NULL){
        strcat(client_print, "No such stock ID\n");
        return cur;
    }
    if((cur->contents).ID < ID) cur = search_stock(cur->right, ID);
    else if((cur->contents).ID > ID) cur = search_stock(cur->left, ID);
    return cur;
}
void buy_stock(int ID, int num)
{
    node* buyed_stock = search_stock(root, ID);
    if(buyed_stock == NULL) return;
    if((buyed_stock->contents).left_stock < num){
        strcat(client_print, "Not enough left stock\n");
    }
    else{
        P(&((buyed_stock->contents).mutex));
        (buyed_stock->contents).left_stock -= num;
        V(&((buyed_stock->contents).mutex));

        strcat(client_print, "[buy] success\n");
    }
}
void sell_stock(int ID, int num)
{
    node* selled_stock = search_stock(root, ID);
    if(selled_stock == NULL) return;
    P(&((selled_stock->contents).mutex));
    (selled_stock->contents).left_stock += num;
    V(&((selled_stock->contents).mutex));

    strcat(client_print, "[sell] success\n");
}

node* insert_to_tree(node* cur, item inserted)
{
    if(cur == NULL){
        cur = (node*)malloc(sizeof(node));
        cur->contents = inserted;
        cur->left = NULL;
        cur->right = NULL;
        return cur;
    }
    else{
        if(inserted.ID > (cur->contents).ID){
            cur->right = insert_to_tree(cur->right, inserted);
        }
        else if(inserted.ID < (cur->contents).ID){
            cur->left = insert_to_tree(cur->left, inserted);
        }
    }
    return cur;
}
void init_stock()
{
    root = (node*)malloc(sizeof(node));
    FILE *fp;
    fp = fopen("stock.txt", "r");
    item cur_item;
    int first = 1;
    while(fscanf(fp, "%d %d %d", &cur_item.ID, &cur_item.left_stock, &cur_item.price) != EOF){
        cur_item.readcnt = 0;
        Sem_init(&cur_item.mutex, 0, 1);
        if(first){
            root->contents = cur_item;
            first = 0;
        }
        root = insert_to_tree(root, cur_item);
    }
    fclose(fp);
}
void write_stock(FILE* fp, node* cur)
{
    if(cur == NULL) return;
    fprintf(fp, "%d %d %d\n", (cur->contents).ID, (cur->contents).left_stock, (cur->contents).price);
    write_stock(fp, cur->left);
    write_stock(fp, cur->right);
}
void save_stock()
{
    FILE* fp;
    fp = fopen("stock.txt", "w");
    write_stock(fp, root);
    fclose(fp);
}
void check_thread(char* buf, int connfd)
{
    if(!strcmp(buf, "show\n")){
        P(&mutex);
        readcnt++;
        if(readcnt == 1) P(&glob);
        V(&mutex);
        
        strcat(client_print, buf);
        print_stock(root);

        P(&mutex);
        readcnt--;
        if(readcnt == 0) V(&glob);
        V(&mutex);

        Rio_writen(connfd, client_print, MAXLINE);
        memset(client_print, 0, sizeof(client_print));
        
    }
    else if(!strcmp(buf, "exit\n")){
        strcat(client_print, buf);
        Rio_writen(connfd, client_print, MAXLINE);
        memset(client_print, 0, sizeof(client_print));
        return;
    }
    else{
        char command[20];
        int ID;
        int num;

        sscanf(buf, "%s %d %d", command, &ID, &num);
        if(!strcmp(command, "buy")) {
            P(&glob);
            strcat(client_print, buf);
            buy_stock(ID, num);
            V(&glob);
        }
        else if(!strcmp(command, "sell")){
            P(&glob);
            strcat(client_print, buf);
            sell_stock(ID, num);
            V(&glob);
        } 
        else {
            strcat(client_print, buf);
            strcat(client_print, "Wrong command, try again.\n");
        }
        Rio_writen(connfd, client_print, MAXLINE);
        memset(client_print, 0, sizeof(client_print));
    }
}
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    int n;
    Pthread_detach(pthread_self());
    Free(vargp);

    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        P(&glob);
        byte_cnt += n;
        printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
        V(&glob);

        
        check_thread(buf, connfd);
    }
    Close(connfd);
    save_stock();
    P(&glob);
    client_cnt--;
    V(&glob);
    return NULL;
}
int main(int argc, char **argv) 
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    pthread_t tid;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    Sem_init(&mutex, 0, 1);
    Sem_init(&glob, 0, 1);

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }
    init_stock();
    listenfd = Open_listenfd(argv[1]);
    
    while (1) {

        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                 client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        P(&glob);
        client_cnt++;
        V(&glob);
        Pthread_create(&tid, NULL, thread, connfdp);
        if(!client_cnt) save_stock();
        save_stock();
    }
    save_stock();
    exit(0);
}
/* $end echoserverimain */
