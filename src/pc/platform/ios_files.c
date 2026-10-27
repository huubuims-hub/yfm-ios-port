/* The system's file sheets (ios_files.h). */
#include "ios_files.h"
#include <stdio.h>
#include <string.h>

typedef int memories_ios_files_translation_unit_is_not_empty;

static IosFilesState state_now = IOS_FILES_IDLE;
static char picked_path[1100];
static char error_text[300];

const char *IosFiles_Path(void) { return picked_path; }
const char *IosFiles_Error(void) { return error_text; }

void IosFiles_Done(void)
{
    if (state_now != IOS_FILES_BUSY) state_now = IOS_FILES_IDLE;
}

#ifdef MEMORIES_IOS

IosFilesState IosFiles_State(void) { return state_now; }
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>

void *Platform_UIKitWindow(void); /* sdl.c: the game's UIWindow */

static id delegate_instance; /* kept for good: a picker holds its delegate weakly */

#define SEL_(name) sel_registerName(name)

static id class_msg(const char *class_name, const char *selector)
{
    Class cls = objc_getClass(class_name);
    if (!cls) return NULL;
    return ((id(*)(Class, SEL))objc_msgSend)(cls, SEL_(selector));
}

static id make_string(const char *text)
{
    Class cls = objc_getClass("NSString");
    if (!cls) return NULL;
    return ((id(*)(Class, SEL, const char *))objc_msgSend)(cls, SEL_("stringWithUTF8String:"), text);
}

static void fail(const char *text)
{
    snprintf(error_text, sizeof(error_text), "%s", text);
    state_now = IOS_FILES_FAILED;
}

/* The sheet's answers come from the system, on the main thread. */
static void did_pick(id self, SEL command, id controller, id urls)
{
    id url = urls ? ((id(*)(id, SEL))objc_msgSend)(urls, SEL_("firstObject")) : NULL;
    id path = url ? ((id(*)(id, SEL))objc_msgSend)(url, SEL_("path")) : NULL;
    const char *chars = path ? ((const char *(*)(id, SEL))objc_msgSend)(path, SEL_("UTF8String")) : NULL;
    (void)self;
    (void)command;
    (void)controller;
    if (!chars) {
        fail("The system gave no file.");
    } else if (snprintf(picked_path, sizeof(picked_path), "%s", chars) >= (int)sizeof(picked_path)) {
        fail("The file's path is too long.");
    } else {
        state_now = IOS_FILES_PICKED;
    }
}

static void was_cancelled(id self, SEL command, id controller)
{
    (void)self;
    (void)command;
    (void)controller;
    state_now = IOS_FILES_CANCELLED;
}

static int make_delegate(void)
{
    Class delegate_class;
    id allocated;
    Protocol *protocol;
    if (delegate_instance) return 0;
    delegate_class = objc_allocateClassPair(objc_getClass("NSObject"), "YfmFilePickerDelegate", 0);
    if (!delegate_class) return -1;
    class_addMethod(delegate_class, SEL_("documentPicker:didPickDocumentsAtURLs:"), (IMP)did_pick, "v@:@@");
    class_addMethod(delegate_class, SEL_("documentPickerWasCancelled:"), (IMP)was_cancelled, "v@:@");
    protocol = objc_getProtocol("UIDocumentPickerDelegate");
    if (protocol) class_addProtocol(delegate_class, protocol);
    objc_registerClassPair(delegate_class);
    allocated = ((id(*)(Class, SEL))objc_msgSend)(delegate_class, SEL_("alloc"));
    delegate_instance = allocated ? ((id(*)(id, SEL))objc_msgSend)(allocated, SEL_("init")) : NULL;
    return delegate_instance ? 0 : -1;
}

