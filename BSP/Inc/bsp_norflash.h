
#ifndef _BSP_NORFLASH_H_
#define _BSP_NORFLASH_H_

typedef struct _NORFLASH_OBJ {
	void (*Init)(void);
	void (*DataRead)(int, void *, int);
	void (*DataWrite)(int, void *, int);
} NORFLASH_OBJ;

void *BSP_NORFLASH_OBJ(void);

#endif
