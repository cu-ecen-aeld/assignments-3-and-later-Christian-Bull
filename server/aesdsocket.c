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
#define PORT "9000"
#define BACKLOG 10

const char *fileName = "/var/tmp/aesdsocketdata";

int write_to_open_file(int fd, const char *data, size_t len) {
  ssize_t nr;
  size_t total_written = 0;
  
  while (total_written < len) {
    nr = write(fd, data + total_written, len - total_written);
    if (nr == -1) {
      perror("write");
      return -1;
    }
    total_written += nr;
  }
  
  return 0;
}

int write_to_file(const char *data, size_t len) {
  int fd;
  ssize_t nr;

  fd = open(fileName, O_RDWR | O_CREAT | O_APPEND | O_SYNC, 0644);
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

// signal handler
void handle_signal(int sig) {
  fprintf(stdout, "Caught signal, exiting\n");
  syslog(LOG_INFO, "Caught signal, exiting\n");

  if (sockfd != -1) {
    close(sockfd);
    fprintf(stdout, "Closed listening socket\n");
  }

  if (remove(fileName) == 0) {
    fprintf(stdout, "Removed tmp file\n");
    syslog(LOG_INFO, "Removed tmp file\n");
  }

  closelog();
  exit(0);
}

void setup_signal_handlers(void) {
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction(SIGINT)");
    exit(1);
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("sigaction(SIGTERM)");
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  int daemon = 0;
  if (argc == 2 && strcmp(argv[1], "-d") == 0) {
    daemon = 1;
  }

  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int new_fd;
  struct addrinfo *servinfo;
  char s[INET6_ADDRSTRLEN];

  // Initialize syslog
  openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
  
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
  }

  // make a socket, bind it, and listen on it:
  res = servinfo;
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
    perror("bind");
    exit(1);
  }

  // fork here
  if (daemon == 1) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    }
    if (pid > 0) {
      // parent exits
      exit(0);
    }

    // child continues as daemon
    if (setsid() < 0) {
      perror("setsid");
      exit(1);
    }

    if (chdir("/") < 0) {
      perror("chdir");
      exit(1);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  freeaddrinfo(servinfo);

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }
  
  if (!daemon) {
    fprintf(stdout, "Server listening on port %s\n", PORT);
  }
  syslog(LOG_INFO, "Server listening on port %s", PORT);

  // now accept an incoming connection:
  while (1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    if (!daemon) {
      printf("server: got connection from %s\n", s);
    }
    syslog(LOG_INFO, "Accepted connection from %s", s);

    char buf[1024];
    ssize_t bytes_received;

    // Open file for writing once at the beginning of data reception
    int write_fd = open(fileName, O_RDWR | O_CREAT | O_APPEND | O_SYNC, 0644);
    if (write_fd == -1) {
      if (!daemon) {
        printf("Error opening file for writing\n");
      }
      syslog(LOG_ERR, "Error opening file for writing");
      close(new_fd);
      continue;
    }

    // Receive data from client and write to open file
    while ((bytes_received = recv(new_fd, buf, sizeof(buf), 0)) > 0) {
      if (write_to_open_file(write_fd, buf, bytes_received) == -1) {
        if (!daemon) {
          printf("Error writing data to file\n");
        }
        syslog(LOG_ERR, "Error writing data to file");
        break;
      }
    }
    
    // Close the write file
    if (close(write_fd) == -1) {
      perror("close write file");
    }

    if (bytes_received == -1) {
      perror("recv");
      close(new_fd);
      continue;
    }

    // send back file contents
    int read_fd = open(fileName, O_RDONLY);
    if (read_fd == -1) {
      if (!daemon) {
        printf("Error opening file for reading\n");
      }
      syslog(LOG_ERR, "Error opening file for reading");
    } else {
      char file_buf[1024];
      ssize_t nread;
      int send_error = 0;
      
      while ((nread = read(read_fd, file_buf, sizeof(file_buf))) > 0 && !send_error) {
        ssize_t total_sent = 0;
        ssize_t sent;
        while (total_sent < nread && !send_error) {
          sent = send(new_fd, file_buf + total_sent, nread - total_sent, 0);
          if (sent == -1) {
            if (!daemon) {
              printf("Error sending data\n");
            }
            syslog(LOG_ERR, "Error sending data");
            send_error = 1;
            break;
          }
          total_sent += sent;
        }
      }

      close(read_fd);
    }

    close(new_fd);
  }
  return 0;
}