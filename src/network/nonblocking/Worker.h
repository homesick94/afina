#ifndef AFINA_NETWORK_NONBLOCKING_WORKER_H
#define AFINA_NETWORK_NONBLOCKING_WORKER_H

#include <memory>
#include <pthread.h>
#include <atomic>
#include <netinet/in.h>
#include <afina/Storage.h>
#include <map>
#include <protocol/Parser.h>

namespace Afina {

// Forward declaration, see afina/Storage.h
class Storage;

namespace Network {
namespace NonBlocking {

/**
 * # Thread running epoll
 * On Start spaws background thread that is doing epoll on the given server
 * socket and process incoming connections and its data
 */
class Worker {
public:
    Worker(std::shared_ptr<Afina::Storage> ps);

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker& operator=(const Worker&) volatile = delete;

    ~Worker();

    /**
     * Spaws new background thread that is doing epoll on the given server
     * socket. Once connection accepted it must be registered and being processed
     * on this thread
     */
    void Start(sockaddr_in &server_addr);

    /**
     * Signal background thread to stop. After that signal thread must stop to
     * accept new connections and must stop read new commands from existing. Once
     * all readed commands are executed and results are send back to client, thread
     * must stop
     */
    void Stop();

    /**
     * Blocks calling thread until background one for this worker is actually
     * been destoryed
     */
    void Join();

    /**
     * Read command and execute
     */
    std::string run_parser(std::string inc_buf, int socket);

    static void *RunProxy (void *p);

protected:
    /**
     * Method executing by background thread
     */
    void OnRun(int sfd);

private:
    pthread_t thread;

    std::atomic<bool> running;

    int serv_socket;

    std::shared_ptr<Afina::Storage> storage;

    Protocol::Parser parser;

    std::string sock_buffer = "";
};

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
#endif // AFINA_NETWORK_NONBLOCKING_WORKER_H
