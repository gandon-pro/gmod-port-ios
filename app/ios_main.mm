#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <OpenGLES/ES3/gl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "static_modules.h"
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <zlib.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>

extern "C" int LauncherMain(int argc, char** argv);
bool InstallSourceModules();

static void BindIOSFramebuffer(GLenum target, GLuint framebuffer) {
    // UIKit's drawable is a nonzero FBO. Source/Android uses 0 to mean the
    // screen; SDL_syswm.h explicitly requires binding the UIKit framebuffer.
    if (framebuffer == 0) {
        SDL_SysWMinfo info{};
        SDL_VERSION(&info.version);
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        if (window && SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_UIKIT)
            framebuffer = info.info.uikit.framebuffer;
    }
    glBindFramebuffer(target, framebuffer);
}

// Pixel size of UIKit's color renderbuffer, the target of sdlmgr's ShowPixels blit. SDL's window
// size is in points and its drawable size may not follow the HighDPI renderbuffer.
void GModPort_DrawableSize(int* w, int* h) {
    *w = *h = 0;
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    if (window && SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_UIKIT && info.info.uikit.colorbuffer) {
        GLint previous = 0;
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
        glBindRenderbuffer(GL_RENDERBUFFER, info.info.uikit.colorbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, w);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, h);
        glBindRenderbuffer(GL_RENDERBUFFER, previous);
    }
    if (*w <= 0 || *h <= 0)
        SDL_GL_GetDrawableSize(window, w, h);
    static int loggedW, loggedH;
    if (*w != loggedW || *h != loggedH) {
        int pw = 0, ph = 0, dw = 0, dh = 0;
        if (window) { SDL_GetWindowSize(window, &pw, &ph); SDL_GL_GetDrawableSize(window, &dw, &dh); }
        SDL_Log("Blit target %dx%d px (window %dx%d pt, SDL drawable %dx%d)", *w, *h, pw, ph, dw, dh);
        loggedW = *w; loggedH = *h;
    }
}

void* GModPort_GLProcAddress(const char* name) {
    if (!std::strcmp(name, "glBindFramebuffer") || !std::strcmp(name, "glBindFramebufferEXT"))
        return reinterpret_cast<void*>(&BindIOSFramebuffer);
    return SDL_GL_GetProcAddress(name);
}

// FileOpenDialog's desktop `open` command has no subprocess equivalent on iOS.
// Present the system Files picker at the requested directory on the UI thread.
void GModPort_OpenFolder(const char* path) {
    NSString* folder = path ? [NSString stringWithUTF8String:path] : nil;
    if (!folder) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        UIWindow* window = nil;
        for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
            if (scene.activationState != UISceneActivationStateForegroundActive ||
                ![scene isKindOfClass:UIWindowScene.class]) continue;
            for (UIWindow* candidate in ((UIWindowScene*)scene).windows)
                if (candidate.isKeyWindow) { window = candidate; break; }
            if (window) break;
        }
        // SDL2 can use the legacy UIApplication lifecycle without UIScenes.
        if (!window)
            for (UIWindow* candidate in UIApplication.sharedApplication.windows)
                if (candidate.isKeyWindow) { window = candidate; break; }
        UIViewController* presenter = window.rootViewController;
        while (presenter.presentedViewController) presenter = presenter.presentedViewController;
        if (!presenter || [presenter isKindOfClass:UIDocumentPickerViewController.class]) return;
        UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
        picker.directoryURL = [NSURL fileURLWithPath:folder isDirectory:YES];
        [presenter presentViewController:picker animated:YES completion:nil];
    });
}

void GModPort_RevealFile(const char* path) {
    NSString* file = path ? [NSString stringWithUTF8String:path] : nil;
    if (file) GModPort_OpenFolder(file.stringByDeletingLastPathComponent.UTF8String);
}

// Everything the app and engine print goes to Documents/gmodport_log.txt, readable via Files.
static FILE* g_log = nullptr;
static void LogToFile(void*, int, SDL_LogPriority, const char* message) {
    if (g_log) { fprintf(g_log, "%s\n", message); fflush(g_log); }
}

// gmcontent.gmpk: GMod content packed by tools/embed_content.py (format documented there).
struct PakHeader { uint64_t total; uint32_t count; unsigned char id[16]; };
static constexpr long kPakHeaderSize = 8 + 8 + 4 + 16;

