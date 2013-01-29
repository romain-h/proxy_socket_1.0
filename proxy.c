
//
//  Proxy.c
//  csc450-Assignment #2
//
//  Created by Romain Hardy and Lucas Personnaz on 12/14/12.
//  Copyright (c) 2013 Romain Hardy / Lucas Personnaz. All rights reserved.
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
#define MAX_LINE 512
#define BUF_SIZE_RECV 124
int verbose = 0; //Verbose Option

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

static int inArray(char * myString);
static int versionHTTP(char * myString);
static int processRequest(int socket);
static int checkHttpRequest(char * requestLine, char ** url, char ** method, status * st);
static int parseUrl(char * request, char * host, char * port, char * path);
static int sendRequest(char * method, char * host, char * port, char * path, char ** filename, char * headerFields, status * st);
static int returnDataToClient(int socket, char ** filename);
static int sendStatusToClient(int socket, status * st);

/* Function inArray
	Test if the string is in request_methods */

static int inArray(char * myString)
{
	int i=0;
	int res=0;
	for (i;i<7;i++) // We compare each element of the request_methods one by one
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
	
static int versionHTTP(char * myString)
{
	int i=0;
	int j=0;
	int res=0;
	int res1, res2, res3;
	char *http="HTTP/";
	char *pch;
	char *comp = malloc(5*sizeof(char));
    bzero(comp,sizeof(comp));
	char *version = malloc(3*sizeof(char));
    bzero(version,sizeof(version));
	
	// We put the first 5 char in comp
	for(i;i<5;i++)
	{
		comp[i]=myString[i];
	}
	int result = strncmp(comp,http,5);
	if(result == 0) // We make sure that the 5 char are equal to HTTP/
	{
		pch=strchr(myString,'/');        
		i=0;
		j = pch - myString+1;
		for(j;j<strlen(myString);j++) // We get the 3 next characters (the version)
		{
			version[i]= myString[j];
			i++;
		}
        
		res1 = strcmp(version,"1.0");
		res2 = strcmp(version,"0.9");
        res3 = strcmp(version,"1.1");
		
		if((res1 == 0) || (res2 == 0) || (res3 == 0))
		{
			res=1;
		}
	}
	return res;
}

/* Function processRequest
	This function is processing the http Request and send it to the server*/

static int processRequest(int socket){
    char *buf = malloc(BUF_SIZE_RECV * sizeof(char));
    bzero(buf,sizeof(buf));
    size_t request_size = BUF_SIZE_RECV;
    char * request = malloc(request_size * sizeof(char));
    bzero(request,sizeof(request));
    char * request_buff = malloc(request_size * sizeof(char));
    bzero(request_buff,sizeof(request_buff));

    //Initialize status that we are going to change during all the process
    status mainStatus = {200, "OK"};

    int received, available=BUF_SIZE_RECV;
    char * url;
	char * method;

    // receive request from client on current socket:
    while ( (received = recv(socket, buf, available, 0) > 0)){

        //Keep actual content of request:
        request_buff = realloc(request_buff, request_size);
        bzero(request_buff,sizeof(request_buff));
        strcpy(request_buff, request);  
        //Realloc double of memory of final request:
        request_size *= 2;  
        request = realloc(request, request_size);
        strcpy(request, request_buff);                     
        strcat(request, buf);
        bzero(buf, BUF_SIZE_RECV);
    
        // Detect end of request:
        if((strstr(request,"\r\n\r\n")!=NULL) || (strstr(request, "\n\n")!=NULL)) 
            break;  
    }

    //Start verify request by HTTP specifications
    // To do that we need to separate orginal request line and options:
    char * pch;
    char * requestFirstLine = malloc(strlen(request)*sizeof(char));
    char * headerFields = malloc(strlen(request)*sizeof(char));
    bzero(requestFirstLine, sizeof(requestFirstLine));
    bzero(headerFields, sizeof(headerFields));

    int i = 0;
    pch = strtok (request,"\r\n");		// Separating the first line (the request) from the header fields
    while(pch != NULL){
        if(i==0)
		{
			strcpy(requestFirstLine, pch);
		}
		else if (i==1)
		{
			strcpy(headerFields, pch);
		}
		else
		{
			strcat(headerFields, "\r\n");
			strcat(headerFields, pch);
		}
		pch = strtok (NULL, "\r\n");
        i++;

    }
	
	
	// We check that the request is properly formatted
    if( checkHttpRequest(requestFirstLine,& url, & method, &mainStatus)) {
        char * host = malloc(strlen(url)*sizeof(char));
        bzero(host, sizeof(host));
        char * port = malloc(5*sizeof(char));
        bzero(port,sizeof(port));
        char * path = malloc(strlen(url)*sizeof(char));
        bzero(path,sizeof(path));
        
       if(parseUrl(url, host, port, path)) // We parse the URL to get host, port, path
       {
           //Initialize filename string
            char * filename;
            
            if(!sendRequest(method, host, port, path, &filename, headerFields, &mainStatus)){
                sendStatusToClient(socket, &mainStatus);
            } else{
                returnDataToClient(socket,&filename);
            } 
            
       }
   } else{
    sendStatusToClient(socket, &mainStatus);
   }      

   return 1;       

}
/* Refers to RFC 1945 5. Request (page 22)
*  Method + URI + HTTP-Version
*/
static int checkHttpRequest(char * requestLine, char ** url, char ** method, status * st)
{
	char * pch;
	pch = strtok (requestLine,sp);
	char *res[3];
    bzero(res,sizeof(res));
	int i=0;
	// We split the request using a space as delimiter
	while ((pch != NULL)&&(i<3))
	{
		res[i]= pch;
        // printf("%s%s\n","RES", res[i] );
		pch = strtok (NULL, sp);
		i++;
	}
	
	if(i<3) // If the request has less than the mandatory arguments
	{
		st->code = 400;
        st->reason =  "Bad Request";
		return 0;
	}
	// We check that the method and the version are correct
	if((inArray(res[0])) && (versionHTTP(res[2])))
	{
		// We save the method and the url in a variable
		*method = res[0];
		*url = res[1];
		return 1;
	}
	st->code = 400;
    st->reason =  "Bad Request";
	return 0;
}

/*http_URL       = "http:" "//" host [ ":" port ] [ abs_path ]

       host           = <A legal Internet host domain name
                         or IP address (in dotted-decimal form),
                         as defined by Section 2.1 of RFC 1123>

       port           = *DIGIT  */
// Parsing URL => Get the requested host and port, and the requested path.

static int parseUrl(char * request, char * host, char * port, char * path)
{
	int i = 0;
	int j = 0;
	int res =0;
	char *http= "http://";
	char *comp = malloc(7*sizeof(char));
	unsigned int lastColumn;
	unsigned int firstSlash;
	// We get the first 7 characters of the url
	for (i;i<7;i++)
	{
		comp[i]=request[i];
	}
	int result = strncmp(http,comp,7);
	// If the 7 characters are http://
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
            host[lastColumn] = '\0';
			
			//We remove the host and the column
			j=0;
			int lengthHost = strlen(host)+1;
			for(j;j<strlen(request)-lengthHost;j++)
			{
				request[j]=request[j+lengthHost];
			}
			request[j]='\0';
			
			if(strrchr(request, '/') != NULL) // If there is a port and a path
			{
				// We get the position of the first slash in the string which is the limit between port and path
				firstSlash = strchr(request, '/') - request;

				// We parse the port
				strncpy(port, request, firstSlash);
                port[firstSlash] = '\0';
				
				//We remove the port
				j=0;
				int lengthPort = strlen(port);	
				for(j;j<strlen(request)-lengthPort;j++)
				{
					request[j]=request[j+lengthPort];
				}
				request[j]='\0';
				//We parse the path
				strcpy(path, request);
				res = 1;
			}
			else // If there is a port but no path
			{
				//We parse the port
				strncpy(port, request, strlen(request));
                port[strlen(request)] = '\0';
				// We set the path as /
				path = "/";
				res = 1;
			}
		}
		else // If there is no port
		{
			if(strchr(request, '/') != NULL) //If there is a path but no port
			{                
				firstSlash = strchr(request,'/') - request;
                
				// We parse the host
				strncpy(host, request, firstSlash);
                host[firstSlash]='\0';
				// We set the port at 80
				strcpy(port,"80");
				
				// We remove the host
				j=0;
				int lengthHost = strlen(host);
                int tmp = strlen(request)-lengthHost;
				for(j;j<tmp;j++)
				{
					request[j]=request[j+lengthHost];
				}
				request[j]='\0';
				//We parse the path
				strcpy(path, request);
				res = 1;
			}
			else // If there is no port and no path
			{
				// We parse the host
				strcpy(host, request);
				// We set the default port at 80 
				strcpy(port,"80");
				// We set the default path at /
				strcpy(path,"/");
				res = 1;
			}
		}
	return res;
}
// Getting Data from the Remote Server => Connection to host port 80 by default  + send a HTTP request for the appropriate file.
static int sendRequest(char * method, char * host, char * port, char * path, char ** filename, char * headerFields, status * st){
        struct hostent *hp;
        struct sockaddr_in sin;
        int socketClientRequest;
        int on = 1;
        char * buf = malloc(512*sizeof(char));
        bzero(buf,sizeof(buf));
        int len;
        FILE * fileRet;

        //Rebuild the original request
        char * request = malloc(500*sizeof(char));
        bzero(request,sizeof(request));

        strcpy(request, method);       
		strcat(request, " ");
        // strcat(request, host);
        strcat(request, path);
        strcat(request, " HTTP/1.0\r\n\r\n");
        // strcat(request, headerFields);
        // strcat(request, "\r\n");

        /* translate host name into peer’s IP address */
        hp = gethostbyname(host);
        if (!hp) {
            st->code = 400;
            st->reason = "Bad Request";
            return 0;
        }

        /* build address data structure */
        bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
        sin.sin_port = htons(atoi(port));

        /* active open */
        if ((socketClientRequest = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            st->code = 500;
            st->reason = "Internal Server Error";
            return 0;
        }

        // Connect to socket
        if (connect(socketClientRequest, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            st->code = 500;
            st->reason = "Internal Server Error";
            return 0;
        }

        // send request to server
        len = strlen(request) +1;
        if(send(socketClientRequest, request, len, 0) < 0){
            st->code = 500;
            st->reason = "Internal Server Error";
            return 0;
        } 

        //Define FileName:
        *filename = "content.txt";
        //Open the file 
        fileRet = fopen(*filename, "w");

        //receive data form server:
        int recv_size;
        while(recv_size = recv(socketClientRequest, buf, 512,0) > 0 ){
            //Store data to the text file:
            fputs(buf,fileRet);
            bzero(buf, 512);        
        }
        //Close the file
        fclose(fileRet);
        // Close the socket connection
        close(socketClientRequest);

        return 1; 
}

// Returning Data to the Client => Once the transaction is complete, the proxy should close the connection
static int returnDataToClient(int socket, char ** filename){
    char *buf  = malloc(512*sizeof(char));
    bzero(buf, sizeof(buf));
    //Open the right file
    FILE * fp;
    fp = fopen(*filename, "r");

    // Read all data from file and send by chunck of 512 bytes.
    while(fgets(buf, sizeof(buf), fp) != NULL){
        if(write(socket , buf , strlen(buf)) < 0){
            if(verbose)
            perror("Error send data from proxy to client");
            exit(1);
        }
        bzero(buf, sizeof(buf));
    }

    fclose(fp);
    return 1;
}
// Returning status to client 
static int sendStatusToClient(int socket, status * st){
    char * stringStatus = malloc(sizeof(st)*sizeof(char));
    bzero(stringStatus, sizeof(stringStatus));
    //cast int to string
    char * code = malloc(3*sizeof(char));    
    sprintf(code, "%d", st->code);

    //Built std RFC status line
    strcpy(stringStatus, "HTTP/1.0 ");       
    strcat(stringStatus, code);
    strcat(stringStatus, " ");
    strcat(stringStatus, st->reason);
    strcat(stringStatus, "\r\n");


    if(write(socket , stringStatus , strlen(stringStatus)) < 0){
        perror("Error send data status code from proxy to client");
    }
}

int main(int argc, const char * argv[])
{
    const char* port;
    socklen_t clilen;
    struct sockaddr_in sckin;
	int main_socket, new_s; // Sockets
    int len, opt = 1;  
     

    // Get port on first argument
    if (argc==2) {
        port = argv[1];
    }
    else if(argc==3){
        port = argv[1];
        if(argv[2] == "-v"){
            verbose = 1;
        }
    }
    else {
        if(verbose){
           perror("usage: Please specify a port as first argument\n"); 
        }
        exit(1);
    }    
    
    /* build address data structure */
    bzero((char *)&sckin, sizeof(sckin));
    sckin.sin_family = AF_INET;
    sckin.sin_addr.s_addr = INADDR_ANY;
    sckin.sin_port = htons(atoi(port));

    /*  We need to open a socket on TCP to handle HTTP requests. */
    if ((main_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        if(verbose)
        perror("ERROR: Error socket creation");
        exit(1);
    }
    
    /* By default a socket is blocking. We should add some option on the main socket server to 
        set master socket to allow multiple connections */
    if (setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))<0) {
        if(verbose)
        perror("ERROR: setsockopt non blocking");
        exit(1);
    }
    
    // Bind socket to the host and port number
    if ((bind(main_socket, (struct sockaddr *)&sckin, sizeof(sckin))) < 0) {
        if(verbose)
        perror("ERROR: bind");
        exit(1);
    }    
    
    // Listen th
    listen(main_socket, MAX_PENDING);

    // Get length
    clilen = sizeof(sckin);

    /* wait for connection, then receive and process HTTP Request */
    while(1) {
        if ((new_s = accept(main_socket, (struct sockaddr *)&sckin, &len)) < 0) {
            if(verbose)
            perror("ERROR: accept");
            exit(1);
        }
        //Process Request for incoming connexion:
        if(processRequest(new_s)){
            //Once processRequest is done clone socket 
            close(new_s);
        }
    }
        
    return 0;
}



