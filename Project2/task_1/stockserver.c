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

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;
node* root;
int byte_cnt = 0;
int client_cnt = 0;
char client_print[50000];
void echo(int connfd);

void init_pool(int listenfd, pool *p)
{
    /* Initially, there are no connected descriptors*/
    int i;
    p->maxi = -1;
    for(i = 0; i < FD_SETSIZE; i++) p->clientfd[i] = -1;

    /* Initially, listenfd is only member of select read set*/
    client_cnt = 0;
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}
void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for(i = 0; i < FD_SETSIZE; i++){
        if(p->clientfd[i] < 0){
            client_cnt++;
            /*Add connected descriptor to the pool*/
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            
            /*Add the descriptor to descriptor set*/
            FD_SET(connfd, &p->read_set);

            /*Update max descriptor and pool high water mark*/
            if(connfd > p->maxfd) p->maxfd = connfd;
            if(i > p->maxi) p->maxi = i;
            break;
        }
    }

    /*Couldn't find and empty slot*/
    if(i == FD_SETSIZE) app_error("add_client error: Too many clients");
}
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
        (buyed_stock->contents).left_stock -= num;
        strcat(client_print, "[buy] success\n");
    }
}
void sell_stock(int ID, int num)
{
    node* selled_stock = search_stock(root, ID);
    (selled_stock->contents).left_stock += num;
    strcat(client_print, "[sell] success\n");
}
void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;
    
    for(i= 0; (i <= p->maxi) && (p->nready > 0); i++){
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        
        /*If the descriptor is ready, echo a text line from it*/
        if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                strcat(client_print, buf);
                if(!strcmp(buf, "show\n")) print_stock(root);
                else if(!strcmp(buf, "exit\n")){
                    Rio_writen(connfd, client_print, MAXLINE);
                    client_cnt--;
                    memset(client_print, 0, sizeof(client_print));
                    return;
                }
                else{
                    char command[20];
                    int ID;
                    int num;

                    sscanf(buf, "%s %d %d", command, &ID, &num);
                    if(!strcmp(command, "buy")) buy_stock(ID, num);
                    else if(!strcmp(command, "sell")) sell_stock(ID, num);
                    else strcat(client_print, "Wrong command, try again.\n");
                }
                Rio_writen(connfd, client_print, MAXLINE);
                memset(client_print, 0, sizeof(client_print));
            }

            /*EOF detected, remove descriptor from pool */
            else{
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                client_cnt--;
            }
        }
    }
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
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }
    init_stock();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    

    while (1) {
        /*Wait for listening/connected descriptor(s) to become ready*/
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        /*If listening descriptor ready, add new client to pool*/
        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }
	    
        check_clients(&pool);

        if(!client_cnt) save_stock();
    }
    exit(0);
}
/* $end echoserverimain */
