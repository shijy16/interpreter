extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);
int b=10;
int c=b;
int main(){
    int a = b;
    PRINT(c);
    b=1000;
    a=b;
    PRINT(a);
    a=11;
    PRINT(b);
    b=a;
    PRINT(b);
}
