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
    char name[MAXNAME + 1];
    int pits[NPITS + 1]; // pits[0..NPITS-1] are the regular pits
    // pits[NPITS] is the end pit
    //other stuff undoubtedly needed here
    int waiting_username;
    // Used for buffered reading of name
    int inbuf;
    int room;
    char *after;
    struct player *next;
};
struct player *playerlist = NULL;


extern void parseargs(int argc, char **argv);
extern void makelistener();
extern int compute_average_pebbles();
extern int game_is_over();  /* boolean */
extern void broadcast(char *s);  /* you need to write this one */
extern ssize_t Write(int fd, const void *buf, size_t count);
extern ssize_t Read(int fd, void *buf, size_t count);
extern struct player *accept_connection();
extern int validate_name(const char *name);
extern int find_network_newline(const char *buf, int n);
extern int find_newline(const char *buf, int n);
extern int read_username(struct player *p);
extern int remove_player(struct player *p);
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
            if (p->fd > nfds) {
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
            struct player *p = accept_connection();
            if (playerlist == NULL) {
                playerlist = p;
                current_player = p;
            } else {
                p->next = playerlist;
                playerlist = p;
            }
        }

        for (struct player *p = playerlist; p; p = p->next) {
            // Try to read in username
            if (p->waiting_username == 1) {
                int ret = read_username(p);
                if (ret == 0 && p->waiting_username == 0) {
                    printf("%s has joined the game\n", p->name);
                } else if (ret == 1) {
                    char error_msg[] = "Name is already used or it is longer than 78 characters!\r\n";
                    Write(p->fd, error_msg, sizeof(error_msg));
                    printf("Invalid name, a player has failed to join the game\n");
                    close(p->fd);
                    if (playerlist == p) {
                        playerlist = p->next;
                    } else {
                        int failed = remove_player(p);
                        if (failed){
                            printf("Removing player from playerlist failed\n");
                        }
                    }

                    if (current_player == p) {
                        current_player = p->next;
                    }
                    free(p);
                }
            } else if (p->waiting_username == 0) {
                // Read in
                // Process if current turn
                //
            } else {
                printf("SOMETHING WENT WRONG PANIC!\n");
            }
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
 * Wrapper with error checking for read
 */
ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t ret;
    if ((ret = read(fd, buf, count)) == -1) {
        perror("read");
        exit(1);
    }
    return ret;
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

void broadcast(char *s) {
    for (struct player *p = playerlist; p; p = p->next) {
        Write(p->fd, s, sizeof(s));
    }
}


struct player *accept_connection() {
    int client_fd = accept(listenfd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(listenfd);
        exit(1);
    }

    struct player *p = malloc(sizeof(struct player));
    p->fd = client_fd;
    // Waiting for a username
    p->waiting_username = 1;
    p->inbuf = 0;
    p->room = sizeof(p->name);
    p->after = p->name;
    p->next = NULL;
    int avg_pebbles = compute_average_pebbles();
    // populate pits
    int i;
    for (i = 0; i < (NPITS - 1); i++) {
        (p->pits)[i] = avg_pebbles;
    }
    // Last pit is end pit (initialized to zero)
    (p->pits)[NPITS - 1] = 0;
    char greeting[] = "Welcome to Mancala. What is your name?\r\n";
    Write(p->fd, greeting, sizeof(greeting));
    return p;
}

int remove_player(struct player *p) {
    // Remove the player from the middle of the list
    struct player *prev_p = NULL;
    for (struct player *x = playerlist; x; x = x->next) {
        if (x == p) {
            prev_p->next = x->next;
            return 0;
        }
        prev_p = x;
    }
    return 1;
}

int read_username(struct player *p) {
    int nbytes = Read(p->fd, p->after, p->room);
    if (nbytes <= 0) {
        return 1;
    }
    // Update inbuf
    p->inbuf = p->inbuf + nbytes;
    int network_newline = find_network_newline(p->name, p->inbuf);
    int newline = find_newline(p->name, p->inbuf);
    if (network_newline > 0) {
        p->waiting_username = 0;
        (p->name)[network_newline - 2] = '\0';
        (p->name)[network_newline - 1] = '\0';
        return validate_name(p->name);
    } else if (newline > 0) {
        p->waiting_username = 0;
        (p->name)[newline - 1] = '\0';
        return validate_name(p->name);
    } else if (p->room == 0) {
        // The name is too long
        return 1;
    } else {
        p->room = sizeof(p->name) - p->inbuf;
        p->after = &((p->name)[p->inbuf]);
    }
    return 0;
}

/*
 * Returns 0 if no other player has the given name, else 1 if any other player has the
 * same name.
 */
int validate_name(const char *name) {
    int count = 0;
    for (struct player *p = playerlist; p; p = p->next) {
        if (strcmp(name, p->name) == 0) {
            count ++;
            if (count == 2) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n) {
    int i;
    for (i = 1; i < n; i ++) {
        if (buf[i - 1] == '\r' && buf[i] == '\n') {
            return i + 1;
        }
    }
    return -1;
}

/*
 * Search the first n characters of buf for a newline (\n).
 * Return one plus the index of the '\n' of the first newline,
 * or -1 if no newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_newline(const char *buf, int n) {
    int i;
    for (i = 0; i < n; i ++) {
        if (buf[i] == '\n') {
            return i + 1;
        }
    }
    return -1;
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
