#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <time.h>
#include <sys/wait.h> 
#include <signal.h>


#define BOARD_SIZE 10
#define NUM_PLAYERS_MIN 2
#define NUM_PLAYERS_MAX 10
#define NUM_SHIPS 5
#define SHIP_SIZES {2,2,3,3,4}

// Structure for a ship
typedef struct {
    int size;
    int hits;
    bool vertical;
    int components[4][2];
} Ship;

// Structure for a player
typedef struct {
    pid_t player_pid;
    Ship ships[NUM_SHIPS];
    bool is_alive;
    bool attacked_positions[BOARD_SIZE][BOARD_SIZE];
    bool attacked_ships[BOARD_SIZE][BOARD_SIZE];
} Player;

// Structure for shared memory
typedef struct {
    bool is_attacked[BOARD_SIZE][BOARD_SIZE];
    Player players[NUM_PLAYERS_MAX];
    int remaining_players;
} SharedData;

void attack_player(SharedData *shared_data, int attacker_index, int attack_x, int attack_y);
void print_ship_positions(Player *player);
bool check_overlap(Player *player, int ship_size, int x, int y, bool vertical);

void game_manager(SharedData *shared_data, int num_players) {
    int winner_pid = -1;
    while (shared_data->remaining_players > 1) {
        for (int i = 0; i < num_players; i++) {
            Player *current_player = &shared_data->players[i];

            if (!current_player->is_alive) {
                continue; // Skip eliminated players
            }

            // Check if the player has lost all ships
            bool has_lost_all_ships = true;
            for (int j = 0; j < NUM_SHIPS; j++) {
                if (current_player->ships[j].size != 0) {
                    has_lost_all_ships = false;
                    break;
                }
            }

            if (has_lost_all_ships) {
                printf("\033[0;31mPlayer %d has lost all ships and is eliminated!\033[0m\n", current_player->player_pid);
                current_player->is_alive = false;
                shared_data->remaining_players--;
                kill(current_player->player_pid, SIGKILL); // Terminate the loser's process
                continue; // Move to the next player
            }

            int attack_x, attack_y;
            do {
                attack_x = rand() % BOARD_SIZE;
                attack_y = rand() % BOARD_SIZE;
            } while (current_player->attacked_positions[attack_x][attack_y]);
            printf("Player %d is attacking position (%d, %d)\n", current_player->player_pid, attack_x, attack_y);
            current_player->attacked_positions[attack_x][attack_y] = true;

            for (int j = 0; j < num_players; j++) {
                if (j != i && shared_data->players[j].is_alive) {
                    if (!shared_data->players[j].attacked_ships[attack_x][attack_y]) {
                        attack_player(shared_data, j, attack_x, attack_y);
                        shared_data->players[j].attacked_ships[attack_x][attack_y] = true;
                    }
                }
            }
        }
    }

    // Find the winner after the game ends
    for (int k = 0; k < num_players; k++) {
        if (shared_data->players[k].is_alive) {
            winner_pid = shared_data->players[k].player_pid;
            break;
        }
    }

    if (winner_pid != -1) {
        printf("\033[0;32mPlayer %d wins!\033[0m\n", winner_pid);
        printf("Game Over\n");
        exit(EXIT_SUCCESS);
    }
}




