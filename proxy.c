//
//  server.c
//  csc450-server
//
//  Created by Romain Hardy on 12/14/12.
//  Copyright (c) 2012 Romain Hardy. All rights reserved.
//

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>
#include <regex.h>


#define MAX_PENDING 4
#define MAX_CLIENTS_SIM 10 
#define MAX_LINE 51

// use US-ASCII space (32):
const char *sp = '\x20';

/* 
Status-Code    = "200"   ; OK
                      | "201"   ; Created
                      | "202"   ; Accepted
                      | "204"   ; No Content
                      | "301"   ; Moved Permanently
                      | "302"   ; Moved Temporarily
                      | "304"   ; Not Modified
                      | "400"   ; Bad Request
                      | "401"   ; Unauthorized
                      | "403"   ; Forbidden
                      | "404"   ; Not Found
                      | "500"   ; Internal Server Error
                      | "501"   ; Not Implemented
                      | "502"   ; Bad Gateway
                      | "503"   ; Service Unavailable
*/                

typedef struct {
   int    code;
   char   *reason;
} status;

//HTTP Methods : 
const char *request_methods[7]={ 
    "GET", "POST", "HEAD", 
    "PUT", "DELETE", "LINK", "UNLINK"};


/* Refers to RFC 1945 5. Request (page 22)
*  Method + URI + HTTP-Version
*/
static int checkHttpRequest(char * requestLine){
    // char *token = NULL;

    // printf("%s%c%s\n", "tijntijnr",sp,"eiqwune");
    // token = strtok(requestLine, sp);
    // printf("Current token: %s.\n", token);
    // while (token) {
    //     printf("Current token: %s.\n", token);
    //     token = strtok(NULL, "\n");
    // }
    return 1;
}

/*http_URL       = "http:" "//" host [ ":" port ] [ abs_path ]

       host           = <A legal Internet host domain name
                         or IP address (in dotted-decimal form),
                         as defined by Section 2.1 of RFC 1123>

       port           = *DIGIT  */
// Parsing URL => Get the requested host and port, and the requested path.

