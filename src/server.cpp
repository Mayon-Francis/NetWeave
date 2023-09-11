#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "signal.h"
#include "sys/time.h"
#include "sys/epoll.h"
#include "thread"
#include "mutex"
#include "condition_variable"
#include "sys/eventfd.h"
#include "atomic"

// Custom
#include "ip/server_ip.cpp"
#include "debug_log/debug.cpp"
#include "worker/pool.cpp"
#include "thread_id.cpp"
#include "logger.cpp"

#define PORT 5000
#define MAX_MSG_LEN 256
#define MAX_CLIENTS 5
#define INFINITE_WAIT -1

int stop_fd = eventfd(0, 0);
int cliSockFds[MAX_CLIENTS], cliCount = -1, masterSockFd;
char usernames[MAX_CLIENTS][50];
int e_fd = epoll_create(MAX_CLIENTS);
struct epoll_event event;
struct epoll_event events[MAX_CLIENTS];

std::mutex all_cli_mutex;
std::condition_variable all_cli_cond;
std::atomic_bool all_cli_avail(true);

std::thread monitorThread;
std::thread monitorThread2;
bool stop = false;

void interruptHandler(int sig)
{
    stop = true;
    logger.info("Caught signal %d, will Exit\n", sig);
    int written = eventfd_write(stop_fd, (eventfd_t)1);
    written = eventfd_write(stop_fd, (eventfd_t)1);

    if (written == -1)
    {
        logger.crash("Couldn't signal stop");
    }
}

/**
 * Add a client to the array of client socket fds
 * Returns the index of the client socket fd in the array
 */
int addClient(int sockFd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] == -1)
        {
            cliSockFds[i] = sockFd;
            return i;
        }
    }
    logger.info("ERROR: Too many clients");
    return -1;
}

int getIndexFromFd(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1 && cliSockFds[i] == fd)
        {
            return i;
        }
    }
    return -1;
}

void removeClient(int sockFd)
{
    for (int i = 0; i <= MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] == sockFd)
        {
            cliSockFds[i] = -1;
            return;
        }
    }
    logger.error("ERROR: Client not found\n");
    return;
}

int sendMessageToSockFd(char msg[], int sockFd)
{
    int ret = send(sockFd, msg, strlen(msg), 0);

    if (ret < 0)
    {
        logger.error("ERROR writing to socket");
    }
    return ret;
}

void sendMessageToAllClientsWithoutLock(char msg[], int exceptSockFd)
{
    debug("Sending message to all clients except %d\n", exceptSockFd);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (cliSockFds[i] != -1 && cliSockFds[i] != exceptSockFd)
        {
            debug("In send to all, sent to %d\n", i);
            sendMessageToSockFd(msg, cliSockFds[i]);
        }
    }
    debug("Sending message to all clients except %d complete\n", exceptSockFd);
}

void sendMessageToAllClientsWithLock(char msg[], int exceptSockFd)
{
    std::unique_lock<std::mutex> lock(all_cli_mutex);
    all_cli_cond.wait(lock, [&]()
                      { return !!all_cli_avail; });
    all_cli_avail = false;
    sendMessageToAllClientsWithoutLock(msg, exceptSockFd);
    all_cli_avail = true;
}

void tryAcceptNewClient(int newSockFd)
{
    std::unique_lock<std::mutex> lock(all_cli_mutex);
    all_cli_cond.wait(lock, [&]()
                      { return !!all_cli_avail; });
    all_cli_avail = false;
    int index = addClient(newSockFd);
    debug("Client added at index %d\n", index);
    if (index == -1)
    {
        send(newSockFd, "reject: too many clients", 24, 0);
        close(newSockFd);
        logger.info("Client rejected\n");
    }

    debug("Sending Success msg");
    send(newSockFd, "success", 7, 0);
    event.data.fd = newSockFd;
    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    int s = epoll_ctl(e_fd, EPOLL_CTL_ADD, newSockFd, &event);
    if (s == -1)
    {
        logger.error("EPOLL_CTL_ADD");
    }
    all_cli_avail = true;
}

void disconnectClient(int sockFd)
{
    std::unique_lock<std::mutex> lock(all_cli_mutex);
    all_cli_cond.wait(lock, [&]()
                      { return !!all_cli_avail; });

    all_cli_avail = false;
    int s = epoll_ctl(e_fd, EPOLL_CTL_DEL, sockFd, &event);
    if (s == -1)
    {
        logger.crash("EPOLL_CTL_DEL");
    }
    close(sockFd);

    int index = getIndexFromFd(sockFd);
    char broadcastMsg[MAX_MSG_LEN];
    int ret = sprintf(broadcastMsg, "%s has disconnected", usernames[index]);
    if (ret < 0)
        logger.error("ERROR in sprintf");
    sendMessageToAllClientsWithoutLock(broadcastMsg, sockFd);
    removeClient(sockFd);

    all_cli_avail = true;
}

