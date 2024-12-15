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

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define LOCATION_FILE "data/locations.json"

int client_sockets[MAX_CLIENTS] = {0};  // Array untuk menyimpan soket klien

// Initialize JSON files
void initialize_files() {
    FILE *file = fopen(LOCATION_FILE, "w");
    if (file) {
        fprintf(file, "[]"); // Initialize empty JSON array
        fclose(file);
    }
}

// Fungsi untuk menyimpan atau memperbarui lokasi berdasarkan username
void save_location(const char *username, double lat, double lon) {
    FILE *file = fopen(LOCATION_FILE, "r");
    cJSON *json_array = NULL;

    if (file) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *file_content = malloc(file_size + 1);
        if (!file_content) {
            fclose(file);
            perror("Failed to allocate memory");
            return;
        }

        fread(file_content, 1, file_size, file);
        file_content[file_size] = '\0';
        fclose(file);

        // Parse JSON array
        json_array = cJSON_Parse(file_content);
        free(file_content);

        if (!json_array || !cJSON_IsArray(json_array)) {
            cJSON_Delete(json_array);
            json_array = cJSON_CreateArray(); // Create new array if invalid
        }
    } else {
        json_array = cJSON_CreateArray(); // Create new array if file doesn't exist
    }

    // Check if username exists in the array
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json_array) {
        cJSON *existing_username = cJSON_GetObjectItem(item, "username");
        if (existing_username && strcmp(existing_username->valuestring, username) == 0) {
            // Update existing location
            cJSON_ReplaceItemInObject(item, "lat", cJSON_CreateNumber(lat));
            cJSON_ReplaceItemInObject(item, "lon", cJSON_CreateNumber(lon));
            goto write_file;
        }
    }

    // If username doesn't exist, add a new object
    cJSON *new_location = cJSON_CreateObject();
    cJSON_AddItemToObject(new_location, "username", cJSON_CreateString(username));
    cJSON_AddItemToObject(new_location, "lat", cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(new_location, "lon", cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(json_array, new_location);

write_file:
    // Write updated JSON array to file
    file = fopen(LOCATION_FILE, "w");
    if (file) {
        char *updated_content = cJSON_PrintUnformatted(json_array);
        if (updated_content) {
            fprintf(file, "%s", updated_content);
            free(updated_content);
        }
        fclose(file);
    }

    cJSON_Delete(json_array);
}


// Handle client connection
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE], message[BUFFER_SIZE];
    char username[BUFFER_SIZE] = {0};

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

    const char *received_username = cJSON_GetStringValue(cJSON_GetObjectItem(json, "username"));
    double received_lat = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "lat"));
    double received_lon = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "lon"));

    // Simpan username
    save_location(received_username, received_lat, received_lon);
    printf("New client connected: %s\n", username);

    cJSON_Delete(json);

    // Simpan client socket
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == 0) {
            client_sockets[i] = client_fd;
            break;
        }
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Handle client messages
        while (1) {
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) break;

            websocket_decode(buffer, message);

            json = cJSON_Parse(message);
            if (!json) continue;

            const char *username = cJSON_GetStringValue(cJSON_GetObjectItem(json, "username"));
            double lat = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "lat"));
            double lon = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "lon"));

            // Simpan lokasi ke file JSON
            save_location(username, lat, lon);

            cJSON_Delete(json);
        }

        close(client_fd);
        exit(0);
    } else if (pid > 0) {
    int last_index = 0; // Indeks pesan terakhir yang sudah dikirim
    cJSON *last_json_array = NULL;  // Menyimpan data JSON array terakhir

    while (1) {
        FILE *file = fopen(LOCATION_FILE, "r");
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

        // Cek apakah ada perubahan data dibandingkan dengan data sebelumnya
        if (last_json_array == NULL || !cJSON_Compare(json_array, last_json_array, 1)) {
            // Kirim data yang berubah
            for (int i = last_index; i < array_size; i++) {
                cJSON *message_obj = cJSON_GetArrayItem(json_array, i);
                if (!message_obj) continue;

                // Ambil elemen JSON
                const char *msg_username = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "username"));
                const char *msg_lat = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "lat"));
                const char *msg_lon = cJSON_GetStringValue(cJSON_GetObjectItem(message_obj, "lon"));

                // Kirim hanya jika bukan pesan dengan usernamenya sendiri
                if (msg_username && strcmp(msg_username, username) != 0) {
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
        }

        // Simpan data JSON array untuk perbandingan pada iterasi berikutnya
        if (last_json_array) {
            cJSON_Delete(last_json_array);
        }
        last_json_array = cJSON_Duplicate(json_array, 1);  // Salin array JSON untuk perbandingan berikutnya

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
