#include "aesdsocket.h"
#include "queue.h" // queue taken from FreeBSD 10c
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>
#define PORT "9000" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue holds
#define FILE_BUF_SIZE 1024

#ifdef USE_AESD_CHAR_DEVICE
const char *fileName = "/dev/aesdchar";
#else
const char *fileName = "/var/tmp/aesdsocketdata";
#endif

int write_to_file(const char *data, size_t len) {
  int fd;
  ssize_t nr;

  syslog(LOG_INFO, "Writing to file");

  fd = open(fileName, O_RDWR | O_CREAT | O_APPEND, 0644); // append to file
  if (fd == -1) {
    perror("open");
    fprintf(stderr, "Error: Opening file %s failed\n", fileName);
    return -1;
  }

  nr = write(fd, data, len);
  if (nr == -1) {
    perror("write");
    fprintf(stderr, "Error: Writing to file %s failed\n", fileName);
    close(fd);
    return -1;
  }

  // flush data to disk -- idk hope this works :pray:
  if (fsync(fd) == -1) {
    perror("fsync");
    close(fd);
    return -1;
  }

  // fprintf(stdout, "Writing \"%s\" to %s\n", text, fileName);

  if (close(fd) == -1) {
    perror("close");
    fprintf(stderr, "Error: Closing file %s failed\n", fileName);
    return -1;
  }

  fflush(stdout);
  fflush(stderr);

  return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int sockfd = -1; // global listening socket
pthread_t ts_thread; // global timestamp thread


// signal handler
void handle_signal(int sig) {
  fprintf(stdout, "Caught signal %d, exiting\n", sig);

  if (sockfd != -1) {
    close(sockfd);
    fprintf(stdout, "Closed listening socket\n");
  }

  closelog();
  exit(0);
}

void setup_signal_handlers(void) {
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0; // don't restart syscalls like accept()

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction(SIGINT)");
    exit(1);
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("sigaction(SIGTERM)");
    exit(1);
  }
}

void *handle_connection(void *connection_param) {

  // take connection parameters and input into struct
  struct thread_data *thread_func_args = (struct thread_data *)connection_param;

  // struct sockaddr_storage their_addr;
  // socklen_t addr_size;
  // addr_size = sizeof their_addr;
  char s[INET6_ADDRSTRLEN];

  syslog(LOG_INFO, "Starting connection handling thread");

  inet_ntop(thread_func_args->client_addr.ss_family,
            get_in_addr((struct sockaddr *)&thread_func_args->client_addr), s,
            sizeof(s));

  printf("server: got connection from %s\n", s);
  syslog(LOG_INFO, "Accepted connection from %s", s);

  char buf[1024] = {0};
  ssize_t bytes_received;
  char *packet_buf = NULL;
  size_t packet_len = 0;
  size_t packet_size = 0;

  while ((bytes_received =
              recv(thread_func_args->client_fd, buf, sizeof(buf), 0)) > 0) {
    for (ssize_t i = 0; i < bytes_received; i++) {
      if (packet_len + 1 > packet_size) {
        // grow buffer
        size_t new_size = packet_size ? packet_size * 2 : 1024;
        char *tmp = realloc(packet_buf, new_size);
        if (!tmp) {
          perror("realloc");
          free(packet_buf);
          packet_buf = NULL;
          packet_len = 0;
          goto client_done;
        }
        packet_buf = tmp;
        packet_size = new_size;
      }

      packet_buf[packet_len++] = buf[i];

      if (buf[i] == '\n') {
        // complete packet found

        pthread_mutex_lock(thread_func_args->mutex);

        int fr = write_to_file(packet_buf, packet_len);
        if (fr == -1) {
          perror("write");
          pthread_mutex_unlock(thread_func_args->mutex);
        }

        // send back contents of file
        int fd_send = open(fileName, O_RDONLY);
        if (fd_send == -1) {
          perror("Error opening file");
          pthread_mutex_unlock(thread_func_args->mutex);
          break;
        }

        char file_buf[FILE_BUF_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = read(fd_send, file_buf, FILE_BUF_SIZE)) > 0) {
          ssize_t bytes_sent = 0;
          while (bytes_sent < bytes_read) {
            ssize_t n = send(thread_func_args->client_fd, file_buf + bytes_sent,
                             bytes_read - bytes_sent, 0);
            if (n == -1) {
              perror("send");
              close(fd_send);
              exit(1);
              pthread_mutex_unlock(thread_func_args->mutex);
            }
            bytes_sent += n;
          }
        }
        close(fd_send);

        // unlock mutex
        pthread_mutex_unlock(thread_func_args->mutex);

        packet_len = 0;
      }
    }
  }
