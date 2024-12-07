#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "server_utils.h"

User* user_list = NULL;
Room* room_list = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE];
    char username[50];
    sprintf(username, "guest%d", socket);
    add_user(socket, username);

    snprintf(buffer, BUFFER_SIZE, "Welcome, %s! Type 'exit' to quit.\n", username);
    send(socket, buffer, strlen(buffer), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(socket, buffer, BUFFER_SIZE, 0);

        if (bytes_read <= 0 || strcmp(buffer, "exit\n") == 0) {
            remove_user(socket);
            close(socket);
            break;
        }

        handle_command(socket, buffer);
    }

    return NULL;
}

void handle_command(int socket, char* buffer) {
    if (strncmp(buffer, "login", 5) == 0) {
        char new_username[50];
        sscanf(buffer, "login %s", new_username);
        update_username(socket, new_username);
    } else if (strncmp(buffer, "create", 6) == 0) {
        char room_name[50];
        sscanf(buffer, "create %s", room_name);
        add_room(room_name);
    } else if (strncmp(buffer, "join", 4) == 0) {
        char room_name[50];
        sscanf(buffer, "join %s", room_name);
        join_room(socket, room_name);
    } else if (strncmp(buffer, "rooms", 5) == 0) {
        list_rooms(socket);
    } else if (strncmp(buffer, "users", 5) == 0) {
        list_users(socket);
    } else {
        send_message_to_room("Lobby", buffer, socket);
    }
}

// Other helper functions here...
