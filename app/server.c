#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    char *method;
    char *resource;
    char *http_version;
    char *headers;
    char *body;
} HttpRequest;

char* parseResource(char *str);
int exists(char *target,char *elements[], int size);
char* findHeader(char *target, char* headers);
void* handleConnection(void*);
HttpRequest* split_request(const char *request);
void free_http_request_parts(HttpRequest *parts);


int g_argc;
char **g_argv;

int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	 int server_fd, client_addr_len;
	 struct sockaddr_in client_addr;
	
	 server_fd = socket(AF_INET, SOCK_STREAM, 0);
	 if (server_fd == -1) {
	 	printf("Socket creation failed: %s...\n", strerror(errno));
	 	return 1;
	 }
	
	 int reuse = 1;
	 if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
	 	printf("SO_REUSEADDR failed: %s \n", strerror(errno));
	 	return 1;
	 }
	
	 struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
	 								 .sin_port = htons(4221),
	 								 .sin_addr = { htonl(INADDR_ANY) },
	 								};
	
	 if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	 	printf("Bind failed: %s \n", strerror(errno));
	 	return 1;
	 }
	
	 int connection_backlog = 10;
	 if (listen(server_fd, connection_backlog) != 0) {
	 	printf("Listen failed: %s \n", strerror(errno));
	 	return 1;
	 }

    printf("Waiting for a client to connect...\n");
    while (1) {
        client_addr_len = sizeof(client_addr);

        int client_socket = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_socket == -1){
            printf("Accept failed: %s \n", strerror(errno));
        }
        int *pclient = malloc(sizeof(int));
        pthread_t t;
        *pclient = client_socket; 
        pthread_create(&t,  NULL, handleConnection, pclient);
   
    } 
    return 0;
}



char* findHeader(char *target, char* req){
 char *line = strtok(req, "\r\n"); 
    while (line != NULL) {
        char *colonPos = strchr(line, ':');
        if (colonPos != NULL) {
            *colonPos = '\0';
            char *headerKey = line;
            char *headerValue = colonPos + 1;

            while (*headerValue == ' ') headerValue++;

            if (strcmp(target, headerKey) == 0){
                return headerValue;
            }
        }
        line = strtok(NULL, "\r\n");
    }
    return NULL;
}



void* handleConnection(void* c_socket){

    int client = *((int*)c_socket);
    free(c_socket);

    char buffer [1024];
    int bytes_read = read(client, buffer, sizeof(buffer)-1);
    if (bytes_read < 0) {
        perror("Read failed");
        close(client);
        return NULL;
    }
    HttpRequest *req = split_request(buffer);
    printf("Client connected\n");
    char res[1024];
        char *resType = NULL;
        char *resName = NULL;
    if (strcmp(req->resource, "/") != 0){
        resType = strdup(req->resource+1);
        if (resType == NULL){

        resType = req->resource;
        } else {

        char *diff = strstr(resType, "/");
        resName = strdup(diff+1); 
        *diff = '\0';
        }
    } else {
        resType = req->resource;
    }
    printf("%s %s\n", resType, resName);

    if (strcmp(req->method, "GET") == 0){

        if (strcmp(resType, "echo") == 0){
            sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(resName), resName);
        } else if (strcmp(resType, "user-agent") == 0){
            char *value = findHeader("User-Agent",req->headers);
            printf("%s\n", value);
            if (value != NULL){
                printf("val: %s\n", value);
                sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(value), value);
            }
        } else if (strcmp(resType, "files") == 0){

            if (g_argc < 3){
                printf("file path not provided\n");
                return NULL;
            }   
            char *fPath = g_argv[2];
            strcat(fPath, resName); 
            FILE *file = fopen(fPath, "r");
            if (!file) {
                sprintf(res, "HTTP/1.1 404 Not Found\r\n\r\n");
                if (send(client, res, strlen(res), 0) < 0) {
                    perror("Send failed");
                }
                free_http_request_parts(req);
                free(resName);
                free(resType);
                close(client);
                return NULL;
            }      
            fseek(file, 0, SEEK_END);
            int file_size = ftell(file);
            rewind(file);

            char *data = (char*) malloc(file_size + 1);

            fread(data, 1, file_size, file);
            data[file_size] = '\0'; 
            printf("%s\n", data);
            sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %lu\r\n\r\n%s", strlen(data), data);
            free(data);
            fclose(file);
        } else if (strcmp(resType, "/") == 0){
            sprintf(res, "HTTP/1.1 200 OK\r\n\r\n");
        } else {
            sprintf(res, "HTTP/1.1 404 Not Found\r\n\r\n");
        }

    } else if (strcmp(req->method, "POST") == 0) {
        if (strcmp(resType, "files") == 0){
            if (mkdir(g_argv[2], 0755) == -1) {
                if (errno != EEXIST) {
                    perror("Error creating directory");
                    return NULL;
                }
            }
            char *fname = malloc(strlen(g_argv[2]) + strlen(resName) + 1);
            sprintf(fname, "%s/%s",g_argv[2], resName);
            FILE *file = fopen(fname, "w");
            if (file == NULL) {
                    perror("Error opening file");
                    return NULL;
            }
            fputs(req->body, file);
            fclose(file);
            free(fname);
            strcpy(res, "HTTP/1.1 201 Created\r\n\r\n");
        }

    }
    printf("%s\n", res);
    if (send(client,res, strlen(res), 0) < 0) {
        printf("Send failed: %s \n", strerror(errno));
    }
    if (req->resource != resType){
        free(resType);
    }
    free(resName);
    close(client);
    free_http_request_parts(req);

    return NULL;
}

HttpRequest* split_request(const char *request) {
    HttpRequest *parts = malloc(sizeof(HttpRequest));
    if (!parts) return NULL;

    parts->method = NULL;
    parts->resource = NULL;
    parts->http_version = NULL;
    parts->headers = NULL;
    parts->body = NULL;

    char *request_copy = strdup(request);
    if (!request_copy) {
        free(parts);
        return NULL;
    }

    char *blank_line = strstr(request_copy, "\r\n\r\n");
    if (blank_line) {
        *blank_line = '\0';
        parts->body = strdup(blank_line + 4);
    }

    char *first_line_end = strstr(request_copy, "\r\n");
    if (first_line_end) {
        *first_line_end = '\0';
        parts->headers = strdup(first_line_end + 2);
    }

    char *method = strtok(request_copy, " ");
    char *resource = strtok(NULL, " ");
    char *http_version = strtok(NULL, " ");

    if (method) parts->method = strdup(method);
    if (resource) parts->resource = strdup(resource);
    if (http_version) parts->http_version = strdup(http_version);

    free(request_copy);
    return parts;
}


void free_http_request_parts(HttpRequest *parts) {
    if (!parts) return;  
    if (parts->method) free(parts->method);
    if (parts->resource) free(parts->resource);
    if (parts->http_version) free(parts->http_version);
    if (parts->headers) free(parts->headers);
    if (parts->body) free(parts->body);
    free(parts);
}
