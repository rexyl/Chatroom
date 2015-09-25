/* A simple chat server-client mode in the internet domain using TCP
 The port number is passed as an argument
 This version runs forever, forking off a separate
 process for each connection
g++ -std=c++11 socket_server.cpp
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <fstream>
#include <unordered_map>
#include <thread>
#include <pthread.h>
#include <iostream>
#include <sstream>

#define LOCK_TIME 5
#define LAST_HOUR 60
#define BLOCK 0
#define WRONG_USER 1
#define CORRECT 2

std::unordered_map<std::string , std::string> m;
std::unordered_map<std::string, time_t> last_list;
std::unordered_map<std::string,in_addr_t> prison;
std::unordered_map<std::string,int> contact_list;
int sockfd;

int authentication(int , in_addr_t);
void alarm_handler(int, std::pair<std::string, in_addr_t>);
void exec_chat(int , std::string);
void sighandler(int sig)
{
    std::cout<< "\nYou press control+c, closing server...\n";
    for (auto& x: contact_list) {     //close all socket that connect to server
        close(x.second);
    }
    close(sockfd);                      //close listening socket
    exit(0);
}
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    /******** handle control+c ******************************/
    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);
    /******** load txt to unordered_map *********************/
    //std::unordered_map<std::string , std::string> m;
    std::string s1,s2;
    std::ifstream ifs;
    ifs.open("/Users/Rex/Desktop/cn4119/cn4119/user_pass.txt");
    while (ifs>>s1 && ifs>>s2)
        m.insert({s1,s2});
    /*****************************************/
    
    int newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
//    std::unordered_map<std::string,in_addr_t> prison;
//    std::unordered_map<std::string,int> contact_list;
//    //std::unordered_map<std::string, time_t> last_list;
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        //exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //domain, type, protocol
    
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;                     //define for ipv4
    serv_addr.sin_port = htons(portno);                         //convert small endian(PC machine) to big endien(net work byte order)
    if (bind(sockfd, (struct sockaddr *) &serv_addr,            //sockfd now stands for serv_addr
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd,5);                                           //5 is capacity of connection
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);
        if (newsockfd < 0)
        {
            error("ERROR on accept");
        }
        std::thread authen_thread(authentication,newsockfd, cli_addr.sin_addr.s_addr);
        authen_thread.detach();
        
    } /* end of while */
    return 0;
}

int authentication(int sock , in_addr_t s_addr ) //(contact_name,sock)
{
    int n;
    char buffer[256];
    bzero(buffer,256);
    
    n = read(sock,buffer,255);
    if (n < 0) error("ERROR reading from socket");
    std::string usr(buffer,255);
    
    n = read(sock,buffer,255);
    if (n < 0) error("ERROR reading from socket");
    std::string pwd(buffer,255);
    usr = usr.substr(0,usr.find('\n'));
    pwd = pwd.substr(0,pwd.find('\n'));
    /******** find usr+ip is in prison or not*****/
    std::unordered_map<std::string,in_addr_t>::iterator p = prison.find(usr);
    /********************************************/
    
    /******** find usr is logged in or not*****/
    std::unordered_map<std::string,int>::iterator l = contact_list.find(usr);
    /********************************************/
    std::unordered_map<std::string,std::string>::const_iterator got = m.find (usr);
    int trial = 1;
    if (got == m.end()) {
        //std::cout<<"Sorry, user does not exist"<<std::endl;
        n = write(sock, "Sorry, user does not exist\n", 27);
        if(n<0) error("ERROR writing to socket");
        close(sock);
        return WRONG_USER;
    }
    else if(p!= prison.end()) {
        if (p->second == s_addr) {        //user in prison
            n = write(sock, "This user name+ip combination still in LOCK TIME\n", 49);
            if (n<0) error("ERROR writring to socket");
        }
    }
    else if(l!= contact_list.end())     //user already logged in
    {
        n = write(sock, "User already logged in\n", 23);
        if (n<0) error("ERROR writring to socket");
        return WRONG_USER;
    }
    else if(got->second==pwd)
    {
        
        std::cout<<"Succeed log in as \""<<got->first<<"\""<<std::endl;
        std::string wel_mes = "Welcome, "+ got->first+"\n";
        n = write(sock,wel_mes.c_str(),wel_mes.size());
        if (n < 0) error("ERROR writing to socket");
        std::thread chat(exec_chat,sock,usr);
        chat.detach();
        contact_list.insert({usr,sock});
        return CORRECT;
    }
    else
    {
        std::string temp(1,3-trial+'0');
        std::string s = "Password is not correct, try agian. "+ temp + " times left!\n";
        n = write(sock, s.c_str(), s.size());
        
        while (trial<3) {
            n = read(sock,buffer,255);
            if (n < 0) error("ERROR reading from socket");
            std::string pwd(buffer,255);
            pwd = pwd.substr(0,pwd.find('\n'));
            if (pwd==got->second) {
                std::cout<<"Succeed log in as \""<<got->first<<"\""<<std::endl;
                std::string wel_mes = "Welcome, "+ got->first+"\n";
                n = write(sock,wel_mes.c_str(),wel_mes.size());
                if (n < 0) error("ERROR writing to socket");
                std::thread chat(exec_chat,sock,usr);
                chat.detach();
                contact_list.insert({usr,sock});
                return CORRECT;
            }
            else
            {
                trial++;
                //std::cout<<"Password is not correct, try agian. "<<3-trial<<" times left!"<<std::endl;
                std::string temp(1,3-trial+'0');
                std::string s = "Password is not correct, try agian. "+ temp + " times left!\n";
                n = write(sock, s.c_str(), s.size());
                if(n<0) error("ERROR writing to socket");
                if(3-trial==0)
                {
                    int lock = LOCK_TIME;
                    //std::cout<<"You have try 3 times, try later after "<< lock <<" second(s)"<<std::endl;
                    std::string temp = std::to_string(lock);
                    std::string s = "You have try 3 times, try later after "+ temp + " seconds later!\n";
                    n = write(sock, s.c_str(), s.size());
                    if(n<0) error("ERROR writing to socket");
                    //block this ip+username pair
                    prison.insert({usr,s_addr});
                    std::pair<std::string, in_addr_t> prisoner({usr,s_addr});
                    int i = LOCK_TIME;
                    std::thread alarm(alarm_handler , LOCK_TIME, prisoner);
                    alarm.detach();
                    close(sock);
                    return BLOCK;
                }
            }
        }
    }
    
    return CORRECT;
}

