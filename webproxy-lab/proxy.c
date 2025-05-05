#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* User-Agent header to send in requests */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

// 함수 선언부
void doit(int fd);
void read_requesthdrs(rio_t *rp);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp);

int main(int argc, char **argv)
{
  int listenfd, connfd; // 클라이언트 요청 수신용 listen 소켓과 connection 소켓
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트명과 포트 저장
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트 주소 정보 저장 구조체

  // 포트번호 인자 체크
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 서버 listen 소켓 열기

  while (1) {
    clientlen = sizeof(clientaddr);  // 클라이언트 주소 구조체 크기 설정
  
    int *connfdp = Malloc(sizeof(int));  // 클라이언트 연결 소켓을 저장할 메모리 동적 할당
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);  
    // 클라이언트 연결 요청 수락 → 연결된 소켓 파일 디스크립터를 connfdp에 저장
  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    // 클라이언트의 IP 주소와 포트를 사람이 읽을 수 있는 문자열로 변환
  
    printf("Accepted connection from (%s, %s)\n", hostname, port);  
    // 연결된 클라이언트 정보를 출력 (디버깅용)
  
    pthread_t tid;  
    Pthread_create(&tid, NULL, thread, connfdp);  
    // 새로운 스레드를 생성하여 thread 함수에서 클라이언트 요청 처리 시작
    // connfdp는 스레드로 전달되어 처리 후 내부에서 free됨
  }
}

void *thread(void *vargp) {
  int connfd = *((int *)vargp); // 전달받은 인자를 정수형 포인터로 변환하여 클라이언트 소켓 파일 디스크립터(connfd) 추출
  Pthread_detach(pthread_self()); // 현재 스레드를 분리(detach) 상태로 설정 → 스레드 종료 시 자원 자동 회수 (join 불필요, 메모리 누수 방지)
  Free(vargp);  // heap에서 할당한 인자 메모리 해제 (connfd 저장한 메모리)
  doit(connfd); // 클라이언트 요청 처리 함수 호출
  Close(connfd);  // 클라이언트 소켓 닫기
  return NULL;  // 스레드 종료 (반환값 없음)
}

// 클라이언트 요청을 처리하는 함수
void doit(int fd)
{
  // 클라이언트로부터 받은 요청과 서버로 전송할 요청 및 응답을 저장할 버퍼들
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; 
  rio_t rio; // Robust I/O 버퍼 (클라이언트용)
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE]; // URI 파싱 결과
  int serverfd; // 서버와의 연결 소켓
  rio_t server_rio; // Robust I/O 버퍼 (서버용)
  char request_hdr[MAXLINE]; // 서버에 보낼 요청 헤더
  size_t n; // 읽은 바이트 수

  // 클라이언트 소켓을 위한 RIO 버퍼 초기화
  Rio_readinitb(&rio, fd);

  // 요청의 첫 번째 라인 (예: "GET http://host/path HTTP/1.1") 읽기
  Rio_readlineb(&rio, buf, MAXLINE);

  // 요청 라인을 파싱해서 메서드(GET 등), URI, HTTP 버전 추출
  sscanf(buf, "%s %s %s", method, uri, version);

  // 나머지 요청 헤더는 무시 (필수 헤더는 우리가 따로 추가함)
  read_requesthdrs(&rio);

  // 요청 정보 출력 (디버깅용)
  printf("Parsed request: %s %s %s\n", method, uri, version);

  // GET 메서드만 지원하며, 다른 메서드는 에러 응답
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not Implemented", "Proxy does not implement this method");
    return;
  }

  // URI에서 hostname, path, port 추출
  parse_uri(uri, hostname, path, port);
  printf("Parsed URI → host: %s, path: %s, port: %s\n", hostname, path, port);

  // 서버와 연결 시도 (실패 시 에러 처리)
  serverfd = Open_clientfd(hostname, port);
  if (serverfd < 0) {
    clienterror(fd, hostname, "502", "Bad Gateway", "Proxy failed to connect to end server");
    Close(fd);
    return;
  }

  // 서버 소켓을 위한 RIO 버퍼 초기화
  Rio_readinitb(&server_rio, serverfd);

  // 서버에 보낼 HTTP 요청 헤더 구성 시작
  request_hdr[0] = '\0'; // 문자열 초기화

  // 요청 라인: GET {path} HTTP/1.0
  sprintf(buf, "GET %s HTTP/1.0\r\n", path);
  strcat(request_hdr, buf);

  // Host 헤더
  sprintf(buf, "Host: %s\r\n", hostname);
  strcat(request_hdr, buf);

  // User-Agent 헤더 (지정된 고정 값)
  strcat(request_hdr, user_agent_hdr);

  // 연결 종료 명시
  strcat(request_hdr, "Connection: close\r\n");
  strcat(request_hdr, "Proxy-Connection: close\r\n");

  // 헤더 종료를 알리는 빈 줄
  strcat(request_hdr, "\r\n");

  // 생성된 헤더 출력 (디버깅용)
  printf("Request header built:\n%s", request_hdr);

  // 서버에 요청 전송
  Rio_writen(serverfd, request_hdr, strlen(request_hdr));

  // 서버로부터 응답을 읽어 클라이언트에게 전달
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(fd, buf, n);
  }

  // 서버 연결 종료
  Close(serverfd);
}

