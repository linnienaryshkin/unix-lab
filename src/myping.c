#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PACKET_SIZE 64
#define REPLY_TIMEOUT_MS 1000

static uint16_t checksum(const unsigned char *data, size_t length)
{
    uint32_t sum = 0;

    while (length > 1)
    {
        sum += ((uint16_t)data[0] << 8) | data[1];
        data += 2;
        length -= 2;
    }

    if (length == 1)
        sum += (uint16_t)data[0] << 8;

    while (sum >> 16)
        sum = (sum & 0xffffU) + (sum >> 16);

    return (uint16_t)~sum;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints = {0};
    struct addrinfo *addresses;
    struct sockaddr_in destination;
    unsigned char packet[PACKET_SIZE] = {0};
    unsigned char reply[65536];
    int socket_fd;
    int result = EXIT_FAILURE;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s host\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Resolve the host to an IPv4 address. */
    hints.ai_family = AF_INET;
    int error = getaddrinfo(argv[1], NULL, &hints, &addresses);
    if (error != 0)
    {
        fprintf(stderr, "%s: %s\n", argv[1], gai_strerror(error));
        return EXIT_FAILURE;
    }
    memcpy(&destination, addresses->ai_addr, sizeof(destination));
    freeaddrinfo(addresses);

    /* Raw ICMP sockets may require administrator privileges. */
    socket_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (socket_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Build one ICMP echo request. */
    packet[0] = 8;
    packet[4] = (unsigned char)((uint16_t)getpid() >> 8);
    packet[5] = (unsigned char)getpid();
    uint16_t packet_checksum = checksum(packet, sizeof(packet));
    packet[2] = (unsigned char)(packet_checksum >> 8);
    packet[3] = (unsigned char)packet_checksum;

    struct timespec sent_at;
    if (clock_gettime(CLOCK_MONOTONIC, &sent_at) == -1)
    {
        perror("clock_gettime");
        goto cleanup;
    }
    if (sendto(socket_fd, packet, sizeof(packet), 0,
               (struct sockaddr *)&destination, sizeof(destination)) == -1)
    {
        perror("sendto");
        goto cleanup;
    }

    /* Ignore unrelated ICMP packets while waiting up to one second. */
    struct pollfd descriptor = {.fd = socket_fd, .events = POLLIN};
    for (;;)
    {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        {
            perror("clock_gettime");
            goto cleanup;
        }

        long remaining_ms =
            (long)(sent_at.tv_sec + 1 - now.tv_sec) * 1000 +
            (sent_at.tv_nsec - now.tv_nsec) / 1000000;
        if (remaining_ms <= 0)
        {
            printf("Request timed out\n");
            result = EXIT_SUCCESS;
            goto cleanup;
        }

        int ready = poll(&descriptor, 1, (int)remaining_ms + 1);
        if (ready == -1)
        {
            if (errno == EINTR)
                continue;
            perror("poll");
            goto cleanup;
        }
        if (ready == 0)
            continue;
        if ((descriptor.revents & POLLIN) == 0)
        {
            errno = EIO;
            perror("poll");
            goto cleanup;
        }

        struct sockaddr_in source;
        socklen_t source_length = sizeof(source);
        ssize_t received = recvfrom(socket_fd, reply, sizeof(reply), 0,
                                    (struct sockaddr *)&source, &source_length);
        if (received == -1)
        {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            goto cleanup;
        }

        /* Skip the IPv4 header and match the reply to this request. */
        size_t ip_header_size = (size_t)(reply[0] & 0x0f) * 4;
        if (received <= 0 || (reply[0] >> 4) != 4 || ip_header_size < 20 ||
            (size_t)received < ip_header_size + 8 ||
            source.sin_addr.s_addr != destination.sin_addr.s_addr ||
            reply[ip_header_size] != 0 || reply[ip_header_size + 1] != 0 ||
            reply[ip_header_size + 4] != packet[4] ||
            reply[ip_header_size + 5] != packet[5])
            continue;

        printf("Reply from %s\n", inet_ntoa(source.sin_addr));
        result = EXIT_SUCCESS;
        break;
    }

cleanup:
    if (close(socket_fd) == -1)
    {
        perror("close");
        result = EXIT_FAILURE;
    }
    return result;
}
