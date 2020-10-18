extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int b=10;
int f(int x,int y) {
    if(x >= 1){
        return y + f(x-1,y);}
    else return 0;
}
int main() {
   int a = f(1,10);
    PRINT(a);
}


#10
