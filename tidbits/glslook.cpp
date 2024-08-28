#include <cli/CLI.hpp>

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

#pragma comment(lib, "liballegro.dll.a")
#pragma comment(lib, "liballegro_image.dll.a")

#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>


///////////////////////////////////////////////////////////////////////////////
// GLSLook renders a glsl fragment shader, print compile errors to stdout and
// reload the shader on file change.
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// error helpers
// - report("message {}", args...)
// - MUST(condition, "message {}", args...)
// - SHOULD(condition, "message {}", args...)
//
template<typename... Ts> struct report
{
    template<typename Format, typename... Args>
    void do_report(const std::source_location &loc, Format format, Args &&...args) {
        std::cout << std::format("[{:%X}] ", std::chrono::system_clock::now());
        std::cout << loc.function_name() << ":" << loc.line() << ": ";
        std::cout << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }

    report(Ts &&...ts, const std::source_location &loc = std::source_location::current()) {
        do_report(loc, ts...);
    }
};

// deduction guide to make source_location magic work
template<typename... Ts> report(Ts &&...) -> report<Ts...>;


#define MUST_EXPR(condition)   MUST(condition, #condition)
#define SHOULD_EXPR(condition) SHOULD(condition, #condition)

template<typename... Args> void MUST(bool condition, const char *message, Args &&...args) {
    if(!condition) {
        report(message, args...);
        throw std::runtime_error(std::vformat(message, std::make_format_args(args...)));
    }
}

template<typename... Args> void SHOULD(bool condition, const char *message, Args &&...args) {
    if(!condition) {
        report(message, args...);
    }
}


///////////////////////////////////////////////////////////////////////////////
// command line options
//
struct Config
{
    int         width                    = 800;
    int         height                   = 800;
    float       fps                      = 30.0f;
    float       fileCheckInterval        = 1.0;
    bool        reportVertexShaderSource = false;
    std::string fragShaderPath;

    std::string json() const {
        return std::vformat(
            R"({{"width":"{}", "height":"{}", "fps":"{}", "fileCheckInterval":"{}", "reportVertexShaderSource":"{}", "fragShaderPath":"{}"}})",
            std::make_format_args(width, height, fps, fileCheckInterval, reportVertexShaderSource,
                                  fragShaderPath));
    }
};


///////////////////////////////////////////////////////////////////////////////
// shader uniforms that are always available
//
struct Uniforms
{
    // float uTime; // exposition only, uses al_get_time()
    float uResolution[2]; // uses al_get_display_width/height()
    float uMouse[2];      // set in event loop
    int   uKeycode;       // set in event loop
};


///////////////////////////////////////////////////////////////////////////////
// file watcher monitors a file for changes by file modification time
//
struct FileWatcher
{
    FileWatcher(ALLEGRO_FS_ENTRY *path) {
        MUST(m_pFsEntry = path, "null fs_entry");
        if(al_fs_entry_exists(m_pFsEntry)) {
            m_lastModified = al_get_fs_entry_mtime(m_pFsEntry);
        }
    }

    ~FileWatcher() {
        if(m_pFsEntry) {
            al_destroy_fs_entry(m_pFsEntry);
        }
    }

    bool changed() {
        if(!al_fs_entry_exists(m_pFsEntry)) {
            if(m_lastModified != 0) {
                m_lastModified = 0;
                return true; // file was deleted
            }
            return false; // file never existed
        }

        MUST(al_update_fs_entry(m_pFsEntry), "failed to update fs_entry");
        time_t previousLastModified = m_lastModified;
        m_lastModified              = al_get_fs_entry_mtime(m_pFsEntry);
        return m_lastModified != previousLastModified;
    }

    ALLEGRO_FS_ENTRY *m_pFsEntry     = nullptr;
    time_t            m_lastModified = 0;
};


///////////////////////////////////////////////////////////////////////////////
// the state of our program
struct AllegroState
{
    AllegroState(const Config &config);
    ~AllegroState();

    void UpdateShaderSource();

