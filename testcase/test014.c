extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int a = 2;
void f(int b,int a){
    PRINT(b);
    return;
    PRINT(100);
    PRINT(a);
}

int main(){
    int b = 1;
    f(2,10);
    PRINT(b);
}
