// Credits: https://sigalrm.blogspot.com/2011/10/cortex-m3-exception-vector-checksum.html
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("usage: cm3_checksum <bin>\n");
        return 1;
    }

    FILE *file;
    if ((file = fopen(argv[1], "r+")) == NULL) {
        return 1;
    }

    /* The checksum of the exception vector */
    unsigned int i, n, checksum=0;
    for (i=0; i<7; i++) {
        fread(&n, 4, 1, file);
        checksum += n;
    }

    /* The 2's complement of the checksum */
    checksum = -checksum;
    printf("checksum: 0x%X\n", checksum);

    /* write back the checksum to location 7 */
    fseek(file, 0x1c, SEEK_SET);
    fwrite(&checksum, 4, 1, file);

    fclose(file);
    return 0;
}
