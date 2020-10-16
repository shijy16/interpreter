extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);
int main(){
    int a = 1 > 2;
    PRINT(a);
    a = 3 < 10;
    PRINT(a);
    a = 3 + 3 + 3 < 10;
    PRINT(a);
    a = 3 < 1 + 3 + 3;
    PRINT(a);
    a = 1 + 1/10;
    PRINT(a);
    a = 9*1 - 3*10;
    PRINT(a);
}