    ALLEGRO_DISPLAY     *display        = nullptr;
    ALLEGRO_EVENT_QUEUE *eventQueue     = nullptr;
    ALLEGRO_SHADER      *shader         = nullptr;
    ALLEGRO_BITMAP      *bitmap         = nullptr;
    ALLEGRO_TIMER       *fpsTimer       = nullptr;
    ALLEGRO_TIMER       *fileCheckTimer = nullptr;
    bool                 running        = true;
    bool                 wantsRedraw    = true;
    FileWatcher          fileWatcher;

    const char *defaultVertShader = nullptr;

    Uniforms uniforms;
};


AllegroState::AllegroState(const Config &config)
    : fileWatcher(al_create_fs_entry(config.fragShaderPath.c_str())) {
    MUST(al_is_system_installed(), "al_init() must be called before creating AllegroState");
    MUST(fileWatcher.m_pFsEntry, "failed to create fs_entry");

    MUST_EXPR(al_init_image_addon());
    MUST_EXPR(al_install_keyboard());
    MUST_EXPR(al_install_mouse());

    al_set_new_display_flags(ALLEGRO_OPENGL | ALLEGRO_PROGRAMMABLE_PIPELINE | ALLEGRO_RESIZABLE);

    MUST_EXPR(display = al_create_display(config.width, config.height));
    MUST_EXPR(eventQueue = al_create_event_queue());
    MUST_EXPR(shader = al_create_shader(ALLEGRO_SHADER_GLSL));
    MUST_EXPR(bitmap = al_create_bitmap(config.width, config.height));
    MUST_EXPR(fpsTimer = al_create_timer(1.0 / config.fps));
    MUST_EXPR(fileCheckTimer = al_create_timer(config.fileCheckInterval));
    MUST_EXPR(defaultVertShader =
                  al_get_default_shader_source(ALLEGRO_SHADER_GLSL, ALLEGRO_VERTEX_SHADER));
    if(config.reportVertexShaderSource) {
        report("default vertex shader:\n{}", defaultVertShader);
    }

    al_register_event_source(eventQueue, al_get_display_event_source(display));
    al_register_event_source(eventQueue, al_get_keyboard_event_source());
    al_register_event_source(eventQueue, al_get_mouse_event_source());
    al_register_event_source(eventQueue, al_get_timer_event_source(fpsTimer));
    al_register_event_source(eventQueue, al_get_timer_event_source(fileCheckTimer));

    UpdateShaderSource();

    al_start_timer(fpsTimer);
    al_start_timer(fileCheckTimer);
}


AllegroState::~AllegroState() {
    if(fpsTimer) {
        al_destroy_timer(fpsTimer);
    }
    if(fileCheckTimer) {
        al_destroy_timer(fileCheckTimer);
    }
    if(bitmap) {
        al_destroy_bitmap(bitmap);
    }
    if(shader) {
        al_destroy_shader(shader);
    }
    if(eventQueue) {
        al_destroy_event_queue(eventQueue);
    }
    if(display) {
        al_destroy_display(display);
    }
}


void AllegroState::UpdateShaderSource() {
    MUST_EXPR(al_use_shader(nullptr));

    if(!al_attach_shader_source(shader, ALLEGRO_VERTEX_SHADER, defaultVertShader)) {
        SHOULD(false, "warning: vertex shader: {}", al_get_shader_log(shader));
        return;
    }

    const char *szFragShaderPath = al_get_fs_entry_name(fileWatcher.m_pFsEntry);
    report("reloading {}", szFragShaderPath);
    if(!al_attach_shader_source_file(shader, ALLEGRO_PIXEL_SHADER, szFragShaderPath)) {
        SHOULD(false, "warning: fragment shader: {}", al_get_shader_log(shader));
        return;
    }

    MUST_EXPR(al_build_shader(shader));
    MUST_EXPR(al_use_shader(shader));

    SHOULD(al_set_shader_float("uTime", al_get_time()), "uTime cannot be set, maybe unused");
    SHOULD(al_set_shader_float_vector("uResolution", 2, uniforms.uResolution, 1),
           "uResolution cannot be set, maybe unused");
    SHOULD(al_set_shader_float_vector("uMouse", 2, uniforms.uMouse, 1),
           "uMouse cannot be set, maybe unused");
    SHOULD(al_set_shader_int("uKeycode", uniforms.uKeycode),
           "uKeycode cannot be set, maybe unused");

    wantsRedraw = true;
}


