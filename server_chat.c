#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include "websocket.h"
#include <cjson/cJSON.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define CHAT_FILE "data/chats.json"
#define USER_FILE "data/users.json"

// Initialize JSON files
void initialize_files() {
    FILE *file = fopen(CHAT_FILE, "w");
    if (file) {
        fprintf(file, "[]"); // Initialize empty JSON array
        fclose(file);
    }
    file = fopen(USER_FILE, "w");
    if (file) {
        fprintf(file, "[]"); // Initialize empty JSON array
        fclose(file);
    }
}

// Fungsi untuk mengonversi waktu dalam format HH:MM:SS ke time_t
time_t convert_time_to_t(const char *time_str) {
    struct tm time_struct = {0};
    int hours, minutes, seconds;

    // Mem-parsing waktu dari string "HH:MM:SS"
    if (sscanf(time_str, "%d:%d:%d", &hours, &minutes, &seconds) != 3) {
        return -1; // Error parsing
    }

    // Mengisi field dalam struktur tm
    time_struct.tm_hour = hours;
    time_struct.tm_min = minutes;
    time_struct.tm_sec = seconds;

    // Mengonversi struktur tm menjadi time_t
    return mktime(&time_struct);
}

// Check if username already exists
int is_username_used(const char *username) {
    FILE *file = fopen(USER_FILE, "r");
    if (!file) return 0; // Jika file tidak ada, anggap username tidak digunakan

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char buffer[file_size + 1];
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    // Parse JSON dari file
    cJSON *user_array = cJSON_Parse(buffer);
    if (!user_array) return 0;

    // Iterasi melalui array untuk memeriksa username
    cJSON *user;
    cJSON_ArrayForEach(user, user_array) {
        const char *stored_username = cJSON_GetStringValue(cJSON_GetObjectItem(user, "username"));
        if (stored_username && strcmp(stored_username, username) == 0) {
            cJSON_Delete(user_array);
            return 1; // Username ditemukan
        }
    }

    cJSON_Delete(user_array);
    return 0; // Username tidak ditemukan
}

// Save username to JSON file
void save_username(const char *username) {
    FILE *file = fopen(USER_FILE, "r");
    cJSON *user_array;

    // Jika file tidak ada atau kosong, inisialisasi array baru
    if (!file) {
        user_array = cJSON_CreateArray();
    } else {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char buffer[file_size + 1];
        fread(buffer, 1, file_size, file);
        buffer[file_size] = '\0';
        fclose(file);

        user_array = cJSON_Parse(buffer);
        if (!user_array) {
            user_array = cJSON_CreateArray();
        }
    }

    // Tambahkan username sebagai objek ke array
    cJSON *new_user = cJSON_CreateObject();
    cJSON_AddStringToObject(new_user, "username", username);
    cJSON_AddItemToArray(user_array, new_user);

    // Tulis kembali JSON ke file
    file = fopen(USER_FILE, "w");
    if (!file) {
        cJSON_Delete(user_array);
        return;
    }

    char *json_string = cJSON_Print(user_array);
    fprintf(file, "%s", json_string);

    // Cleanup
    free(json_string);
    cJSON_Delete(user_array);
    fclose(file);
}

// Save message to JSON file
void save_message(const char *username, const char *message, const char *type) {
    FILE *file = fopen(CHAT_FILE, "r+");
    if (!file) return;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char buffer[file_size + 1];
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';

    // Get current time
    time_t raw_time;
    struct tm *time_info;
    char time_str[9]; // HH:MM:SS

    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", time_info);

    FILE *output = fopen(CHAT_FILE, "w");
    if (!output) {
        fclose(file);
        return;
    }

    if (file_size > 2) { // If JSON array is not empty
        buffer[file_size - 1] = '\0'; // Remove closing bracket
        fprintf(output, "%s,{\"username\":\"%s\",\"message\":\"%s\",\"time\":\"%s\",\"type\":\"%s\"}]", buffer, username, message, time_str, type);
    } else {
        fprintf(output, "[{\"username\":\"%s\",\"message\":\"%s\",\"time\":\"%s\",\"type\":\"%s\"}]", username, message, time_str, type);
    }

    fclose(file);
    fclose(output);
}

