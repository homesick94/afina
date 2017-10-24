#ifndef AFINA_NETWORK_BLOCKING_SERVER_H
#define AFINA_NETWORK_BLOCKING_SERVER_H

#include <atomic>
#include <vector>
#include <pthread.h>

#include <afina/network/Server.h>

namespace Afina {
namespace Network {
namespace Blocking {

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */

struct single_worker;

class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps);
    ~ServerImpl();

    // See Server.h
    void Start(uint32_t port, uint16_t workers) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void RunAcceptor();

    /**
     * Method is running for each connection
     */
    void RunConnection(int socket, int idx);

private:
    static void *RunAcceptorProxy(void *p);
    static void *RunConnectionProxy(void *p);
    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publish changes cross thread
    // bounds
    std::atomic<bool> running;

    // Thread that is accepting new connections
    pthread_t accept_thread;

    // Maximum number of client allowed to exists concurrently
    // on server, permits access only from inside of accept_thread.
    // Read-only
    uint16_t max_workers;

    // Port to listen for new connections, permits access only from
    // inside of accept_thread
    // Read-only
    uint32_t listen_port;

    // Threads that are processing connection data, permits
    // access only from inside of accept_thread
    std::vector<pthread_t> connections;

    std::vector<std::atomic_bool> finished_workers;
    std::vector<single_worker> connection_workers;
};

// struct for pthread
struct single_worker
{
  ServerImpl *parent_server = nullptr;
  int socket = -1;
  int idx = -1;

  single_worker (ServerImpl *serv, int sock, int i) : parent_server (serv), socket (sock), idx (i)
  {
  }
};

} // namespace Blocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_BLOCKING_SERVER_H
