/** \file */


#include "acl-helper.h"
#include "log.h"
#include "conf.h"
#include "resolve.h"

#include <sys/select.h>
#include <fcntl.h>

#ifdef USE_SSL
  #include <openssl/ssl.h>
#endif

#include "ssl.h"


#ifdef USE_SSL
static const SSL_METHOD *method;
#endif

//! init SSL engine
//! \return 0 if ok
int ssl_init(void) {

#ifdef USE_SSL
  // prepare SSL engine
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  // init SSL engine
  SSL_library_init();

  // load methods
  method = SSLv23_client_method();
#endif

  // done
  return 0;
}



//! connect to remote host, get its SSL cert, examine it
//! and return cert verification code (see /usr/include/openssl/x509_vfy.h)
//! \param hostname remote host to connect
//! \param port remote host port
//! \param timeout connection timeout
//! \return ssl verification code or -1 on error
int ssl_verify_host(char *hostname, unsigned port, int timeout) {

#ifdef USE_SSL

  long ssl_verify_res = -1; 
  SSL_CTX *ssl_ctx = NULL;
  SSL *ssl = NULL;
 
  // resolve the hostname (get 1-st resolved ip only)
  in_addr_t host_ip;
  if (resolve_host(hostname, &host_ip, 1) < 1) {
    wlog(L_WARN, "ssl: failed to resolve host '%s'", hostname);
    return -1;
  } else
    host_ip = htonl(host_ip);

  // check port validness
  port = (port <= 0 || port >= 0xFFFF) ? 443 : port;

  wlog(L_DEBUG5, "ssl: connecting to '%s:%d' (%s)", hostname, port, inet_ntoa(*((struct in_addr*)&host_ip)));

  // create a socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    wlog(L_ERR, "ssl: failed to create socket: %s", strerror(errno));
    goto OUCH;
  }

  // put the socket in non-blocking mode
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

  // prepare to connect
  struct sockaddr_in *haddr = calloc(1, sizeof(struct sockaddr_in));
  assert(haddr);
  haddr->sin_family=AF_INET;
  haddr->sin_port=htons(port);
  haddr->sin_addr.s_addr = host_ip;

  // connect to remote host (ASYNC!)
  int conn_err = connect(sockfd, (struct sockaddr *)haddr, (socklen_t)sizeof(struct sockaddr));
  if (conn_err < 0) {

    // prepare select() params
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval ts = {.tv_sec = config.ssl_timeout, .tv_usec = 0};

     // wait for the socket to become writable
    int sel_err = select(sockfd + 1, NULL, &fdset, NULL, &ts);

    // failure
    if (sel_err < 0)
      wlog(L_ERR, "ssl: pselect() failed: %s", strerror(errno));
    // timeout
    else if (sel_err == 0)
      wlog(L_WARN, "ssl: connection to '%s:%d' timed out", hostname, port);
    
    // to be sure!
    conn_err = connect(sockfd, (struct sockaddr *)haddr, (socklen_t)sizeof(struct sockaddr));
    
    wlog(L_DEBUG9, "ssl: conn vars selerr=%d conerr=%d errno=%d", sel_err, conn_err, errno);

  }

  if (conn_err < 0) {
    wlog(L_WARN, "ssl: connection to '%s:%d' failed: %s", hostname, port, strerror(errno));
    goto OUCH;
  } else
    wlog(L_DEBUG1, "ssl: connected!");

  // restore socket blocking state
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) & ~O_NONBLOCK);

  // create new SSL context
  ssl_ctx = SSL_CTX_new(method);
  if (! ssl_ctx) {
    wlog(L_ERR, "ssl: SSL_CTX_new() failed");
    goto OUCH;
  }

  // init SSL context, load CA bundle, etc
  SSL_CTX_set_default_verify_paths(ssl_ctx);
  //! \todo move CA bundle loading into ssl_init()
  SSL_CTX_load_verify_locations(ssl_ctx, config.ssl_ca_file, NULL);
  SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
  SSL_CTX_set_verify_depth(ssl_ctx, 10);

  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);

  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
  SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

  // create new SSL connection state obj
  ssl = SSL_new(ssl_ctx);

  // set hostname (tlsext mode)
  SSL_set_tlsext_host_name(ssl, hostname);
 
  // bind connected socket to SSL conn object
  SSL_set_fd(ssl, sockfd);

  // connect to remote host via SSL
  if (SSL_connect(ssl) != 1) {
    wlog(L_WARN, "ssl: SSL connection to '%s:%d' failed", hostname, port);
    goto OUCH;
  }

  // get cert verification result
  ssl_verify_res = SSL_get_verify_result(ssl);
  wlog(L_DEBUG3, "ssl: cert verification for '%s:%d' = %d", hostname, port, ssl_verify_res); 


OUCH:
  // cleanups
  if (ssl)
    SSL_free(ssl);
  if (ssl_ctx)
    SSL_CTX_free(ssl_ctx);
  free(haddr);
  if (sockfd >= 0)
    close(sockfd);


  // all done, return the result
  return ssl_verify_res;
#else
  return -1;
#endif
}



