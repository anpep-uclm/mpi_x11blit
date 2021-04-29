/*
 * mpi_x11blit -- Renders raw RGB data supplied by peers in parallel
 * Copyright (c) 2021 Ángel Pérez <angel@ttm.sh>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <limits.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <X11/Xlib.h>
#include <pshmem.h>
#include <unistd.h>
#endif

#define PROGNAME "mpi_x11blit"

#define BITMAP_WIDTH 400
#define BITMAP_HEIGHT 400
#define BITMAP_BPP 3
#define BITMAP_STRIDE (BITMAP_BPP * BITMAP_WIDTH)

#ifndef min
/* Already defined by <Windows.h> */
#define min(a, b)                                                             \
    __extension__({                                                           \
        __typeof(a) _a = a;                                                   \
        __typeof(b) _b = b;                                                   \
        _a < _b ? _a : _b;                                                    \
    })
#endif

#ifndef RGB
/* Already defined by Windows.h */
#define RGB(r, g, b) (((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff))
#endif

#define MPI_Check(v)                                                          \
    {                                                                         \
        int _v = v;                                                           \
        if (_v != MPI_SUCCESS)                                                \
            handle_error(_v, #v);                                             \
    }
#define MPI_Check_close(f, v)                                                 \
    {                                                                         \
        int _v = v;                                                           \
        if (_v != MPI_SUCCESS) {                                              \
            MPI_File_close(f);                                                \
            handle_error(_v, #v);                                             \
        }                                                                     \
    }
#define _logf(d, f, ...)                                                      \
    {                                                                         \
        fprintf(d, PROGNAME "(%c%d): " f "\n", g_is_renderer ? 'r' : 'w',     \
            g_rank, ##__VA_ARGS__);                                           \
        fflush(d);                                                            \
    }

#define logf(f, ...) _logf(stdout, f, ##__VA_ARGS__)
#define errf(f, ...) _logf(stderr, f, ##__VA_ARGS__)

struct rgb_point {
    uint16_t x, y;
    uint8_t r, g, b;
};

static int g_rank = -1, g_size = -1, g_is_renderer = 0;

/* Generic MPI error handler.
 * This function gets called from within the MPI_Check() macro in case a MPI
 * call does not succeed. Do not attempt to call this handler manually.
 * This function does not return.
 * @mpi_error: MPI status code
 * @expr: Failing expression
 */
static void handle_error(int mpi_error, const char *expr)
{
    char msg_buf[BUFSIZ];
    int msg_len = -1;

    if (MPI_Error_string(mpi_error, msg_buf, &msg_len) != MPI_SUCCESS
        && msg_len > 0) {
        fprintf(stderr, PROGNAME "(%d): MPI error %d (`%s')\n", g_rank,
            mpi_error, expr);
    } else {
        fprintf(stderr, PROGNAME "(%d): MPI error %d (`%s'): %s\n", g_rank,
            mpi_error, expr, msg_buf);
    }

    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    MPI_Finalize();
    _exit(EXIT_FAILURE);
}

/* Waits for incoming data from other peers in the network and renders the
 * received pixels to an X11 window.
 */
static void perform_rendering(MPI_Comm *child_comm)
{
#ifndef _WIN32
    /* Create window. */
    const char *display_name = getenv("DISPLAY");
    Display *display = XOpenDisplay(display_name);
    if (!display) {
        errf("could not open display: %s", display_name);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
        _exit(EXIT_FAILURE);
    }

    int screen_num = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0,
        0, BITMAP_WIDTH, BITMAP_HEIGHT, 0, BlackPixel(display, screen_num),
        BlackPixel(display, screen_num));
    XSelectInput(display, window, StructureNotifyMask);
    XMapWindow(display, window);
    XFlush(display);

    GC ctx = XCreateGC(display, window, 0, NULL);
    if (ctx < 0) {
        errf("could not create GC");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
        _exit(EXIT_FAILURE);
    }

    /* Wait for window to be created. */
    XEvent event;
    do {
        XNextEvent(display, &event);
    } while (event.type != MapNotify);

    /* Receive RGB triplets. */
    struct rgb_point point;
    for (size_t i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i++) {
        MPI_Recv(&point, sizeof(point), MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE,
            MPI_ANY_TAG, *child_comm, MPI_STATUS_IGNORE);
        XSetForeground(display, ctx, RGB(point.r, point.g, point.b));
        XDrawPoint(display, window, ctx, point.x, point.y);
        XFlush(display);
    }

    sleep(5);
    XCloseDisplay(display);
#else
    HINSTANCE hInstance = GetModuleHandle(NULL);

    /* Register window class. */
    LPCSTR szClassName = PROGNAME;
    WNDCLASSEX WndClassEx;
    WndClassEx.cbSize = sizeof(WNDCLASSEX);
    WndClassEx.style = 0;
    WndClassEx.lpfnWndProc = DefWindowProc;
    WndClassEx.cbClsExtra = 0;
    WndClassEx.cbWndExtra = 0;
    WndClassEx.hInstance = hInstance;
    WndClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    WndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
    WndClassEx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    WndClassEx.lpszMenuName = NULL;
    WndClassEx.lpszClassName = szClassName;
    WndClassEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&WndClassEx)) {
        errf("window registration failed (%08x)", GetLastError());
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
        _exit(EXIT_FAILURE);
        return 0;
    }

    /* Create window. */
    HWND hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, szClassName,
        PROGNAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, BITMAP_WIDTH, BITMAP_HEIGHT, NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        errf("window creation failed (%08x)", GetLastError());
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
        _exit(EXIT_FAILURE);
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    /* Draw bitmap. */
    HDC hDC = GetDC(hWnd);

    for (size_t i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i++) {
        /* Receive RGB triplets. */
        struct rgb_point point;
        MPI_Recv(&point, sizeof(point), MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE,
            MPI_ANY_TAG, *child_comm, MPI_STATUS_IGNORE);
        SetPixel(hDC, point.x, point.y, RGB(point.r, point.g, point.b));
    }

    ReleaseDC(hWnd, hDC);
    DeleteDC(hDC);

    /* Window event loop. */
    MSG Msg;
    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }    
