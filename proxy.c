
//
//  server.c
//  csc450-server
//
//  Created by Romain Hardy on 12/14/12.
//  Copyright (c) 2012 Romain Hardy. All rights reserved.
//

#include <stdio.h>
#include <string.h>
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
const char *sp = "\x20";

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

	
/* Function inArray
	Test if the string is in request_methods */

static int inArray(char * myString)
{
	int i=0;
	int res=0;
	for (i;i<7;i++)
	{
		int result = strncmp(myString,request_methods[i],7);
		if(result == 0)
		{
			res=1;
		}
	}
	return res;
}

/* Function versionHTTP
	Get the HTTP version from a string */
	
static float versionHTTP(char * myString)
{
	int i=0;
	int j=0;
	int res=0;
	int res1;
	int res2;
	char *http="HTTP/";
	char comp[5];
	char version[3];
	float httpVersion;
	for(i;i<5;i++)
	{
		comp[i]=myString[i];
	}
	for(j;j<3;j++)
	{
		version[j]=myString[j+5];
	}
	int result = strncmp(comp,http,5);
	if(result == 0)
	{
		res1 = strcmp(version,"1.0");
		res2 = strcmp(version,"0.9");
		
		if((res1 == 0) || (res2 == 0))
		{
			res=1;
		}
	}
	return res;
}

/* Refers to RFC 1945 5. Request (page 22)
*  Method + URI + HTTP-Version
*/
static int checkHttpRequest(char * requestLine, char ** url)
{
	char * pch;
	pch = strtok (requestLine,sp);
	char *res[3];
	int i=0;
	while (pch != NULL)
	{
		res[i]= pch;
		pch = strtok (NULL, sp);
		i++;
	}
	if(i<3)
	{
		status erreur = {400, "Bad Request"};
		return 0;
	}
	if((inArray(res[0])) && (versionHTTP(res[2])))
	{
		*url = res[1];
		return 1;
	}
	status erreur = {400, "Bad Request"};
	// printf("%d %s", erreur.code, erreur.reason);
	return 0;
}

/*http_URL       = "http:" "//" host [ ":" port ] [ abs_path ]

       host           = <A legal Internet host domain name
                         or IP address (in dotted-decimal form),
                         as defined by Section 2.1 of RFC 1123>

       port           = *DIGIT  */
// Parsing URL => Get the requested host and port, and the requested path.