void clientMonitor()
{
    while (true)
    {
        int e_r_val = epoll_wait(e_fd, events, MAX_CLIENTS, 1000);
        if (stop)
        {
            return;
        }
        if (e_r_val == -1)
        {
            logger.crash("Epoll Returned error");
        }
        else if (e_r_val == 0)
        {
            // Timeout
        }
        else if (e_r_val < 0)
        {
            logger.crash("unexpected less than 0");
        }
        else
        {
            debug("Thread %zu Recieved epoll event\n", this_thread_id);
            for (int i = 0; i < e_r_val; i++)
            {
                int sockFd = events[i].data.fd;
                if (sockFd == stop_fd)
                {
                    debug("Stopping monitor thread %zu \n", this_thread_id);
                    return;
                }
                else if (sockFd == masterSockFd)
                {
                    struct sockaddr_in cliAddr;
                    socklen_t cliLen = sizeof(cliAddr);
                    int newSockFd = accept(masterSockFd, (struct sockaddr *)&cliAddr, &cliLen);
                    if (newSockFd < 0)
                    {
                        logger.error("ERROR on accept");
                        continue;
                    }
                    tryAcceptNewClient(newSockFd);
                }
                else if (
                    (events[i].events & EPOLLHUP) ||
                    (events[i].events & EPOLLRDHUP))
                {
                    logger.info("Client Disconnected");
                    disconnectClient(sockFd);
                    continue;
                }
                else if ((events[i].events & EPOLLERR) ||
                         (!(events[i].events & EPOLLIN)))
                {
                    logger.error("epoll error");
                    int s = epoll_ctl(e_fd, EPOLL_CTL_DEL, sockFd, &event);
                    if (s == -1)
                    {
                        logger.crash("EPOLL_CTL_DEL");
                    }
                    disconnectClient(sockFd);
                    continue;
                }
                else
                {

                    char buffer[MAX_MSG_LEN];
                    bzero(buffer, MAX_MSG_LEN);

                    int n = recv(sockFd, buffer, MAX_MSG_LEN - 1, 0);
                    if (n < 0)
                    {
                        perror("ERROR reading from socket");
                        disconnectClient(sockFd);
                    }
                    if (n == 0)
                    {
                        logger.info("Client disconnected\n");
                        disconnectClient(sockFd);
                    }

                    /**
                     * Remove the newline character from the end of the message
                     */
                    buffer[strcspn(buffer, "\n")] = 0;

                    if (strcmp(buffer, "exit") == 0)
                    {
                        logger.info("Client disconnected through exit command\n");
                        disconnectClient(sockFd);
                    }

                    if (strncmp(buffer, "username:", 9) == 0)
                    {
                        char *username = buffer + 9;
                        logger.info("%s joined the chat!\n", username);
                        int index = getIndexFromFd(sockFd);
                        strcpy(usernames[index], username);
                        char broadcast[MAX_MSG_LEN];
                        int ret = snprintf(broadcast, MAX_MSG_LEN, "%s joined the chat!", username);
                        if (ret < 0)
                            logger.error("Error in snprintf");
                        char ackUserSuccess[22] = "ack:username:success\0";
                        sendMessageToSockFd(ackUserSuccess, sockFd);
                        sendMessageToAllClientsWithLock(broadcast, sockFd);
                    }

                    if (strncmp(buffer, "chat:", 5) == 0)
                    {
                        /**
                         *  Incoming format: chat:[id]:[username]:message
                         */
                        char *msgId = strtok(buffer + 5, ":");

                        /**
                         *  Broadcast format: chat:[username]:message
                         */
                        char *username = strtok(NULL, ":");
                        /**
                         * +1 for the first colon
                         */
                        char *msg = username + strlen(username) + 1;
                        char broadcast[MAX_MSG_LEN];
                        int ret = snprintf(broadcast, MAX_MSG_LEN, "chat:%s:%s", username, msg);
                        if (ret < 0)
                            logger.error("Error in snprintf");

                        int index = getIndexFromFd(sockFd);
                        sendMessageToAllClientsWithLock(broadcast, sockFd);

                        /**
                         *  Ack format: ack:chat:[id]
                         */
                        char payload[MAX_MSG_LEN] = "ack:chat:";
                        strcat(payload, msgId);
                        sendMessageToSockFd(payload, sockFd);
                    }
                }
            }
        }
    }
}

int main()
{
    signal(SIGINT, interruptHandler);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        cliSockFds[i] = -1;
    }

    masterSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (masterSockFd < 0)
    {
        logger.crash("ERROR opening socket");
    }

    struct sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    servAddr.sin_port = htons(PORT);

    if (bind(masterSockFd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        logger.crash("ERROR on binding");
    }

    /**
     * All thread must be woken up to stop
     */
    event.data.fd = stop_fd;
    event.events = EPOLLIN | EPOLLET;
    int s = epoll_ctl(e_fd, EPOLL_CTL_ADD, stop_fd, &event);
    if (s == -1)
    {
        logger.crash("EPOLL_CTL_ADD stop_fd");
    }

    event.data.fd = masterSockFd;
    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    s = epoll_ctl(e_fd, EPOLL_CTL_ADD, masterSockFd, &event);
    if (s == -1)
    {
        logger.crash("EPOLL_CTL_ADD masterSockFd");
    }

    int e_main_stop = epoll_create(1);
    struct epoll_event e_stop_events[1];
    struct epoll_event e_stop_event;
    e_stop_event.events = EPOLLIN | EPOLLERR;
    e_stop_event.data.fd = stop_fd;
    epoll_ctl(e_main_stop, EPOLL_CTL_ADD, stop_fd, &e_stop_event);

    int workerCount = std::thread::hardware_concurrency() / 2;
    if (workerCount < 1)
    {
        workerCount = 1;
    }
    WorkerPool<void(), void> pool(workerCount);

    for (int i = 0; i < workerCount; i++)
    {
        pool.add_task(&clientMonitor);
    }

    if (listen(masterSockFd, MAX_CLIENTS) == -1)
    {
        logger.crash("ERROR on listen");
    }

    printServerIpAddresses();
    logger.info("Server is listening on port %d\n", PORT);

    while (true)
    {
        int n = epoll_wait(e_main_stop, e_stop_events, 1, 1500);
        if (stop)
        {
            break;
        }
    }

    close(e_fd);
    close(masterSockFd);
}