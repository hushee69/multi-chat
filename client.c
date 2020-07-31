/*
 * Author : KANITA Nada
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/select.h>
#include <strings.h>
#include <string.h>

#define STDIN_FILENO		0
#define STDOUT_FILENO		1

#define SERVER				"0.0.0.0"
#define PORT				"6666"
#define PSEUDO_LEN			15
#define MAX_BUFF			512
#define MAX_CLIENTS			12

#define MENU				"/menu"
#define QUIT				"/quit"
#define LIST				"/list"
#define KICK				"/kick"
#define CHANGE				"/change"

const char *SERVER_CLOSE_MESSAGE = "Nantes chat has closed its servers, goodbye";
const char *CONNECTION_ESTABLISHED = "Connection established with the server";

typedef enum CLIENT_TYPE
{
	REGULAR, ADMINISTRATOR
} client_type;

typedef enum CLIENT_STATUS
{
	VISIBLE, INVISIBLE
} client_status;

typedef struct CLIENT_INFO
{
	int sock;								// socket to send / recv data
	char ip[INET6_ADDRSTRLEN];				// server ip
	char port[5];							// server port
	char pseudo[15];
	client_type type;
	client_status status;
} client_info;

void die_error(const char *msg);
client_info *fill_client_info(char *ip, char *port, char *pseudo, client_type type, client_status status);
void print_client_info(client_info *ci);
void client_create_socket_and_connect(client_info *ci);
int send_client_info(client_info *ci);
void send_message(int sock, const char *msg);
int recv_message(int sock, char *out_buffer, int buffer_len);

void print_menu()
{
	const char *menu = "/menu: shows the menu\n"\
						"/list: list of people that are connected\n"\
						"@pseudo: send a private message to [pseudo]\n"\
						"/kick: kick a user out\n"
						"/quit: quitter le chat\n";
	
	fprintf(stderr, "%s", menu);
}

void recv_list_of_clients(int client_sock)
{
	// can have maximum no of clients connected
	int names_buffer_len = (MAX_CLIENTS * PSEUDO_LEN) + (PSEUDO_LEN * 2);
	char *names_buffer = calloc(names_buffer_len, sizeof(char));
	
	int bytes_recvd = recv_message(client_sock, names_buffer, names_buffer_len);
	if( bytes_recvd == -1 )
	{
		die_error("recv clients list");
	}
	fprintf(stderr, "LIST OF CLIENTS\n%s", names_buffer);
	
	return;
}

void client_loop(client_info *c_info)
{
	client_info ci = *(fill_client_info(SERVER, c_info->port, c_info->pseudo, c_info->type, c_info->status));
	client_create_socket_and_connect(&ci);
	
	send_client_info(&ci);
	
	fd_set readfds;
	int state;
	
	char msg_buf[MAX_BUFF];
	char recv_buf[MAX_BUFF];
	int str_ptr, len;
	
	int bytes_recvd;
	
	char *clients_list = NULL;
	
	while( 1 )
	{
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);					// standard input
		FD_SET(ci.sock, &readfds);						// send/recv socket of client
		
		state = select(ci.sock + 1, &readfds, NULL, NULL, NULL);
		if( state == -1 )
		{
			die_error("select");
		}
		
		if( FD_ISSET(STDIN_FILENO, &readfds) )
		{
			// enter detected - sending a message
			// empty the buffer
			memset(&msg_buf, 0, MAX_BUFF);
			// get pseudo len
			str_ptr = 0;
			len = strlen(ci.pseudo);
			// concatenate the pseudo in the message
			strncat(msg_buf, ci.pseudo, len);
			str_ptr += len;
			// concatenate ': ' in message
			strncat(msg_buf, ": ", 2);
			str_ptr += 2;
			// now msg contains - "pseudo: "
			fgets(msg_buf + str_ptr, MAX_BUFF, stdin);
			len = strlen(msg_buf) - 1;
			// remove newline added by fgets
			msg_buf[len] = '\0';
			
			// if we have something to send
			if( strlen(msg_buf + str_ptr) )
			{
				// compare if it is a special command
				// str_ptr gives us the index to the start of the actual message
				// since we have prefixed the message buffer with the pseudo
				if( !strncmp(msg_buf + str_ptr, "/", 1) )
				{
					// special command
					if( !strncmp(msg_buf + str_ptr, MENU, len) )
					{
						print_menu();
					}
					else if( !strncmp(msg_buf + str_ptr, QUIT, len) )
					{
						fprintf(stderr, "Goodbye %s\n", ci.pseudo);
						break;
					}
					else if( !strncmp(msg_buf + str_ptr, LIST, len) )
					{
						// show list of connected users
						send_message(ci.sock, msg_buf + str_ptr);
						recv_list_of_clients(ci.sock);
					}
					else if( !strncmp(msg_buf + str_ptr, KICK, strlen(KICK)) )
					{
						// kick out a user
						send_message(ci.sock, msg_buf + str_ptr);
					}
					else if( !strncmp(msg_buf + str_ptr, CHANGE, strlen(CHANGE)) )
					{
						// change pseudo
						memset(ci.pseudo, '\0', PSEUDO_LEN);
						strncpy(ci.pseudo, msg_buf + str_ptr + strlen(CHANGE) + 1, PSEUDO_LEN);
						// send new pseudo to server to update
						send_message(ci.sock, msg_buf + str_ptr);
					}
					else
					{
						// send the message if none of the special commands are recognized
						fprintf(stderr, "Info: Special command unrecognized\n");
						send_message(ci.sock, msg_buf);
					}
				}
				else if( !strncmp(msg_buf + str_ptr, "@", 1) )
				{
					// send private message
					// concatenate " - private from pseudo" so the person receiving knows who sent the message
					const char *from_pseudo = " - private from ";
					strncat(msg_buf, from_pseudo, strlen(from_pseudo));
					strncat(msg_buf, ci.pseudo, strlen(ci.pseudo));
					send_message(ci.sock, msg_buf + str_ptr);
				}
				else
				{
					if( ci.status != INVISIBLE )
					{
						send_message(ci.sock, msg_buf);
					}
				}
			}
		}
		
		if( FD_ISSET(ci.sock, &readfds) )
		{
			// ready to read from socket
			memset(&recv_buf, 0, MAX_BUFF);
			bytes_recvd = recv_message(ci.sock, recv_buf, MAX_BUFF);
			if( bytes_recvd <= 0 )
			{
				break;
			}
			fprintf(stderr, "%s\n", recv_buf);
		}
	}
	
	close(ci.sock);
	
	return;
}

/********************************
 ********************************
 * main function				*
 ********************************
/*****************************/
int main(int argc, char *argv[])
{
	if( argc != 5 )
	{
		fprintf(stderr, "usage: %s [port] [pseudo] [usertype] [userstatus]\n", argv[0]);
		exit(-1);
	}
	
	client_info ci;
	
	memset(ci.port, 0, sizeof(ci.port));
	strncpy(ci.port, argv[1], 4);
	
	memset(ci.pseudo, 0, sizeof(ci.pseudo));
	strncpy(ci.pseudo, argv[2], sizeof(ci.pseudo));
	
	ci.type = atoi(argv[3]);
	ci.status = atoi(argv[4]);
	
	client_loop(&ci);
	
	return 0;
}

