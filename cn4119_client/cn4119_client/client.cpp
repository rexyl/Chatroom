#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <thread>
#include <iostream>
#define TIME_OUT 30

time_t timer;
bool timeout;
int sockfd;
void error(char *msg)
{
    perror(msg);
    _exit(0);
}
void sighandler(int sig)
{
    std::cout<< "\nYou press control+c, closing client...\n";
    write(sockfd, "logout", 6);
    close(sockfd);
    exit(0);
}
void printer()
{
    char buffer[256];
    while (1) {
        bzero(buffer,256);
        int n = read(sockfd,buffer,255);
        if (n < 0)
            error("ERROR reading from socket");
        printf("\n%s",buffer);
        //printf("Command: ");
    }
    //printf("Command: ");

}
void timeout_alarm()
{
    int sec = difftime(time(NULL), timer);
    while (sec / 60 < TIME_OUT  ) {
        sleep(TIME_OUT*60 - sec);
        sec = difftime(time(NULL), timer);
    }
    printf("Inactive for %d mins, please login agian!\n",TIME_OUT);
    write(sockfd, "logout", 6);
    timeout = 1;
    close(sockfd);
    return;
}
int main(int argc, char *argv[])
{
    int portno, n;
    /********************************/
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    /********************************/
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    char buffer[256];
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        _exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    
    printf("Please enter the username: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0)
        error("ERROR writing to socket");
    
    printf("Please enter the password: ");
    bzero(buffer,256);
    fgets(buffer,255,stdin);
    n = write(sockfd,buffer,strlen(buffer));
    if (n < 0)
        error("ERROR writing to socket");
    
    bzero(buffer,256); //read feedback (welcome message or block or wrong username)
    n = read(sockfd,buffer,255);
    if (n < 0)
        error("ERROR reading from socket");
    
    int trial = 1;
    while(!strncmp(buffer, "Password",8))  //Password is not correct, try again
    {
        printf("%s\n",buffer);
        printf("Please enter the password: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        n = write(sockfd,buffer,strlen(buffer));
        if (n < 0)
        error("ERROR writing to socket");
        trial++;
        bzero(buffer, 256);
        n = read(sockfd, buffer, 255);
        if (trial == 3 && !strncmp(buffer, "Password", 8) ) {  //You have try 3 times, lock
            printf("%s\n",buffer);
            close(sockfd);
            return 0;
        }
    }
    printf("%s\n",buffer);
    if (!strncmp(buffer, "This", 4)) {  //This user name+ip combination still in LOCK TIME
        close(sockfd);
        return 0;
    }
    if (!strncmp(buffer, "User", 4)) {  //User already logged in
        close(sockfd);
        return 0;
    }
    
    if (!strncmp(buffer, "Sorry", 5)) {  //Sorry, user does not exist
        close(sockfd);
        return 0;
    }
    
    timer = time(NULL); //record time when log in
    std::thread print_thread(printer) , timeout_ala(timeout_alarm);
    print_thread.detach();
    timeout_ala.detach();
    timeout = 0;
    while(1)
    {
        printf("Command: ");
        bzero(buffer,256);
        fgets(buffer,255,stdin);
        if (timeout) {
            break;
        }
//        if (difftime(time(NULL), timer) / 60 > TIME_OUT  ) {
//            printf("Inactive for %d mins, please login agian!\n",TIME_OUT);
//            write(sockfd, "logout", 6);
//            break;
//        }
//        timer = time(NULL); // if not timeout, refresh timer
        
        if (!strncmp(buffer, "logout", 6)) {
            n = write(sockfd,buffer,strlen(buffer));
            if (n < 0)
                continue;
            break;
        }
        if ( strncmp(buffer, "broadcast message ", 18)
            && strncmp(buffer, "whoelse", 7)
            && strncmp(buffer, "wholast ", 8)
            && strncmp(buffer, "broadcast user ", 15)
            && strncmp(buffer, "message ", 8))
        {
            std::cout<<"Usage: "
            <<"whoelse\n"
            <<"wholast <number>\n"
            <<"broadcast message <message>\n"
            <<"broadcast user <user> <user> ... <user> message <message>\n"
            <<"message <user> <message>\n"
            <<"logout"<<std::endl;
            continue;
        }
        n = write(sockfd,buffer,strlen(buffer));
        if (n < 0)
            continue;
    }
    return 0;
}
//
//void timeout()
//{
//    while (difftime(time(NULL), timer) / 60 <= TIME_OUT ) {
//        sleep(1);
//    }
//    return;
//}