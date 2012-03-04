#include <stdio.h>

extern void internalFunction();

void externalFunction()
{
    printf("LIB: externalFunction\n");
    printf("LIB: now calling internalFunction\n");
    internalFunction();
    printf("LIB: now calling internalFunction again\n");
    internalFunction();
}

