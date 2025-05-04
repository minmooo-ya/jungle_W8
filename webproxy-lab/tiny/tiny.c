/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);  
// 클라이언트 요청을 처리하는 함수 (정적 or 동적 콘텐츠 결정 포함)

void read_requesthdrs(rio_t *rp);  
// 요청 헤더를 읽고 무시하는 함수 (헤더 라인들을 읽기만 함)

int parse_uri(char *uri, char *filename, char *cgiargs);  
// 요청 URI를 분석하여 정적 or 동적 콘텐츠 판단하고
// filename과 CGI 인자를 분리해서 저장

void serve_static(int fd, char *filename, int filesize, char *method);  
// 정적 콘텐츠 (예: HTML, 이미지)를 클라이언트에 전송하는 함수

void get_filetype(char *filename, char *filetype);  
// 파일 확장자를 기반으로 MIME 타입 결정 (예: .html → text/html)

void serve_dynamic(int fd, char *filename, char *cgiargs);  
// 동적 콘텐츠 (CGI 프로그램)를 실행하고 그 출력을 클라이언트에 전달

void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);  
// 에러 발생 시 HTML 에러 페이지를 만들어 클라이언트에게 전송
/*	
  1.	프로그램 실행 시 포트 번호를 인자로 받음
	2.	Open_listenfd로 서버 소켓 생성 후 해당 포트 열기
	3.	무한 루프:
	  •	Accept로 클라이언트 요청 수락
	  •	Getnameinfo로 클라이언트 정보 읽기
	  •	doit()으로 요청 처리
	  •	Close로 연결 종료
*/
int main(int argc, char **argv)
{
  int listenfd, connfd; // listenfd: 듣기 전용 소켓, connfd: 연결된 클라이언트 소켓
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트 이름과 포트 저장
  socklen_t clientlen; // 클라이언트 주소 구조체 크기 저장용 변수
  struct sockaddr_storage clientaddr; // 클라이언트 주소 정보를 담는 구조체

  /* Check command line args */
  if (argc != 2)  // 인자 개수가 2개가 아니면 (프로그램명 + 포트번호)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // 사용법 출력
    exit(1); // 비정상 종료
  }

  listenfd = Open_listenfd(argv[1]); // 포트 번호로 리스닝 소켓 생성

  while (1) // 무한 루프: 클라이언트 요청을 계속 처리
  {
    clientlen = sizeof(clientaddr); // clientaddr의 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    // 클라이언트 연결 요청 수락 → 연결된 소켓 connfd 리턴

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    // 클라이언트 주소 정보를 사람이 읽을 수 있는 문자열로 변환

    printf("Accepted connection from (%s, %s)\n", hostname, port); 
    // 클라이언트 접속 정보 출력

    doit(connfd);  // 클라이언트 요청 처리 (정적/동적 구분 포함)
    Close(connfd); // 클라이언트 연결 종료
  }
}

/*
	1.	요청 라인 읽기
  → “GET /index.html HTTP/1.1” 이런 요청을 파싱
	2.	GET 메소드인지 확인
	3.	헤더 무시하고 URI 분석
  → /cgi-bin/adder?15000&213 → cgiargs = "15000&213"
	4.	파일 존재 여부 stat()로 확인
	5.	정적 or 동적 판단해서 적절한 함수 호출
	•	정적 → serve_static()
	•	동적 → serve_dynamic()
	6.	권한 오류, 파일 없음 시 → clienterror()로 HTML 에러 응답 생성
  */

