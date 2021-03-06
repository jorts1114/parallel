#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "PQ.h"
#include <pthread.h>
#include <sys/timeb.h>
#include <sys/random.h>

#define P 10007 // medium sized prime, used in hash
#define K 10 // size of hash table thread block
#define NLOCKS 101 // ceil(P/K)


/* -------------------------------- */
/* ----------- List ops ----------- */
/* -------------------------------- */

typedef struct node {
    void *data; //generic data
    struct node *next; //successor node
} node;

node* node_create(void *data, node *next) {
    node* newnode = (node*)malloc(sizeof(node));
    newnode->data = data;
    newnode->next = next;
    return newnode;
}

node* node_push(node* head, void *data) {
    node* newnode = node_create(data, head);
    head = newnode;
    return head;
}

node* node_append(node* head, void *data) {
    node* current = head;
    while (current->next != NULL)
        current = current->next;
    node* newnode = node_create(data, NULL);
    current->next = newnode;
    return head;
}

node* node_search(node* head, void *data) {
    node* current = head;
    while (current != NULL) {
        if (current->data == data)
            return current;
        current = current->next;
    }
    return NULL;
}

void node_dispose(node* head) {
    node *current, *temp;
    if (head != NULL) {
        current = head->next;
        head->next = NULL;
        while (current != NULL) {
            temp = current->next;
            free(current);
            current = temp;
        }
    }
}

/* -------------------------------- */
/* ---------- Hash table ---------- */
/* -------------------------------- */

typedef struct entry {
    unsigned int key;
    node* values;
} entry;

typedef struct hashtable { // of size P
    int num_entries;
    pthread_rwlock_t locks[NLOCKS];
    //pthread_mutex_t locks[NLOCKS];
    entry** table;
} hashtable;

// init table with keys=0 and no values
hashtable* create_table() {
    entry** tab = malloc(P*sizeof(entry*));
    hashtable* newtable = malloc(sizeof(hashtable));
    newtable->table = tab;
    for (int i=0; i<P; i++) {
        newtable->table[i]->key = 0;
        newtable->table[i]->values = NULL;
    }
    for (int j=0; j<NLOCKS; j++) {
        //pthread_mutex_init(&newtable->locks[j], NULL);
        pthread_rwlock_init(&newtable->locks[j], NULL);
    }
    return newtable;
}

void free_table(hashtable* h) {
    for (int i=0; i<P; i++) {
        node_dispose(h->table[i]->values);
    }
    for (int j=0; j<NLOCKS; j++) {
        //pthread_mutex_destroy(&h->locks[j]);
        pthread_rwlock_destroy(&h->locks[j]);
    }
    free(h->table);
    free(h);
}

// TODO: Come up with something intelligent
unsigned int hash_state(int a[4][4]) {
    unsigned int key = 1;
    int i;
    for (i=0; i<4; i++) {
        key *= a[0][i];
    }
    return key % P;
}

void add_to_table(hashtable* h, int a[4][4]) {
    unsigned int key = hash_state(a);
    int tn = (int) floor((key/P)*NLOCKS);
    pthread_rwlock_wrlock(&h->locks[tn]);
    //pthread_mutex_lock(&h->locks[tn]);
    h->table[key]->key = key;
    node_append(h->table[key]->values, a);
    h->num_entries++;
    pthread_rwlock_unlock(&h->locks[tn]);
    //pthread_mutex_unlock(&h->locks[tn]);
}

int check_table(hashtable* h, int a[4][4]) {
    unsigned int key = hash_state(a);
    int tn = (int) floor((key/P)*NLOCKS);
    pthread_rwlock_rdlock(&h->locks[tn]);
    //pthread_mutex_lock(&h->locks[tn]);
    node* res = node_search(h->table[key]->values, a);
    pthread_rwlock_unlock(&h->locks[tn]);
    //pthread_mutex_unlock(&h->locks[tn]);
    if (res)
        return 1;
    return 0;
}

int** rand_state() {
    // for testing purposes, we need only care about
    // the rop row
    int a[4][4];
    srand(time(NULL));
    for (int i=0; i<4; i++) {
        a[0][i] = rand() % 20; // this doesn't matter
    }
    return a;
}

