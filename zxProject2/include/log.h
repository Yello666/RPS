#if defined(OS_ANDROID)
#include <android/log.h>

#define LOG(fmt...) __android_log_print(ANDROID_LOG_DEBUG,__FILE__,##fmt)

#else
#define LOG(fmt...)  do { fprintf(stderr,"%s %s ",__DATE__,__TIME__); fprintf(stderr, ##fmt); } while(0)
#endif