#endif
}

/* Reads raw RGB data from the supplied input file and sends them out so the
 * renderer process can blit those pixels.
 * @input_path: Path to the file containing the data
 */
static void read_data(const char *input_path)
{
    /* Open input file. */
    MPI_File input_file;
    logf("opening file `%s' for reading", input_path);
    MPI_Check(MPI_File_open(MPI_COMM_WORLD, input_path, MPI_MODE_RDONLY,
        MPI_INFO_NULL, &input_file));

    /* Calculate chunk length for each peer. */ 
    MPI_Offset input_len;
    MPI_Check_close(&input_file, MPI_File_get_size(input_file, &input_len));
    if (input_len % BITMAP_STRIDE) {
        errf("invalid input length. Expected a multiple of %d but got %lld.",
            BITMAP_STRIDE, input_len);
        MPI_File_close(&input_file);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        MPI_Finalize();
        _exit(EXIT_FAILURE);
    }

    MPI_Offset chunk_len = input_len / g_size,
               chunk_start = chunk_len * g_rank,
               chunk_end = min(input_len, chunk_start + chunk_len) - 1;
    chunk_len = chunk_end - chunk_start + 1;
    logf("%lld bytes: [%lld, %lld]", chunk_len, chunk_start, chunk_end);

    /* Allocate buffer for reading chunk. */
    uint8_t *buf = malloc(chunk_len);

    /* Read from file. */
    MPI_Check(MPI_File_read_at_all(input_file, chunk_start, buf, chunk_len,
        MPI_UNSIGNED_CHAR, MPI_STATUS_IGNORE));

    /* Send to main process. */
    MPI_Comm parent_comm;
    MPI_Comm_get_parent(&parent_comm);

    size_t strides = chunk_len / BITMAP_BPP;
    struct rgb_point point;

    for (size_t off = 0; off < strides; off++) {
        uint8_t *triplet = buf + (off * BITMAP_BPP);
        point.x = (chunk_start / 3 + off) % BITMAP_WIDTH;
        point.y = (chunk_start / 3 + off) / BITMAP_WIDTH;
        point.r = triplet[0];
        point.g = triplet[1];
        point.b = triplet[2];
        MPI_Check(MPI_Send(
            &point, sizeof(point), MPI_UNSIGNED_CHAR, 0, 0, parent_comm));
    }

    MPI_Check(MPI_File_close(&input_file));
}

/* Parse the number of workers from the command line arguments.
 * Returns -1 on failure, parsed value on success
 * @str: String to be parsed
 */
static int parse_num_workers(char *str)
{
    errno = 0;
    char *endptr;
    long result = strtol(str, &endptr, 10);

    if (endptr == str)
        return -1; /* nothing parsed */

    if ((result == INT_MAX || result == INT_MIN) && errno == ERANGE)
        return -1; /* out of range */
    return (int)result;
}

/* Program entry point.
 * This function returns EXIT_SUCCESS or EXIT_FAILURE if an error occurred
 * during MPI initialization.
 * @argc: Number of arguments
 * @argv: Arguments passed to the program, excluding those of mpicc
 */
int main(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: " PROGNAME " NUM_WORKERS INPUT_FILE\n\n");
        return EXIT_SUCCESS;
    }

    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fprintf(stderr, PROGNAME ": error: MPI initialization failed\n");
        return EXIT_FAILURE;
    }

    MPI_Check(MPI_Comm_rank(MPI_COMM_WORLD, &g_rank));
    MPI_Check(MPI_Comm_size(MPI_COMM_WORLD, &g_size));

    MPI_Comm parent_comm;
    MPI_Check(MPI_Comm_get_parent(&parent_comm));

    if (parent_comm == MPI_COMM_NULL && g_rank == 0) {
        /* I'm the renderer process. */
        g_is_renderer = 1;

        /* Spawn as many worker processes as needed. */
        int num_workers = parse_num_workers(argv[1]);
        if (num_workers < 1) {
            fprintf(stderr,
                PROGNAME "(%d): error: invalid number of workers (%d)\n",
                g_rank, num_workers);
            MPI_Check(MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE));
            MPI_Check(MPI_Finalize());
            return EXIT_FAILURE;
        }

        MPI_Comm child_comm;
        char *children_argv[] = { argv[1], argv[2], NULL };
        MPI_Check(MPI_Comm_spawn(argv[0], children_argv, num_workers,
            MPI_INFO_NULL, 0, MPI_COMM_WORLD, &child_comm, MPI_ERRCODES_IGNORE));

        /* Perform rendering. */
        perform_rendering(&child_comm);
    } else {
        /* Perform parallel read. */
        read_data(argv[2]);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
