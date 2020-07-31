Multichat Client / Server Application

Authors
	JANDU Harry

Compilation instructions
Begin by compiling the server with the following command
	gcc -o serveur serveur.c

Then compile the client using the following command
	gcc -o client client.c

Execution instructions
Begin by executing the server
	./serveur [port sur laquelle on veut attendre les connexions]
For example: ./serveur 6666

Then execute several times the client executable in different terminals
	./client [port] [pseudo] [type] [status]
[port] - the port on which to connect
[pseudo] - the pseudo to connect with
[type] - 0 for a REGULAR user and 1 for an ADMINISTRATOR
[status] - 0 for VISIBLE and 1 for INVISIBLE

Examples:
	./client 6666 nada 1 0	=> Nada is an administrator with VISIBLE status
	./client 6666 alexandre 0 0 => Alexandre is a regular user with VISIBLE status
	./client 6666 jeanphilippe 0 1 => Jeanphilippe is a regular user with INVISIBLE status

Fonctionnalites
	/menu => shows the menu
	/kick [pseudo] => removes a user, only an administrator can kick out users
	/change [nouveau pseudo] => change username
	/quit => quit
	@[pseudo] [msg] => send a private message

