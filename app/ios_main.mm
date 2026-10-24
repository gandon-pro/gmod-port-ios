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
        [[NSFileManager defaultManager] createDirectoryAtPath:cfg withIntermediateDirectories:YES attributes:nil error:nil];
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
            @[resource(@"touch_default.cfg"), [cfg stringByAppendingPathComponent:@"touch_default.cfg"]]]];
        NSString* touchSource = resource(@"touch");
        for (NSString* name in [fm contentsOfDirectoryAtPath:touchSource error:nil])
            if ([name.pathExtension isEqualToString:@"vtf"])
                [copies addObject:@[[touchSource stringByAppendingPathComponent:name], [touchDir stringByAppendingPathComponent:name]]];
        if (copies.count == 2) SDL_Log("No touch textures in bundle at %s", touchSource.UTF8String);
        for (NSArray<NSString*>* copy in copies) {
            [fm removeItemAtPath:copy[1] error:nil];
            if (![fm copyItemAtPath:copy[0] toPath:copy[1] error:nil])
                SDL_Log("Cannot install %s", copy[1].UTF8String);
        }
        if (![[NSFileManager defaultManager] fileExistsAtPath:[game stringByAppendingPathComponent:@"maps/gm_construct.bsp"]])
            SDL_Log("gm_construct may be inside a VPK; content must be imported into %s", game.UTF8String);
        std::vector<std::string> args={"GModPort","-basedir",documents.UTF8String,"-game","garrysmod","-novid","-insecure","-console","-condebug","-dev","2",
                                       "+touch_enable","1","+exec","touch_default.cfg","+map","gm_construct"};
        std::vector<char*> argv;
        for (auto& arg:args) argv.push_back(&arg[0]);
        return LauncherMain(static_cast<int>(argv.size()),argv.data());
    }
}
