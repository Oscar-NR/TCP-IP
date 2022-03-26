

#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<errno.h>


#define PORT 5200 		//Default port of the server
#define BUFFER_SIZE 512	//Size of the buffer for recieve and send messages

//the local machine IP will be used when the program is executed without specify the IP
const char DEFAULT_IP_SERVER[]="127.0.0.1";


//Funcion to copy the set of descriptors, because the select will modify the same
void copy_fdset(fd_set* destiny, fd_set* origen, int biggest_sd_check)
{
	int i;
	FD_ZERO(destiny);
	for(i=0; i<biggest_sd_check;i++)
	{
		if( FD_ISSET(i,origen) )
		{
			FD_SET(i, destiny);
		}	
	}
}

//Write can fail and write less character than should, this custom function take care of all the characters are writed
ssize_t custom_write(int fd, char* message, size_t message_lenght)
{
	ssize_t to_write = message_lenght;
	ssize_t total_writed = 0;
	ssize_t writed;
	do
	{
		errno= 0;
		writed = write(fd, message+ total_writed, to_write);
		if(writed>= 0)
		{
			total_writed+= writed;
			to_write -= writed;		
		}
		
	}while(
		( (writed>0) && (total_writed< message_lenght)) ||
		(errno == EINTR));
	
	if(total_writed>0)
	{
		return total_writed;	
	}else
	{
		//Maybe an error happened, we can return that with this
		return writed;
	}
}



int main(int argc, char* argv[])
{
	char buffer[BUFFER_SIZE];
	uint32_t IP_direction;

	int connect_sd;				//socket descriptor which will conect to the server	
	
	int received, sended;		//to made the code easier to read
	int select_result;			//to made the select function more readable
	
	struct sockaddr_in server_address;	
	
	fd_set original_set, modified_set;
	
	
	//Check if the program have received a IP direction as parameter  or should use the default IP direction
	if(argc <2)
	{
		IP_direction = inet_addr(DEFAULT_IP_SERVER);
	}else
	{
		IP_direction = inet_addr(argv[1]);
		if(IP_direction < 0)
		{
			perror("error in the IP assignation");
			exit(1);		
		}	
	}
	
	//Create the socket to conect with the server
	connect_sd = socket(PF_INET, SOCK_STREAM, 0);
	if(socket < 0)
	{
		perror("error creating the connection socket");
		exit(1);		
	}	

	//Filling the address
	memset(&server_address, 0, sizeof(server_address));	//Cleaning the memory
	server_address.sin_family = AF_INET;				//select the IPv4 of the protocols
	memcpy(&server_address.sin_addr, &IP_direction, 4);	//Copy the IP direction
	server_address.sin_port = htons(PORT);				//the number of the port, doing htons to avoid problems with the big/little endian
	
	//Connecting to the server
	if(  connect( connect_sd, (struct sockaddr*) &server_address, sizeof(server_address) ) <0   )
	{
		perror("error in the connection with the server");
		exit(1);		
	}	
	
	//Similar as in the server but only one connection and the file descriptor of the keyboard, so select will unlock when the user write in the keyboard
	FD_ZERO(&original_set);
	FD_SET(connect_sd, &original_set);
	FD_SET(0, &original_set);
	
	printf("Welcome to the chat\n");
	
	while(1)
	{
		
		copy_fdset(&modified_set, &original_set, connect_sd+1);
	
		select_result = select(connect_sd+1, &modified_set, NULL, NULL, NULL);	
		
		//if we don't use select_result, the select function and the check for error would be harder to read
		if(select_result < 0)
		{
			perror("error in the select");
			exit(1);
		}
		
		//Check if the program wake because an incoming message
		if( FD_ISSET(connect_sd, &modified_set) )
		{
			received = recv(connect_sd, buffer, BUFFER_SIZE, 0);
			if(received > 0)
			{
				//add the End of string character and show in the console
				buffer[received]='\0';
				printf("-%s\n", buffer);
			}else
			{
				if(received == 0)
				{
					//This means that server has closed the connection
					close(connect_sd);
					printf("The server has closed the chat\n");
					return 0;				
				}
				else
				{
					//if the code reach this point means that a error happens in the reception of the message
					perror("A error happens in the reception of the message");
					close(connect_sd);
					//Finish the execution because an error happened
					exit(1);
				}			
			}
		
		}
		
		//Check if the program has wake up because the user has writed a message
		if( FD_ISSET(0, &modified_set) )
		{
			received = read(0, buffer, BUFFER_SIZE);
			if(received>0)
			{
				//Send the message to the server
				sended = custom_write(connect_sd, buffer, received);
				if(sended != received)
				{
					perror("Error sending the message");
				}				
			}	
		}
	
		
	}
	
	
	return 0;
}



















