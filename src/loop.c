/** \file */


#include "acl-helper.h"
#include "log.h"
#include "tree.h"
#include "misc.h"
#include "conf.h"
#include "checker.h"
#include "url.h"

#include "loop.h"


// here we start a thread for every incoming squid request
// which the exits

//! a threaded function itself
static void *process_request(void *);

//! running threads counter
static int tcounter = 0;

//! mutex for threads counter var
static pthread_mutex_t tcounter_mutex = PTHREAD_MUTEX_INITIALIZER;

//! condvar for threads counter var
static pthread_cond_t tcounter_condvar = PTHREAD_COND_INITIALIZER;


//! main loop: read stdin, start a thread, process a request
//! \return 0 if ok
int loop_run(void) {

  char buf[SQUID_BUF_SIZE];
  pthread_t thread_id;
  pthread_attr_t thread_attr;

  // create detached threads by default
  pthread_attr_init(&thread_attr);
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

  // main loop: read stdin
  while (fgets(buf, sizeof(buf) - 1, stdin)) {

    wlog(L_DEBUG9, "busy threads: %d/%d", tcounter, config.concurrency);

    // strip blanks
    char *sbuf = strip_blanks(buf);

    // ignore empty lines
    if (! *sbuf)
      continue;

    // wait for threads num to go down (if too many running)
    // and start new thread after that
    pthread_mutex_lock(&tcounter_mutex);
    if (tcounter > config.concurrency) {
      wlog(L_DEBUG9, "waiting for free threads");
      pthread_cond_wait(&tcounter_condvar, &tcounter_mutex);
    }
    tcounter ++;
    pthread_mutex_unlock(&tcounter_mutex);

    // make a copy for a thread to work with
    void *sbuf_p = (void *)strdup(sbuf);
    assert(sbuf_p);

    // create a processing thread (will detach itself)
    if (pthread_create(&thread_id , &thread_attr, process_request, sbuf_p) < 0) {
      wlog(L_ERR, "thread creation failed: %s", strerror(errno));
      return 1;
    }

  }

  return 0;
}


//! main request processing function
//! \param buf a string read from stdin
//! \return nothing, actually
void *process_request(void *buf) {

  wlog(L_DEBUG7, "got from squid [%s]", (char *)buf);

  char *respline = NULL;

  // parse squid input line
  char **tokens = calloc(SQUID_MAX_TOKENS + 1, sizeof(char *));
  assert(tokens);
  int tokens_num = parse_string((char *)buf, tokens, " +", SQUID_MAX_TOKENS + 1);

  // no need in this anymore
  free(buf);

  // decode tokens (if they are %% encoded)
  // decoded string is always shorter then original, so no worries here
  int i = 0;
  for (i = 0; i < tokens_num; i ++) {
    tokens[i] = url_indecode(tokens[i]);
    wlog(L_DEBUG9, "decoded input token %d: [%s]", i, tokens[i]);
  }

  // call the checkers!
  // single threaded mode
  char *seq_id = "";
  if (config.concurrency == 0)
    respline = checkers_call(tokens, tokens_num - 1);
  // concurrente mode, salvage first token (seq id)
  else {
    seq_id = tokens[0];
    respline = checkers_call(tokens + 1, tokens_num - 2);
  }

  // ok, send the resp to squid
  if (respline) {
    wlog(L_DEBUG7, "sending to squid: [%s %s]", seq_id, respline);
    if (*seq_id) {
      fputs(seq_id, stdout);
      fputc(' ', stdout);
    }
    fputs(respline, stdout);
    fputc('\n', stdout);
    free(respline);
  }

  // cleanups!
  for (-- tokens_num; tokens_num >= 0; -- tokens_num) {
    if (tokens[tokens_num])
      free(tokens[tokens_num]);
  }
  free(tokens);

  // decrease threads counter
  pthread_mutex_lock(&tcounter_mutex);
  tcounter --;
  pthread_cond_signal(&tcounter_condvar);
  pthread_mutex_unlock(&tcounter_mutex);

  
  // all done
  pthread_exit(NULL);
}