int main() {
    srand(time(NULL));
    int num_players; 
    do {
        printf("Input the number of players (between %d and %d): ", NUM_PLAYERS_MIN, NUM_PLAYERS_MAX);
        scanf("%d", &num_players);
    } while (num_players < NUM_PLAYERS_MIN || num_players > NUM_PLAYERS_MAX);
    
    printf("Number of players: %d\n", num_players);

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to create shared memory segment");
        exit(EXIT_FAILURE);
    }

    SharedData *shared_data = (SharedData *)shmat(shmid, NULL, 0);
    if (shared_data == (SharedData *)(-1)) {
        perror("Failed to attach shared memory segment");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            shared_data->is_attacked[i][j] = false;
        }
    }

    shared_data->remaining_players = num_players;

    for (int i = 0; i < num_players; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Failed to fork player process");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            srand(time(NULL) ^ getpid()); // Seed the random number generator with process ID
            Player *current_player = &shared_data->players[i];
            current_player->player_pid = getpid();
            current_player->is_alive = true;

            // Initialize attacked positions and attacked ships matrices
            for (int row = 0; row < BOARD_SIZE; row++) {
                for (int col = 0; col < BOARD_SIZE; col++) {
                    current_player->attacked_positions[row][col] = false;
                    current_player->attacked_ships[row][col] = false;
                }
            }

            int ship_sizes[NUM_SHIPS] = SHIP_SIZES;
            for (int j = 0; j < NUM_SHIPS; j++) {
                bool vertical = rand() % 2 == 0;
                int ship_size = ship_sizes[j];
                int x, y;

                do {
                    if (vertical) {
                        x = rand() % (BOARD_SIZE - ship_size + 1);
                        y = rand() % BOARD_SIZE;
                    } else {
                        x = rand() % BOARD_SIZE;
                        y = rand() % (BOARD_SIZE - ship_size + 1);
                    }
                } while (check_overlap(current_player, ship_size, x, y, vertical));

                current_player->ships[j].size = ship_size;
                current_player->ships[j].hits = 0;
                current_player->ships[j].vertical = vertical;

                for (int k = 0; k < ship_size; k++) {
                    if (vertical) {
                        current_player->ships[j].components[k][0] = x + k;
                        current_player->ships[j].components[k][1] = y;
                    } else {
                        current_player->ships[j].components[k][0] = x;
                        current_player->ships[j].components[k][1] = y + k;
                    }
                }
            }
            print_ship_positions(current_player);
            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < num_players; i++) {
        wait(NULL);
    }

    // Game manager process
    game_manager(shared_data, num_players);
for (int i = 0; i < num_players; i++) {
        wait(NULL);
    }
    // Cleanup shared memory
    if (shmdt(shared_data) == -1) {
        perror("Failed to detach shared memory segment");
        exit(EXIT_FAILURE);
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("Failed to delete shared memory segment");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void attack_player(SharedData *shared_data, int attacker_index, int attack_x, int attack_y) {
    Player *target_player = &shared_data->players[attacker_index];

    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship *ship = &target_player->ships[i];
        if (ship->size == 0) continue;

        for (int j = 0; j < ship->size; j++) {
            if (attack_x == ship->components[j][0] && attack_y == ship->components[j][1]) {
                ship->hits++;
                printf("Player %d's ship hit!\n", target_player->player_pid);
                if (ship->hits == ship->size) {
                    ship->size = 0;
                    printf("\033[0;33mPlayer %d's ship sunk!\033[0m\n", target_player->player_pid);
                    
                    // Count remaining ships for the target player
                    int remaining_ships = 0;
                    for (int k = 0; k < NUM_SHIPS; k++) {
                        if (target_player->ships[k].size != 0) {
                            remaining_ships++;
                        }
                    }
                    if (remaining_ships > 0) {
                        printf("Player %d has %d ship(s) left.\n", target_player->player_pid, remaining_ships);
                    }
                }
                return;
            }
        }
    }
}

void print_ship_positions(Player *player) {
    printf("Player %d's ship positions:\n", player->player_pid);
    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship *ship = &player->ships[i];
        printf("Ship %d, size %d, [", i + 1, ship->size);
        for (int j = 0; j < ship->size; j++) {
            printf("[%d,%d]", ship->components[j][0], ship->components[j][1]);
            if (j < ship->size - 1) {
                printf(",");
            }
        }
        printf("]\n");
    }
    printf("\n");
}

bool check_overlap(Player *player, int ship_size, int x, int y, bool vertical) {
    for (int i = 0; i < NUM_SHIPS; i++) {
        Ship *ship = &player->ships[i];
        if (ship->size == 0) continue;

        for (int j = 0; j < ship->size; j++) {
            int ship_x = ship->components[j][0];
            int ship_y = ship->components[j][1];
            
            if (vertical) {
                if (x <= ship_x && ship_x < x + ship_size && y == ship_y) {
                    return true;
                }
            } else {
                if (x == ship_x && y <= ship_y && ship_y < y + ship_size) {
                    return true;
                }
            }
        }
    }
    return false;
}
