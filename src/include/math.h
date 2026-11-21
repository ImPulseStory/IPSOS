#ifndef IPSOS_MATH_H
#define IPSOS_MATH_H
#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
int isinf(double value);
int isnan(double value);
long double ldexpl(long double value, int exponent);
double strtod(const char *str, char **endptr);
float strtof(const char *str, char **endptr);
long double strtold(const char *str, char **endptr);
#endif
