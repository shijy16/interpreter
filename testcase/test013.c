extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);
int main(){
    int a = 1;
    int b = 3;
    if (a > b){
        PRINT(a);
    }else{
        PRINT(b);
    }

}
