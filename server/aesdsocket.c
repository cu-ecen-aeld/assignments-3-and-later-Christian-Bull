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

const char *fileName = "/var/tmp/aesdsocketdata";

int write_to_file(const char *data, size_t len) {
  int fd;
  ssize_t nr;

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

int main(void) {
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sockfd, new_fd;
  struct addrinfo *servinfo;
  char s[INET6_ADDRSTRLEN];

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

  if (listen(sockfd, BACKLOG) == -1) {
    fprintf(stderr, "gai error: listen");
    exit(1);
    return -1;
  }
  fprintf(stdout, "Server listening on port %s\n", PORT);

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
    printf("server: got connection from %s\n", s);
    syslog(LOG_INFO, "Accepted connection from %s", s);

    char buf[1024];
    ssize_t bytes_received;
    char *packet_buf = NULL;
    size_t packet_len = 0;
    size_t packet_size = 0;

    while ((bytes_received = recv(new_fd, buf, sizeof(buf), 0)) > 0) {
      for (ssize_t i = 0; i < bytes_received; i++) {
        if (packet_len + 1 > packet_size) {
          // grow buffer
          size_t new_size = packet_size ? packet_size * 2 : 1024;
          char *tmp = realloc(packet_buf, new_size);
          if (!tmp) {
            perror("realloc");
            free(packet_buf);
            break;
          }
          packet_buf = tmp;
          packet_size = new_size;
        }

        packet_buf[packet_len++] = buf[i];

        if (buf[i] == '\n') {
          // complete packet found
          int fr = write_to_file(packet_buf, packet_len);
          if (fr == -1) {
            perror("write");
          }

          // send back contents of file
          int fd_send = open(fileName, O_RDONLY);
          if (fd_send == -1) {
            perror("Error opening file");
            break;
          }
          char file_buf[1024];
          ssize_t nread;
          while ((nread = read(fd_send, file_buf, sizeof(file_buf))) > 0) {
            if (send(new_fd, file_buf, nread, 0) == -1) {
              perror("Error sending contents of file");
              break;
            }
          }
          close(fd_send);

          packet_len = 0;
        }
      }
    }

    close(new_fd); // parent doesn't need this
  }
}
