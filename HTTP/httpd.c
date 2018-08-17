#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <strings.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX 1024
#define HOME_PAGE "index.html"
#define PAGE_404 "wwwroot/404.html"

static void usage(const char* proc)
{
    printf("\nUsage:\n\t%s port\n\n", proc);
}

static int startup(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(2);
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);

    if(bind(sock, (struct sockaddr*)&local, sizeof(local))< 0)
    {
        perror("bind");
        exit(3);
    }

    if(listen(sock, 5) < 0)
    {
        perror("listen");
        exit(4);
    }
    return sock;
}

int get_line(int sock, char line[], int size)
{
    assert(line);
    assert(size > 0);
    //行分隔符 \n  \r\n \r 
    //统一按 \n来处理
    char c = 'a';   //只要初始化c不等于\n即可
    int i = 0;
    ssize_t s = 0;
    //字符中只有一个\n，可以将\n读出来

    while(i < size -1 && c != '\n')
    {
        //recv  ssize_t recv(int sockfd, void* buf, size_t len, int flags); 
        s =  recv(sock, &c, 1, 0);
        if(s > 0)
        {
            if(c == '\r')
            {
                // \r->\n  or  \r\n -> \n 
                // MSG_PEEK 检查下一个字符，不拿出来
                recv(sock, &c, 1, MSG_PEEK);
                //行分隔符只有\r
                if(c != '\n')
                {
                    c = '\n';
                }
                else
                {
                    recv(sock, &c, 1, 0); 
                }
            }
            line[i++] = c;   // \n
        }
    }
    line[i] = '\0';
    return i;
}

void clear_header(int sock)
{
    char line[MAX];
    do{
        get_line(sock, line, sizeof(line));
        printf("%s",line);
    }while(strcmp(line, "\n"));
}


void show_404(int sock)
{
    char line[1024];
    sprintf(line, "HTTP/1.0 404 Not Fount\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "Content-Type:text/html;charset=ISO-8859-1\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "\r\n");
    send(sock, line, strlen(line), 0);

    struct stat st;
    stat(PAGE_404, &st);
    int fd = open(PAGE_404, O_RDONLY);
    sendfile(sock, fd, NULL, st.st_size);
}

void echo_error(int sock, int code)
{
    switch(code)
    {
        case 400:
            break;
        case 403:
            break;
        case 404:
            show_404(sock);
            break;
        case 500:
            break;
        case 503:
            break;
        default:
            break;
    }
}

int echo_www(int sock, char* path, int size)
{
    int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        return 404;
    }
    clear_header(sock);
    char line[MAX];
    sprintf(line, "HTTP/1.0 200 OK\r\n");
    send(sock, line, strlen(line), 0); 
    sprintf(line, "Content-Type:text/html;charset=ISO-8859-1\r\n");
    send(sock, line, strlen(line), 0);
    sprintf(line, "\r\n");
    send(sock, line, strlen(line), 0);

    sendfile(sock, fd, NULL, size);
    close(fd);
    return 200;
}


int exc_cgi(int sock, char* method, char* path, char* query_string)
{
    char line[MAX];
    char method_env[MAX/10];
    char queryString[MAX];
    char content_length_env[MAX/10];
    int content_length = -1;
    if(strcasecmp(method, "GET") == 0)
    {
        clear_header(sock); 
    }
    else //POST
    {
        do{
            get_line(sock, line, sizeof(line));
            //Content-Length: 12
            if(strncmp(line, "Content-Length: ", 16) == 0)
            {
                content_length = atoi(line+16); 
            }
        }while(strcmp(line, "\n"));
        if(content_length == -1)
        {
            return 400;
        }
    }

    int input[2];   //child 角度 
    int output[2];  

    pipe(input);
    pipe(output);

    //path
    pid_t id = fork();
    if(id < 0)
    {
        return 503;
    }
    else if(id == 0)
    {
        close(input[1]);
        close(output[0]);

        sprintf(method_env, "METHOD=%s",method);
        putenv(method_env);

        if(strcasecmp(method, "GET") == 0)
        {
            sprintf(queryString, "QUERY_STRING=%s",query_string);
            putenv(queryString);
        }
        else
        {
            sprintf(content_length_env, "CONTENT_LENGTH=%d",content_length); 
            putenv(content_length_env);
        }

        dup2(input[0],0);
        dup2(output[1], 1);
        //程序替换不替换文件描述符
        execl(path, path, NULL); //执行谁， 怎么执行（可以带路径）

        exit(1);
    }
    else
    {
        close(input[0]);
        close(output[1]);
        char c;
        if(strcasecmp(method, "POST") == 0)
        {
            int i = 0;
            for(; i < content_length;i++)
            {
                recv(sock, &c, 1, 0);
                write(input[1], &c, 1);
            }
        }
        sprintf(line, "HTTP/1.0 200 OK\r\n") ;
        send(sock, line, strlen(line), 0);
        sprintf(line, "Content-Type:text/html;charset=ISO-8859-1\r\n");
        send(sock, line, strlen(line), 0);
        sprintf(line, "\r\n");
        send(sock, line, strlen(line), 0);
        while(read(output[0], &c, 1)>0)
        {
           send(sock, &c, 1, 0); 
        }
        waitpid(id, NULL, 0);
    }
    return 200;
}

