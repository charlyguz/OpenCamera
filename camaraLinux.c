#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <X11/Xlib.h>       
#include <X11/Xutil.h>   


#define WIDTH 640
#define HEIGHT 480
#define BUFFER_COUNT 4

struct buffer {
    void *start;
    size_t length;
};


void yuyv_to_rgbx(unsigned char *yuyv, unsigned char *rgbx, int width, int height, int yuyv_stride) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int i = y * yuyv_stride + x * 2;
            
            
            int y0 = yuyv[i];      
            int u = yuyv[i + 1];   
            int y1 = yuyv[i + 2];  
            int v = yuyv[i + 3];   

            // Conversión para el primer píxel
            int r = y0 + 1.402 * (v - 128);
            int g = y0 - 0.34414 * (u - 128) - 0.71414 * (v - 128);
            int b = y0 + 1.772 * (u - 128);
            
            // Ajuste de valores
            rgbx[(y * width + x) * 4] = (b < 0) ? 0 : (b > 255) ? 255 : b;      // B
            rgbx[(y * width + x) * 4 + 1] = (g < 0) ? 0 : (g > 255) ? 255 : g;  // G
            rgbx[(y * width + x) * 4 + 2] = (r < 0) ? 0 : (r > 255) ? 255 : r;  // R
            rgbx[(y * width + x) * 4 + 3] = 0;                                  // A

            // Conversión para el segundo píxel (mismos U y V)
            r = y1 + 1.402 * (v - 128);
            g = y1 - 0.34414 * (u - 128) - 0.71414 * (v - 128);
            b = y1 + 1.772 * (u - 128);
            
            rgbx[(y * width + x + 1) * 4] = (b < 0) ? 0 : (b > 255) ? 255 : b;      // B
            rgbx[(y * width + x + 1) * 4 + 1] = (g < 0) ? 0 : (g > 255) ? 255 : g;  // G
            rgbx[(y * width + x + 1) * 4 + 2] = (r < 0) ? 0 : (r > 255) ? 255 : r;  // R
            rgbx[(y * width + x + 1) * 4 + 3] = 0;                                  // A
        }
    }
}

int main() {
    const char *device = "/dev/video0";
    int fd = open(device, O_RDWR);
    
    if (fd == -1) {
        perror("Error al abrir el dispositivo");
        return 1;
    }

    // Configurar formato
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Error al configurar formato");
        close(fd);
        return 1;
    }

    // Configurar buffers
    struct v4l2_requestbuffers req = {0};
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = BUFFER_COUNT;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Error en VIDIOC_REQBUFS");
        close(fd);
        return 1;
    }

    struct buffer *buffers = calloc(req.count, sizeof(*buffers));
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("Error en VIDIOC_QUERYBUF");
            close(fd);
            return 1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
                                MAP_SHARED, fd, buf.m.offset);

        if (buffers[i].start == MAP_FAILED) {
            perror("Error en mmap");
            close(fd);
            return 1;
        }

        ioctl(fd, VIDIOC_QBUF, &buf);
    }

    // Iniciar captura
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("Error en VIDIOC_STREAMON");
        close(fd);
        return 1;
    }

    // Configurar ventana X11
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error al abrir el display de X11\n");
        close(fd);
        return 1;
    }

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        0, 0, WIDTH, HEIGHT, 0,
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );

    XMapWindow(display, window);
    XFlush(display);

    // Crear el Graphics Context (GC)
    GC gc = XCreateGC(display, window, 0, NULL); 

    XWindowAttributes window_attrs;
    XGetWindowAttributes(display, window, &window_attrs);
    
    printf("Formato visual: R=%08lx, G=%08lx, B=%08lx\n",
           window_attrs.visual->red_mask,
           window_attrs.visual->green_mask,
           window_attrs.visual->blue_mask);


    XImage *ximage = XCreateImage(
        display,
        window_attrs.visual,
        window_attrs.depth,
        ZPixmap,
        0,
        (char *)malloc(WIDTH * HEIGHT * 4),
        WIDTH,
        HEIGHT,
        32,  // bitmap_pad para 4 bytes por píxel (RGBX)
        WIDTH * 4  // bytes_per_line (ancho * 4 bytes)
    );


    while (1) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Error en VIDIOC_DQBUF");
            break;
        }

        // 1. Convertir YUYV a RGB
        yuyv_to_rgbx(buffers[buf.index].start, (unsigned char *)ximage->data, WIDTH, HEIGHT, fmt.fmt.pix.bytesperline);

        // 2. Mostrar la imagen en la ventana
        XPutImage(display, window, gc, ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
        XFlush(display);

        // 3. Re-enviar el buffer
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Error en VIDIOC_QBUF");
            break;
        }
    }




        
    
    XDestroyImage(ximage);
    XFreeGC(display, gc);  // Liberar el Graphics Context
    XCloseDisplay(display);
    close(fd);
    return 0;
}