void alarm_handler(int timer , std::pair<std::string, in_addr_t> prisoner)
{
    std::cout << "Prisoner "<<prisoner.first<<" lock for " << timer<<" seconds" << std::endl;
    sleep(timer);
    prison.erase(prisoner.first);
    return;
}

/******** exec_chat() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.q
 *****************************************/
void exec_chat (int sock , std::string usr)
{
    int n;
    char buffer[256];
    
    bzero(buffer,256);
    while(1)
    {
        bzero(buffer,256);
        n = read(sock,buffer,255);
        if (n < 0)
        {
            continue;
        }
        
        if (!strncmp(buffer, "logout", 6)) {
            std::cout<<"user \""<<usr<<"\" is going to logout\n"<<std::endl;
            contact_list.erase(usr);
            time_t now=time(NULL);
            last_list.insert({usr,now});
            break;
        }
        if (!strncmp(buffer, "whoelse", 7)) {
            std::string list;
            for(auto& x:contact_list)
            {
                if (x.first != usr) {
                    list += x.first+" ";
                }
            }
            list += "\n";
            n = write(sock,list.c_str(),list.size());
            if (n < 0) continue;
            continue;
        }
        if (!strncmp(buffer, "wholast", 7)) {
            std::string remain(buffer+7);
            remain = remain.substr(0,remain.find("\n"));
            int boundary = atoi(remain.c_str());
            std::string list;
            std::string s;
            for(auto& x:last_list)
            {
                time_t now = time(NULL);
                int time_in_min = difftime(now, x.second)/60;
                if (time_in_min <= boundary && time_in_min <= LAST_HOUR) {
                    s += x.first + " logged out " + std::to_string(time_in_min) + " mins before\n";
                }
                else if(time_in_min > LAST_HOUR)                     //logout for 60 mins or longer
                {
                    last_list.erase(x.first);
                }
            }
            for(auto &x:contact_list)
            {
                if (x.first != usr) {
                    s += x.first + " is now online\n";
                }
            }
            if (s.size()==0) {
                s = "No one was online within" + std::to_string(boundary) + " mins\n";
            }
            n = write(sock,s.c_str(),s.size());
            if (n < 0) continue;
            continue;
        }
        if (!strncmp(buffer, "broadcast message", 17)) {
            std::string remain(buffer+18);
            remain = remain.substr(0,remain.find("\n"));
            remain = "Message from " + usr + ": " + remain + "\n";
            //std::unordered_map<std::string, int> broadcast_list;
            for(auto& x:contact_list)
            {
                if (x.first != usr) {
                    n = write(x.second,remain.c_str(),remain.size());
                    if (n < 0) continue;
                }
            }
            continue;
        }
        if (!strncmp(buffer, "broadcast user", 14)) {
            //std::unordered_map<std::string, int> broadcast_list;
            std::string remain(buffer+15) , userlist;
            userlist = remain.substr(0,remain.find("message"));
            remain = remain.substr(remain.find("message") + 7);
            remain = remain.substr(0,remain.find("\n"));
            remain = "Message from " + usr + ": " + remain + "\n";
            std::stringstream ss;
            ss<<userlist;
            std::string temp;
            while (ss>>temp) {
                std::unordered_map<std::string, int>::iterator it = contact_list.find(temp);
                if (it != contact_list.end()) {
                    if (temp != usr) {
                        n = write(it->second,remain.c_str(),remain.size());
                        if (n < 0) continue;
                    }
                }
                else
                {
                    std::cout<<"User "<<temp<<" is not in contact list\n";
                }
            }
            continue;
        }
        if (!strncmp(buffer, "message", 7)) {
            //std::unordered_map<std::string, int> broadcast_list;
            std::string remain(buffer+8) , pri_user;
            pri_user = remain.substr(0,remain.find(" "));
            remain = remain.substr(remain.find(" ") + 1);
            remain = remain.substr(0,remain.find("\n"));
            remain = "Message from " + usr + ": " + remain + "\n";
            std::unordered_map<std::string, int>::iterator it = contact_list.find(pri_user);
            if (it != contact_list.end() && pri_user!=usr) {
                n = write(it->second,remain.c_str(),remain.size());
                if (n < 0) continue;
            }
            continue;
        }

    }
}