###################### SQUID.CONF INCLUDIES ########################

# useful debug channels:
# 26 - ssl
# 28 - acl
# 82 - ext acl
# 93 - icap
# 4  - Error Generation
# 29 - Authenticator

#debug_options 82,3 28,3 29,3



####################### Localhost and Local networks definitions #########
# Example rule allowing access from your local networks.
# Adapt to list your (internal) IP networks from where browsing
# should be allowed
#acl localnet src 10.0.0.0/8	# RFC1918 possible internal network
#acl localnet src 172.16.0.0/12	# RFC1918 possible internal network
#acl localnet src 192.168.0.0/16	# RFC1918 possible internal network
#acl localnet src fc00::/7       # RFC 4193 local private network range
#acl localnet src fe80::/10      # RFC 4291 link-local (directly plugged) machines

#acl SSL_ports port 443
#acl Safe_ports port 80		# http
#acl Safe_ports port 21		# ftp
#acl Safe_ports port 443		# https
#acl Safe_ports port 70		# gopher
#acl Safe_ports port 210		# wais
#acl Safe_ports port 1025-65535	# unregistered ports
#acl Safe_ports port 280		# http-mgmt
#acl Safe_ports port 488		# gss-http
#acl Safe_ports port 591		# filemaker
#acl Safe_ports port 777		# multiling http
acl CONNECT method CONNECT

#
# Recommended minimum Access Permission configuration:
#
# Deny requests to certain unsafe ports
#http_access deny !Safe_ports

# Deny CONNECT to other than secure SSL ports
#http_access deny CONNECT !SSL_ports

# We strongly recommend the following be uncommented to protect innocent
# web applications running on the proxy server who think the only
# one who can access services on "localhost" is a local user
http_access deny to_localhost



# open incoming regular and SSL port
http_port 19080 name=ssl-port ssl-bump generate-host-certificates=on dynamic_cert_mem_cache_size=4MB cert=/opt/data/ssl/default.crt key=/opt/data/ssl/default.key


# Initial ssl bump and other related settings
sslcrtd_program /usr/lib64/squid/ssl_crtd -s /var/lib/ssl_db -M 4MB
sslcrtd_children 8 startup=1 idle=1


# External Squid Helper
external_acl_type eacltype children-max=1 children-startup=1 concurrency=30 %ACL %METHOD %SRC %un %PROTO %DST %PORT %PATH %URI /opt/bin/acl-helper -c /opt/etc/acl-helper.conf
acl eacl external eacltype


# External squid helper related ACLs
acl eacl_block_c_and_c_servers           note block_c_and_c_servers 1
acl eacl_block_phishing_sites            note block_phishing_sites 1
acl eacl_block_cookie_stealing           note block_cookie_stealing 1
acl eacl_block_malicious_content_sites   note block_malicious_content_sites 1
acl eacl_block_malicious_urls            note block_malicious_urls 1
acl eacl_block_browser_exploits          note block_browser_exploits 1

acl eacl_ssl_inspection_enabled   note ssl_inspection_enabled 1

acl eacl_ssl_allow_untrusted      note block_untrusted_certificate 0
acl eacl_ssl_block_untrusted      note block_untrusted_certificate 1
acl eacl_ssl_pass_untrusted       note block_untrusted_certificate 2

acl eacl_ssl_block_revocated      note block_revocated_certificate 1

acl eacl_ssl_exclude_domain       note ssl_exclude_domain 1

acl eacl_ssl_cert_ok              note ssl_error 0

acl eacl_auth_krb5_off            note auth_krb5 0
acl eacl_auth_db_off              note auth_db   0
acl eacl_auth_ad_off              note auth_ad   0


# Authentication

# KRB5
auth_param negotiate program /usr/lib64/squid/negotiate_kerberos_auth -i -s GSS_C_NO_NAME -k /etc/HTTP7.keytab
auth_param negotiate children 5 startup=0 idle=1
auth_param negotiate keep_alive on
acl acl_auth_krb5 proxy_auth REQUIRED
http_access allow all eacl !eacl_auth_krb5_off acl_auth_krb5

# NTLMv2 AUTH
auth_param ntlm program /usr/bin/ntlm_auth --helper-protocol=squid-2.5-ntlmssp
auth_param ntlm children 5 startup=0 idle=1
auth_param ntlm keep_alive on
acl acl_auth_ntlm proxy_auth REQUIRED
http_access allow all eacl !eacl_auth_ad_off acl_auth_ntlm

# BASIC AUTH
auth_param digest program /opt/bin/auth-digest.pl
auth_param digest children 5 startup=0 idle=1
auth_param digest realm TESTREALM
auth_param digest nonce_garbage_interval 5 minutes
auth_param digest nonce_max_duration 30 minutes
auth_param digest nonce_max_count 50
acl acl_auth_db proxy_auth REQUIRED
http_access allow all eacl !eacl_auth_db_off acl_auth_db


# Deny bad urls
http_access deny all eacl eacl_block_c_and_c_servers
http_access deny all eacl eacl_block_phishing_sites
http_access deny all eacl eacl_block_cookie_stealing
http_access deny all eacl eacl_block_malicious_content_sites
http_access deny all eacl eacl_block_malicious_urls
http_access deny all eacl eacl_block_browser_exploits

