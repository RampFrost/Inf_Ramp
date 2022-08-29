#include <stdio.h>
#include <stdlib.h>

int* functionDangling()
{
    int a,b,sum;
    a = 83;
    b = 93;
    sum = a+b;
    return &sum;
}

int main(int argc, char const *argv[])
{
    // case 1: de allocation of memory block
    int *ptr = (int *)malloc(7 * sizeof(int));
    ptr[0] = 83;
    ptr[0] = 63;
    ptr[0] = 73;
    ptr[0] = 33;
    printf("what is your name buddy?")
    free(ptr); // now our pointer is a dangling pointer.
    // case 2:Function returning local variable that is not accessible in the ongoing function/main function
    int *dangptr = functionDangling(); // now our pointer has become a dangling pointer.
    // case 3 : if a variable goes out of scope.

   int *danglingptr ;
   {//here we are creating local scope in main function using thes curly braces.

    int a = 12;
    danglingptr = &a;
    
    } /*here variable 'a' goes out of scope therefore danglingptr is pointing to a location which is 
    freed now hence our pointer danglingptr is now dangling pointer .  */
 

    return 0;
}
