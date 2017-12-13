#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Utils.h"

#include <unistd.h>
#include <sys/stat.h>

#include <netdb.h>
#include <protocol/Parser.h>
#include <afina/execute/Command.h>

#define MAX_EVENTS 10

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps) : running(false), storage (ps) {
    // TODO: implementation here
}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}


void *Worker::RunProxy(void *p) {
    Worker *srv = reinterpret_cast<Worker *>(p);
    try {
        srv->OnRun(srv->serv_socket);
    } catch (std::runtime_error &ex) {
        std::cerr << "Server fails: " << ex.what() << std::endl;
    }
    return 0;
}

// See Worker.h
void Worker::Start(sockaddr_in &server_addr) {
  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
  // TODO: implementation here

  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
  running.store(true);

  serv_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serv_socket == -1) {
      throw std::runtime_error("Failed to open socket");
    }

  int opts = 1;
  if (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(opts)) == -1) {
      close(serv_socket);
      throw std::runtime_error("Socket setsockopt() failed");
    }

//#ifdef SO_REUSEPORT
//  if (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEPORT, &opts, sizeof(opts)) == -1) {
//      close(serv_socket);
//      throw std::runtime_error("Socket setsockopt() failed");
//    }
//#endif

  if (bind(serv_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
      close(serv_socket);
      throw std::runtime_error("Socket bind() failed");
    }

  make_socket_non_blocking(serv_socket);

  if (listen(serv_socket, 5) == -1) {
      close(serv_socket);
      throw std::runtime_error("Socket listen() failed");
    }

  if (pthread_create(&thread, NULL, Worker::RunProxy, this) < 0) {
      throw std::runtime_error("Could not create worker thread");
    }
}

// See Worker.h
void Worker::Stop() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
    running.store(false);
}

// See Worker.h
void Worker::Join() {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;
    // TODO: implementation here
}

// See Worker.h
void Worker::OnRun(int sfd) {
    std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.

    int s;
    int efd;
    epoll_event event;
    epoll_event events[MAX_EVENTS];

    efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create");
        abort();
    }

    event.data.fd = sfd;
    event.events = EPOLLIN;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        perror("epoll_ctl");
        abort();
    }

    /* The event loop */
    while (running.load()) {
        int n, i;

        n = epoll_wait(efd, events, MAX_EVENTS, 0);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }

            else if (sfd == events[i].data.fd) {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof in_addr;
                    infd = accept(sfd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    s = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
                                    NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n",
                               infd, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    make_socket_non_blocking(infd);
                    if (s == -1)
                        abort();

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        perror("epoll_ctl");
                        abort();
                    }
                }
                continue;
            } else {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read(events[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            perror("read");
                            done = 1;
                        }
                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }

                    auto input_string = sock_buf_mapping[events[i].data.fd] + std::string(buf, count);
                    // do not use this
                    sock_buf_mapping[events[i].data.fd] = run_parser (input_string, events[i].data.fd);
                }

                if (done) {
                    printf("Closed connection on descriptor %d\n", events[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close(events[i].data.fd);
                    sock_buf_mapping[events[i].data.fd] = "";
                }
            }
        }
    }

    std::cout << "Closing..." << std::endl;
    close(sfd);
}

std::string Worker::run_parser (std::string buf_in, int sock) {
  std::cout << "network debug: " << __PRETTY_FUNCTION__ << std::endl;

  std::string buf = buf_in;

  while (buf.size()) {

      Protocol::Parser pr;
//      pr.Reset();

      size_t parsed = 0;
      bool was_parsed = false;

      size_t first_symbol = buf.find_first_not_of("\r\n");
      if (first_symbol != std::string::npos)
        buf = buf.substr(first_symbol, buf.size() - first_symbol);
      else
        buf = "";

      try {
        was_parsed = pr.Parse(buf, parsed);
      } catch (std::runtime_error &ex) {
        std::string error = std::string("SERVER_ERROR ") + ex.what() + "\n";
        if (send(sock, error.data(), error.size(), 0) <= 0) {
            close(sock);
          }
        return "";
      }

      if (!was_parsed) {
          return buf;
        }

      uint32_t body_size = 0;
      std::unique_ptr<Execute::Command> command = pr.Build(body_size);

      buf = buf.substr(parsed, buf.size() - parsed);

      std::string out;
      try {
        command->Execute(*storage, buf.substr(0, body_size), out);
        out += "\r\n";
        if (send(sock, out.data(), out.size(), 0) <= 0) {
            close(sock);
            return "";
          }
      } catch (std::runtime_error &ex) {
        std::string error = std::string("SERVER_ERROR ") + ex.what() + "\n";
        if (send(sock, error.data(), error.size(), 0) <= 0) {
            close(sock);
          }
        return "";
      }

      buf = buf.substr(body_size, buf.size() - body_size);
    }

  return buf;
}


} // namespace NonBlocking
} // namespace Network
} // namespace Afina
