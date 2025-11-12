// stupid simple peer to peer simulated botnet
 
/*
although this may look more like a simple P2P gossip protocol, it's
a building stone in P2P botnets.
*/
 
/*
ATTENTION!!!
Botnets are NO joke, nor should they be used illegaly.
This sample will NEVER include actual live command & control
functionality to prevent people adopting this as a template for
their illegal doings. A simple message print is ONLY included.
USE THIS LEGALLY!!!
*/
 
// I would recommend knowing some C and linked lists before jumping into this code.
 
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/ip.h>
#include<netinet/tcp.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/time.h>
#include<time.h>
#include<sys/epoll.h>
#include<ctype.h>
#include<errno.h>
#include<pthread.h>
 
#define INET_ADDR(o1, o2, o3, o4) htonl(o1 << 24 | o2 << 16 | o3 << 8 | o4)
 
/* 
    this is the config section, here you can edit bootstrap node, etc.
*/
#define CONFIG_BOOTSTRAP_ADDR INET_ADDR(127,0,0,1)
#define CONFIG_BOOTSTRAP_PORT 8080
#define CONFIG_LISTEN_ADDR INET_ADDR(127,0,0,1)
#define CONFIG_LISTEN_PORT 8080
 
uint32_t util_rand() 
{
    uint32_t state = ((uint32_t)getpid() ^ (uint32_t)getppid());
    state ^= (uint32_t)time(NULL);
    state ^= (uint32_t)clock();
    state ^= state << 17;
    state ^= state >> 5;
    state ^= state << 13;
    return state;
}
 
typedef struct Node 
{
    uint32_t addr;
    uint16_t port;
    uint32_t id;
    struct Node *next;
} node_t;
 
void list_append_node(node_t **head, uint32_t addr, uint16_t port, uint32_t id) 
{
    node_t *node = (node_t *)malloc(sizeof(node_t));
    if (node == NULL) return;
    node->addr = addr;
    node->port = port;
    node->id = id;
    node->next = NULL;
 
    if (*head == NULL) 
    {
        *head = node;
        return;
    }
 
    node_t *current = *head;
    while (current->next != NULL) 
    {
        current = current->next;
    }
 
    current->next = node;
}
 
void list_delete_node(node_t **head, uint32_t id) 
{
    node_t *current = *head;
    node_t *prev = NULL;
 
    if (current != NULL && current->id == id) 
    {
        *head = current->next;
        free(current);
        return;
    }
 
    while (current != NULL && current->id != id) 
    {
        prev = current;
        current = current->next;
    }
 
    if (current == NULL) return;
 
    prev->next = current->next;
    free(current);
}
 
void list_print_list(node_t **head) 
{
    node_t *current = *head;
    while (current != NULL) 
    {
        fprintf(stdout, "[ID: %d | IP: %d | PORT: %d]\n", current->id, current->addr, current->port);
        current = current->next;
    }
    fprintf(stdout, "NULL\n");
}
 
int peer_establish_connection(uint32_t addr, uint16_t port) 
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
 
    struct timeval timeout;
    timeout.tv_usec = 0;
    timeout.tv_sec = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(timeout)) < 0) return -1; 
 
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
 
    struct sockaddr_in peer_addr;
    peer_addr.sin_port = htons(port);
    peer_addr.sin_family = PF_INET;
    peer_addr.sin_addr.s_addr = addr;
 
    if (connect(fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) return -1;
    return fd;
}
 
