#ifndef WEBSOCKET_H
#define WEBSOCKET_H
#define MAX_BUFFER_SIZE 1024  // Definisikan makro di sini


// Deklarasi fungsi yang ada di websocket.c
char* base64_encode(const unsigned char *data, size_t len);
char* get_websocket_accept_key(const char* sec_websocket_key);
int websocket_encode(const char *message, char *frame);
int websocket_decode(char *frame, char *message);
int handle_handshake(int client_fd, char *buffer);

#endif
