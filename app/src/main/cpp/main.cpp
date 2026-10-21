#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "renderer.h"
#include <cmath>

#define LOG_TAG "BLENDORA3D"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x0040
#endif

static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;
static Renderer renderer;
static bool rendererReady = false;
static float rotationXAngle = 0.45f;
static float rotationYAngle = 0.65f;
static float lastX = 0.0f;
static float lastY = 0.0f;
static bool dragging = false;
static float moveX = 0.0f;
static float moveY = 0.0f;
static float objectScale = 1.0f;
static float lastPinchDistance = 0.0f;
static float lastCenterX = 0.0f;
static float lastCenterY = 0.0f;
static int width = 1;
static int height = 1;

static void destroyEGL() {
    if (display != EGL_NO_DISPLAY) {
        if (rendererReady) {
            renderer.shutdown();
            rendererReady = false;
        }
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    surface = EGL_NO_SURFACE;
    context = EGL_NO_CONTEXT;
}

static bool initEGL(ANativeWindow* window) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY ||
        !eglInitialize(display, nullptr, nullptr)) {
        LOGE("eglInitialize failed");
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) ||
        numConfigs < 1) {
        LOGE("eglChooseConfig failed");
        destroyEGL();
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    surface = eglCreateWindowSurface(display, config, window, nullptr);

    if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(display, surface, surface, context)) {
        LOGE("EGL context or surface creation failed");
        destroyEGL();
        return false;
    }

    EGLint w = 1, h = 1;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    width = w > 0 ? w : 1;
    height = h > 0 ? h : 1;
    renderer.resize(width, height);
    rendererReady = renderer.init();
    if (!rendererReady) LOGE("Renderer initialization failed");
    return rendererReady;
}

static void handleCommand(android_app* app, int32_t command) {
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window && display == EGL_NO_DISPLAY) {
                initEGL(app->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            destroyEGL();
            break;
        case APP_CMD_WINDOW_RESIZED:
            if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
                EGLint w = 1, h = 1;
                eglQuerySurface(display, surface, EGL_WIDTH, &w);
                eglQuerySurface(display, surface, EGL_HEIGHT, &h);
                width = w > 0 ? w : 1;
                height = h > 0 ? h : 1;
                renderer.resize(width, height);
            }
            break;
    }
}


static int32_t handleInput(android_app*, AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    size_t count = AMotionEvent_getPointerCount(event);
    if (count == 0) return 0;

    float x = AMotionEvent_getX(event, 0);
    float y = AMotionEvent_getY(event, 0);

    if (action == AMOTION_EVENT_ACTION_DOWN) {
        lastX = x;
        lastY = y;
        dragging = true;
        lastPinchDistance = 0.0f;
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_POINTER_DOWN && count >= 2) {
        float x1 = AMotionEvent_getX(event, 1);
        float y1 = AMotionEvent_getY(event, 1);
        float dx = x1 - x;
        float dy = y1 - y;
        lastPinchDistance = std::sqrt(dx * dx + dy * dy);
        lastCenterX = (x + x1) * 0.5f;
        lastCenterY = (y + y1) * 0.5f;
        dragging = false;
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_MOVE && count >= 2) {
        float x1 = AMotionEvent_getX(event, 1);
        float y1 = AMotionEvent_getY(event, 1);
        float dx = x1 - x;
        float dy = y1 - y;
        float distance = std::sqrt(dx * dx + dy * dy);
        float centerX = (x + x1) * 0.5f;
        float centerY = (y + y1) * 0.5f;

        if (lastPinchDistance > 1.0f && distance > 1.0f) {
            objectScale *= distance / lastPinchDistance;
            if (objectScale < 0.25f) objectScale = 0.25f;
            if (objectScale > 3.0f) objectScale = 3.0f;
        }

        if (height > 0) {
            moveX += (centerX - lastCenterX) * 4.0f / height;
            moveY -= (centerY - lastCenterY) * 4.0f / height;
        }

        lastPinchDistance = distance;
        lastCenterX = centerX;
        lastCenterY = centerY;
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_MOVE && dragging) {
        rotationYAngle += (x - lastX) * 0.01f;
        rotationXAngle += (y - lastY) * 0.01f;
        lastX = x;
        lastY = y;
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_POINTER_UP) {
        dragging = false;
        lastPinchDistance = 0.0f;
        return 1;
    }

    if (action == AMOTION_EVENT_ACTION_UP ||
        action == AMOTION_EVENT_ACTION_CANCEL) {
        dragging = false;
        lastPinchDistance = 0.0f;
        return 1;
    }

    return 1;
}

void android_main(android_app* app) {
    app_dummy();
    app->onAppCmd = handleCommand;
    app->onInputEvent = handleInput;

    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        int ident = ALooper_pollOnce(
            display == EGL_NO_DISPLAY ? -1 : 16,
            nullptr, &events, reinterpret_cast<void**>(&source)
        );

        if (ident == ALOOPER_POLL_ERROR) break;
        if (source) source->process(app, source);
        if (app->destroyRequested) {
            destroyEGL();
            return;
        }

        if (rendererReady && display != EGL_NO_DISPLAY &&
            surface != EGL_NO_SURFACE) {
            renderer.draw(rotationXAngle, rotationYAngle, moveX, moveY, objectScale);
            eglSwapBuffers(display, surface);
        }
    }
}
