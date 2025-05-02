#include "csapp.h" // CS:APP 책에서 제공하는 라이브러리 포함 (소켓, I/O 관련 함수들)

/*
 * 클라이언트 프로그램: 서버에 연결하여 사용자의 입력을 보내고,
 * 서버로부터 echo 응답을 받아 다시 출력함.
 */

int main(int argc, char **argv)
{
    int clientfd;                  // 서버와 연결된 소켓 디스크립터
    char *host, *port;             // 명령줄 인자로 받은 호스트 주소와 포트 번호
    char buf[MAXLINE];             // 데이터 송수신 버퍼
    rio_t rio;                     // Robust I/O를 위한 rio 버퍼 구조체

    // 인자 개수 확인: host와 port가 없으면 사용법 안내 후 종료
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // 사용법 안내 메시지
        exit(0); // 프로그램 종료
    }

    // 명령줄 인자에서 호스트 주소와 포트 번호 추출
    host = argv[1];
    port = argv[2];

    // 서버에 TCP 연결을 시도하고, 연결된 소켓 디스크립터 반환
    clientfd = Open_clientfd(host, port);

    // Robust I/O를 위한 rio 버퍼 초기화
    Rio_readinitb(&rio, clientfd);

    // 사용자로부터 입력을 받아 서버에 보내고, 서버로부터 echo를 받아 출력
    while (Fgets(buf, MAXLINE, stdin) != NULL) {          // 사용자 입력 받기
        Rio_writen(clientfd, buf, strlen(buf));           // 서버에 데이터 전송
        Rio_readlineb(&rio, buf, MAXLINE);                // 서버로부터 echo 응답 읽기
        Fputs(buf, stdout);                               // 응답을 화면에 출력
    }

    // 연결 종료
    Close(clientfd);
    exit(0); // 프로그램 정상 종료
}
/*[1] 사용자로부터 서버 주소와 포트 번호 입력 받음
    ↓
[2] 해당 서버로 TCP 연결 시도 (connect)
    ↓
[3] robust I/O용 버퍼(rio)를 소켓에 연결해 초기화
    ↓
[4] 사용자 입력을 한 줄씩 받아
    ↓
[5] 서버에 전송하고
    ↓
[6] 서버로부터 echo 응답을 받아
    ↓
[7] 화면에 출력
    ↓
[8] 이 과정을 반복 (Ctrl+D로 종료 시 종료)*/