void driver1() {
    hashtable* ht = create_table();
    time_t now = time(NULL);
    for (int i=0; i<10000; i++) {
        add_to_table(ht, rand_state());
    }
    int n = time(NULL) - now;
    printf("time: %d\n", n);
}



/* -------------------------------- */
/* ---------- Puzzle game --------- */
/* -------------------------------- */

//the finished game. 0 represents the empty tile
const int done[4][4] = {
        {1,   2,   3,  4},
        {5,   6,   7,  8},
        {9,  10,  11, 12},
        {13, 14,  15,  0}};

int is_done(int s[4][4]) {
    int i, j;
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            if (s[i][j] != done[i][j])
                return 0; // not done
        }
    }
    return 1; // done
}

int** copyarray(int s[4][4]) {
    int i, j, temp[4][4];
    for (i=0; i<4; i++) {
        for (j = 0; j < 4; j++) {
            temp[i][j] = s[i][j];
        }
    }
    return (int **)temp;
}

//movement controls: designate motion of the empty tile

int** left(int s[4][4]) {
    int i, j, temp, **ret = copyarray(s);
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            if (ret[i][j] == 0 && j != 0) {
                temp = ret[i][j-1];
                ret[i][j-1] = 0;
                ret[i][j] = temp;
                return ret;
            }
        }
    }
}

int** right(int s[4][4]) {
    int i, j, temp, **ret = copyarray(s);
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            if (ret[i][j] == 0 && j != 3) {
                temp = ret[i][j+1];
                ret[i][j+1] = 0;
                ret[i][j] = temp;
                return ret;
            }
        }
    }
}

int** up(int s[4][4]) {
    int i, j, temp, **ret = copyarray(s);
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            if (ret[i][j] == 0 && i != 0) {
                temp = ret[i-1][j];
                ret[i-1][j] = 0;
                ret[i][j] = temp;
                return ret;
            }
        }
    }
}

int** down(int s[4][4]) {
    int i, j, temp, **ret = copyarray(s);
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            if (ret[i][j] == 0 && i != 3) {
                temp = ret[i+1][j];
                ret[i+1][j] = 0;
                ret[i][j] = temp;
                return ret;
            }
        }
    }
}

typedef struct coord {
    int x;
    int y;
} coord;

coord locate(int a, int s[4][4]) {
    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
            if (s[i][j] == a) {
                coord res;
                res.x = i;
                res.y = j;
                return res;
            }}
        }
}

int manhattan(int s[4][4]) {
    int res = 0;
    for (int a=0; a<16; a++) {
        coord c = locate(a, s);
        coord r = locate(a, done);
        res += abs(c.x - r.x) + abs(c.y - r.y);
    }
    return res;
}

bool compare(int (*s)[4][4], int (*t)[4][4]) {
    int sdist = manhattan(*s);
    int tdist = manhattan(*t);
    if (sdist < tdist)
        return true;
    else
        return false;
}

typedef struct instance {
    int isdone;
    int** s;
    hashtable* table;
    pq_t queue;
} instance;

instance create_instance(int s[4][4]) {
    instance it;
    it.isdone = 0;
    it.s = s;
    it.table = create_table();
    it.queue = pq_new_queue(0, &compare, NULL);
    return it;
}

int subsolve(instance it) {
    add_to_table(it.table, it.s);
    pq_enqueue(it.queue, it.s, NULL);
    if (it.isdone > 0 || is_done(it.s)) {
        it.isdone += 1;
        pthread_exit(NULL);
    }
    else
        while (it.isdone<1) {
            pthread_create(NULL, NULL, pq_enqueue(it.queue, left(it.s), NULL), NULL);
            pthread_create(NULL, NULL, pq_enqueue(it.queue, right(it.s), NULL), NULL);
            pthread_create(NULL, NULL, pq_enqueue(it.queue, down(it.s), NULL), NULL);
            pthread_create(NULL, NULL, pq_enqueue(it.queue, up(it.s), NULL), NULL);
        }

}

int solve(int s[4][4]) {
    instance it = create_instance(s);
    subsolve(it);
}



void solve_2(int s[4][4]) {
    instance it = create_instance(s);
    int finished = 0;
    while (!finished) {
        pthread_create()
    }
}

int main() {
    hashtable* ht = create_table();
    return 0;
}