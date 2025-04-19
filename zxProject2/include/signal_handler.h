#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

// 全局信号标志
extern volatile sig_atomic_t sigint_flag;

// 初始化信号处理器
void sigaction_init(void);

#endif