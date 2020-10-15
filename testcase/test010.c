extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);
int b=10;
int main(){
    int a = 1;
    PRINT(a);
    b=1000;
    PRINT(b);
    b=a;
    PRINT(b);
}
