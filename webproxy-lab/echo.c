#include "csapp.h" // 유틸리티 함수 및 상수 포함

// 클라이언트와 연결된 소켓을 통해 데이터를 받고, 그대로 돌려주는 함수
void echo(int connfd)
{
    size_t n;               // 읽은 바이트 수 저장 변수
    char buf[MAXLINE];      // 메시지 저장용 버퍼
    rio_t rio;              // robust I/O 구조체

    Rio_readinitb(&rio, connfd); // rio 구조체 초기화 (connfd 기준)

    // 클라이언트가 보낸 메시지를 줄 단위로 계속 읽음
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", (int)n); // 로그 출력
        Rio_writen(connfd, buf, n); // 받은 메시지를 클라이언트에 다시 보냄 (echo)
    }
}

/* robust buffered input: 예외 상황에도 끄떡없는 효율적인 줄 단위 입력 처리 방식
   	•	Rio_readinitb() → 버퍼 초기화
	•	Rio_readlineb() → 줄 단위로 robust하게 읽기
	•	Rio_writen() → 정확히 n바이트 쓸 때까지 반복해서 write
	1.	rio를 소켓 connfd에 연결해서 robust read 준비
	2.	줄 단위로 클라이언트 메시지 수신
	3.	받은 내용을 로그로 출력
	4.	받은 줄을 그대로 다시 클라이언트로 보냄
	5.	클라이언트가 연결을 종료할 때까지 반복
*/