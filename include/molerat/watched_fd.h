#ifndef MOLERAT_WATCHED_FD_H
#define MOLERAT_WATCHED_FD_H

/* You probably shouldn't be using this directly!  socket.h provides a
 * higher-level interface onto sockets. */

/* Event bits */

enum {
	WATCHED_FD_IN = 1,
	WATCHED_FD_OUT = 4,

	/* WATCHED_FD_ERR does not need to be set in the interest. */
	WATCHED_FD_ERR = 8
};

/* A handler takes the event bits actually recieved, and the exiting
 * interest set, and returns the new interest set. Note that the
 * handler gets called with the poll lock held. Be careful to avoid
 * deadlock!
 *
 * Handlers are edge-triggered: After a handled is called, it will not
 * necessarily receive those events again until it re-registers its
 * interest. (Although when using epoll it can re-arm a registered fd
 * merely by making the appropriate system calls.)
 */
typedef uint8_t poll_events_t;
typedef void (*watched_fd_handler_t)(void *data, poll_events_t events);

struct watched_fd *watched_fd_create(int fd, watched_fd_handler_t handler,
				     void *data);
void watched_fd_destroy(struct watched_fd *w);

/* This ORs the given event bits into the interest bits. */
bool_t watched_fd_set_interest(struct watched_fd *w, poll_events_t event,
			       struct error *err);

/* Change the handler for the watched_fd */
void watched_fd_set_handler(struct watched_fd *w, watched_fd_handler_t handler,
			    void *data);

#endif
