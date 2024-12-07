#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8888
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct User {
    int socket;
    char username[50];
    struct User* next;
    char rooms[10][50]; // Max 10 rooms per user
    int room_count;
} User;

typedef struct Room {
    char name[50];
    User* users;
    struct Room* next;
} Room;

User* user_list = NULL;
Room* room_list = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_to_room(const char* room_name, const char* message, int exclude_socket);
void sigintHandler(int sig);
void* handle_client(void* client_socket);
void add_user(int socket, const char* username);
void remove_user(int socket);
void add_room(const char* room_name);
void add_user_to_room(const char* room_name, User* user);
void remove_user_from_room(const char* room_name, User* user);
void list_rooms(int client_socket);
void list_users(int client_socket);

int main() {
    int server_socket, client_socket, addr_len;
    struct sockaddr_in server_addr, client_addr;

    signal(SIGINT, sigintHandler);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat server started on port %d\n", PORT);

    while (1) {
        addr_len = sizeof(client_addr);
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, &client_socket) != 0) {
            perror("Thread creation failed");
            continue;
        }
    }

    return 0;
}

void sigintHandler(int sig) {
    printf("\nServer shutting down...\n");

    User* current_user = user_list;
    while (current_user) {
        close(current_user->socket);
        User* temp = current_user;
        current_user = current_user->next;
        free(temp);
    }

    Room* current_room = room_list;
    while (current_room) {
        Room* temp = current_room;
        current_room = current_room->next;
        free(temp);
    }

    pthread_mutex_destroy(&mutex);
    exit(0);
}

void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    char buffer[BUFFER_SIZE];
    char username[50];
    sprintf(username, "guest%d", socket);
    add_user(socket, username);

    snprintf(buffer, BUFFER_SIZE, "Welcome to the chat server, %s! Type 'exit' to quit.\n", username);
    send(socket, buffer, strlen(buffer), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(socket, buffer, BUFFER_SIZE, 0);

        if (bytes_read <= 0 || strcmp(buffer, "exit\n") == 0) {
            snprintf(buffer, BUFFER_SIZE, "%s has left the chat.\n", username);
            broadcast_to_room("Lobby", buffer, socket);
            remove_user(socket);
            close(socket);
            break;
        }

        // Handle commands
        if (strncmp(buffer, "login", 5) == 0) {
            sscanf(buffer, "login %s", username);
            pthread_mutex_lock(&mutex);
            User* user = user_list;
            while (user) {
                if (user->socket == socket) {
                    strcpy(user->username, username);
                    break;
                }
                user = user->next;
            }
            pthread_mutex_unlock(&mutex);
            snprintf(buffer, BUFFER_SIZE, "Username updated to %s.\n", username);
            send(socket, buffer, strlen(buffer), 0);
        } else if (strncmp(buffer, "create", 6) == 0) {
            char room_name[50];
            sscanf(buffer, "create %s", room_name);
            add_room(room_name);
            snprintf(buffer, BUFFER_SIZE, "Room '%s' created.\n", room_name);
            send(socket, buffer, strlen(buffer), 0);
        } else if (strncmp(buffer, "join", 4) == 0) {
            char room_name[50];
            sscanf(buffer, "join %s", room_name);
            add_user_to_room(room_name, user_list);
        } else if (strncmp(buffer, "rooms", 5) == 0) {
            list_rooms(socket);
        } else if (strncmp(buffer, "users", 5) == 0) {
            list_users(socket);
        } else {
            // Broadcast message to Lobby
            broadcast_to_room("Lobby", buffer, socket);
        }
    }

    return NULL;
}

void add_user(int socket, const char* username) {
    User* new_user = (User*)malloc(sizeof(User));
    new_user->socket = socket;
    strcpy(new_user->username, username);
    new_user->room_count = 0;
    new_user->next = NULL;

    pthread_mutex_lock(&mutex);
    new_user->next = user_list;
    user_list = new_user;
    pthread_mutex_unlock(&mutex);

    add_user_to_room("Lobby", new_user);
}

void remove_user(int socket) {
    pthread_mutex_lock(&mutex);
    User** current = &user_list;
    while (*current) {
        if ((*current)->socket == socket) {
            User* temp = *current;
            *current = (*current)->next;
            free(temp);
            break;
        }
        current = &(*current)->next;
    }
    pthread_mutex_unlock(&mutex);
}

void add_room(const char* room_name) {
    pthread_mutex_lock(&mutex);
    Room* new_room = (Room*)malloc(sizeof(Room));
    strcpy(new_room->name, room_name);
    new_room->users = NULL;
    new_room->next = room_list;
    room_list = new_room;
    pthread_mutex_unlock(&mutex);
}

void list_rooms(int client_socket) {
    pthread_mutex_lock(&mutex);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "Available rooms:\n");
    Room* current = room_list;
    while (current) {
        strcat(buffer, current->name);
        strcat(buffer, "\n");
        current = current->next;
    }
    send(client_socket, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&mutex);
}

void list_users(int client_socket) {
    pthread_mutex_lock(&mutex);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    snprintf(buffer, BUFFER_SIZE, "Connected users:\n");
    User* current = user_list;
    while (current) {
        strcat(buffer, current->username);
        strcat(buffer, "\n");
        current = current->next;
    }
    send(client_socket, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&mutex);
}

void broadcast_to_room(const char* room_name, const char* message, int exclude_socket) {
    pthread_mutex_lock(&mutex);
    Room* current_room = room_list;
    while (current_room) {
        if (strcmp(current_room->name, room_name) == 0) {
            User* current_user = current_room->users;
            while (current_user) {
                if (current_user->socket != exclude_socket) {
                    send(current_user->socket, message, strlen(message), 0);
                }
                current_user = current_user->next;
            }
            break;
        }
        current_room = current_room->next;
    }
    pthread_mutex_unlock(&mutex);
}