static int parseUrl(char * request, char * host, int * port, int * path)
{
	int i = 0;
	int j = 0;
	int res =0;
	char *http= "http://";
	char comp[7];
	int lastColumn;
	int firstSlash;
	for (i;i<7;i++)
	{
		comp[i]=request[i];
	}
	int result = strncmp(http,comp,7);
	if(result == 0)
	{
		// We remove the http://
		for(j;j<strlen(request)-7;j++)
		{
			request[j]=request[j+7];
		}
		request[j]='\0';
	}
		
		if(strrchr(request, ':')!= NULL) // If there is a port
		{	
		// We get the position of the last column in the string which is the limit between host and port
			lastColumn = strrchr(request, ':') - request ;
			// We parse the host 
			strncpy(host, request, lastColumn);
			
			//We remove the host and the column
			j=0;
			int lengthHost = strlen(host)+1;
			for(j;j<strlen(request)-lengthHost;j++)
			{
				//printf("%d\n",j);
				request[j]=request[j+lengthHost];
			}
			request[j]='\0';
			
			if(strrchr(request, '/') != NULL) // If there is a path
			{
				// We get the position of the first slash in the string which is the limit between port and path
				firstSlash = strchr(request, '/') - request;

				// We parse the port
				strncpy(port, request, firstSlash);
				
				//We remove the port
				j=0;
				int lengthPort = strlen(port);	
				for(j;j<strlen(request)-lengthPort;j++)
				{
					request[j]=request[j+lengthPort];
				}
				request[j]='\0';
				
				strcpy(path, request);
				res = 1;
			}
			else // If there is no path
			{
				strncpy(port, request, strlen(request));
				path = "/";
				res = 1;
			}
		}
		else // If there is no port
		{
			if(strchr(request, '/') != NULL) //If there is a path
			{
				firstSlash = strchr(request,'/') - request;
				// We parse the host
				strncpy(host, request, firstSlash);
				strcpy(path,"80");
				
				// We remove the host
				j=0;
				int lengthHost = strlen(host);
				for(j;j<strlen(request)-lengthHost;j++)
				{
					request[j]=request[j+lengthHost];
				}
				request[j]='\0';
				
				strcpy(path, request);
				res = 1;
			}
			else
			{
				strcpy(host, request);
				strcpy(port,"80");
				strcpy(path,"/");
				res = 1;
			}
		}
	return res;
}
// Getting Data from the Remote Server => Connection to host port 80 by default  + send a HTTP request for the appropriate file.
static int sendRequest(char * host, char * port, char * path, char ** filename){

        struct hostent *hp;
        struct sockaddr_in sin;
        int socketClientRequest;
        int on = 1;
        char * buf[512];
        int len;
        FILE * fileRet;
        char * request = malloc(500*sizeof(char));

        // request = "GET\x20/\x20HTTP/1.0\r\n\r\n";
        // path = "/";
        // host = "hrdy.me";
        // port = "80";

        // request = "GET / HTTP/1.0\r\nUser-Agent: curl/7.21.4 (universal-apple-darwin11.0) libcurl/7.2.4 OpenSSL/0.9.8r zlib/1.2.5\nHost: hrdy.me\nAccept: */*\r\n\r\n";
        // request = "GET / HTTP/1.0\nHost: google.com\r\n\r\n";
            strcpy(request, "GET ");            
            strcat(request, path);
            strcat(request, " HTTP/1.0");

            printf("%s\n",  request);

            

        /* translate host name into peer’s IP address */
        hp = gethostbyname(host);
        if (!hp) {
            fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
            exit(1);
        }


        /* build address data structure */
        bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
        sin.sin_port = htons(atoi(port));


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
        }else{
            printf("%s\n","Moi j'ai envoyé ta foutue request c'est plus mon probleme...." );
        }

        //Define FileName:
        *filename = "content.txt";
        //Open the file 
        fileRet = fopen(*filename, "w");

        //receive data form server:
        int recv_size;
        // while(read(socketClientRequest, buf, 512 - 1) > 0){
        //     //Store data to the text file:
        //     fputs(buf,fileRet);
        //     bzero(buf, 512);
        // }

        while(recv_size = recv(socketClientRequest, buf, 512,0) > 0 ){
            printf("%s%d\n", "JAI RECU :::::::::::::::",recv_size);
            //Store data to the text file:
            fputs(buf,fileRet);
            bzero(buf, 512);
            // if(recv_size < 512){
            //     break;
            // }            
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
        // printf("%s", buf );
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
                
                    char * url;
                    //Start verify request by HTTP specification:                 
                    if( checkHttpRequest(buf,& url)){
                        char * host = malloc(strlen(url)*sizeof(char));
                        char * port = malloc(5*sizeof(char));
                        char * path = malloc(strlen(url)*sizeof(char));
                       if(parseUrl(url, host, port, path))
                       {
                            printf("%s\n%s\n%s\n", host, port, path);

                            
                            char * filename;
                            
                            sendRequest(host, port, path, &filename);  
                            returnDataToClient(client_socket[i],&filename); 
                       }
                   }


                        // char bufRet[512];
                        // FILE * fp;
                        // fp = fopen("filename", "r");

                        // while(fgets(bufRet, sizeof(bufRet), fp)){
                        //      // printf("%x", buf );
                        // // int n = 0;
                        // // char c;

                        //     // do {
                        //     //   n++;  
                        //     //   c = fgetc (fp);
                        //     //   printf("%c\n",c );
                        //     //   if(n<511){
                        //     //     // buf[n] = c;
                        //     //     strcat(c, buf);
                        //     //   } else{
                        //     //     n = 0;
                        //         if(send(client_socket[i], bufRet, sizeof(bufRet), 0) < 0){
                        //             perror("Error send data from proxy to client");
                        //         }
                        //         // bzero(bufRet, 512);
                        //         // }

                              
                              
                        //     // } while (c != EOF);
                        
                            
                        //     bzero(bufRet, 512);
                        // }

                        // fclose(fp);
                      
                    


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



