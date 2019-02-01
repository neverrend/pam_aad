#include <nss.h>
#include <pwd.h>
#include <shadow.h>
#include <string.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <unistd.h> // sleep()

#include "cJSON.h"
#include "utils.h"

#define CONF_FILE "/etc/libnss-aad.conf"
#define LOG_FILE "/var/log/nss_aad.log"
#define USER_FIELD "mailNickname"

int get_users(char *tenant, char *access_token, char *users);
int get_oauth2_token(char *client_id, char *client_secret, char *tenant, char *access_token);
int fill_json_buffer(char *json_buf, char *raw_response, int *start, int *end);
int find_json_bounds(char *json_buf, int *start, int *end);

enum nss_status _nss_aad_getpwnam_r(const char *name, struct passwd *p, char *buffer, size_t buflen, int *errnop)
{
    char auth_token[2048];
    char *users[1024];
    char *config_file, *tenant;
    cJSON *config, *client;
    int i;
    FILE *log;

    log = fopen(LOG_FILE, "w+");
    setbuf(log, NULL);

    config_file = load_file(CONF_FILE);
    if(config_file == NULL)
    {
        return NSS_STATUS_NOTFOUND;
    }

    config = cJSON_Parse(config_file);

    client = cJSON_GetObjectItem(config, "client");
    tenant = cJSON_GetObjectItem(config, "tenant")->valuestring;

    get_oauth2_token(cJSON_GetObjectItem(client, "id")->valuestring,
		    cJSON_GetObjectItem(client, "secret")->valuestring,
		    tenant, auth_token);

    get_users(tenant, auth_token, users);
/*
    strcpy(p->pw_name, name);
    strcpy(p->pw_passwd, "x");
*/
    fprintf(log, "user=%s, uid=%u, gid=%u\n", p->pw_name, p->pw_uid, p->pw_gid);

    for(i = 0; i < strlen(users); i++)
    {
        if(strcmp(name, users[i]) == 0)
	{
	    strcpy(p->pw_name, users[i]);
	    fprintf(log, "p->pw_name: %s\n", p->pw_name);
	    fclose(log);
	    return NSS_STATUS_SUCCESS;
	}
    }

    fclose(log);
    return NSS_STATUS_TRYAGAIN;
}


enum nss_status _nss_aad_getspnam_r(const char *name, struct spwd *s, char *buffer, size_t buflen, int *errnop)
{
    FILE *log;
    log = fopen(LOG_FILE, "w+");
    setbuf(log, NULL);
/*
    strcpy(s->sp_namp, name);
    strcpy(s->sp_pwdp, "*");
*/
    fprintf(log, "test");

    fclose(log);
    return NSS_STATUS_TRYAGAIN;
}
/*
 * Function: get_oauth2_token
 * --------------------------
 * https://docs.microsoft.com/en-us/azure/active-directory/develop/v1-oauth2-client-creds-grant-flow
 *
 *  *client_id: char array containing the client id
 *
 *  *client_secret: char array containing _url-encoded_ client secret
 *
 *  *tenant: char array containing the tenant
 *
 *  *access_token: char array containing the access token
 *
 *  returns 0 if completion successful, 1 if it fails.
 */
