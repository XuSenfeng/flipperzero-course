#pragma once

// 一个演示 fap_private_libs 的最小私有库
// 头文件放在 include/ 子目录，通过 application.fam 里的
// fap_include_paths=["include"] 暴露给父级 fap 使用

// 求和
int my_add(int a, int b);

// 求积（仅当编译时定义了 MYMATH_ENABLE_MUL 才真正相乘，否则返回 0）
// 用于演示 application.fam 里的 cdefines 参数
int my_mul(int a, int b);
