extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int f(int x,int y){
    if(y > 0)
        return x + f(x + 10,0);
    else return 0;
}

int main() {
   int a;
   int b = 10;
   a = f(-10 +  b *(- 5),10);
   if (a > 0 ) {
     b = a;
   } else {
     b = -a;
   }
   PRINT(b);
}
