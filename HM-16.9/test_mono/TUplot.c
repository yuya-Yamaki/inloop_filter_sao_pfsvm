#include <stdio.h>
#include <stdlib.h>


typedef unsigned char img_t;
typedef struct {
  int height;
  int width;
  int maxval;
  img_t **val;
} IMAGE;

FILE *fileopen(char *filename, const char *mode)
{
    FILE *fp;
    fp = fopen(filename, mode);
    if (fp == NULL) {
        fprintf(stderr, "Can\'t open %s!\n", filename);
        exit(1);
    }
    return (fp);
}

void *alloc_mem(size_t size)
{
    void *ptr;
    if ((ptr = (void *)malloc(size)) == NULL) {
        fprintf(stderr, "Can\'t allocate memory (size = %d)!\n", (int)size);
        exit(1);
    }
    return (ptr);
}

void **alloc_2d_array(int height, int width, int size)
{
    void **mat;
    char *ptr;
    int k;

    mat = (void **)alloc_mem(sizeof(void *) * height + height * width * size);
    ptr = (char *)(mat + height);
    for (k = 0; k < height; k++) {
        mat[k] =  ptr;
        ptr += width * size;
    }
    return (mat);
}

IMAGE *alloc_image(int width, int height, int maxval)
{
    IMAGE *img;
    img = (IMAGE *)alloc_mem(sizeof(IMAGE));
    img->width = width;
    img->height = height;
    img->maxval = maxval;
    img->val = (img_t **)alloc_2d_array(img->height, img->width,
                                        sizeof(img_t));
    return (img);
}

void free_image(IMAGE *img)
{
    if (img != NULL && img->val != NULL) {
        free(img->val);
        free(img);
    } else {
        fprintf(stderr, "! error in free_image()\n");
        exit(1);
    }
}

IMAGE *read_pgm(char *filename)
{
    int i, j, width, height, maxval;
    char tmp[256];
    IMAGE *img;
    FILE *fp;

    fp = fileopen(filename, "rb");
    fgets(tmp, 256, fp);
    if (tmp[0] != 'P' || tmp[1] != '5') {
        fprintf(stderr, "Not a PGM file!\n");
        exit(1);
    }
    while (*(fgets(tmp, 256, fp)) == '#');
    sscanf(tmp, "%d %d", &width, &height);
    while (*(fgets(tmp, 256, fp)) == '#');
    sscanf(tmp, "%d", &maxval);
    img = alloc_image(width, height, maxval);
    for (i = 0; i < img->height; i++) {
	for (j = 0; j < img->width; j++) {
            img->val[i][j] = (img_t)fgetc(fp);
	}
    }
    fclose(fp);
    return (img);
}

void write_pgm(IMAGE *img, char *filename)
{
    int i, j;
    FILE *fp;
    fp = fileopen(filename, "wb");
    fprintf(fp, "P5\n%d %d\n%d\n", img->width, img->height, img->maxval);
    for (i = 0; i < img->height; i++) {
	for (j = 0; j < img->width; j++) {
            putc(img->val[i][j], fp);
	}
    }
    fclose(fp);
    return;
}

//main関数
//argv[0]=argc,argv[1]=TUinfo.log,argv[2]=input.pgm,argv[3]=output.pgm
int main(int argc, char **argv)
{
    int cux, cuy, bx, by, x, y, w, h;
    char tmp[256];
    IMAGE *simg, *dimg;
    FILE *fp;

    if (argc != 4) {
	printf("usage: %s TUinfo.log input.pgm, output.pgm\n", argv[0]);
        return (0);
    }
    fp = fileopen(argv[1], "rb");
    simg = read_pgm(argv[2]);
    dimg = alloc_image(simg->width, simg->height, 255);
    for (y = 0; y < dimg->height; y++) {
	for (x = 0; x < dimg->width; x++) {
	    dimg->val[y][x] = simg->val[y][x] * 0.8;//可視化した時に見やすくするために0.8倍している
	}
    }
    /*TUinfo.txtからブロックサイズ情報を取得*/
    while (fscanf(fp, "%s%d%d%d%d", tmp, &x, &y, &w, &h) != EOF) {
  //CUなら座標(x,y)をcux,cuyに代入
	if (tmp[0] == 'C') {
	    cux = x;
	    cuy = y;
	} /*TUなら*/else {
	    bx = cux + x;//bx:TUのx座標
	    by = cuy + y;//by:TUのy座標
	    for (x = 0; x < w; x++) {
		dimg->val[by][bx + x] = 255;
	    }
	    if (by + h < dimg->height) {
		for (x = 0; x < w; x++) {
		    dimg->val[by + h][bx + x] = 255;
		}
	    }
	    for (y = 0; y < h; y++) {
		dimg->val[by + y][bx] = 255;
	    }
	    if (bx + w < dimg->width) {
		for (y = 0; y < h; y++) {
		    dimg->val[by + y][bx + w] = 255;
		}
	    }
	}
    }
    write_pgm(dimg, argv[3]);
    return (0);
}
