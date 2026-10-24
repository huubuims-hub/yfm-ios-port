/* The system's file sheets (ios_files.h). */
#include "ios_files.h"
#include <stdio.h>
#include <string.h>

typedef int memories_ios_files_translation_unit_is_not_empty;

static IosFilesState state_now = IOS_FILES_IDLE;
static char picked_path[1100];
static char error_text[300];

IosFilesState IosFiles_State(void) { return state_now; }
const char *IosFiles_Path(void) { return picked_path; }
const char *IosFiles_Error(void) { return error_text; }

void IosFiles_Done(void)
{
    if (state_now != IOS_FILES_BUSY) state_now = IOS_FILES_IDLE;
}

#ifdef MEMORIES_IOS
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
    id type, types, allocated, picker = NULL;
    Class picker_class = objc_getClass("UIDocumentPickerViewController");
    Class type_class = objc_getClass("UTType");
    if (begin()) return -1;
    /* Any file: a backup's name ends in .yfmbak, which the system has no type for. */
    type = picker_class && type_class
               ? ((id(*)(Class, SEL, id))objc_msgSend)(type_class, SEL_("typeWithIdentifier:"), make_string("public.data"))
               : NULL;
    types = type ? ((id(*)(Class, SEL, id))objc_msgSend)(objc_getClass("NSArray"), SEL_("arrayWithObject:"), type) : NULL;
    allocated = types ? ((id(*)(Class, SEL))objc_msgSend)(picker_class, SEL_("alloc")) : NULL;
    /* asCopy: the system copies the chosen file into the app, whatever service it is on. */
    if (allocated)
        picker = ((id(*)(id, SEL, id, BOOL))objc_msgSend)(allocated, SEL_("initForOpeningContentTypes:asCopy:"), types,
                                                         YES);
    return present(picker);
}
#else
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