int get_oauth2_token(char *client_id, char *client_secret, char *tenant, char *access_token)
{
    BIO* bio;
    cJSON *json;
    SSL_CTX* ctx;

    char buf[1024];
    char json_buf[2048];
    char post_buf[1024];
    char response_buf[2048];
    char write_buf[2048];
    int size, start, end;

    strcpy(response_buf, " ");

    SSL_library_init();

    ctx = SSL_CTX_new(SSLv23_client_method());

    if (ctx == NULL)
        printf("ctx is null\n");

    bio = BIO_new_ssl_connect(ctx);
    BIO_set_conn_hostname(bio, "login.microsoftonline.com:443");

    if(BIO_do_connect(bio) <= 0)
    {
        printf("Failed connection\n");
	return 1;
    }
    else
    {
        printf("Connected\n");
    }

    strcpy(post_buf, "resource=");
    strcat(post_buf, "00000002-0000-0000-c000-000000000000");
    strcat(post_buf, "&client_id=");
    strcat(post_buf, client_id);
    strcat(post_buf, "&client_secret=");
    strcat(post_buf, client_secret);
    strcat(post_buf, "&grant_type=client_credentials");

    strcpy(write_buf, "POST /");
    strcat(write_buf, tenant);
    strcat(write_buf, "/oauth2/token HTTP/1.1\r\n");
    strcat(write_buf, "User-Agent: azure_authenticator_pam/1.0 \r\n");
    strcat(write_buf, "Host: login.microsoftonline.com\r\n");
    strcat(write_buf, "Content-Length: 177\r\n");
    strcat(write_buf, "Connection: close \r\n");
    strcat(write_buf, "\r\n");
    strcat(write_buf, post_buf);
    strcat(write_buf, "\r\n");

    if(BIO_write(bio, write_buf, strlen(write_buf)) <= 0)
    {
        if(!BIO_should_retry(bio))
        {
            printf("Do retry\n");
	}

	printf("Failed write\n");
    }

    for (;;)
    {
        size = BIO_read(bio, buf, 1023);

	if(size <= 0 )
	{
	    break;
	}
	buf[size] = 0;
	strcat(response_buf, buf);
    }

    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    find_json_bounds(response_buf, &start, &end);
    fill_json_buffer(json_buf, response_buf, &start, &end);
    json = cJSON_Parse(json_buf);
    strcpy(access_token, cJSON_GetObjectItem(json, "access_token")->valuestring);

    if(access_token[0] == '\0')
    {
        printf("Failed to parse json\n");
        return 1;
    }
    return 0;
}

/*
 * Function: get_users
 * -----------------------
 * https://docs.microsoft.com/en-us/previous-versions/azure/ad/graph/api/users-operations
 *
 *  *tenant: char array containing the tenant
 *
 *  *access_token: char array containing the access token
 *
 *  *users: char array containing a list of users
 *
 *  returns 0 if completion successful, 1 if it fails.
 */
int get_users(char *tenant, char *access_token, char *users)
{
    BIO* bio;
    cJSON *json;
    SSL_CTX* ctx;

    char buf[1024];
    char json_buf[204800];
    char response_buf[204800];
    char write_buf[204800];
    int size, start, end, user_cnt = 0;

    const cJSON *user = NULL;
    const cJSON *user_data = NULL;

    strcpy(response_buf, " ");

    SSL_library_init();

    ctx = SSL_CTX_new(SSLv23_client_method());

    if (ctx == NULL)
        printf("ctx is null\n");

    bio = BIO_new_ssl_connect(ctx);
    BIO_set_conn_hostname(bio, "graph.windows.net:443");

    if(BIO_do_connect(bio) <= 0)
    {
        printf("Failed connection\n");
	return 1;
    }
    else
    {
        printf("Connected\n");
    }

    strcpy(write_buf, "GET /");
    strcat(write_buf, tenant);
    strcat(write_buf, "/users?api-version=1.6 HTTP/1.1\r\n");
    strcat(write_buf, "Authorization: Bearer ");
    strcat(write_buf, access_token);
    strcat(write_buf, "\r\n");
    strcat(write_buf, "User-Agent: azure_authenticator_pam/1.0 \r\n");
    strcat(write_buf, "Host: graph.windows.net\r\n");
    strcat(write_buf, "Connection: close \r\n");
    strcat(write_buf, "\r\n");

    if(BIO_write(bio, write_buf, strlen(write_buf)) <= 0)
    {
        if(!BIO_should_retry(bio))
        {
            printf("Do retry\n");
	}

	printf("Failed write\n");
    }

    for (;;)
    {
        size = BIO_read(bio, buf, 1023);

	if(size <= 0 )
	{
	    break;
	}
	buf[size] = 0;
	strcat(response_buf, buf);
    }

    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    find_json_bounds(response_buf, &start, &end);
    fill_json_buffer(json_buf, response_buf, &start, &end);

    json = cJSON_Parse(json_buf);
    if(json == NULL)
    {
        printf("json is null\n");
	return 1;
    }

    user_data = cJSON_GetObjectItem(json, "value");
    if(user_data == NULL)
    {
        printf("user_data is null\n");
	return 1;
    }

    cJSON_ArrayForEach(user, user_data)
    {
        users[user_cnt] = (char *) malloc(10*sizeof(cJSON_PrintUnformatted(user)));

        char *field = cJSON_GetObjectItem(user, USER_FIELD);
	strcpy(users[user_cnt], field);
	user_cnt++;
    }

    return 0;
}
