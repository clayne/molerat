#include <stdio.h>

#include <molerat/socket.h>
#include <molerat/tasklet.h>

#define BUF_SIZE 20

struct echoer {
	struct mutex mutex;
	struct tasklet tasklet;
	struct socket *socket;
	char *buf;
	size_t len;
	size_t pos;
	bool_t verbose;
};

static void echoer_echo(void *v_e)
{
	struct echoer *e = v_e;
	struct error err;
	ssize_t res;

	error_init(&err);

	for (;;) {
		if (e->pos == e->len) {
			switch (res = socket_read(e->socket, e->buf, BUF_SIZE,
						  &e->tasklet, &err)) {
			case STREAM_WAITING:
				goto waiting;

			case STREAM_END:
				goto done;

			case STREAM_ERROR:
				goto error;
			}

			e->len = res;
			e->pos = 0;
		}

		switch (res = socket_write(e->socket, e->buf + e->pos,
					  e->len - e->pos, &e->tasklet, &err)) {
		case STREAM_WAITING:
			goto waiting;

		case STREAM_ERROR:
			goto error;
		}

		e->pos += res;
	}

 waiting:
	mutex_unlock(&e->mutex);
	return;

 done:
	socket_close(e->socket, &err);
	if (e->verbose)
		fprintf(stderr, "Connection closed\n");

 error:
	if (!error_ok(&err))
		fprintf(stderr, "%s\n", error_message(&err));

	tasklet_fini(&e->tasklet);
	socket_destroy(e->socket);
	mutex_unlock_fini(&e->mutex);
	free(e->buf);
	free(e);
	error_fini(&err);
}


static struct echoer *echoer_create(struct socket *s, bool_t verbose)
{
	struct echoer *e = xalloc(sizeof *e);
	mutex_init(&e->mutex);
	tasklet_init(&e->tasklet, &e->mutex, e);
	e->socket = s;
	e->buf = xalloc(BUF_SIZE);
	e->pos = e->len = 0;
	e->verbose = verbose;

	mutex_lock(&e->mutex);
	tasklet_goto(&e->tasklet, echoer_echo);

	return e;
}

struct echo_server {
	struct mutex mutex;
	struct tasklet tasklet;
	struct server_socket *server_socket;
	bool_t verbose;
};

static void announce_connection(struct socket *s)
{
	struct error err;
	struct sockaddr *sa;
	char *printed;

	error_init(&err);

	sa = socket_peer_address(s, &err);
	if (sa) {
		printed = print_sockaddr(sa, &err);
		if (printed) {
			fprintf(stderr, "Connection from %s\n", printed);
			free(printed);
		}

		free(sa);
	}

	if (!error_ok(&err))
		fprintf(stderr, "%s\n", error_message(&err));

	error_fini(&err);
}

static void echo_server_accept(void *v_es)
{
	struct echo_server *es = v_es;
	struct error err;
	struct socket *s;

	error_init(&err);

	while ((s = server_socket_accept(es->server_socket, &es->tasklet,
					 &err))) {
		if (es->verbose)
			announce_connection(s);

		echoer_create(s, es->verbose);
	}

	if (!error_ok(&err))
		fprintf(stderr, "%s\n", error_message(&err));

	mutex_unlock(&es->mutex);
	error_fini(&err);
}

struct echo_server *echo_server_create(struct server_socket *s,
				       bool_t verbose)
{
	struct echo_server *es = xalloc(sizeof *es);
	mutex_init(&es->mutex);
	tasklet_init(&es->tasklet, &es->mutex, es);
	es->server_socket = s;
	es->verbose = verbose;

	mutex_lock(&es->mutex);
	tasklet_goto(&es->tasklet, echo_server_accept);

	return es;
}

void echo_server_destroy(struct echo_server *es)
{
	mutex_lock(&es->mutex);
	tasklet_fini(&es->tasklet);
	server_socket_destroy(es->server_socket);
	mutex_unlock_fini(&es->mutex);
	free(es);
}

struct sockaddr **echo_server_addresses(struct echo_server *es,
					struct error *err)
{
	return server_socket_addresses(es->server_socket, err);
}