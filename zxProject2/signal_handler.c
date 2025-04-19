#include "include/config.h"
#include "include/signal_handler.h"


volatile sig_atomic_t sigint_flag = 0;

static void sigint_handler(int sig) {
    printf("[srv] SIGINT received, shutting down...\n");
    sigint_flag = 1;
}

void sigaction_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_handler;
    //sa.sa_flags = SA_RESTART;  // 自动重启被中断的系统调用

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction init failed");
    }
}