static bool ReadPakHeader(const char* path, PakHeader& header) {
    FILE* f = path ? fopen(path, "rb") : nullptr;
    if (!f) return false;
    unsigned char raw[kPakHeaderSize];
    bool ok = fread(raw, 1, sizeof raw, f) == sizeof raw && !memcmp(raw, "GMPK1\0\0\0", 8);
    fclose(f);
    if (!ok) return false;
    memcpy(&header.total, raw + 8, 8);
    memcpy(&header.count, raw + 16, 4);
    memcpy(header.id, raw + 20, 16);
    return true;
}

class Inflater {
public:
    explicit Inflater(FILE* f) : f_(f), in_(1 << 20) { ok_ = inflateInit(&z_) == Z_OK; }
    ~Inflater() { inflateEnd(&z_); }
    bool Read(void* out, size_t n) {
        if (!ok_) return false;
        z_.next_out = static_cast<Bytef*>(out);
        z_.avail_out = static_cast<uInt>(n);
        while (z_.avail_out) {
            if (!z_.avail_in) {
                z_.next_in = in_.data();
                z_.avail_in = static_cast<uInt>(fread(in_.data(), 1, in_.size(), f_));
                if (!z_.avail_in) return false;
            }
            int r = inflate(&z_, Z_NO_FLUSH);
            if (r == Z_STREAM_END) return z_.avail_out == 0;
            if (r != Z_OK && r != Z_BUF_ERROR) return false;
        }
        return true;
    }
private:
    FILE* f_;
    z_stream z_{};
    bool ok_ = false;
    std::vector<Bytef> in_;
};

static bool UnpackContent(const std::string& pak, const std::filesystem::path& dest,
                          std::atomic<uint64_t>& done, std::string& error) {
    FILE* f = fopen(pak.c_str(), "rb");
    if (!f || fseek(f, kPakHeaderSize, SEEK_SET)) { error = "cannot open " + pak; if (f) fclose(f); return false; }
    auto in = std::make_unique<Inflater>(f);
    std::vector<char> buffer(4 << 20);
    bool ok = false;
    for (;;) {
        uint16_t length = 0;
        uint64_t size = 0;
        if (!in->Read(&length, 2)) { error = "archive truncated"; break; }
        if (!length) { ok = true; break; }
        std::string rel(length, '\0');
        if (!in->Read(&rel[0], length) || !in->Read(&size, 8)) { error = "archive truncated"; break; }
        if (rel[0] == '/' || rel.find("..") != std::string::npos) { error = "bad path " + rel; break; }
        std::filesystem::path out = dest / rel;
        std::error_code ec;
        std::filesystem::create_directories(out.parent_path(), ec);
        FILE* o = fopen(out.c_str(), "wb");
        if (!o) { error = "cannot create " + out.string(); break; }
        while (size) {
            size_t n = static_cast<size_t>(std::min<uint64_t>(size, buffer.size()));
            if (!in->Read(buffer.data(), n)) { error = "archive truncated in " + rel; break; }
            if (fwrite(buffer.data(), 1, n, o) != n) { error = "write failed (disk full?) " + rel; break; }
            size -= n;
            done += n;
        }
        if (fclose(o) != 0 && error.empty()) error = "write failed (disk full?) " + rel;
        if (!error.empty()) break;
    }
    in.reset();
    fclose(f);
    return ok;
}

// Unpacks on a worker thread while the main thread keeps UIKit responsive and draws a progress bar.
static bool UnpackWithProgress(const std::string& pak, const std::string& dest, uint64_t total, std::string& error) {
    std::atomic<uint64_t> done{0};
    std::atomic<bool> finished{false};
    bool ok = false;
    std::thread worker([&] { ok = UnpackContent(pak, dest, done, error); finished = true; });
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        window = SDL_CreateWindow("GModPort", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (window) renderer = SDL_CreateRenderer(window, -1, 0);
    }
    uint64_t logged = 0;
    while (!finished) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}
        uint64_t now = done;
        if (renderer) {
            int w = 0, h = 0;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_Rect frame{w / 10, h / 2 - h / 40, w * 8 / 10, h / 20};
            SDL_Rect bar = frame;
            bar.w = total ? static_cast<int>(frame.w * (double)now / (double)total) : 0;
            SDL_SetRenderDrawColor(renderer, 16, 16, 16, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 40, 120, 255, 255);
            SDL_RenderFillRect(renderer, &bar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &frame);
            SDL_RenderPresent(renderer);
        }
        if (now - logged >= (256ull << 20)) { SDL_Log("Unpacked %llu / %llu MB", now >> 20, total >> 20); logged = now; }
        SDL_Delay(50);
    }
    worker.join();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);  // the launcher initializes video itself with its own hints
    return ok;
}