/* The view controller on top: the window's root, then whatever it shows. */
static id top_controller(void)
{
    id window = (id)Platform_UIKitWindow(), controller;
    if (!window) {
        id application = class_msg("UIApplication", "sharedApplication");
        if (application) window = ((id(*)(id, SEL))objc_msgSend)(application, SEL_("keyWindow"));
    }
    if (!window) return NULL;
    controller = ((id(*)(id, SEL))objc_msgSend)(window, SEL_("rootViewController"));
    while (controller) {
        id next = ((id(*)(id, SEL))objc_msgSend)(controller, SEL_("presentedViewController"));
        if (!next) break;
        controller = next;
    }
    return controller;
}

/* Common start: checks, then the delegate. -1 with the error set. */
static int begin(void)
{
    error_text[0] = '\0';
    if (state_now == IOS_FILES_BUSY) {
        snprintf(error_text, sizeof(error_text), "A file sheet is already open.");
        return -1;
    }
    if (!pthread_main_np()) {
        snprintf(error_text, sizeof(error_text), "The file sheet can only be opened from the main thread.");
        return -1;
    }
    if (make_delegate()) {
        snprintf(error_text, sizeof(error_text), "Could not set up the file sheet.");
        return -1;
    }
    picked_path[0] = '\0';
    return 0;
}

static int present(id picker)
{
    id top = top_controller();
    if (!picker) {
        snprintf(error_text, sizeof(error_text), "Could not open the file sheet.");
        return -1;
    }
    if (!top) {
        snprintf(error_text, sizeof(error_text), "There is no window to show the file sheet on.");
        return -1;
    }
    ((void (*)(id, SEL, id))objc_msgSend)(picker, SEL_("setDelegate:"), delegate_instance);
    state_now = IOS_FILES_BUSY;
    ((void (*)(id, SEL, id, BOOL, id))objc_msgSend)(top, SEL_("presentViewController:animated:completion:"), picker, YES,
                                                      NULL);
    return 0;
}

int IosFiles_Save(const char *path)
{
    id string, url, array, allocated, picker = NULL;
    Class picker_class = objc_getClass("UIDocumentPickerViewController");
    if (begin()) return -1;
    string = picker_class && path ? make_string(path) : NULL;
    url = string ? ((id(*)(Class, SEL, id))objc_msgSend)(objc_getClass("NSURL"), SEL_("fileURLWithPath:"), string) : NULL;
    array = url ? ((id(*)(Class, SEL, id))objc_msgSend)(objc_getClass("NSArray"), SEL_("arrayWithObject:"), url) : NULL;
    allocated = array ? ((id(*)(Class, SEL))objc_msgSend)(picker_class, SEL_("alloc")) : NULL;
    /* asCopy: the sheet saves a copy and the file in the app stays. */
    if (allocated)
        picker = ((id(*)(id, SEL, id, BOOL))objc_msgSend)(allocated, SEL_("initForExportingURLs:asCopy:"), array, YES);
    return present(picker);
}

int IosFiles_Open(void)
{
    id types, allocated, picker = NULL, list[2];
    Class picker_class = objc_getClass("UIDocumentPickerViewController");
    Class type_class = objc_getClass("UTType");
    int n = 0;
    if (begin()) return -1;
    /* Any file. A backup's name ends in .yfmbak, which the system has no type for, and
     * a service (iCloud Drive) may describe such a file as a plain item rather than as
     * data, which would show it greyed out: both are accepted. */
    if (picker_class && type_class) {
        static const char *const names[] = {"public.item", "public.data"};
        int i;
        for (i = 0; i < 2; i++) {
            id type = ((id(*)(Class, SEL, id))objc_msgSend)(type_class, SEL_("typeWithIdentifier:"), make_string(names[i]));
            if (type) list[n++] = type;
        }
    }
    types = n ? ((id(*)(Class, SEL, const id *, unsigned long))objc_msgSend)(objc_getClass("NSArray"),
                                                                           SEL_("arrayWithObjects:count:"), list,
                                                                           (unsigned long)n)
              : NULL;
    allocated = types ? ((id(*)(Class, SEL))objc_msgSend)(picker_class, SEL_("alloc")) : NULL;
    /* asCopy: the system copies the chosen file into the app, whatever service it is on. */
    if (allocated)
        picker = ((id(*)(id, SEL, id, BOOL))objc_msgSend)(allocated, SEL_("initForOpeningContentTypes:asCopy:"), types,
                                                         YES);
    return present(picker);
}
#elif !defined(_WIN32)
/* Android, Linux and macOS: SDL's own file dialogs (the system's document
 * picker on Android, so Google Drive and any other provider are in it). The
 * sheet answers with a path, or on Android a content:// URI that only SDL's
 * file functions open; the copy between it and the app is made here, once a
 * frame, when the menu asks for the state. */
