#include "mymath.h"

// 这个 .c 文件位于库源码根目录，会被默认的 sources=["*.c*"] 规则自动收集编译

int my_add(int a, int b) {
    return a + b;
}

int my_mul(int a, int b) {
#ifdef MYMATH_ENABLE_MUL
    // 只有 application.fam 里通过 cdefines 定义了 MYMATH_ENABLE_MUL 时才编译这段
    return a * b;
#else
    return 0;
#endif
}
