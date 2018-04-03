#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXNAME 80  /* maximum permitted name size, not including \0 */
#define NPITS 6  /* number of pits on a side, not including the end pit */
#define NPEBBLES 4 /* initial number of pebbles per pit */
#define MAXMESSAGE (MAXNAME + 50) /* maximum permitted message size, not including \0 */

int port = 3000;
int listenfd;

struct player {
    int fd;
    char name[MAXNAME+1]; 
    int pits[NPITS+1];  // pits[0..NPITS-1] are the regular pits 
                        // pits[NPITS] is the end pit
    //other stuff undoubtedly needed here
    int waiting_username;
    struct player *next;
};
struct player *playerlist = NULL;


extern void parseargs(int argc, char **argv);
extern void makelistener();
extern int compute_average_pebbles();
extern int game_is_over();  /* boolean */
extern void broadcast(char *s);  /* you need to write this one */
// accept_connection
// validate_name
// buffered_read
// get_board_state
// broadcast_exclude


int main(int argc, char **argv) {
    char msg[MAXMESSAGE];

    parseargs(argc, argv);
    makelistener();

    // Store the highest number file descriptor, currently listenfd
    int nfds = listenfd;
    // Store the player whose turn it is
    struct player *current_player = NULL;

    while (!game_is_over()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        // Add in listenfd
        FD_SET(listenfd, &read_fds);
        // Add in all player fds
        for (struct player *p = playerlist; p; p = p->next) {
            FD_SET(p->fd, &read_fds);
            if (p->fd > nfds){
                nfds = p->fd;
            }
        }

        if (select(nfds + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(1);
        }

         //  Check if listenfd is ready
        if (FD_ISSET(listenfd, &read_fds)) {
           // Accept the client
        }

    }

    broadcast("Game over!\r\n");
    printf("Game over!\n");
    for (struct player *p = playerlist; p; p = p->next) {
        int points = 0;
        for (int i = 0; i <= NPITS; i++) {
            points += p->pits[i];
        }
        printf("%s has %d points\r\n", p->name, points);
        snprintf(msg, MAXMESSAGE, "%s has %d points\r\n", p->name, points);
        broadcast(msg);
    }

    return 0;
}

/*
 * Wrapper with error checking for write
 */
ssize_t Write(int fd, const void *buf, size_t count) {
    ssize_t ret;
    if ((ret = write(fd, buf, count)) == -1) {
        perror("write");
        exit(1);
    }
    return ret;
}

void broadcast(char *s){
    for (struct player *p = playerlist; p; p = p->next) {
        Write(p->fd, s, sizeof(s));
    }
}


struct player *accept_connection() {
    int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    struct player *p = malloc(sizeof(struct player));
    p->fd = client_fd;
    // Waiting for a username 
    p->waiting_username = 1;
    int avg_pebbles = compute_average_pebbles();
    // populate pits
    int i;
    for(i = 0; i < (NPITS-1); i++){
        (p->pits)[i] = avg_pebbles;
    }
    // Last pit is end pit (initialized to zero)
    (p->pits)[NPITS-1] = 0;
}



void parseargs(int argc, char **argv) {
    int c, status = 0;
    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
        case 'p':
            port = strtol(optarg, NULL, 0);  
            break;
        default:
            status++;
        }
    }
    if (status || optind != argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        exit(1);
    }
}


void makelistener() {
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int on = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
               (const char *) &on, sizeof(on)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}



/* call this BEFORE linking the new player in to the list */
int compute_average_pebbles() { 
    struct player *p;
    int i;

    if (playerlist == NULL) {
        return NPEBBLES;
    }

    int nplayers = 0, npebbles = 0;
    for (p = playerlist; p; p = p->next) {
        nplayers++;
        for (i = 0; i < NPITS; i++) {
            npebbles += p->pits[i];
        }
    }
    return ((npebbles - 1) / nplayers / NPITS + 1);  /* round up */
}


int game_is_over() { /* boolean */
    int i;

    if (!playerlist) {
       return 0;  /* we haven't even started yet! */
    }

    for (struct player *p = playerlist; p; p = p->next) {
        int is_all_empty = 1;
        for (i = 0; i < NPITS; i++) {
            if (p->pits[i]) {
                is_all_empty = 0;
            }
        }
        if (is_all_empty) {
            return 1;
        }
    }
    return 0;
}
