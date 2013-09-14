/* copyright 2013 Jess Balint */
/* Note:
   this method is not foolproof.
   multiple readers shouldnt be connected to one pipe.
   TODO there should be a method to sync on the pipe before reading it.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include "tmux.h"

void gn_init(void);
void gn_open_pipes(void);
void gn_session_set_current(struct session *s, struct winlink *wl);

const char *gn_pipe_path = "/tmp/tmux-global-notify.fifo";

/* we do iterate this in every call, so... keep it small for now */
#define MAX_PIPES 20
static FILE *pipes[MAX_PIPES];

void gn_open_pipes()
{
  struct stat buf;
  int statrc;

  /* check that we're not conflicted */
  if (!(statrc = stat(gn_pipe_path, &buf)) && !S_ISFIFO(buf.st_mode)) {
	log_debug("Global notify accept cannot any new connections. %s exists and is not a pipe.",
			  gn_pipe_path);
	return;
  } else if (statrc && errno != ENOENT) {
	log_debug("Global notify accept cannot any new connections. Cannot stat(%s): %s",
			  gn_pipe_path, strerror(errno));
	return;
  } else if (statrc && errno == ENOENT) {
	/* create pipe if non-existing */
	if (mkfifo(gn_pipe_path, S_IRUSR | S_IWUSR)) {
	  log_debug("Global notify cannot accept any new connections. Cannot mkfifo(%s): %s",
				gn_pipe_path, strerror(errno));
	  return;
	}
  }

  for (;;) { /* TODO useless to loop here.. multiple opens this fast won't be fruitful */
	int fd;
	FILE **pipe = NULL;
	int pipe_slot = 0;

	/* find open slot */
	for (int i = 0; i < MAX_PIPES; ++i) {
	  if (pipes[i] == NULL) {
		pipe_slot = i;
		pipe = pipes + i;
		break;
	  }
	}
	if (pipe == NULL) {
	  log_debug("Global notify connection table full.");
	  return;
	}

	/* open connection */
	if ((fd = open(gn_pipe_path, O_WRONLY | O_NONBLOCK)) == -1) {
	  if (errno == ENXIO)
		log_debug("Global notify, no client on socket at open()");
	  else
		log_debug("Global notify cannot connect. Cannot open(): %s",
				  strerror(errno));
	  break; /* no client */
	}
	if (!(*pipe = fdopen(fd, "w"))) {
	  close(fd);
	  log_debug("Global notify cannot connect. Cannot fdopen(): %s",
				strerror(errno));
	  break;
	}

	log_debug("Global notify accepted fd %d in pipe slot %d. Resetting pipe.\n",
			  fd, pipe_slot);
	/* reset for next connection */
	unlink(gn_pipe_path);
	if (mkfifo(gn_pipe_path, S_IRUSR | S_IWUSR)) {
	  log_debug("Global notify cannot accept any new connections. Cannot mkfifo(%s): %s",
				gn_pipe_path, strerror(errno));
	  return;
	}
  }
}

void gn_init()
{
  for (int i = 0; i < MAX_PIPES; ++i) {
	pipes[i] = NULL;
  }
  gn_open_pipes();
}

void gn_session_set_current(struct session *s, struct winlink *wl)
{
  const char *session_name = s->name;
  const char *window_name = wl->window->name;
  const int window_id = wl->window->id;
  /* const char *pane_cmd = wl->window->active->cmd; */
  const char *pane_shell = wl->window->active->shell;
  const char *pane_cwd = wl->window->active->cwd;
  gn_open_pipes();
  for (int i = 0; i < MAX_PIPES; ++i) {
	if (pipes[i]) {
	  if (0 > fprintf(pipes[i], "session_set_current %s %s %d (shell=%s,cwd=%s)\n",
					  session_name, window_name, window_id,
					  pane_shell, pane_cwd)) {
		log_debug("Global notify error writing to pipe[%d]. Closing.", i);
		fclose(pipes[i]);
		pipes[i] = NULL;
		continue;
	  }
	  fflush(pipes[i]);
	}
  }
}