/*
 * FUNCTION DEFINITIONS
*/
void die_error(const char *msg)
{
	perror(msg);
	exit(-1);
}

int send_client_info(client_info *ci)
{
	// send pseudo
	int bytes_sent = send(ci->sock, ci->pseudo, strlen(ci->pseudo), 0);
	if( bytes_sent <= 0 )
	{
		die_error("send client info - pseudo");
	}
	
	// send type
	bytes_sent = send(ci->sock, &(ci->type), sizeof(ci->type), 0);
	if( bytes_sent <= 0 )
	{
		die_error("send client info - type");
	}
	
	// send status
	bytes_sent = send(ci->sock, &(ci->status), sizeof(ci->status), 0);
	if( bytes_sent <= 0 )
	{
		die_error("send client info - status");
	}
	
	fprintf(stderr, "%s\n", CONNECTION_ESTABLISHED);
	
	return bytes_sent;
}

void send_message(int sock, const char *msg)
{
	int bytes_sent = send(sock, msg, strlen(msg), 0);
	if( bytes_sent == -1 )
	{
		die_error("send message");
	}
	
	return;
}

void client_create_socket_and_connect(client_info *ci)
{
	struct addrinfo *addrinfo = NULL, hints;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	int state = getaddrinfo(ci->ip, ci->port, &hints, &addrinfo);
	if( state != 0 )
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(state));
		exit(-1);
	}
	
	// create socket and put it in the client info struct
	ci->sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if( ci->sock == -1 )
	{
		die_error("socket error");
	}
	
	// attempt to connect
	state = connect(ci->sock, addrinfo->ai_addr, addrinfo->ai_addrlen);
	if( state == -1 )
	{
		die_error("connect error");
	}
	
	return;
}

int recv_message(int sock, char *out_buffer, int buffer_len)
{
	return recv(sock, out_buffer, buffer_len, 0);
}

client_info *fill_client_info(char *ip, char *port, char *pseudo, client_type type, client_status status)
{
	client_info *ci = malloc(sizeof(client_info));
	
	ci->sock = -1;
	
	strncpy(ci->ip, ip, strlen(ip));
	strncpy(ci->port, port, strlen(port));
	strncpy(ci->pseudo, pseudo, strlen(pseudo));
	
	ci->type = type;
	ci->status = status;
	
	return ci;
}

void print_client_info(client_info *ci)
{
	printf("sock %d\n", ci->sock);
	printf("ip: %s\n", ci->ip);
	printf("port: %s\n", ci->ip);
	printf("pseudo: %s\n", ci->pseudo);
	
	printf("type: ");
	switch( ci->type )
	{
		case REGULAR:
			printf("Regular\n");
			break;
		case ADMINISTRATOR:
			printf("Administrator\n");
			break;
	}
	
	printf("status: ");
	switch( ci->status )
	{
		case VISIBLE:
			printf("Visible\n");
			break;
		case INVISIBLE:
			printf("Invisible\n");
			break;
	}
	
	return;
}

