/*
 * (C) Emiel Mols, 2010. Released under the Simplified BSD License.
 * Attribution is very much appreciated.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <getopt.h>

#include <zmq.h>

#define SEND_BUFFER_SIZE 8192

void recv(void* socket, int type, int verbose) {

	if (type == ZMQ_PUSH || type == ZMQ_PUB)
		return;

	int ok;

	int64_t rcvmore;
	size_t rcvmore_size = sizeof(rcvmore);

	zmq_msg_t msg;
	zmq_msg_init(&msg);
	if (ok < 0) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return;
	}

	do {
		ok = zmq_recv(socket, &msg, 0);
		if (ok < 0) {
			fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
			return;
		}

		zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore, &rcvmore_size);

		size_t size = zmq_msg_size(&msg);
		if (verbose)
			fprintf(stderr, "receiving %ld bytes\n", size);

		fwrite(zmq_msg_data(&msg), 1, size, stdout);
	} while (rcvmore);
}

void send(void* socket, int type, int verbose) {

	if (type == ZMQ_PULL || type == ZMQ_SUB)
		return;

	char stack_buffer[SEND_BUFFER_SIZE + sizeof(char *)];
	*(char **)(stack_buffer+SEND_BUFFER_SIZE) = NULL;

	char* buffer = stack_buffer;

	size_t total = 0;
	while (1) {
		size_t read = fread(buffer, 1, SEND_BUFFER_SIZE, stdin);
		total += read;

		if (read != SEND_BUFFER_SIZE)
			break;

		char *new_buffer = malloc(SEND_BUFFER_SIZE + sizeof(char *));
		*((char **)&new_buffer[SEND_BUFFER_SIZE]) = NULL;
		*((char **)&buffer[SEND_BUFFER_SIZE]) = new_buffer;
		buffer = new_buffer;
	}

	int ok;

	zmq_msg_t msg;
	ok = zmq_msg_init_size(&msg, total);
	if (ok < 0) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
	}
	else {
		char *dest = (char *)zmq_msg_data(&msg);

		buffer = stack_buffer;
		while (buffer != NULL) {
			memcpy(dest, buffer, SEND_BUFFER_SIZE);

			buffer = *(char **)&buffer[SEND_BUFFER_SIZE];
			dest += SEND_BUFFER_SIZE;
		}

		if (verbose)
			fprintf(stderr, "sending %ld bytes\n", total);

		ok = zmq_send(socket, &msg, 0);
		if (ok < 0)
			fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
	}

	buffer = stack_buffer;
	while (buffer != NULL) {
		if (buffer != stack_buffer)
			free(buffer);

		buffer = *(char **)&buffer[SEND_BUFFER_SIZE];
	}

}

int main(int argc, char *argv[]) {
	
	int type = ZMQ_PUSH;
	char *endpoint = NULL;
	int bind = 0;
	int verbose = 0;

	char c;
	while ((c = getopt(argc, argv, "t:e:bv")) != -1) {
		if (c == 't') {
			if (!strcasecmp(optarg, "pull"))
				type = ZMQ_PULL;
			else if (!strcasecmp(optarg, "req"))
				type = ZMQ_REQ;
			else if (!strcasecmp(optarg, "rep"))
				type = ZMQ_REP;
			else if (!strcasecmp(optarg, "pub"))
				type = ZMQ_PUB;
			else if (!strcasecmp(optarg, "sub"))
				type = ZMQ_PUB;
		}
		else if (c == 'e') {
			endpoint = optarg;
		}
		else if (c == 'b') {
			bind = 1;
		}
		else if (c == 'v') {
			verbose = 1;
		}
	}

	if (!endpoint) {
		fprintf(stderr, "usage: %s [-t type] -e endpoint [-b] [-v]\n", argv[0]);
		fprintf(stderr, "  -t : PUSH | PULL | REQ | REP | PUB | SUB\n");
		fprintf(stderr, "  -e : endpoint, e.g. \"tcp://127.0.0.1:5000\"\n");
		fprintf(stderr, "  -b : bind instead of connect\n");
		fprintf(stderr, "  -v : verbose output to stderr\n");
		return 254;
	}

	void *ctx = zmq_init(1);
	if (ctx == NULL) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return 1;
	}

	void *socket = zmq_socket(ctx, type);
	if (socket == NULL) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return 1;
	}

	int ok;

	if (bind)
		ok = zmq_bind(socket, endpoint);
	else
		ok = zmq_connect(socket, endpoint);

	if (ok < 0) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return 1;
	}
	else if (verbose && bind) {
		fprintf(stderr, "bound to %s\n", endpoint);
	}
	else if (verbose) {
		fprintf(stderr, "connecting to %s\n", endpoint);
	}

	if (type == ZMQ_REP) {
		recv(socket, type, verbose);
		send(socket, type, verbose);
	}
	else {
		send(socket, type, verbose);
		recv(socket, type, verbose);
	}

	ok = zmq_close(socket);
	if (ok < 0) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return 1;
	}

	ok = zmq_term(ctx);
	if (ok < 0) {
		fprintf(stderr, "error %d: %s\n", errno, zmq_strerror(errno));
		return 1;
	}

	return 0;
}