#include <SDL3/SDL.h>
#include "jni_guard.h" /* last: after SDL's own headers */
#include "paths.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#define COPY_LIMIT ((Sint64)80 << 20)

static SDL_AtomicInt dialog_done;
static int dialog_choice;     /* 1 chosen, 0 cancelled, -1 failed: written before dialog_done */
static char dialog_uri[2048];
static char dialog_error[300];
static int dialog_save;       /* the sheet up is a save sheet */
static char save_source[1100];

static void fail(const char *text)
{
    snprintf(error_text, sizeof(error_text), "%s", text && *text ? text : "The file dialog failed.");
    state_now = IOS_FILES_FAILED;
}

/* On the dialog's own thread (Android's Java thread): the answer, published last. */
static void SDLCALL chosen(void *userdata, const char *const *files, int filter)
{
    (void)userdata;
    (void)filter;
    dialog_error[0] = '\0';
    if (!files) {
        snprintf(dialog_error, sizeof(dialog_error), "%s", SDL_GetError());
        dialog_choice = -1;
    } else if (!files[0]) {
        dialog_choice = 0;
    } else if (strlen(files[0]) >= sizeof(dialog_uri)) {
        snprintf(dialog_error, sizeof(dialog_error), "The chosen path is too long.");
        dialog_choice = -1;
    } else {
        snprintf(dialog_uri, sizeof(dialog_uri), "%s", files[0]);
        dialog_choice = 1;
    }
    SDL_SetAtomicInt(&dialog_done, 1);
}

static int begin(void)
{
    error_text[0] = '\0';
    if (state_now == IOS_FILES_BUSY) {
        snprintf(error_text, sizeof(error_text), "A file dialog is already open.");
        return -1;
    }
    SDL_SetAtomicInt(&dialog_done, 0);
    dialog_choice = 0;
    picked_path[0] = '\0';
    return 0;
}

int IosFiles_Save(const char *path)
{
    static const SDL_DialogFileFilter filters[] = {{"YFM backup (.yfmbak)", "yfmbak"}};
    const char *name = path ? strrchr(path, '/') : NULL;
    if (begin()) return -1;
    if (!path || snprintf(save_source, sizeof(save_source), "%s", path) >= (int)sizeof(save_source)) {
        snprintf(error_text, sizeof(error_text), "The backup's path is too long.");
        return -1;
    }
    dialog_save = 1;
    state_now = IOS_FILES_BUSY;
    SDL_ShowSaveFileDialog(chosen, NULL, NULL, filters, 1, name ? name + 1 : path);
    return 0;
}

int IosFiles_Open(void)
{
    /* Any document, as the disc and the mods are picked: a .yfmbak has no type of its own. */
    static const SDL_DialogFileFilter filters[] = {{"YFM backup", "*"}};
    if (begin()) return -1;
    dialog_save = 0;
    state_now = IOS_FILES_BUSY;
    SDL_ShowOpenFileDialog(chosen, NULL, NULL, filters, 1, NULL, false);
    return 0;
}

