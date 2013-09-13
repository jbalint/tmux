/* copyright 2013 Jess Balint */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "tmux.h"

void gn_init(void);
void gn_session_set_current(struct session *s, struct winlink *wl);

const char *gn_pipe_path = "/tmp/tmux-global-notify.fifo";

static FILE *gn_pipe = 0;

void gn_init()
{
  struct stat buf;

  if (!stat(gn_pipe_path, &buf)) {
	if (!S_ISFIFO(buf.st_mode)) {
	  log_debug("Global notify cannot proceed. %s exists and is not a pipe.", gn_pipe_path);
	  return;
	}
  } else if (errno == ENOENT) {
	if (mkfifo(gn_pipe_path, S_IRUSR | S_IWUSR)) {
	  log_debug("Global notify cannot proceed. Cannot mkfifo(%s): %s",
				gn_pipe_path, strerror(errno));
	  return;
	}
  } else {
	log_debug("Global notify cannot proceed. Cannot stat(%s): %s",
			  gn_pipe_path, strerror(errno));
	return;
  }

  if (!(gn_pipe = fopen(gn_pipe_path, "a"))) {
	log_debug("Global notify cannot proceed. Cannot fopen(%s): %s",
			  gn_pipe_path, strerror(errno));
  }
}

void gn_session_set_current(struct session *s, struct winlink *wl)
{
  if (!gn_pipe)
	return;
  fprintf(gn_pipe, "session_set_current %s %s %d\n",
		  s->name, wl->window->name, wl->window->id);
  fflush(gn_pipe);
}
