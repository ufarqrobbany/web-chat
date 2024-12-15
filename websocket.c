#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sys/socket.h>
#include "websocket.h"


// Base64 encoding function
char* base64_encode(const unsigned char *data, size_t len) {
    size_t output_length = 4 * ((len + 2) / 3);
    char *base64_output = (char *)malloc(output_length + 1);
    if (base64_output == NULL) {
        perror("Memory allocation error");
        return NULL;
    }

    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
    int out_len = 0;
    EVP_EncodeInit(ctx);
    EVP_EncodeUpdate(ctx, (unsigned char *)base64_output, &out_len, data, len);
    EVP_EncodeFinal(ctx, (unsigned char *)base64_output + out_len, &out_len);
    base64_output[output_length] = '\0';
    EVP_ENCODE_CTX_free(ctx);

    return base64_output;
}

// Function to generate the Sec-WebSocket-Accept key
char* get_websocket_accept_key(const char* sec_websocket_key) {
    const char *magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat_key[512];
    snprintf(concat_key, sizeof(concat_key), "%s%s", sec_websocket_key, magic_key);

    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)concat_key, strlen(concat_key), sha1_hash);

    return base64_encode(sha1_hash, SHA_DIGEST_LENGTH);
}

// Function to encode WebSocket frame
int websocket_encode(const char *message, char *frame) {
    size_t message_length = strlen(message);
    size_t frame_length = 2;

    frame[0] = 0x81; // FIN = 1, opcode = 0x1 (text frame)

    if (message_length <= 125) {
        frame[1] = message_length;
    } else if (message_length <= 65535) {
        frame[1] = 126;
        frame[2] = (message_length >> 8) & 0xFF;
        frame[3] = message_length & 0xFF;
        frame_length += 2;
    } else {
        frame[1] = 127;
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (message_length >> (56 - i * 8)) & 0xFF;
        }
        frame_length += 8;
    }

    memcpy(frame + frame_length, message, message_length);
    frame_length += message_length;

    return frame_length;
}

// Function to decode WebSocket frame
int websocket_decode(char *frame, char *message) {
    unsigned char *payload = (unsigned char*) frame;
    size_t payload_length = payload[1] & 0x7F;
    int masking_key_offset = 2;

    if (payload_length == 126) {
        payload_length = (payload[2] << 8) | payload[3];
        masking_key_offset = 4;
    } else if (payload_length == 127) {
        payload_length = 0;
        for (int i = 0; i < 8; i++) {
            payload_length = (payload_length << 8) | payload[2 + i];
        }
        masking_key_offset = 10;
    }

    unsigned char *masking_key = payload + masking_key_offset;
    unsigned char *payload_data = payload + masking_key_offset + 4;

    for (size_t i = 0; i < payload_length; i++) {
        message[i] = payload_data[i] ^ masking_key[i % 4];
    }
    message[payload_length] = '\0';

    return payload_length;
}

// Function to perform WebSocket handshake
int handle_handshake(int client_fd, char *buffer) {
    char *sec_websocket_key = NULL;
    char *header_line = strtok(buffer, "\r\n");
    while (header_line != NULL) {
        if (strncmp(header_line, "Sec-WebSocket-Key:", 18) == 0) {
            sec_websocket_key = header_line + 19;
            break;
        }
        header_line = strtok(NULL, "\r\n");
    }

    if (sec_websocket_key) {
        char *accept_key = get_websocket_accept_key(sec_websocket_key);
        char response[MAX_BUFFER_SIZE];
        snprintf(response, sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n", accept_key);

        if (send(client_fd, response, strlen(response), 0) < 0) {
            perror("Failed to send handshake response");
            free(accept_key);
            return -1;
        }
        free(accept_key);
        return 0;
    }

    printf("Invalid handshake request\n");
    return -1;
}