void doit(int fd)
{
  int is_static; // 요청이 정적인지(파일 요청) 동적인지(CGI 실행 요청) 판별하는 플래그
  struct stat sbuf; // 파일 상태 정보를 저장할 구조체 (크기, 권한 등)
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; 
  // 클라이언트 요청 줄 파싱용 버퍼들
  char filename[MAXLINE], cgiargs[MAXLINE]; // 요청한 파일 이름, CGI 인자 저장
  rio_t rio; // Robust I/O 구조체

  Rio_readinitb(&rio, fd); // Robust I/O 초기화: connfd를 기반으로 rio 버퍼 설정
  Rio_readlineb(&rio, buf, MAXLINE); // 요청 라인의 첫 줄을 읽음
  printf("Request headers:\n");
  printf("%s", buf); // 요청 라인 출력

  // 요청 라인에서 메소드(GET), URI, 버전 파싱
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strstr(uri, "favicon.ico")) {
    // *** 요청 헤더는 반드시 다 읽어줘야 함 ***
    read_requesthdrs(&rio);  // 안 읽으면 소켓에 남은 데이터로 에러남
    printf("Ignoring favicon.ico request\n");
    return;
  }

  // Tiny는 GET 메소드만 지원. 다른 메소드는 에러 반환
  // 연습문제 11.11을 위해 HEAD추가
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", 
                "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio); // 요청 헤더들을 읽고 버림 (내용은 무시)

  // URI 분석 → 정적 요청이면 파일 이름 추출, 동적이면 CGI 인자도 분리
  is_static = parse_uri(uri, filename, cgiargs);

  // 요청한 파일의 상태 확인 (존재하는지, 어떤 종류인지 등)
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", 
                "Tiny couldn't find this file");
    return;
  }

  if (is_static) {
    // 정적 콘텐츠: 일반 파일이어야 하고, 사용자 읽기 권한이 있어야 함
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); // 정적 파일을 클라이언트로 전송
  }
  else {
    // 동적 콘텐츠: 실행 가능해야 함 (executable flag 확인)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", 
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // CGI 프로그램 실행 및 응답 전송
  }
}

// 클라이언트에게 HTTP 에러 응답을 HTML 형태로 전송하는 함수
/*
  1.	HTML 형태로 에러 메시지를 구성하고
	2.	HTTP 응답 헤더를 먼저 전송한 다음
	3.	HTML 본문을 클라이언트에게 전송하는 구조*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // 응답 헤더(buf)와 응답 본문(body)를 담을 버퍼

  // 응답 본문(HTML) 구성 시작

  sprintf(body, "<html><title>Tiny Error</title>"); // HTML 문서의 제목
  sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body); // 배경색 지정 (오타: bacolor → bgcolor)

  sprintf(body, "%s%s : %s\r\n", body, errnum, shortmsg); // 상태 코드 및 간단한 설명
  sprintf(body, "%s<p>%s : %s\r\n", body, longmsg, cause); // 자세한 설명과 원인
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 서버 정보 푸터

  // HTTP 응답 헤더 전송

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 상태 줄 생성 (예: HTTP/1.0 404 Not found)
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에게 상태 줄 전송

  sprintf(buf, "Content-type: text/html\r\n"); // MIME 타입 명시 (HTML)
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에게 전송

  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 본문 길이 명시 + 헤더 종료
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에게 전송

  // HTML 본문 전송

  Rio_writen(fd, body, strlen(body)); // 클라이언트에게 본문(HTML) 전송
}

//헤더만 다 읽고 버리는 함수
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];  // 요청 헤더의 각 줄을 저장할 버퍼

  Rio_readlineb(rp, buf, MAXLINE);  // 첫 번째 헤더 줄 읽기

  // 빈 줄("\r\n")이 나올 때까지 반복해서 헤더를 계속 읽음
  while (strcmp(buf, "\r\n")) {
      Rio_readlineb(rp, buf, MAXLINE);  // 다음 줄 읽기
      printf("%s", buf);                // 읽은 헤더 줄을 출력 (디버깅용)
  }

  return;  // 헤더를 모두 읽고 나면 함수 종료
}

//요청된 URI가 정적(static) 콘텐츠인지, 동적(dynamic, CGI) 콘텐츠인지 구분하고
//필요한 정보를 추출하는 핵심 함수
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  //[정적 콘텐츠] 요청이면: URI에 "cgi-bin"이 없음
  if (!strstr(uri, "cgi-bin")) { 
    strcpy(cgiargs, "");       // 정적 파일에는 CGI 인자가 없으므로 빈 문자열로 초기화
    strcpy(filename, ".");     // 현재 디렉토리 기준
    strcat(filename, uri);     // 예: "/index.html" → "./index.html"

    // URI가 '/'로 끝나면 디폴트로 home.html을 붙임
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");

      return 1;  // 정적 콘텐츠임을 의미
  }

  //[동적 콘텐츠] 요청이면: URI에 "cgi-bin" 포함
  else {
    ptr = index(uri, '?');  // '?' 문자가 있으면 쿼리 스트링 시작 위치

    if (ptr) {
      strcpy(cgiargs, ptr + 1); // '?' 뒤의 문자열을 cgiargs에 복사
      *ptr = '\0';              // '?' 위치를 NULL로 바꿔서 uri를 반으로 나눔
    }
    else
      strcpy(cgiargs, ""); // 인자가 없으면 빈 문자열

    strcpy(filename, ".");   // 현재 디렉토리 기준
    strcat(filename, uri);   // CGI 프로그램 경로 완성 (예: "./cgi-bin/adder")

    return 0;  // 동적 콘텐츠임을 의미
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
    int srcfd;                      // 파일 디스크립터
    char *srcp, filetype[MAXLINE]; // 파일을 메모리에 매핑할 포인터, MIME 타입 저장용
    char buf[MAXBUF];              // 응답 헤더 저장용 버퍼

     //1. 응답 헤더 생성 및 클라이언트에 전송
    
    get_filetype(filename, filetype);  // 파일 확장자 기반으로 MIME 타입 결정

    // 상태 줄 + 헤더들 작성
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);              // keep-alive X
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);   // 응답 본문 크기
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // MIME 타입 (ex. text/html)

    Rio_writen(fd, buf, strlen(buf)); // 클라이언트에 헤더 전송

    printf("Response headers:\n");    // 서버 로그 출력
    printf("%s", buf);

      //2. 파일 본문을 메모리에 매핑 후 클라이언트로 전송

    srcfd = Open(filename, O_RDONLY, 0); // 파일 열기 (읽기 전용)
    
    // 파일을 메모리에 매핑 (mmap): 빠르게 전송하기 위해 메모리에 바로 매핑
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd); // 파일 디스크립터는 닫아도 mmap으로 접근 가능
    // 연습문제 11.11
    if (strcasecmp(method, "HEAD") != 0) {
      Rio_writen(fd, srcp, filesize); // GET 요청일 때만 전송
    }
    Munmap(srcp, filesize);         // 메모리 매핑 해제
/*  
    csapp 숙제문제 11.9 
    malloc + rio_readn + rio_writen 사용하기

    srcfd = Open(filename, O_RDONLY, 0); // 파일 열기 (읽기 전용)
    void *bbuf = malloc(filesize);
    rio_readn(srcfd, bbuf, filesize);
    rio_writen(fd, bbuf, filesize);
    Close(srcfd);
    free(bbuf);
*/

}