// Handle client connection
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE], message[BUFFER_SIZE];
    char username[BUFFER_SIZE] = {0};
    time_t join_time;
    struct tm *join_time_info;
    char time_str[9];

    // Lakukan WebSocket handshake
    if (recv(client_fd, buffer, BUFFER_SIZE, 0) <= 0 || handle_handshake(client_fd, buffer) < 0) {
        close(client_fd);
        return;
    }

    // Terima pesan koneksi awal dari klien
    recv(client_fd, buffer, BUFFER_SIZE, 0);
    websocket_decode(buffer, message);

    // Parse JSON untuk mendapatkan username
    cJSON *json = cJSON_Parse(message);
    if (!json) {
        close(client_fd);
        return;
    }

    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
    if (strcmp(type, "connect") == 0) {
        const char *received_username = cJSON_GetStringValue(cJSON_GetObjectItem(json, "username"));

        // Periksa apakah username sudah digunakan
        if (is_username_used(received_username)) {
            printf("Username %s already in use\n", received_username);

            // Kirim pesan error ke klien
            cJSON *error_response = cJSON_CreateObject();
            cJSON_AddStringToObject(error_response, "type", "error");
            cJSON_AddStringToObject(error_response, "message", "Username is already in use.");
            char *error_message = cJSON_Print(error_response);
            char frame[BUFFER_SIZE];
            int frame_len = websocket_encode(error_message, frame);
            send(client_fd, frame, frame_len, 0);

            free(error_message);
            cJSON_Delete(error_response);
            cJSON_Delete(json);

            close(client_fd);
            return;
        }

        // Simpan username
        strncpy(username, received_username, BUFFER_SIZE);
        save_username(username);

        printf("New client connected: %s\n", username);

        save_message(username, "bergabung!", "announcement");  // Menyimpan pengumuman ke file chats.json
        time(&join_time);
        join_time_info = localtime(&join_time);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", join_time_info);
    }

    cJSON_Delete(json);


    pid_t pid = fork();

    if (pid == 0) {
        // Handle client messages
        while (1) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) break;

            websocket_decode(buffer, message);

            json = cJSON_Parse(message);
            if (!json) continue;

            const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(json, "type"));
            if (strcmp(type, "message") == 0) {
                const char *username = cJSON_GetStringValue(cJSON_GetObjectItem(json, "username"));
                const char *message = cJSON_GetStringValue(cJSON_GetObjectItem(json, "message"));
                save_message(username, message, "message");
            }

            cJSON_Delete(json);
        }

        close(client_fd);
        exit(0);
    } else if (pid > 0) {
        int last_index = 0; // Indeks pesan terakhir yang sudah dikirim

        while (1) {
            FILE *file = fopen(CHAT_FILE, "r");
            if (!file) {
                perror("Failed to open chat file");
                return;
            }

            // Baca seluruh isi file
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            fseek(file, 0, SEEK_SET);

            char *file_content = malloc(file_size + 1);
            if (!file_content) {
                perror("Failed to allocate memory for file content");
                fclose(file);
                return;
            }

            fread(file_content, 1, file_size, file);
            file_content[file_size] = '\0';
            fclose(file);

            // Parse JSON array
            cJSON *json_array = cJSON_Parse(file_content);
            free(file_content);

            if (!json_array) {
                fprintf(stderr, "Failed to parse JSON\n");
                sleep(1);
                continue;
            }

            int array_size = cJSON_GetArraySize(json_array);

            // Kirim hanya pesan baru yang belum dikirimkan
            for (int i = last_index; i < array_size; i++) {
                cJSON *message_obj = cJSON_GetArrayItem(json_array, i);
                if (!message_obj) continue;

                // Ambil elemen JSON
                const char *msg_username = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "username"));
                const char *msg_message = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "message"));
                const char *msg_time = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "time"));
                const char *msg_type = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "type"));

                // Konversi waktu pesan ke time_t
                time_t message_time = convert_time_to_t(msg_time);
                time_t sjoin_time = convert_time_to_t(time_str);

                // Kirim hanya jika waktu pesan setelah waktu bergabung klien dan bukan pesan dengan usernamenya sendiri
                if(msg_username && strcmp(msg_username, username) != 0 && difftime(message_time, sjoin_time) > 0) {
                    char *message_json = cJSON_Print(message_obj);
                    if (message_json) {
                        char frame[BUFFER_SIZE];
                        int frame_len = websocket_encode(message_json, frame);
                        send(client_fd, frame, frame_len, 0);
                        free(message_json);
                    }
                }
            }

            // Update indeks pesan terakhir yang sudah dikirim
            last_index = array_size;

            cJSON_Delete(json_array);

            // Tunggu sebelum membaca file lagi
            sleep(1);
        }
    }
}


int main() {
    initialize_files();

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Signal handler to reap zombie processes
    signal(SIGCHLD, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    printf("Server is running on port %d\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (fork() == 0) {
            close(server_fd);
            handle_client(new_socket);
            exit(0);
        }
        close(new_socket);
    }

    return 0;
}