void *handler_request(void *arg)
{
    int sock = (int64_t)arg;
    char line[MAX];
    char method[MAX/10];
    char url[MAX];
    size_t i = 0;
    size_t j = 0;
    int status_code = 200;
    int cgi = 0;
    char* query_string = NULL;
    char path[MAX];
    

    get_line(sock, line, sizeof(line));
    while(i < sizeof(method)-1 && j < sizeof(line) && !isspace(line[j]))
    {
        method[i] = line[j];
        i++,j++;
    }
    method[i] = '\0';
    //提取方法
    if(strcasecmp(method, "GET") == 0)
    {

    }
    else if(strcasecmp(method, "POST") == 0)
    {
        //POST方法必须用cgi
        cgi = 1;
    }
    else
    {
        clear_header(sock);
        status_code = 404; 
        goto end;
    }
    i = 0;
    //提取url
    while(j < strlen(line) && isspace(line[j]))
    {
        j++;
    }
    i = 0;
    while(i < sizeof(url)-1 && j < sizeof(line) && !isspace(line[j]))
    {
        url[i] = line[j];
        i++,j++;
    }
    url[i] = '\0';
#ifdef DEBUG
    printf("line: %s\n", line);
    printf("method: %s, url: %s\n",method, url);
#endif
    //method, url
    //GET 通过url传参 POST 通过请求正文传参
    if(strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while(*query_string) 
        {
            if(*query_string == '?')
            {
                *query_string = '\0';
                query_string++;
                cgi = 1;
                break;
            }
            query_string++;
        }
    }
    //method GET/POST, url, GET(query_string), cgi
    sprintf(path, "wwwroot%s",url);//path(wwwroot/, wwwroot/a/b.html) 
    if(path[strlen(path)-1] == '/')
    {
        strcat(path, HOME_PAGE);
    }

    struct stat st;
    if(stat(path, &st) < 0)
    {
        clear_header(sock);
        status_code = 404;
        goto end;
    }
    else
    {
        if(S_ISDIR(st.st_mode))
        {
            strcat(path, HOME_PAGE);
        }
        else if((st.st_mode & S_IXUSR ) || (st.st_mode & S_IXGRP) ||(st.st_mode & S_IXOTH))
        {
            cgi = 1; 
        }
        else
        {
            //DO NOTHING
        }

        if(cgi)
        {
            status_code =  exc_cgi(sock, method, path, query_string);       
        }
        else
        {
            //普通文件，get方法，没有cgi，没有参数
            status_code = echo_www(sock, path,st.st_size);
        }
    }


end:
    if(status_code != 200)
    {
        echo_error(sock, status_code);
    }
    close(sock);
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        usage(argv[0]) ;
        return 1;
    }
    int listen_sock = startup(atoi(argv[1]));

    for( ; ; )
    {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int64_t new_sock = accept(listen_sock, (struct sockaddr*)&client, &len);
        if(new_sock < 0)
        {
            perror("accept");
            continue;
        }
        //连接成功
        pthread_t id;
        pthread_create(&id, NULL, handler_request, (void*)new_sock);
        pthread_detach(id);
    }
}
