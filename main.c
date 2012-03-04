#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <dlfcn.h>
#include "patcher.h"

int gDidPatch = 0;

void sneakAttack()
{
    printf("MAIN: sneak attack!\n");
}

extern void internalFunction()
{
    printf("MAIN: internalFunction\n");

    
    if( gDidPatch == 0 ) {
        printf("MAIN: patching theLib.so...\n");
        int result = patchFunctionForLib("./theLib.so", "internalFunction", &sneakAttack);
        printf("MAIN: patch result: %s\n", result == 0 ? "SUCCESS!" : "FAILURE!");
        gDidPatch = 1;
    }
}

int main(int argc, void **argv)
{
    printf("opening theLib.so\n");
    void * lib = dlopen("./theLib.so", RTLD_LAZY);
    if( lib == 0 ) {
        printf("Couldn't open theLib.so\n");
        return 1;
    }



    printf("MAIN: calling externalFunction\n");
    void(*externalFunction)() = dlsym(lib, "externalFunction");
    externalFunction(); 
    printf("MAIN: calling my internalFunction\n");
    internalFunction(); 

    return 0;
}