client_done:
  if (packet_buf) {
    syslog(LOG_INFO, "Closing client connection from %s", s);

    free(packet_buf);
    packet_buf = NULL;
    packet_len = 0;
    packet_size = 0;
    thread_func_args->thread_complete_success = true;
  }
  close(thread_func_args->client_fd);

  return thread_func_args;
}

void *timestamp_thread(void *arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;

    syslog(LOG_INFO, "Starting timestamp thread");

    while (1) {
        sleep(10);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "timestamp:%Y-%m-%d %H:%M:%S\n", t);

        syslog(LOG_INFO, "Logging time to file %s", time_str);

        pthread_mutex_lock(mutex);

        if (write_to_file(time_str, strlen(time_str)) == -1) {
            syslog(LOG_ERR, "Error writing timestamp to file");
        }

        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {

  openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_DAEMON);

  pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

  int daemon = 0;
  if (argc == 2 && strcmp(argv[1], "-d") == 0) {
    daemon = 1;
  }

  syslog(LOG_INFO, "Writing to file %s", fileName);

  // for tracking threads
  TAILQ_HEAD(thread_list, thread_data);
  struct thread_list active_threads;
  TAILQ_INIT(&active_threads);

  #ifndef USE_AESD_CHAR_DEVICE
    int fd_clear = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_clear == -1) {
      perror("Failed to clear file");
    } else {
      close(fd_clear);
    }
  #endif

  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sockfd, new_fd;
  struct addrinfo *servinfo;

  setup_signal_handlers();

  // first, load up address structs with getaddrinfo():
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "gai error: %s\n", gai_strerror(status));
    exit(1);
    return -1;
  }

  // make a socket, bind it, and listen on it:
  res = servinfo;
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  int bind_status;
  if ((bind_status = bind(sockfd, res->ai_addr, res->ai_addrlen)) < 0) {
    fprintf(stderr, "gai error: %s\n", gai_strerror(bind_status));
    exit(1);
    return -1;
  }

  freeaddrinfo(servinfo);

  // fork after binding
  if (daemon) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(EXIT_FAILURE);
    }
    if (pid > 0) {
      // parent exits
      exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) { // create new session
      perror("setsid");
      exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
      perror("chdir");
      exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_RDWR);   // stderr
  }

  if (listen(sockfd, BACKLOG) == -1) {
    fprintf(stderr, "gai error: listen");
    exit(1);
    return -1;
  }
  fprintf(stdout, "Server listening on port %s\n", PORT);

  // output time to file every 10 seconds
  #ifndef USE_AESD_CHAR_DEVICE
    pthread_t ts_thread;
    if (pthread_create(&ts_thread, NULL, timestamp_thread, &global_mutex) != 0) {
        perror("Error creating timestamp thread");
        exit(1);
    }
  #endif

  // now accept an incoming connection:
  while (1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    // setup thread
    struct thread_data *tdata = malloc(sizeof(*tdata));

    tdata->mutex = &global_mutex;
    tdata->thread = malloc(sizeof(pthread_t));
    tdata->thread_complete_success = false;
    tdata->client_fd = new_fd;
    memcpy(&tdata->client_addr, &their_addr, addr_size);
    tdata->addr_len = addr_size;

    int rc = pthread_create(tdata->thread, NULL, handle_connection, tdata);
    if (rc != 0) {
      syslog(LOG_ERR, "Creating thread failed");
      perror("pthread_create");
      close(new_fd);
      free(tdata);
      return false;
    }

    TAILQ_INSERT_TAIL(&active_threads, tdata, entries);

    // loop through threads
    struct thread_data *item, *tmp;

    TAILQ_FOREACH_SAFE(item, &active_threads, entries, tmp) {
        if (item->thread_complete_success) {
            pthread_join(*item->thread, NULL);
            TAILQ_REMOVE(&active_threads, item, entries);
            free(item->thread);
            free(item);
        }
    }
  }
}
