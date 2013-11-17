#pragma once

int initKMalloc(void);
void *kmalloc(unsigned int size);
int kfree(const void *obj);
