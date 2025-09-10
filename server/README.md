Notes

close file descriptors from socket() and accept()

use shutdown() in signal handler

bind() use SO_REUSEADDR

valgrind

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=/tmp/valgrind-out.txt ./aesdsocket