static int parseUrl(char * request, char * host, int * port){

}
// Getting Data from the Remote Server => Connection to host port 80 by default  + send a HTTP request for the appropriate file.
static int sendRequest(char * request, int *port, char ** filename){

        struct hostent *hp;
        struct sockaddr_in sin;
        int socketClientRequest;
        int on = 1;
        char * buf[512];
        int len;
        FILE * fileRet;
        // request = "GET\x20/\x20HTTP/1.0\r\n\r\n";


        request = "GET / HTTP/1.1\r\nUser-Agent: curl/7.21.4 (universal-apple-darwin11.0) libcurl/7.2.4 OpenSSL/0.9.8r zlib/1.2.5\nHost: hrdy.me\nAccept: */*\r\n\r\n";


        /* translate host name into peer’s IP address */
        hp = gethostbyname("hrdy.me");
        if (!hp) {
            fprintf(stderr, "simplex-talk: unknown host: %s\n", "google");
            exit(1);
        }


        /* build address data structure */
        bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
        sin.sin_port = htons((uint16_t)port);


        /* active open */
        if ((socketClientRequest = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("simplex-talk: socket");
            exit(1);
        }

        // setsockopt(socketClientRequest, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
        // Connect to socket
        if (connect(socketClientRequest, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            perror("simplex-talk: connect");
            close(socketClientRequest);
            exit(1);
        }

        // send request to server
        len = strlen(request) + 1;
        if(send(socketClientRequest, request, len, 0) < 0){
            perror("simplex-talk: Send buffer");
        }

        //Define FileName:
        *filename = "efwesc";
        //Open the file 
        fileRet = fopen(*filename, "w");

        //receive data form server:
        while(read(socketClientRequest, buf, 512 - 1) > 0){
            //Store data to the text file:
            fputs(buf,fileRet);
            bzero(buf, 512);
        }

        //Close the file
        fclose(fileRet);
        // Close the socket connection
        close(socketClientRequest);

            // 
            // while(1){

            //     while(recv(socketClientRequest, buf, sizeof(buf), 0) > 0 ){
            //         // printf("%s\n", "Tu dois recevoir un truc ici");
            //         // printf("%s\n", buf);
            //         fputs(buf, stdout);

            //     } 
            //         break;
                
             

            // }    
             printf("%s\n", "Jai fini");

 
}

// Returning Data to the Client => Once the transaction is complete, the proxy should close the connection
static int returnDataToClient(int socket, char ** filename){
    char buf[512];
    FILE * fp;
    fp = fopen(*filename, "r");

    while(fgets(buf, sizeof(buf), fp) != NULL){
        printf("%s", buf );
        if(send(socket, buf, sizeof(buf), 0) < 0){
            perror("Error send data from proxy to client");
        }
        bzero(buf, 512);
    }

    fclose(fp);
    return 1;
}

int main(int argc, const char * argv[])
{
    int opt=1;
    const char* port;
    struct sockaddr_in sckin;
    char buf[MAX_LINE];
    int len, i;
    socklen_t clilen;
    int main_socket, new_s, new_conns;
    //Simultaneous connection on socket:
    int max_clients = MAX_CLIENTS_SIM;
    int client_socket[MAX_CLIENTS_SIM];
    fd_set master_set;
    

    //initialise table of int all client_socket[] to 0 
    for (i = 0; i < max_clients; i++){
        client_socket[i] = 0;
    }

    // Get port on first argument
    if (argc==2) {
        port = argv[1];
    }
    else {
        perror("usage: simplex-talk port\n");
        exit(1);
    }
    
    
    /* build address data structure */
    bzero((char *)&sckin, sizeof(sckin));
    sckin.sin_family = AF_INET;
    sckin.sin_addr.s_addr = INADDR_ANY;
    // inet_pton(AF_INET, "127.0.0.1", &sckin.sin_addr);
    sckin.sin_port = htons(atoi(port));

    /* setup passive open */
    if ((main_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("simplex-talk: socket");
        exit(1);
    }
    
    /* By default a socket is blocking. We should add some option on the main socket server to 
        set master socket to allow multiple connections */
    if (setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) {
        perror("simplex-talk: setsockopt non blocking");
        exit(1);
    }
    
    // Bind socket to the host and port number
    if ((bind(main_socket, (struct sockaddr *)&sckin, sizeof(sckin))) < 0) {
        perror("simplex-talk: bind");
        exit(1);
    }
    
    
    // Listen th
    listen(main_socket, MAX_PENDING);

    // Get length
    clilen = sizeof(sckin);
    
    
    while(1){
        
        // Set up file descriptor for the main socket
        FD_ZERO(&master_set);
        
        /* specify socket to listen for new connections */
        FD_SET(main_socket, &master_set);
        // Initialize file descriptor for all clients
        for (i=0; i<max_clients; i++) {
            if (client_socket[i] > 0) {
                FD_SET(client_socket[i], &master_set);
            }
        }
        
        
        /* wait for connection, without timeout */
        new_conns = select(max_clients+3, &master_set, NULL, NULL, NULL);
        if(new_conns < 0){
            perror("simplex-talk: select");
            exit(1);
        }
        // Main socket used : New client want to set up a connection
        if (FD_ISSET(main_socket, &master_set)) {
            /* Open the new socket as 'new_socket' */
            if((new_s = accept(main_socket, (struct sockaddr *) &sckin, &clilen )) < 0){
                perror("Error: Accept");
                exit(1);
            }
            
            /* add new socket to list of sockets */
            for (i=0; i<max_clients; i++) {
                //Check if this client socket is empty and so available
                if (client_socket[i] == 0) {
                    // Add the new socket so the client list
                    client_socket[i] = new_s;
                    // leave the loop
                    i = max_clients;
                }
            }
            
        }
        
        // Check all current client in the queue to see if they want to do IO 
        for (i = 0; i < max_clients; i++){
            // Check the current file descriptor
            if (FD_ISSET(client_socket[i], &master_set)) {
              // Print message
              if(recv(client_socket[i], buf, sizeof(buf), 0) > 0 ){
                    //Start verify request by HTTP specification:                 
                    if( checkHttpRequest(buf)){
                        printf("%s\n", "Apero");
                        int * port;
                        char * filename;
                        port = 80;
                        sendRequest(buf, port,&filename);                
                        returnDataToClient(client_socket[i], &filename);

                    } 
              } 
              //else close connection and release the client socket:
              else{
                  close(client_socket[i]);
                  client_socket[i] = 0;
              }
            }            
        } 
    }
        
    return 0;
}

