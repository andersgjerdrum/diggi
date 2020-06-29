#ifndef SETJMP_DIGGI_H
#define SETJMP_DIGGI_H
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long __jmp_buf_d[8];

typedef struct __jmp_buf_tag_d {
	__jmp_buf_d __jb;
	unsigned long __fl;
	unsigned long __ss[128/sizeof(long)];
} jmp_buf_d[1];

int setjmp_d(jmp_buf_d env);
void longjmp_d(jmp_buf_d env, int val);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif