/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

// HTTP transaction 한개 실행하는 function
void doit(int fd);
// Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않기 때문에 이들을 읽고 무시하는 함수
void read_requesthdrs(rio_t *rp);
// URI를 domian, path, cgiargs로 나누는 함수
int parse_uri(char *uri, char *filename, char *cgiargs);
// filetype 가져오는 함수
void get_filetype(char *filename, char *filetype);
// 정적 컨텐츠 처리하는 함수
void serve_static(int fd, char *filename, int filesize);
// 동적 컨텐츠 처리하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs);
//HTTP 에러 메세지 보내는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);


int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // open a listening socket
  // 인자로 받은 port에 listenfd 생성
  listenfd = Open_listenfd(argv[1]);  

  while (1) {
    clientlen = sizeof(clientaddr);

    // accepting a connection request
    // connfd 연결 식별자 소켓에 clientaddr 저장
    // int Accept(int listenfd, struct sockaddr *addr, int *addrlen);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept

    // 클라이언트의 주소를 getnameinfo 함수에 넣어, hostname(ip주소)와 포트 번호를 얻고 프린트
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // perform a transaction
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

// handles one HTTP transaction
void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // Read request line and headers 
  // 식별자 fd를 주소 rio에 위치한 읽기 버퍼와 연결
  Rio_readinitb(&rio, fd);
  // 요청 라인을 읽음
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);   // ex) GET/ HTTP / 1.1

  // GET 요청인지 확인
  if (strcasecmp(method, "GET")){ // strcasecmp - 대소문자 구분하지 않고 스트링 비교
                                  // 둘이 같으면 0 return
    clienterror(fd, method, "501", "Not implemented", 
        "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio);

  /* parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  // stat 함수는 filename 받아서 stat 구조체 멤버들 채워줌
  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", 
                "Tiny couldn't find this file");
    return;
  }

  // static(정적) 컨텐츠라면
  if (is_static){
    // 보통 파일이라는 것과 읽기 권한을 가지고 있는지 확인, stat함수가 해줬던 거
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  // dynamic(동적) 컨텐츠라면
  else{
    // 보통 파일이라는 것과 실행 가능한지
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR % sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

// send an HTTP response to the client with appropriate status code and status message
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  
  char buf[MAXLINE], body[MAXBUF];
  
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "%s Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않기 때문에 이들을 읽고 무시하는 함수
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*
  Tiny assumes that the home directory for static content is its current directory and
  that the home directory for executables is ./cgi-bin. Any URI that contains the
  string cgi-bin is assumed to denote a request for dynamic content. The default
  filename is ./home.html. 
  parse_uri implements these policies.
*/
int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;
  // static content
  if (!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) -1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  // dynamic content
  else{
    ptr = index(uri, '?');
    if (ptr){
      strcpy(cgiargs, ptr +1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");

    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// derive file type from filename
// strstr returns a pointer to the first occurence of str2 in str1, or null
void get_filetype(char *filename, char *filetype){
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image.jpg");
  // MP4 파일
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video.mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_static(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = malloc(filesize);

  // ssize_t rio_readn(int fd, void *usrbuf, size_t n)
  // send file content to virtual memory
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);

  // ssize_t rio_writen(int fd, void *usrbuf, size_t n)
  Rio_writen(fd, srcp, filesize);
  free(srcp);
  
  /* Original verison
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
  */
}

void serve_dynamic(int fd, char *filename, char *cgiargs){
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 만듬
  if (Fork() == 0){
    //setenv(const char* name, const char* value, int overwrite)
    setenv("QUERY_STRING", cgiargs, 1);
    // redirect stdout to client
    Dup2(fd, STDOUT_FILENO);  
    // run CGI program
    // 부모와 자식이 다른 작업하면서 둘 다 살아있게 하기 위해 사용
    Execve(filename, emptylist, environ);
  }
  // 자식 프로세스가 올바르게 종료되는걸 기다림
  Wait(NULL);
}