int main(int argc, char **argv) try {
    Config config;

#ifndef CLI11_VERSION
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <fragment shader path>" << std::endl;
        return 1;
    }
    config.fragShaderPath = argv[1];
#else

#    pragma comment(lib, "shell32.lib")
    CLI::App app{"GLSLook"};
    argv = app.ensure_utf8(argv);

    app.description(
        "Render a glsl fragment shader, print compile errors to stdout and reload the "
        "shader on file change.");

    app.add_option("-x,--width", config.width, "Window width")->default_val(config.width);
    app.add_option("-y,--height", config.height, "Window height")->default_val(config.height);
    app.add_option("-f,--fps", config.fps, "Frames per second")->default_val(config.fps);
    app.add_option("-i,--interval", config.fileCheckInterval, "File check interval in seconds")
        ->default_val(config.fileCheckInterval);
    app.add_flag("-r,--report-vertex", config.reportVertexShaderSource,
                 "Report vertex shader source");
    app.add_option("fragShaderPath", config.fragShaderPath, "Fragment shader path")->required();

    CLI11_PARSE(app, argc, argv);
#endif

    report("config: {}", config.json());

    MUST_EXPR(al_init());
    AllegroState state(config);

    while(state.running) {
        if(state.wantsRedraw && al_is_event_queue_empty(state.eventQueue)
           && al_get_current_shader()) {
            state.wantsRedraw = false;

            al_use_shader(state.shader);

            al_set_shader_float("uTime", al_get_time());
            state.uniforms.uResolution[0] = al_get_display_width(state.display);
            state.uniforms.uResolution[1] = al_get_display_height(state.display);
            al_set_shader_float_vector("uResolution", 2, state.uniforms.uResolution, 1);
            al_set_shader_float_vector("uMouse", 2, state.uniforms.uMouse, 1);
            al_set_shader_int("uKeycode", state.uniforms.uKeycode);

            al_draw_bitmap(state.bitmap, 0, 0, 0);
            al_flip_display();
        }

        ALLEGRO_EVENT event;
        al_wait_for_event(state.eventQueue, &event);

        switch(event.type) {
        case ALLEGRO_EVENT_DISPLAY_CLOSE: {
            state.running = false;
            break;
        }
        case ALLEGRO_EVENT_DISPLAY_RESIZE: {
            al_destroy_bitmap(state.bitmap);
            MUST(state.bitmap = al_create_bitmap(event.display.width, event.display.height),
                 "could not re-create bitmap.");
            state.wantsRedraw = true;
            MUST_EXPR(al_acknowledge_resize(state.display));
            break;
        }
        case ALLEGRO_EVENT_TIMER: {
            if(event.timer.source == state.fpsTimer) {
                state.wantsRedraw = true;
            } else if(event.timer.source == state.fileCheckTimer) {
                if(state.fileWatcher.changed()) {
                    state.UpdateShaderSource();
                }
            }
            break;
        }
        case ALLEGRO_EVENT_KEY_DOWN: {
            state.uniforms.uKeycode = event.keyboard.keycode;
            report("keydown: {}", state.uniforms.uKeycode);

            switch(event.keyboard.keycode) {
            case ALLEGRO_KEY_ESCAPE: // exit
                state.running = false;
                break;
            case ALLEGRO_KEY_P: // pause rendering
                if(al_get_timer_started(state.fpsTimer)) {
                    al_stop_timer(state.fpsTimer);
                } else {
                    al_start_timer(state.fpsTimer);
                }
                break;
            }
            break;
        }
        case ALLEGRO_EVENT_KEY_UP: {
            state.uniforms.uKeycode = 0;
            report("keyup: {}", state.uniforms.uKeycode);
            break;
        }
        case ALLEGRO_EVENT_MOUSE_AXES: {
            state.uniforms.uMouse[0] = event.mouse.x;
            state.uniforms.uMouse[1] = event.mouse.y;
            break;
        }
        }
    }
    return 0;
} catch(const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 1;
}


///////////////////////////////////////////////////////////////////////////////
// wannahaves
//
// - custom uniforms from files or stdin
// - fps counter
// - coloured console output
// - shortcuts: fullscreen, pause rendering, etc.
// - mouse buttons
// - sound!