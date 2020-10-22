extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   char* a;
   a = (char*)MALLOC(sizeof(char)*10);
   a[0] = 'a';
   a[9] = 'b';
   int b = (int) a[0];
   PRINT(b);
   b = (int) a[9];
   PRINT(b);
}
