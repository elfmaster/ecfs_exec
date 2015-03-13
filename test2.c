
extern int b[];
extern int B(int *, int);

int A() {
        return B(b, b[0]);
}
