#include "csapp.h" // CS:APP 라이브러리 함수 포함

void echo(int connfd); // 클라이언트 메시지를 받아서 되돌려주는 함수 선언, connfd:연결된 클라이언트 소켓의 파일 디스크립터

int main(int argc, char **argv)
{
    int listenfd, connfd;  // listenfd: 서버가 리스닝할 소켓(항상 열려있음), connfd: 연결이 수락되면 생성되는 소켓 (클라이언트와 1:1 통신용)
    socklen_t clientlen;   // 클라이언트 주소 구조체 크기
    struct sockaddr_storage clientaddr; // 클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트의 호스트명과 포트 번호 저장용

    if (argc != 2) { // 인자가 2개가 아니면 (프로그램 이름 + 포트 번호)
        fprintf(stderr, "usage: %s <port>\\n", argv[0]); // 올바른 사용법을 에러 출력에 안내
        exit(0); // 프로그램 종료 (정상 종료 상태 코드 0)
    }
    // 서버가 클라이언트를 기다리고 → 연결 수락하고 → echo 처리하고 → 다시 대기하는 전체 흐름
    listenfd = Open_listenfd(argv[1]); // 포트 열고 리스닝 소켓 생성

    while (1) { // 클라이언트 연결을 무한히 수신
        clientlen = sizeof(struct sockaddr_storage); // 주소 구조체 크기 설정
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 수락
    
        // 클라이언트 주소를 사람이 읽을 수 있는 문자열로 변환
        Getnameinfo((SA *)&clientaddr, clientlen, 
                    client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
    
        printf("Connected to (%s, %s)\n", client_hostname, client_port); // 연결 정보 출력
    
        echo(connfd);       // echo 함수 실행: 데이터 받고 다시 보내기
        Close(connfd);      // 연결 종료
    }
    exit(0);
}
/*	1.	포트를 열고(listenfd)
	2.	클라이언트 연결 수락(accept)
	3.	연결된 소켓(connfd)로 메시지를 받음
	4.	받은 그대로 다시 돌려줌 (echo)
	5.	연결 닫고 다음 요청 기다림
    
    [서버 실행]
    ↓
    [포트 열기: socket → bind → listen]
        ↓
    [클라이언트 연결 기다림]
        ↓
    [연결 수락: accept → connfd 생성]
        ↓
    [클라이언트 IP/포트 출력]
        ↓
    [connfd로 클라이언트와 통신: echo(connfd)]
        ↓
    [연결 종료: Close(connfd)]
        ↓
    [다시 다음 클라이언트 요청 대기]*/