// 클라이언트에게 오류 응답을 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
  sprintf(body, "%s%s : %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// URI에서 hostname, path, port를 파싱하는 함수
void parse_uri(char *uri, char *hostname, char *path, int *port)
{
  char *hostbegin, *hostend, *pathbegin, *portbegin;

  // URI가 "http://"로 시작하는 경우 이를 건너뛰고 host 정보 시작 위치 지정
  if (strncasecmp(uri, "http://", 7) == 0)
    hostbegin = uri + 7;
  else
    hostbegin = uri;

  // '/' 문자가 있는 경우 → 경로(path) 시작 부분 찾기
  pathbegin = strchr(hostbegin, '/');
  if (pathbegin) {
    // path 복사
    strcpy(path, pathbegin);
    // host 부분만 추출하기 위해 '/' 위치를 널 문자로 끊음
    *pathbegin = '\0';
  } else {
    // '/'가 없다면 path는 빈 문자열
    path[0] = '\0';
  }

  // ':' 문자가 있는 경우 → 포트 번호가 명시된 경우
  portbegin = strchr(hostbegin, ':');
  if (portbegin) {
    *portbegin = '\0';            // host와 port를 구분
    strcpy(hostname, hostbegin);  // host 복사
    strcpy(port, portbegin + 1);  // ':' 다음 포트 복사
  } else {
    // 포트가 명시되지 않았다면 기본 포트는 80
    strcpy(hostname, hostbegin);
    strcpy(port, "80");
  }
}

// 요청 헤더를 읽어들이는 함수. 불필요한 헤더는 무시함
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  while (1) {
    // 한 줄씩 요청 헤더를 읽는다
    Rio_readlineb(rp, buf, MAXLINE);

    // 빈 줄이면 헤더 끝 (HTTP에서 헤더 끝은 빈 줄로 표시)
    if (!strcmp(buf, "\r\n")) break;

    // 아래의 헤더는 우리가 프록시에서 직접 구성하므로 무시
    if (!strncasecmp(buf, "Host:", 5) ||
        !strncasecmp(buf, "User-Agent:", 11) ||
        !strncasecmp(buf, "Connection:", 11) ||
        !strncasecmp(buf, "Proxy-Connection:", 17)) continue;

    // 그 외의 다른 헤더들도 현재 구현에서는 무시
  }
}

/*     병렬처리 프록시 구현에서의 흐름	
  1.	메인 루프
    •	Accept()로 클라이언트 요청 대기
    •	연결되면 connfd를 동적 메모리 할당 후
	2.	스레드 생성
    •	pthread_create()로 새 스레드 생성
    •	connfd를 인자로 thread() 함수에 전달
	3.	스레드 처리 (thread() 함수)
    •	doit(connfd) 호출해서 요청 처리
    •	응답 완료 후 Close()
    •	Pthread_detach()로 자원 자동 회수
    •	Free()로 connfd 메모리 해제
	4.	doit() 함수
	  •	요청 파싱 → 서버에 요청 → 응답 받아 클라이언트에 전달
  
          생각하면 좋을 포인트들
  •	각 요청은 별도 스레드에서 독립적으로 처리
	•	메인 스레드는 끊임없이 Accept()만 수행
	•	detach로 스레드 메모리 누수 방지*/