/*
 * get_filetype - Derive file type from filename
 * 요청한 파일의 확장자를 보고, 적절한 MIME 타입 문자열을 filetype에 복사해줌
 */
void get_filetype(char *filename, char *filetype)
{
  // 파일 이름에 ".html" 포함되어 있으면 → HTML 문서
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");

  // ".gif" → GIF 이미지
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");

  // ".png" → PNG 이미지
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");

  // ".jpg" → JPEG 이미지
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  
    //숙제문제 11.7
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpeg");

  // 위에 해당하지 않으면 → 일반 텍스트로 처리
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE];                   // 응답 헤더 저장용 버퍼
  char *emptylist[] = { NULL };        // 인자 없는 execve용 인자 리스트 (argv = NULL)

  /*1. 클라이언트에게 HTTP 응답 헤더 전송 */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));    // 상태 줄 전송

  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));    // 서버 정보 헤더 전송

  /*2. 자식 프로세스 생성하여 CGI 실행 */
  if (Fork() == 0) {  // 자식 프로세스라면
    /* 환경 변수 설정: CGI 인자 전달 (QUERY_STRING) */
    setenv("QUERY_STRING", cgiargs, 1);  // "15000&213" 같은 문자열

    /* 클라이언트로 직접 출력하도록 stdout을 fd로 리다이렉트 */
    Dup2(fd, STDOUT_FILENO);  // printf → 브라우저로 바로 전달

    /* CGI 프로그램 실행 (예: ./cgi-bin/adder) */
    Execve(filename, emptylist, environ);  // 인자 없이, 환경 변수 전달
  }

  /*3. 부모 프로세스는 자식 종료를 기다림 */
  Wait(NULL);  // 좀비 프로세스 방지 (자식 회수)
}