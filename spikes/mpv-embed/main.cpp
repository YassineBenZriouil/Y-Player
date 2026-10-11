// Phase 0 spike: compare libmpv embedding paths on this machine.
//   wid : native child window, mpv owns presentation (vo=gpu-next, gpu-api=d3d11)
//   gl  : QOpenGLWidget + libmpv render API (vo=libmpv, OpenGL)
// Usage: mpv-embed-spike <wid|gl> <file-or-url> [seconds] [hwdec]
// Prints one JSON line with measurements, then exits. Throwaway code — not product code.

#include <QApplication>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QTimer>
#include <QWidget>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <functional>
#include <string>

static double cpuSeconds() {
    FILETIME c, e, k, u;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    auto toS = [](FILETIME f) { return (double(f.dwHighDateTime) * 4294967296.0 + f.dwLowDateTime) / 1e7; };
    return toS(k) + toS(u);
}

static std::string getStr(mpv_handle* h, const char* name) {
    char* s = mpv_get_property_string(h, name);
    std::string r = s ? s : "";
    mpv_free(s);
    return r;
}

static int64_t getInt(mpv_handle* h, const char* name) {
    int64_t v = -1;
    mpv_get_property(h, name, MPV_FORMAT_INT64, &v);
    return v;
}

class GlView : public QOpenGLWidget {
public:
    explicit GlView(mpv_handle* h) : mpv_(h) {}
    ~GlView() override {
        makeCurrent();
        if (ctx_) mpv_render_context_free(ctx_);
        doneCurrent();
    }

protected:
    void initializeGL() override {
        mpv_opengl_init_params gl{
            [](void*, const char* name) -> void* {
                return reinterpret_cast<void*>(QOpenGLContext::currentContext()->getProcAddress(name));
            },
            nullptr};
        int advanced = 1;
        mpv_render_param params[]{{MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
                                  {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
                                  {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
        if (mpv_render_context_create(&ctx_, mpv_, params) < 0) {
            std::fprintf(stderr, "render context create failed\n");
            std::exit(2);
        }
        mpv_render_context_set_update_callback(
            ctx_, [](void* self) { QMetaObject::invokeMethod(static_cast<GlView*>(self), [s = static_cast<GlView*>(self)] { s->update(); }, Qt::QueuedConnection); },
            this);
    }
    void paintGL() override {
        const qreal dpr = devicePixelRatioF();
        mpv_opengl_fbo fbo{int(defaultFramebufferObject()), int(width() * dpr), int(height() * dpr), 0};
        int flip = 1;
        mpv_render_param params[]{{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                                  {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(ctx_, params);
    }

private:
    mpv_handle* mpv_;
    mpv_render_context* ctx_ = nullptr;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <wid|gl> <file> [seconds] [hwdec]\n", argv[0]);
        return 1;
    }
    const std::string mode = argv[1];
    const char* file = argv[2];
    const int seconds = argc > 3 ? std::atoi(argv[3]) : 20;
    const char* hwdec = argc > 4 ? argv[4] : "auto-safe";

    mpv_handle* mpv = mpv_create();
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "hwdec", hwdec);
    mpv_set_option_string(mpv, "keep-open", "yes");
    mpv_set_option_string(mpv, "loop-file", "inf");

    QWidget* view = nullptr;
    if (mode == "wid") {
        view = new QWidget;
        view->setAttribute(Qt::WA_NativeWindow);
        view->setAttribute(Qt::WA_PaintOnScreen);  // Qt must not paint over mpv's swapchain
        view->setAttribute(Qt::WA_NoSystemBackground);
        mpv_set_option_string(mpv, "vo", "gpu-next");
        mpv_set_option_string(mpv, "gpu-api", "d3d11");
        int64_t wid = static_cast<int64_t>(view->winId());
        mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);
    } else {
        mpv_set_option_string(mpv, "vo", "libmpv");
        view = new GlView(mpv);
    }
    view->resize(1280, 720);
    view->show();

    if (mpv_initialize(mpv) < 0) return 3;

    QElapsedTimer clock;
    double ttffMs = -1, cpuAtWarm = 0, wallAtWarm = 0;
    const int warmupMs = 3000;

    auto finish = [&] {
        if (!mpv) return;
        const double wall = clock.elapsed() / 1000.0 - wallAtWarm;
        const double cpu = cpuSeconds() - cpuAtWarm;
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
        std::printf(
            "{\"mode\":\"%s\",\"hwdec_req\":\"%s\",\"hwdec_current\":\"%s\",\"vo\":\"%s\",\"codec\":\"%s\","
            "\"size\":\"%sx%s\",\"fps\":\"%s\",\"ttff_ms\":%.1f,\"cpu_pct_one_core\":%.1f,"
            "\"drops_vo\":%lld,\"drops_dec\":%lld,\"delayed\":%lld,\"mistimed\":%lld,"
            "\"working_set_mb\":%.1f,\"private_mb\":%.1f,\"seconds_measured\":%.1f}\n",
            mode.c_str(), hwdec, getStr(mpv, "hwdec-current").c_str(), getStr(mpv, "current-vo").c_str(),
            getStr(mpv, "video-codec").c_str(), getStr(mpv, "width").c_str(), getStr(mpv, "height").c_str(),
            getStr(mpv, "container-fps").c_str(), ttffMs, wall > 0 ? 100.0 * cpu / wall : -1.0,
            (long long)getInt(mpv, "frame-drop-count"), (long long)getInt(mpv, "decoder-frame-drop-count"),
            (long long)getInt(mpv, "vo-delayed-frame-count"), (long long)getInt(mpv, "mistimed-frame-count"),
            pmc.WorkingSetSize / 1048576.0, pmc.PrivateUsage / 1048576.0, wall);
        std::fflush(stdout);
        delete view;  // render context must die before the mpv core
        mpv_set_wakeup_callback(mpv, nullptr, nullptr);
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
        app.quit();
    };

    std::function<void()> drain = [&] {
        while (mpv) {
            mpv_event* ev = mpv_wait_event(mpv, 0);
            if (ev->event_id == MPV_EVENT_NONE) break;
            if (ev->event_id == MPV_EVENT_PLAYBACK_RESTART && ttffMs < 0) {
                ttffMs = clock.nsecsElapsed() / 1e6;
                QTimer::singleShot(warmupMs, [&] { cpuAtWarm = cpuSeconds(); wallAtWarm = clock.elapsed() / 1000.0; });
                QTimer::singleShot(warmupMs + seconds * 1000, finish);
            }
            if (ev->event_id == MPV_EVENT_END_FILE) {
                auto* ef = static_cast<mpv_event_end_file*>(ev->data);
                if (ef->reason == MPV_END_FILE_REASON_ERROR) {
                    std::fprintf(stderr, "playback error: %s\n", mpv_error_string(ef->error));
                    QTimer::singleShot(0, finish);
                }
            }
        }
    };
    // mpv calls this from its own thread; hop to the UI thread before touching anything.
    struct Ctx { QApplication* app; std::function<void()>* drain; } ctx{&app, &drain};
    mpv_set_wakeup_callback(mpv, [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        QMetaObject::invokeMethod(c->app, [d = c->drain] { (*d)(); }, Qt::QueuedConnection);
    }, &ctx);

    clock.start();
    const char* cmd[]{"loadfile", file, nullptr};
    mpv_command(mpv, cmd);
    return app.exec();
}
