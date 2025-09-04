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

int write_to_file(const char *text) {
  int fd;
  ssize_t nr;

  fd = open(fileName, O_RDWR | O_CREAT | O_APPEND, 0644); // append to file
  if (fd == -1) {
    perror("open");
    fprintf(stderr, "Error: Opening file %s failed\n", fileName);
    return -1;
  }

  nr = write(fd, text, strlen(text));
  if (nr == -1) {
    perror("write");
    fprintf(stderr, "Error: Writing to file %s failed\n", fileName);
    close(fd);
    return -1;
  }

  fprintf(stdout, "Writing \"%s\" to %s\n", text, fileName);

  // add newline, messy but avoids appending text
  nr = write(fd, "\n", 1);
  if (nr == -1) {
    perror("write");
    fprintf(stderr, "Error: Writing newline to file %s failed\n", fileName);
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

    while ((bytes_received = recv(new_fd, buf, sizeof(buf), 0)) > 0) {
        ssize_t nr = write(fd, buf, bytes_received);
        if (nr == -1) {
            perror("write");
            break;
        }
    }

    // write connection IP to file
    if (write_to_file(s) == -1) {
      fprintf(stderr, "writing connection info to file failed");
      exit(1);
    }

    close(new_fd); // parent doesn't need this
  }
}