/* The backup in the app, written to what the player chose. */
static int copy_out(void)
{
    static unsigned char buffer[1 << 16];
    FILE *in = fopen(save_source, "rb");
    SDL_IOStream *out;
    size_t got;
    int ok = 1;
    if (!in) {
        snprintf(error_text, sizeof(error_text), "The backup could not be read: %s.", strerror(errno));
        return 0;
    }
    out = SDL_IOFromFile(dialog_uri, "wb");
    if (!out) {
        snprintf(error_text, sizeof(error_text), "Could not write there: %s", SDL_GetError());
        fclose(in);
        return 0;
    }
    while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (SDL_WriteIO(out, buffer, got) != got) {
            snprintf(error_text, sizeof(error_text), "Could not write there: %s", SDL_GetError());
            ok = 0;
            break;
        }
    }
    fclose(in);
    if (!SDL_CloseIO(out) && ok) {
        snprintf(error_text, sizeof(error_text), "Could not finish writing there: %s", SDL_GetError());
        ok = 0;
    }
    return ok;
}

/* What the player chose, copied to a file of ours (the right to read it may not last, and a
 * cloud provider's stream may not seek). */
static int copy_in(void)
{
    static unsigned char buffer[1 << 16];
    char folder[1100];
    SDL_IOStream *in;
    FILE *out;
    size_t got;
    Sint64 total = 0;
    int ok = 1;
    if (snprintf(folder, sizeof(folder), "%s/imports", Paths_UserDir()) >= (int)sizeof(folder) ||
        (mkdir(folder, 0755) && errno != EEXIST) ||
        snprintf(picked_path, sizeof(picked_path), "%s/picked.yfmbak", folder) >= (int)sizeof(picked_path)) {
        snprintf(error_text, sizeof(error_text), "Could not make a place for the file.");
        return 0;
    }
    in = SDL_IOFromFile(dialog_uri, "rb");
    if (!in) {
        snprintf(error_text, sizeof(error_text), "Could not read that file: %s", SDL_GetError());
        return 0;
    }
    out = fopen(picked_path, "wb");
    if (!out) {
        snprintf(error_text, sizeof(error_text), "Could not write in the app's folder: %s.", strerror(errno));
        SDL_CloseIO(in);
        return 0;
    }
    while ((got = SDL_ReadIO(in, buffer, sizeof(buffer))) > 0) {
        total += (Sint64)got;
        if (total > COPY_LIMIT) {
            snprintf(error_text, sizeof(error_text), "That file is larger than any backup this game makes.");
            ok = 0;
            break;
        }
        if (fwrite(buffer, 1, got, out) != got) {
            snprintf(error_text, sizeof(error_text), "Could not copy the file: %s.", strerror(errno));
            ok = 0;
            break;
        }
    }
    if (ok && SDL_GetIOStatus(in) == SDL_IO_STATUS_ERROR) {
        snprintf(error_text, sizeof(error_text), "Could not read that file: %s", SDL_GetError());
        ok = 0;
    }
    SDL_CloseIO(in);
    if (fclose(out) && ok) {
        snprintf(error_text, sizeof(error_text), "Could not copy the file: %s.", strerror(errno));
        ok = 0;
    }
    if (!ok) remove(picked_path);
    return ok;
}

IosFilesState IosFiles_State(void)
{
    if (state_now != IOS_FILES_BUSY || !SDL_GetAtomicInt(&dialog_done)) return state_now;
    if (dialog_choice < 0) {
        fail(dialog_error);
    } else if (dialog_choice == 0) {
        state_now = IOS_FILES_CANCELLED;
    } else if (dialog_save) {
        if (copy_out()) {
            snprintf(picked_path, sizeof(picked_path), "%s", dialog_uri);
            state_now = IOS_FILES_PICKED;
        } else {
            fail(error_text);
        }
    } else if (copy_in()) {
        state_now = IOS_FILES_PICKED;
    } else {
        fail(error_text);
    }
    return state_now;
}
#else
IosFilesState IosFiles_State(void) { return state_now; }
int IosFiles_Save(const char *path)
{
    (void)path;
    snprintf(error_text, sizeof(error_text), "Not available on this system.");
    return -1;
}
int IosFiles_Open(void)
{
    snprintf(error_text, sizeof(error_text), "Not available on this system.");
    return -1;
}
#endif
