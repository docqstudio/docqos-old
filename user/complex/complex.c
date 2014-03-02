#include <stdio.h>

int main(int argc,const char *argv[])
{
   int r,i;
   char buffer[128];
   printf("Please input a complex number:");
   fgets(buffer,sizeof(buffer),stdin); /*Gets the string.*/

   if(sscanf(buffer,"%d+%di",&r,&i) == 2)
      goto next;
   if(sscanf(buffer,"%di+%d",&i,&r) == 2)
      goto next;
   if(sscanf(buffer,"%d+i%d",&r,&i) == 2)
      goto next;
   if(sscanf(buffer,"i%d+%d",&i,&r) == 2)
      goto next; /*Try all formats we supported.*/
   for(i = 0;buffer[i] != '\n';++i)
      ;
   buffer[i] = '\0'; /*Change '\n' to '\0'.*/
   printf("The string you inputted \"%s\" is not supported!\n",buffer);
   printf("The complex number formats we supported are:\n");
   printf("    a+bi a+ib bi+a ib+a (a and b are both real numbers).\n");
   return -1;
next:
   printf("The real number is %d,the imaginary number is %d.\n",r,i);
          /*Print the information to the screen.*/
   return 0;
}