static void Fatal(const std::string& message) {
    SDL_Log("FATAL: %s", message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GModPort", message.c_str(), nullptr);
}

int main(int, char**) {
    @autoreleasepool {
        NSString* logDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        NSString* logPath = [logDir stringByAppendingPathComponent:@"gmodport_log.txt"];
        g_log = fopen(logPath.UTF8String, "w");
        if (g_log) {
            setvbuf(g_log, nullptr, _IONBF, 0);
            dup2(fileno(g_log), STDOUT_FILENO);
            dup2(fileno(g_log), STDERR_FILENO);
            SDL_LogSetOutputFunction(LogToFile, nullptr);
            NSString* build = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
            fprintf(g_log, "GModPort build %s\n", build ? build.UTF8String : "?");
        }
        if (!InstallSourceModules()) { SDL_Log("Module table: %s",gmodport::LastModuleError()); return 1; }
        NSString* documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        NSString* game = [documents stringByAppendingPathComponent:@"garrysmod"];
        [[NSFileManager defaultManager] createDirectoryAtPath:game withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* cfg = [game stringByAppendingPathComponent:@"cfg"];
        NSString* scripts = [game stringByAppendingPathComponent:@"scripts"];
        [[NSFileManager defaultManager] createDirectoryAtPath:scripts withIntermediateDirectories:YES attributes:nil error:nil];
        [[NSFileManager defaultManager] createDirectoryAtPath:cfg withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* ui = [game stringByAppendingPathComponent:@"resource"];
        [[NSFileManager defaultManager] createDirectoryAtPath:ui withIntermediateDirectories:YES attributes:nil error:nil];
        // Bundled files always win: a GMod garrysmod/ folder copied in via Files brings its own
        // gameinfo.txt, which mounts paths that do not exist on the phone.
        NSFileManager* fm = [NSFileManager defaultManager];
        NSString* touchDir = [game stringByAppendingPathComponent:@"materials/vgui/touch"];
        [fm createDirectoryAtPath:touchDir withIntermediateDirectories:YES attributes:nil error:nil];
        // CMake may place MACOSX_PACKAGE_LOCATION Resources at the bundle root or in Resources/.
        NSString* bundle = [NSBundle mainBundle].resourcePath;
        NSString* (^resource)(NSString*) = ^NSString*(NSString* name) {
            NSString* flat = [bundle stringByAppendingPathComponent:name];
            return [fm fileExistsAtPath:flat] ? flat : [[bundle stringByAppendingPathComponent:@"Resources"] stringByAppendingPathComponent:name];
        };
        NSMutableArray<NSArray<NSString*>*>* copies = [NSMutableArray arrayWithArray:@[
            @[resource(@"gameinfo.txt"), [game stringByAppendingPathComponent:@"gameinfo.txt"]],
            @[resource(@"touch_default.cfg"), [cfg stringByAppendingPathComponent:@"touch_default.cfg"]],
            @[resource(@"weapon_physgun.txt"), [scripts stringByAppendingPathComponent:@"weapon_physgun.txt"]],
            // GMod-style main menu (GameUI): entries and the logo panel (materials/gmp/logo in the port pack).
            @[resource(@"GameMenu.res"), [ui stringByAppendingPathComponent:@"GameMenu.res"]],
            @[resource(@"GameLogo.res"), [ui stringByAppendingPathComponent:@"GameLogo.res"]]]];
        NSString* touchSource = resource(@"touch");
        for (NSString* name in [fm contentsOfDirectoryAtPath:touchSource error:nil])
            if ([name.pathExtension isEqualToString:@"vtf"])
                [copies addObject:@[[touchSource stringByAppendingPathComponent:name], [touchDir stringByAppendingPathComponent:name]]];
        if (copies.count == 5) SDL_Log("No touch textures in bundle at %s", touchSource.UTF8String);
        for (NSArray<NSString*>* copy in copies) {
            [fm removeItemAtPath:copy[1] error:nil];
            if (![fm copyItemAtPath:copy[0] toPath:copy[1] error:nil])
                SDL_Log("Cannot install %s", copy[1].UTF8String);
        }
        // gameinfo.txt mounts all content through Documents/gmcontent: a folder unpacked from the
        // bundle's gmcontent.gmpk (tools/embed_content.py) or, without one, a link to Documents
        // itself for folders copied in via Files.
        NSString* contentDir = [documents stringByAppendingPathComponent:@"gmcontent"];
        NSString* idFile = [contentDir stringByAppendingPathComponent:@".gmpk_id"];
        NSString* pak = resource(@"gmcontent.gmpk");
        NSDictionary* contentAttrs = [fm attributesOfItemAtPath:contentDir error:nil];
        BOOL contentIsLink = [contentAttrs.fileType isEqualToString:NSFileTypeSymbolicLink];
        PakHeader header{};
        if (ReadPakHeader(pak.UTF8String, header)) {
            NSMutableString* pakId = [NSMutableString string];
            for (unsigned char b : header.id) [pakId appendFormat:@"%02x", b];
            NSString* haveId = [NSString stringWithContentsOfFile:idFile encoding:NSUTF8StringEncoding error:nil];
            if (contentIsLink || ![haveId isEqualToString:pakId]) {
                SDL_Log("Unpacking %s: %u files, %llu MB", pakId.UTF8String, header.count, header.total >> 20);
                NSString* partial = [documents stringByAppendingPathComponent:@"gmcontent.partial"];
                [fm removeItemAtPath:contentDir error:nil];
                [fm removeItemAtPath:partial error:nil];
                uint64_t freeBytes = [[fm attributesOfFileSystemForPath:documents error:nil][NSFileSystemFreeSize] unsignedLongLongValue];
                if (freeBytes < header.total + (512ull << 20)) {
                    Fatal("Not enough free space to unpack game content: need " + std::to_string((header.total >> 20) + 512) +
                          " MB, have " + std::to_string(freeBytes >> 20) + " MB.");
                    return 1;
                }
                std::string error;
                if (!UnpackWithProgress(pak.UTF8String, partial.UTF8String, header.total, error)) {
                    [fm removeItemAtPath:partial error:nil];
                    Fatal("Unpacking game content failed: " + error);
                    return 1;
                }
                [pakId writeToFile:[partial stringByAppendingPathComponent:@".gmpk_id"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
                if (![fm moveItemAtPath:partial toPath:contentDir error:nil]) { Fatal("Cannot rename gmcontent.partial"); return 1; }
                SDL_Log("Unpacked game content");
            }
            SDL_Log("Content: unpacked %s", contentDir.UTF8String);
        } else if (!contentAttrs || contentIsLink) {
            [fm removeItemAtPath:contentDir error:nil];
            if (![fm createSymbolicLinkAtPath:contentDir withDestinationPath:@"." error:nil])
                SDL_Log("Cannot link %s", contentDir.UTF8String);
            SDL_Log("Content: Documents (copied via Files)");
        } else {
            SDL_Log("Content: previously unpacked %s", contentDir.UTF8String);
        }
        if (![fm fileExistsAtPath:[contentDir stringByAppendingPathComponent:@"garrysmod/garrysmod_dir.vpk"]])
            SDL_Log("No GMod content: garrysmod/garrysmod_dir.vpk missing in %s", contentDir.UTF8String);
        // 1080 lines at the screen's own aspect (iPhone 14: 2338x1080), so the blit does not stretch.
        CGSize screen = UIScreen.mainScreen.bounds.size;
        const double aspect = std::max(screen.width, screen.height) / std::max(1.0, (double)std::min(screen.width, screen.height));
        const std::string width = std::to_string(static_cast<int>(1080 * aspect + 1) & ~1);
        SDL_Log("Backbuffer %sx1080 (screen %.0fx%.0f pt)", width.c_str(), screen.width, screen.height);
        // Touches reach the game (touch.cpp) and vgui as finger events. SDL's default touch->mouse
        // emulation would add every finger as mouse look and a MOUSE1 click (Android disables it too).
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        std::vector<std::string> args={"GModPort","-basedir",documents.UTF8String,"-game","garrysmod","-novid","-insecure","-console","-condebug","-dev","2","-w",width,"-h","1080",
                                       "+touch_enable","1","+exec","touch_default.cfg"};  // starts at the main menu
        std::vector<char*> argv;
        for (auto& arg:args) argv.push_back(&arg[0]);
        // Source resolves -game and gameinfo search paths relative to the working directory.
        chdir(documents.UTF8String);
        return LauncherMain(static_cast<int>(argv.size()),argv.data());
    }
}
