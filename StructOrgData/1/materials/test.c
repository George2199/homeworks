#include <stdio.h>

typedef
struct element
{
    int data;
    struct element * next;
    struct element * prev;
} node;

void filecopy(FILE *ifp, FILE *ofp)
{
    int c;
    while ((c = getc(ifp)) != EOF)
    putc(c, ofp);
}

int read_file(char *argv[])
{
    FILE *fp;
    if ((fp = fopen(*++argv, "r")) == NULL)
    {
        printf("can't open file %s\n", *argv);
        return 0;
    } else {
        filecopy(fp, stdout);
        fclose(fp);
    }
    return 1;
}

int main()
{
    char *files[1];
    files[1] = "./b.txt";
    read_file(files);
    return 0;
}