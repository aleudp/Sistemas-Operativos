#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>

#define NUM_NODES 20
#define MAX_ROUTE_LENGTH NUM_NODES

// Aristas del grafo
typedef struct {
    int source;
    int destination;
    int cost;
    sem_t semaphore;
} Edge;

// Grafo
typedef struct {
    int numNodes;
    int numEdges;
    Edge *edges;
} Graph;

// ID Threads
typedef struct {
    int id;
    Graph *graph;
} Threads;

// Variables globales
int global_min_cost = INT_MAX;
int global_min_route[MAX_ROUTE_LENGTH];
int global_min_route_length = 0;
pthread_mutex_t min_cost_mutex;
volatile int stop_threads = 0;
time_t start_time;
int thread_with_min_cost = -1; 

// Función principal de encontrar la ruta con menor costo
void *find_route(void *args) {
    Threads *thread = (Threads *)args;
    Graph *graph = thread->graph;

    while (!stop_threads) {
        if (difftime(time(NULL), start_time) >= 60) {
            stop_threads = 1;
            break;
        }
        int current_node = 0;
        int finish_node = graph->numNodes - 1;
        int *route = (int *)malloc(graph->numNodes * sizeof(int));
        if (!route) {
            perror("Error allocating memory for route");
            pthread_exit(NULL);
        }
        int route_index = 0;
        int total_cost = 0;
        route[route_index++] = current_node;

        while (current_node != finish_node) {
            int *neighbors = (int *)malloc(graph->numNodes * sizeof(int));
            if (!neighbors) {
                perror("Error allocating memory for neighbors");
                free(route);
                pthread_exit(NULL);
            }
            int num_neighbors = 0;
            int next_edge_index = -1;

            for (int i = 0; i < graph->numEdges; i++) {
                if (graph->edges[i].source == current_node) {
                    neighbors[num_neighbors++] = graph->edges[i].destination;
                }
            }

            if (num_neighbors == 0) {
                free(neighbors);
                break;
            }

            int next_node_index = rand() % num_neighbors;
            int next_node = neighbors[next_node_index];

            for (int i = 0; i < graph->numEdges; i++) {
                if (graph->edges[i].source == current_node && graph->edges[i].destination == next_node) {
                    next_edge_index = i;
                    break;
                }
            }

            free(neighbors);

            if (next_edge_index == -1) {
                break;
            }

            sem_wait(&(graph->edges[next_edge_index].semaphore));
            total_cost += graph->edges[next_edge_index].cost;
            route[route_index++] = next_node;
            current_node = next_node;
            sem_post(&(graph->edges[next_edge_index].semaphore));
        }

        if (current_node == finish_node) {
            pthread_mutex_lock(&min_cost_mutex);
            if (total_cost < global_min_cost) {
                global_min_cost = total_cost;
                global_min_route_length = route_index;
                for (int i = 0; i < route_index; i++) {
                    global_min_route[i] = route[i];
                }
                thread_with_min_cost = thread->id; 
                printf("Thread %d encontró un nuevo costo mínimo: %d, Con la ruta: ", thread->id, global_min_cost);
                for (int i = 0; i < global_min_route_length; i++) {
                    printf("%d ", global_min_route[i]);
                }
                printf("\n");
            }
            pthread_mutex_unlock(&min_cost_mutex);
        }
        usleep(10000);
        free(route);
    }

    return NULL;
}

int main() {
    srand(time(NULL));

    int num_threads;
    int semaphore_limit;

    do {
        printf("Ingrese el valor N correspondiente a la cantidad de Threads (5, 10, o 20): ");
        scanf("%d", &num_threads);
    } while (num_threads != 5 && num_threads != 10 && num_threads != 20);

    do {
        printf("Ingrese el valor M correspondiente al límite de threads por arista (2 o 3): ");
        scanf("%d", &semaphore_limit);
    } while (semaphore_limit != 2 && semaphore_limit != 3);

    Edge edges[] = {
        {0, 1, 1, {0}},
        {0, 2, 2, {0}},
        {1, 3, 3, {0}},
        {1, 4, 1, {0}},
        {2, 4, 2, {0}},
        {2, 5, 3, {0}},
        {3, 6, 1, {0}},
        {3, 7, 2, {0}},
        {4, 7, 3, {0}},
        {4, 8, 1, {0}},
        {5, 8, 2, {0}},
        {5, 9, 3, {0}},
        {6, 10, 1, {0}},
        {7, 10, 2, {0}},
        {7, 11, 3, {0}},
        {8, 11, 1, {0}},
        {8, 12, 2, {0}},
        {9, 12, 3, {0}},
        {9, 13, 1, {0}},
        {10, 14, 2, {0}},
        {11, 14, 3, {0}},
        {11, 15, 1, {0}},
        {12, 15, 2, {0}},
        {12, 16, 3, {0}},
        {13, 16, 1, {0}},
        {13, 17, 2, {0}},
        {14, 18, 3, {0}},
        {15, 18, 1, {0}},
        {16, 18, 2, {0}},
        {18, 19, 3, {0}}
    };

    int num_edges = sizeof(edges) / sizeof(edges[0]);

    Graph route;
    route.numNodes = NUM_NODES;
    route.numEdges = num_edges;
    route.edges = edges;

    for (int i = 0; i < num_edges; i++) {
        if (sem_init(&(route.edges[i].semaphore), 0, semaphore_limit) != 0) {
            perror("Error in semaphore initialization");
            return 1;
        }
    }

    if (pthread_mutex_init(&min_cost_mutex, NULL) != 0) {
        perror("Error in mutex initialization");
        return 1;
    }

    pthread_t threads[num_threads];
    Threads threadInfos[num_threads];

    start_time = time(NULL);
    printf("El programa se ejecutará por 60 segundos, utilizando %d threads en total y %d threads máximo por semáforo en cada Arista de los nodos",num_threads,semaphore_limit);
    printf("\n");
    for (int i = 0; i < num_threads; i++) {
        threadInfos[i].id = i + 1;
        threadInfos[i].graph = &route;
        if (pthread_create(&threads[i], NULL, find_route, (void *)&threadInfos[i]) != 0) {
            perror("Error in thread creation");
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error in thread join");
            return 1;
        }
    }

    for (int i = 0; i < num_edges; i++) {
        if (sem_destroy(&(route.edges[i].semaphore)) != 0) {
            perror("Error in semaphore destruction");
            return 1;
        }
    }

    if (pthread_mutex_destroy(&min_cost_mutex) != 0) {
        perror("Error in mutex destruction");
        return 1;
    }
    printf("El tiempo ha finalizado");
    printf("\n");
    printf("El Thread %d encontró la ruta [", thread_with_min_cost);
    for (int i = 0; i < global_min_route_length; i++) {
        printf("%d%s", global_min_route[i], i < global_min_route_length - 1 ? " " : " ");
    }
        printf("], ");

    printf("que corresponde a la ruta con menor costo, con un valor de %d.\n", global_min_cost);

    return 0;
}
