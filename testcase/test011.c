extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);
int x = 100 + 100 - 100 + 101;
int y = x + 100 -100 -100-101;
int main(){
    int b = 1 + 1;
    int a;
    a = 100;
    a = a - b + 100 + a + 2 + 2 -b;
    PRINT(b);
    PRINT(a);
    PRINT(x);
    PRINT(y);
}
