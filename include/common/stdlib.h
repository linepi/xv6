#pragma once
#include <stdarg.h>
#include "common/types.h"

int             rand();
int             rand_r(unsigned int *);
void            srand(uint);
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
int             strcmp(const char*, const char*);
char *					strcat(char *s, const char *append);
char*           strncpy(char*, const char*, int);
char*           strcpy(char*, const char*);
char*						strchr(const char *s, char c);
char*						strrchr(const char *s, char c);
char *					strtok(char *s, const char *delim);
char *					strtok_r(char *s, const char *delim, char **last);
int 						snprintf(char* str, uint64 size, const char* format, ...);
int 						sprintf(char *str, const char *format, ...);
int 						vsnprintf(char* str, uint64 size, const char* format, va_list arg);
unsigned long 	strtoul(const char *nptr, char **endptr, int base);
long 						strtol(const char *nptr, char **endptr, int base);
int 						atoi(const char *s);
double 					strtod(const char *string, char **endPtr);
int 						isspace(int c);
int 						isdigit(int c);
int 						isupper(int c);
int 						isalpha(int c);
char *					basename(const char *path);
int 						bscanf(const char *buffer, const char *format, ...);