void peer_listen_loop(uint32_t addr, uint16_t port) 
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
 
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) return; 
 
    struct timeval timeout;
    timeout.tv_usec = 0;
    timeout.tv_sec = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(timeout)) < 0) return; 
 
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
 
    struct sockaddr_in peer_addr;
    peer_addr.sin_port = htons(port);
    peer_addr.sin_family = PF_INET;
    peer_addr.sin_addr.s_addr = addr;
 
    if (bind(fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) return;
 
    if (listen(fd, SOMAXCONN) < 0) return;
 
    int efd = epoll_create1(0);
    if (efd < 0) return;
 
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
 
    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) < 0) return;
 
    while (1) 
    {
        struct epoll_event events[1024];
        int n = epoll_wait(efd, events, 1024, 100);
        if (n > 0) 
        {
            for (int i = 0; i < n; i++) 
            {
                if (events[i].data.fd == fd) 
                {
                    socklen_t len = sizeof(peer_addr);
                    int cfd = accept(fd, (struct sockaddr *)&peer_addr, &len);
                    if (cfd < 0) continue;
 
                    memset((struct epoll_event *)&event, 0, sizeof(event));
                    event.events = EPOLLIN;
                    event.data.fd = cfd;
 
                    if (epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event) < 0) continue;
                }
                else
                {
                    char buff[1024];
                    ssize_t n = recv(events[i].data.fd, buff, sizeof(buff) - 1, MSG_NOSIGNAL);
                    if (n < 0 || errno == EWOULDBLOCK || errno == EAGAIN) continue;
                    else if (n == 0) 
                    {
                        if (epoll_ctl(efd, EPOLL_CTL_DEL, events[i].data.fd, NULL) < 0) continue;
                    }
 
                    buff[n] = '\0';
 
                    /* 
                    in the real world, this would usually involve much more complex cryptography to ensure
                    only the bot master can propagate commands throughout the network, but since we are in
                    simple C, can't really do a lot more than this lol.
                    */
                    if (strncmp(buff, "admin", 5) == 0) 
                    {
                        // simple print for now...
                        fprintf(stdout, "%s\n", buff);
                    }
                }
            }
        }
    }
}
 
node_t *head = NULL;
 
void *peer_gossip(void *arg) 
{
    int fd = peer_establish_connection(CONFIG_BOOTSTRAP_ADDR, CONFIG_BOOTSTRAP_PORT);
    if (fd < 0) return NULL;
 
    while (1) 
    {   
        sleep(5);
 
        int op = 0x01; // simple opcode to ask bootstrap node for active peers
        ssize_t n = send(fd, (int *)&op, sizeof(op), MSG_NOSIGNAL);
        if (n < 0 || errno == EWOULDBLOCK || errno == EAGAIN) continue;
        else if (n == 0) 
        {
            close(fd);
            break;
        }
 
        char buff[1024];
        n = recv(fd, buff, sizeof(buff) - 1, MSG_NOSIGNAL);
        if (n < 0 || errno == EWOULDBLOCK || errno == EAGAIN) continue;
        else if (n == 0) 
        {
            close(fd);
            break;
        }
 
        close(fd);
 
        char *token = strtok(buff, "\n");
        while (token != NULL) 
        {
            int i = 0;
            while (token[i] != ':' && token[i] != '\0') i++;
            if (token[i] == '\0') 
            {
                token = strtok(NULL, "\n");
                continue;
            }
 
            char ip[i-1];
            for (int j = 0; j < i - 1; j++) 
            {
                ip[j] = token[j];
            }
 
            uint32_t addr;
            if (inet_pton(PF_INET, ip, &addr) <= 0) break;
 
            uint16_t port = (uint16_t)atoi(token + i + 1);
 
            fd = peer_establish_connection(addr, port);
            if (fd < 0) 
            {
                token = strtok(NULL, "\n");
                continue;
            }
            else 
            {
                close(fd);
 
                node_t *current = head;
                while (current != NULL) 
                {
                    if (current->addr == addr && current->port == port) 
                    {
                        token = strtok(NULL, "\n");
                        continue;
                    }
                    current = current->next;
                }
                list_append_node(&head, addr, port, util_rand());
            }
 
            token = strtok(NULL, "\n");
        }
 
        node_t *current = head;
        while (current != NULL) 
        {
            // simple brute force technique to get a valid id, not very efficient, but it still works :D
            // the more peers that are in the network the better
            if (current->id == util_rand()) 
            {
                fd = peer_establish_connection(current->addr, current->port);
                if (fd < 0) 
                {
                    current = current->next;
                    continue;
                }
 
                /*
                again in the real world we would rather store a message and propagate it,
                also we need to include 'admin' here for the node to accept our message,
                but since we don't the node won't print our message
                */
                n = send(fd, (const char *)"Hello, World!", 14, MSG_NOSIGNAL);
                if (n < 0 || errno == EWOULDBLOCK || errno == EAGAIN) break;
                else if (n == 0) 
                {
                    list_delete_node(&head, current->id);
                    close(fd);
                    current = current->next;
                    continue;
                }
 
                break;
            }
            current = current->next;
        }        
    }
 
    return NULL;
}
 
int main(void) 
{
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, peer_gossip, NULL) < 0) return -1;
    peer_listen_loop(CONFIG_LISTEN_ADDR, CONFIG_LISTEN_PORT);
    pthread_join(thread_id, NULL);
    return 0;
}
 
