#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define floatAbs(x) ((x) < 0 ? -(x) : (x))

int width = 480;
int height = 640;

typedef struct {
    float x, y;
    unsigned char r, g, b;
} Vertex;

typedef struct {
    Vertex* vertices;
    int width, height;
    long int size;
} MImage;

void getResolution(MImage img) {
    width = img.width;
    height = img.height;
}


Vertex getPixel(MImage img, int x, int y) {
    Vertex vertex = {x, y, 0, 0, 0};
    int i;
    if (x < 0 || x >= width || y < 0 || y >= height) return vertex;
    for (i = 0; i < img.size; i++) {
        if (img.vertices[i].x == x && img.vertices[i].y == y) {
            vertex = img.vertices[i];
            return vertex;
        }
    }
    return vertex;
}

void setPixel(XImage *img, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    int offset;
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    offset = (y * width + x) * 4;
    img->data[offset + 0] = (char)b;
    img->data[offset + 1] = (char)g;
    img->data[offset + 2] = (char)r;
    img->data[offset + 3] = 0;
}

void drawRectangle(XImage *img, Vertex v0, Vertex v1, unsigned char r, unsigned char g, unsigned char b) {
    bool rx = false;
    bool ry = false;
    int x;
    int y;
    if (v1.x < v0.x)
        rx = true;
    if (v1.y < v0.y)
        ry = true;
    x = v0.x;
    y = v0.y;
    setPixel(img, x, y, r, g, b);
    while (true) {
        y = v0.y;
        while (true) {
            setPixel(img, x, y, r, g, b);
            if (ry) {
                if (y <= v1.y)
                    break;
                y--;
            }
            else {
                if (y >= v1.y)
                    break;
                y++;
            }
        }
        if (rx) {
            if (x <= v1.x)
                break;
            x--;
        }
        else {
            if (x >= v1.x)
                break;
            x++;
        }
    }
}

void renderTexture(XImage *img, MImage texture, Vertex p) {
    int minX, minY, i;
    printf("Rendering texture... Max iterations: %ld\n", texture.size);
    minX = p.x; minY = p.y;
    printf("%d %d\n", texture.width, texture.height);
    printf("%d %d\n", img->width, img->height);
    for (i = 0; i < texture.size; i++) {
        setPixel(img, texture.vertices[i].x + minX, texture.vertices[i].y + minY, texture.vertices[i].r, texture.vertices[i].g, texture.vertices[i].b);
    }
    printf("Texture rendered.\n");
}

MImage loadPngAsVertices(const char* filename, int* out_count) {
    int width, height, total_vertices, y, x, index;
    MImage vertices;
    FILE* fp = fopen(filename, "rb");
    png_structp png;
    png_infop info;
    png_bytep* rowPointers;
    if (!out_count) {
        fprintf(stderr, "Invalid argument: out_count is NULL\n");
        return vertices;
    }
    if (!fp) {
        fprintf(stderr, "Error opening PNG file %s\n", filename);
        return vertices;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Error creating PNG read structure\n");
        fclose(fp);
        return vertices;
    }
    info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Error creating PNG info structure\n");
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return vertices;
    }
    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error during PNG read\n");
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return vertices;
    }
    png_init_io(png, fp);
    png_read_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
    fclose(fp);
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    vertices.width = width;
    vertices.height = height;
    rowPointers = png_get_rows(png, info);
    if (png_get_color_type(png, info) != PNG_COLOR_TYPE_RGBA) {
        fprintf(stderr, "Only RGBA color type is supported\n");
        png_destroy_read_struct(&png, &info, NULL);
        return vertices;
    }
    total_vertices = width * height;
    vertices.vertices = (Vertex*)malloc(total_vertices * sizeof(Vertex));
    vertices.size = total_vertices;
    if (!vertices.vertices) {
        fprintf(stderr, "Memory allocation failed\n");
        png_destroy_read_struct(&png, &info, NULL);
        return vertices;
    }
    for (y = 0, index = 0; y < height; y++) {
        for (x = 0; x < width; x++, index++) {
            png_byte* px = &rowPointers[y][x * 4];
            vertices.vertices[index].x = (float)x;
            vertices.vertices[index].y = (float)y;
            vertices.vertices[index].r = px[0];
            vertices.vertices[index].g = px[1];
            vertices.vertices[index].b = px[2];
        }
    }
    png_destroy_read_struct(&png, &info, NULL);
    *out_count = total_vertices;
    return vertices;
}

void renderX11(char* filename) {
    Display *display;
    Window win;
    GC gc;
    XEvent event;
    XImage *image;
    MImage uzhi;
    int count;
    Atom wmDeleteMessage;
    Vertex v0;
    display = XOpenDisplay(NULL);
    uzhi = loadPngAsVertices(filename, &count);
    getResolution(uzhi);
    if (!display) {
        fprintf(stderr, "Error: X11 unavailable!\n");
        return;
    }
    win = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, width, height, 0, BlackPixel(display, DefaultScreen(display)), WhitePixel(display, DefaultScreen(display)));
    XStoreName(display, win, "XorgRender");
    XSelectInput(display, win, ExposureMask | KeyPressMask);
    XMapWindow(display, win);
    gc = XCreateGC(display, win, 0, NULL);
    wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, win, &wmDeleteMessage, 1);
    image = XCreateImage(display, DefaultVisual(display, 0), DefaultDepth(display, 0), ZPixmap, 0, (char *)malloc(width * height * 4), width, height, 32, 0);
    if (!image) {
        fprintf(stderr, "Error: Cannot create XImage!\n");
        return;
    }
    v0.x = 0;
    v0.y = 0;
    renderTexture(image, uzhi, v0);
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            XPutImage(display, win, gc, image, 0, 0, 0, 0, width, height);
        }
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == wmDeleteMessage) {
            break;
        }
        if (event.type == DestroyNotify) {
            break;
        }
    }
    XDestroyImage(image);
    XFreeGC(display, gc);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
}

int main(int argc, char* argv[]) {
    if (getenv("DISPLAY")) {
        if (argc > 1)
            renderX11(argv[1]);
        else {
            printf("Filepath required.\n");
            renderX11("filerqr.png");
        }
    }
    else {
        printf("X11 is not launching.\n");
    }
    return 0;
}