# Allow SSL excluded domains
http_access allow all eacl eacl_ssl_inspection_enabled eacl_ssl_exclude_domain


# Deny or allow access to bad SSL certs (NO REVOCKATED CHECK FOR NOW)
http_access allow all eacl eacl_ssl_inspection_enabled !eacl_ssl_cert_ok eacl_ssl_pass_untrusted
http_access allow all eacl eacl_ssl_inspection_enabled !eacl_ssl_cert_ok eacl_ssl_allow_untrusted

http_access deny  all eacl eacl_ssl_inspection_enabled !eacl_ssl_cert_ok eacl_ssl_block_untrusted
http_access deny  all eacl eacl_ssl_inspection_enabled !eacl_ssl_cert_ok eacl_ssl_block_revocated




# SSL cert bump decision
acl acl_ssl_bump_off all-of all eacl !eacl_ssl_inspection_enabled
acl acl_ssl_bump_off all-of all eacl eacl_ssl_inspection_enabled eacl_ssl_exclude_domain
acl acl_ssl_bump_off all-of all eacl eacl_ssl_inspection_enabled eacl_ssl_pass_untrusted !eacl_ssl_cert_ok

ssl_bump splice acl_ssl_bump_off
ssl_bump bump all

# general icap settings
icap_enable on

icap_service service_req reqmod_precache bypass=1 icap://127.0.0.1:1344/request
icap_service service_resp respmod_precache icap://127.0.0.1:1344/response bypass=off on-overload=bypass

icap_client_username_header X-Client-Username
icap_client_username_encode off

# icap access settings
adaptation_send_client_ip on
adaptation_send_username on
adaptation_access service_resp allow all
adaptation_access service_req deny acl_ssl_bump_off
 
# finally allow all
http_access allow all


####### SSL ERRORS as returned by eacl via 'ssl_error' note
#
#define         ACL_HELPER_ERROR                              (-1)
#define         X509_V_OK                                       0
#/* illegal error (for uninitialized values, to avoid X509_V_OK): 1 */
#define         X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT            2
#define         X509_V_ERR_UNABLE_TO_GET_CRL                    3
#define         X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE     4
#define         X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE      5
#define         X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY   6
#define         X509_V_ERR_CERT_SIGNATURE_FAILURE               7
#define         X509_V_ERR_CRL_SIGNATURE_FAILURE                8
#define         X509_V_ERR_CERT_NOT_YET_VALID                   9
#define         X509_V_ERR_CERT_HAS_EXPIRED                     10
#define         X509_V_ERR_CRL_NOT_YET_VALID                    11
#define         X509_V_ERR_CRL_HAS_EXPIRED                      12
#define         X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD       13
#define         X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD        14
#define         X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD       15
#define         X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD       16
#define         X509_V_ERR_OUT_OF_MEM                           17
#define         X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT          18
#define         X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN            19
#define         X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY    20
#define         X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE      21
#define         X509_V_ERR_CERT_CHAIN_TOO_LONG                  22
#define         X509_V_ERR_CERT_REVOKED                         23
#define         X509_V_ERR_INVALID_CA                           24
#define         X509_V_ERR_PATH_LENGTH_EXCEEDED                 25
#define         X509_V_ERR_INVALID_PURPOSE                      26
#define         X509_V_ERR_CERT_UNTRUSTED                       27
#define         X509_V_ERR_CERT_REJECTED                        28
#/* These are 'informational' when looking for issuer cert */
#define         X509_V_ERR_SUBJECT_ISSUER_MISMATCH              29
#define         X509_V_ERR_AKID_SKID_MISMATCH                   30
#define         X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH          31
#define         X509_V_ERR_KEYUSAGE_NO_CERTSIGN                 32
#define         X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER             33
#define         X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION         34
#define         X509_V_ERR_KEYUSAGE_NO_CRL_SIGN                 35
#define         X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION     36
#define         X509_V_ERR_INVALID_NON_CA                       37
#define         X509_V_ERR_PROXY_PATH_LENGTH_EXCEEDED           38
#define         X509_V_ERR_KEYUSAGE_NO_DIGITAL_SIGNATURE        39
#define         X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED       40
#define         X509_V_ERR_INVALID_EXTENSION                    41
#define         X509_V_ERR_INVALID_POLICY_EXTENSION             42
#define         X509_V_ERR_NO_EXPLICIT_POLICY                   43
#define         X509_V_ERR_DIFFERENT_CRL_SCOPE                  44
#define         X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE        45
#define         X509_V_ERR_UNNESTED_RESOURCE                    46
#define         X509_V_ERR_PERMITTED_VIOLATION                  47
#define         X509_V_ERR_EXCLUDED_VIOLATION                   48
#define         X509_V_ERR_SUBTREE_MINMAX                       49
#define         X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE          51
#define         X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX        52
#define         X509_V_ERR_UNSUPPORTED_NAME_SYNTAX              53
#define         X509_V_ERR_CRL_PATH_VALIDATION_ERROR            54
#/* The application is not happy */
#define         X509_V_ERR_APPLICATION_VERIFICATION             50


