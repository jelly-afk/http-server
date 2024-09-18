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

char* parseUrl(char *str);
int exists(char *target,char *elements[], int size);
char* findHeader(char *target, char* headers);
void* handleConnection(void*);

int main() {
//	 Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

//	 You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

//	 Uncomment this block to pass the first stage
	
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

char* parseUrl(char *str){
    int length = strlen(str);
    int i;
    for (i = 1;i < length;i++){
        if (str[i] == '/'){
            break;
        }
    }
    char *result = (char*)malloc((i + 1) * sizeof(char));
    if (result == NULL) {
        return "";
    }
    strncpy(result, str, i);
    result[i] = '\0';  
    char *elements[] = {
        "/echo",
        "/user-agent"
    };
    int size = sizeof(elements) / sizeof(elements[0]);
    if (exists(result, elements,size) == 1){
        return result;
    }
    free(result);
    result = strdup("/");
    return result;
}

int exists(char *target,char *elements[], int size) {
    for (int i = 0; i < size; i++) {
        if (strcmp(target, elements[i]) == 0) {
            return 1;  
        }
    }
    return 0;  
}

char* findHeader(char *target, char* req){
 char *line = strtok(req, "\r\n"); // Get the first line of the request
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
    char *token;
    char *req = strdup(buffer);
    token = strtok(buffer, "\r\n");
    printf("token: %d\n", strcmp(token, "GET / HTTP/1.1"));
    printf("Client connected\n");
      token = strtok(token, " ");
    token = strtok(NULL," ");
    char res[1024];
    printf("rToken: %s\n", token);
    char *parsed = parseUrl(token);
    printf("parsed: %s\n", parsed);
    if (strcmp(parsed, "/echo") == 0){
        char *st = strtok(token, "/");
        st = strtok(NULL, "/");
        sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(st), st);
    } else if (strcmp(parsed, "/user-agent") == 0){
        printf("working\n");
        char *value = findHeader("User-Agent",req);
        printf("%s\n", value);
        if (value != NULL){
            printf("val: %s\n", value);
            sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %lu\r\n\r\n%s", strlen(value), value);
        }
    } else {
        if (strcmp(token, "/") == 0){
            sprintf(res, "HTTP/1.1 200 OK\r\n\r\n");
        } else {
            sprintf(res, "HTTP/1.1 404 Not Found\r\n\r\n");
        }
    }
    if (send(client,res, strlen(res), 0) < 0) {
        printf("Send failed: %s \n", strerror(errno));
    }
    close(client);

    free(req);
    free(parsed);
    return NULL;
}
