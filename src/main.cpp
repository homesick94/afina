#include <chrono>
#include <iostream>
#include <memory>
#include <uv.h>

#include <cxxopts.hpp>

#include <afina/Storage.h>
#include <afina/Version.h>
#include <afina/network/Server.h>

#include "network/blocking/ServerImpl.h"
#include "network/nonblocking/ServerImpl.h"
#include "network/uv/ServerImpl.h"
#include "storage/MapBasedGlobalLockImpl.h"
#include <afina/Executor.h>

#include <iostream>
#include <fstream>

#include <unistd.h>

typedef struct {
    std::shared_ptr<Afina::Storage> storage;
    std::shared_ptr<Afina::Network::Server> server;
} Application;

// Handle all signals catched
void signal_handler(uv_signal_t *handle, int signum) {
    Application *pApp = static_cast<Application *>(handle->data);

    std::cout << "Receive stop signal" << std::endl;
    uv_stop(handle->loop);
}

// Called when it is time to collect passive metrics from services
void timer_handler(uv_timer_t *handle) {
    Application *pApp = static_cast<Application *>(handle->data);
    std::cout << "Start passive metrics collection" << std::endl;
}

int main(int argc, char **argv) {
    // Build version
    // TODO: move into Version.h as a function
    std::stringstream app_string;
    app_string << "Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "." << Afina::Version_Patch;
    if (Afina::Version_SHA.size() > 0) {
        app_string << "-" << Afina::Version_SHA;
    }

    // Command line arguments parsing
    cxxopts::Options options("afina", "Simple memory caching server");
    try {
        // TODO: use custom cxxopts::value to print options possible values in help message
        // and simplify validation below
        options.add_options()("s,storage", "Type of storage service to use", cxxopts::value<std::string>());
        options.add_options()("n,network", "Type of network service to use", cxxopts::value<std::string>());
        options.add_options()("h,help", "Print usage info");
        options.add_options()("d,daemon", "Run daemon");
        options.add_options()("t,pool_thread", "Run pool thread", cxxopts::value<int> ());
        options.add_options()("p,pid_print", "Print daemon pid to file", cxxopts::value<std::string>());
        options.parse(argc, argv);

        if (options.count("help") > 0) {
            std::cerr << options.help() << std::endl;
            return 0;
        }
    } catch (cxxopts::OptionParseException &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    // Start boot sequence
    Application app;
    std::cout << "Starting " << app_string.str() << std::endl;

    // Build new storage instance
    std::string storage_type = "map_global";
    if (options.count("storage") > 0) {
        storage_type = options["storage"].as<std::string>();
    }

    if (storage_type == "map_global") {
        app.storage = std::make_shared<Afina::Backend::MapBasedGlobalLockImpl>();
    } else {
        throw std::runtime_error("Unknown storage type");
    }

    // Build  & start network layer
    std::string network_type = "uv";
    if (options.count("network") > 0) {
        network_type = options["network"].as<std::string>();
    }

    if (network_type == "uv") {
        app.server = std::make_shared<Afina::Network::UV::ServerImpl>(app.storage);
    } else if (network_type == "blocking") {
        app.server = std::make_shared<Afina::Network::Blocking::ServerImpl>(app.storage);
    } else if (network_type == "nonblocking") {
        app.server = std::make_shared<Afina::Network::NonBlocking::ServerImpl>(app.storage);
    } else {
        throw std::runtime_error("Unknown network type");
    }

    if (options.count("daemon") > 0)
      {
        pid_t pid = ::fork ();
        if (pid < 0)
          {
            std::cout << "Error forking!" << std::endl;
            return 0;
          }
        else if (pid > 0)
          {
            std::cout << "Closing main process..." << std::endl;
            return 0;
          }

        // child process here
        if (::setsid () < 0)
          {
            printf ("Cannot setsid().\n");
            return 0;
          }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
      }

    if (options.count("pid_print") > 0)
      {
        std::string filename = options["pid_print"].as<std::string>();
        std::ofstream output_file_stream;
        output_file_stream.open (filename.c_str ());
        if (!output_file_stream.is_open ())
          return 0;

        auto pid_num = ::getpid ();
        output_file_stream << pid_num << std::endl;
        output_file_stream.close ();
      }

    if (options.count("pool_thread") > 0)
      {
        int threads_count = options["pool_thread"].as<int>();
        Afina::Executor executor ("pool", threads_count);

        auto func1 = [](){ std::cout << "Func 1 " << std::endl; };
        auto func2 = [](){ std::cout << "Func 2 " << std::endl; };
        auto func3 = [](){ std::cout << "Func 3 " << std::endl; };
        auto func4 = [](){ std::cout << "Func 4 " << std::endl; };

//        std::cout << executor.Execute (func1) << std::endl;
//        std::cout << executor.Execute (func2) << std::endl;
//        std::cout << executor.Execute (func3) << std::endl;
//        std::cout << executor.Execute (func4) << std::endl;

        executor.Execute (func1);
        executor.Execute (func2);
        executor.Execute (func3);
        executor.Execute (func4);

        executor.Stop(true);

        return 0;
      }

    // Init local loop. It will react to signals and performs some metrics collections. Each
    // subsystem is able to push metrics actively, but some metrics could be collected only
    // by polling, so loop here will does that work
    uv_loop_t loop;
    uv_loop_init(&loop);

    uv_signal_t sig;
    uv_signal_init(&loop, &sig);
    uv_signal_start(&sig, signal_handler, SIGTERM | SIGKILL);
    sig.data = &app;

    uv_timer_t timer;
    uv_timer_init(&loop, &timer);
    timer.data = &app;
    uv_timer_start(&timer, timer_handler, 0, 5000);

    // Start services
    try {
        app.storage->Start();
        app.server->Start(8080);

        // Freeze current thread and process events
        std::cout << "Application started" << std::endl;
        uv_run(&loop, UV_RUN_DEFAULT);

        // Stop services
        app.server->Stop();
        app.server->Join();
        app.storage->Stop();

        std::cout << "Application stopped" << std::endl;
    } catch (std::exception &e) {
        std::cerr << "Fatal error" << e.what() << std::endl;
    }

    return 0;
}
