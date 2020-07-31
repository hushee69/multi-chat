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

#define SERVER				"0.0.0.0"
#define PORT				"6666"
#define MAX_BUFF			512
#define PSEUDO_LEN			15
#define MAX_CLIENTS			12

#define LIST				"/list"
#define KICK				"/kick"
#define CHANGE				"/change"

const char *client_joined = "Server: [%s] has joined the chat\n";
const char *client_left = "Server: [%s] has left the chat\n";
const char *etoiles = "****************************************************************";

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
	char pseudo[PSEUDO_LEN];
	client_type type;
	client_status status;
} client_info;

void print_client_info(client_info *ci);

void die_error(const char *msg)
{
	perror(msg);
	exit(-1);
}

int recv_client_info(client_info *ci)
{
	// recv pseudo first
	memset(&ci->pseudo, 0, PSEUDO_LEN);
	int bytes_recvd = recv(ci->sock, ci->pseudo, PSEUDO_LEN, 0);
	if( bytes_recvd == -1 )
	{
		die_error("recv client info - pseudo");
	}
	fprintf(stderr, "pseudo: %s\n", ci->pseudo);
	
	// recv client type - administrator, regular, ..
	bytes_recvd = recv(ci->sock, &(ci->type), sizeof(ci->type), 0);
	if( bytes_recvd == -1 )
	{
		die_error("recv client info - type");
	}
	// check type in case of connection error and receiving incorrect data
	// if more user types are added, they need to be checked here
	if( ci->type != REGULAR && ci->type != ADMINISTRATOR )
	{
		fprintf(stderr, "error in connection - type\n");
		return -1;
	}
	fprintf(stderr, "type is %d\n", ci->type);
	
	// recv client status - visible, invisible, ..
	bytes_recvd = recv(ci->sock, &(ci->status), sizeof(ci->status), 0);
	if( bytes_recvd == -1 )
	{
		die_error("recv client info - status");
	}
	if( ci->status != VISIBLE && ci->status != INVISIBLE )
	{
		fprintf(stderr, "error in connection - status\n");
		return -1;
	}
	fprintf(stderr, "status is %d\n", ci->status);
	
	return 0;
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

/*
 * @params
 * clients_list: the client list
 * server_socket: server socket (used for accept()'ing connections)
 * nb_clients: total number of clients
 * max_fd: highest socket in select
*/
int add_client_to_list(client_info *clients_list, int server_socket, int *nb_clients, int *max_fd)
{
	char *welcome_message = calloc(MAX_BUFF * 3, sizeof(char));
	client_info ci;
	
	memset(ci.ip, '\0', INET6_ADDRSTRLEN);
	memset(ci.ip, '\0', 5);
	ci.sock = accept(server_socket, NULL, NULL);
	if( ci.sock == -1 )
	{
		perror("add client");
		return -1;
	}
	
	int res = recv_client_info(&ci);
	if( res == -1 )
	{
		return -1;
	}
	
	int cur = *nb_clients;
	clients_list[cur] = ci;
	// debug line
	print_client_info(&clients_list[cur]);
	// send client welcome message
	strncat(welcome_message, etoiles, strlen(etoiles));
	strncat(welcome_message, "\n", 1);
	strcat(welcome_message, "*\tWelcome ");
	strncat(welcome_message, ci.pseudo, strlen(ci.pseudo));
	strcat(welcome_message, " - to the chat of Nantes University\t*\n");
	strncat(welcome_message, etoiles, strlen(etoiles));
	send_message(ci.sock, welcome_message);
	
	cur += 1;
	*nb_clients = cur;
	if( *max_fd < ci.sock )
	{
		*max_fd = ci.sock;
	}
	
	return (cur - 1);				// last person that joined
}

/*
 * @params
 * clients: list of clients
 * to_remove: the client to remove
 * nb_clients: number of clients, this parameter is modified
*/
void remove_client_from_list(client_info *clients, int to_remove, int *nb_clients)
{
	close(clients[to_remove].sock);
	// need to modify the clients list since we have removed a client
	int i, total = *nb_clients - 1;
	for( i = to_remove; i < total; ++i )
	{
		clients[i] = clients[i + 1];
	}
	*nb_clients = total;
	
	return;
}

int recv_message(int csock, char *out_buffer, int size)
{
	return recv(csock, out_buffer, size, 0);
}

/*
 * @params
 * clients: clients list
 * exclude: don't send to this client
*/
void send_to_all_clients(client_info *clients, int nb_clients, char *message, int exclude)
{
	int i = 0;
	for( i = 0; i < nb_clients; ++i )
	{
		if( i != exclude )
		{
			send_message(clients[i].sock, message);
		}
	}
	
	return;
}

void send_list_of_clients(int which_socket, client_info *clients, int nb_clients)
{
	// try to malloc enough space for pseudos and newlines
	int names_buffer_len = (nb_clients * PSEUDO_LEN) + (PSEUDO_LEN * 2);
	char *names_buffer = calloc(names_buffer_len, sizeof(char));
	
	int i;
	for( i = 0; i < nb_clients; ++i )
	{
		if( clients[i].status == VISIBLE )
		{
			strncat(names_buffer, clients[i].pseudo, strlen(clients[i].pseudo));
			strncat(names_buffer, "\n", 1);
		}
	}
	
	// send the list of clients
	send_message(which_socket, names_buffer);
	
	// no longer needed
	free(names_buffer);
	names_buffer = NULL;
	
	return;
}

// search until first occurrence of specified char
/*
 * @params
 * text: the string in which to find first occurrence of character
 * c: the character to find
 * return value: returns positive integer if character found, -1 otherwise
*/
int find_until(char *text, char c)
{
	int index = 0, found = -1;
	char *tmp = text;
	while( tmp[index] != '\0' )
	{
		if( tmp[index] == c )
		{
			found = index;
			break;
		}
		index++;
	}
	
	return found;
}

// find user in list of clients and return index
// used for sending private messages and kicking out users
int find_user_index(client_info *clients, int nb_clients, char *pseudo)
{
	int i, index = -1;
	for( i = 0; i < nb_clients; ++i )
	{
		if( !strncmp(clients[i].pseudo, pseudo, strlen(clients[i].pseudo)) )
		{
			// found the pseudo
			index = i;
		}
	}
	
	return index;
}

int main(int argc, char **argv)
{
	if( argc != 2 )
	{
		fprintf(stderr, "Usage: %s [port to listen on]\n", argv[0]);
		exit(-1);
	}
	
	int server_sock;
	int client_sock;
	
	struct sockaddr_storage their_addr;
	socklen_t addrlen;
	
	char their_ip[INET6_ADDRSTRLEN];
	
	int status;
	
	struct addrinfo *addrinfo, hints;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	
	status = getaddrinfo(SERVER, argv[1], &hints, &addrinfo);
	if( status != 0 )
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(-1);
	}
	
	// create server socket
	server_sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if( server_sock == -1 )
	{
		die_error("server socket");
	}
	
	status = bind(server_sock, addrinfo->ai_addr, addrinfo->ai_addrlen);
	if( status == -1 )
	{
		die_error("bind");
	}
	
	freeaddrinfo(addrinfo);
	
	status = listen(server_sock, SOMAXCONN);
	
	int i, j, bytes_recvd, bytes_sent;
	char buf[MAX_BUFF];
	
	client_info clients[MAX_CLIENTS];
	int nb_clients = 0, client_no;
	char message_buf[MAX_BUFF];
	
	fd_set read_fds;
	int fdmax = server_sock;
	
	char *joined = calloc(MAX_BUFF, sizeof(char));
	char *left = calloc(MAX_BUFF, sizeof(char));
	
	fprintf(stderr, "IP address: %s\nPort: %s\n", SERVER, argv[1]);
	fprintf(stderr, "Server set up - waiting for incoming connections\n");
	
	for( ;; )
	{
		FD_ZERO(&read_fds);
		FD_SET(server_sock, &read_fds);
		for( i = 0; i < nb_clients; ++i )
		{
			FD_SET(clients[i].sock, &read_fds);
		}
		status = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
		if( status == -1 )
		{
			die_error("select");
		}
		
		if( FD_ISSET(server_sock, &read_fds) )
		{
			// listener has got connection
			if( nb_clients < MAX_CLIENTS )
			{
				// we can still add more clients
				client_no = add_client_to_list(clients, server_sock, &nb_clients, &fdmax);
				if( client_no > -1 )
				{
					snprintf(joined, MAX_BUFF, "Server: [%s] has joined the chat", clients[client_no].pseudo);
					if( clients[client_no].status != INVISIBLE )
					{
						send_to_all_clients(clients, nb_clients, joined, client_no);
					}
				}
			}
			else
			{
				fprintf(stderr, "We don't have any more space to welcome visitors\n");
				FD_CLR(server_sock, &read_fds);
			}
		}
		for( i = 0; i < nb_clients; ++i )
		{
			// go through the clients list and see if any of them have sent a message
			if( FD_ISSET(clients[i].sock, &read_fds) )
			{
				// found a message
				memset(&message_buf, 0, MAX_BUFF);
				bytes_recvd = recv_message(clients[i].sock, message_buf, MAX_BUFF);
				if( bytes_recvd <= 0 )
				{
					snprintf(left, MAX_BUFF, "Server: [%s] has left the chat", clients[i].pseudo);
					remove_client_from_list(clients, i, &nb_clients);
					if( clients[i].status != INVISIBLE )
					{
						send_to_all_clients(clients, nb_clients, left, -1);
					}
				}
				else
				{
					// compare the message with special command
					if( !strncmp(message_buf, LIST, strlen(LIST)) )
					{
						// list command found
						send_list_of_clients(clients[i].sock, clients, nb_clients);
					}
					else if( !strncmp(message_buf, "@", 1) )
					{
						// find index of pseudo if it exists
						int pseudo_index = find_user_index(clients, nb_clients, message_buf + 1);
						if( pseudo_index > -1 )
						{
							send_message(clients[pseudo_index].sock, message_buf);
						}
					}
					else if( !strncmp(message_buf, KICK, strlen(KICK)) && clients[i].type == ADMINISTRATOR )
					{
						int pseudo_index = find_user_index(clients, nb_clients, message_buf + strlen(KICK) + 1);
						if( pseudo_index > -1 )
						{
							const char *kicked_message = "You have been kicked out from the chat";
							send_message(clients[pseudo_index].sock, kicked_message);
							remove_client_from_list(clients, pseudo_index, &nb_clients);
						}
					}
					else if( !strncmp(message_buf, CHANGE, strlen(CHANGE)) )
					{
						char *updated_pseudo_msg = calloc(MAX_BUFF, sizeof(char));
						
						strncat(updated_pseudo_msg, clients[i].pseudo, strlen(clients[i].pseudo));
						// change the pseudo and update it in the clients list
						memset(clients[i].pseudo, '\0', PSEUDO_LEN);
						strncpy(clients[i].pseudo, message_buf + strlen(CHANGE) + 1, PSEUDO_LEN);
						
						if( clients[i].status != INVISIBLE )
						{
							// inform on name change if and only if the client is visible to others
							// send message to all clients informing about the change
							const char *changed_pseudo = " has changed their pseudo to ";
							strncat(updated_pseudo_msg, changed_pseudo, strlen(changed_pseudo));
							strncat(updated_pseudo_msg, clients[i].pseudo, strlen(clients[i].pseudo));
							
							send_to_all_clients(clients, nb_clients, updated_pseudo_msg, i);
						}
					}
					else
					{
						send_to_all_clients(clients, nb_clients, message_buf, i);
					}
				}
			}
		}
	}
	
	return 0;
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

