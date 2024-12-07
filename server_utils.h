#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#define BUFFER_SIZE 1024
#define MAX_ROOMS 10

typedef struct User {
    int socket;
    char username[50];
    struct User* next;
    char rooms[MAX_ROOMS][50];
    int room_count;
} User;

typedef struct Room {
    char name[50];
    User* users;
    struct Room* next;
} Room;

void* handle_client(void* client_socket);
void handle_command(int socket, char* buffer);
void add_user(int socket, const char* username);
void remove_user(int socket);
void add_room(const char* room_name);
void join_room(int socket, const char* room_name);
void list_rooms(int client_socket);
void list_users(int client_socket);
void send_message_to_room(const char* room_name, const char* message, int exclude_socket);
void clean_up();

#endif
