#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <SDL.h>
#include "static_modules.h"
#include <string>
#include <vector>

extern "C" int LauncherMain(int argc, char** argv);
bool InstallSourceModules();

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

int main(int, char**) {
    @autoreleasepool {
        if (!InstallSourceModules()) { SDL_Log("Module table: %s",gmodport::LastModuleError()); return 1; }
        NSString* documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        NSString* game = [documents stringByAppendingPathComponent:@"garrysmod"];
        [[NSFileManager defaultManager] createDirectoryAtPath:game withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* cfg = [game stringByAppendingPathComponent:@"cfg"];
        [[NSFileManager defaultManager] createDirectoryAtPath:cfg withIntermediateDirectories:YES attributes:nil error:nil];
        for (NSString* filename in @[@"gameinfo.txt", @"touch_default.cfg"]) {
            NSString* source = [[NSBundle mainBundle] pathForResource:filename ofType:nil];
            NSString* destination = [filename hasSuffix:@".cfg"] ? [cfg stringByAppendingPathComponent:filename] : [game stringByAppendingPathComponent:filename];
            if (![[NSFileManager defaultManager] fileExistsAtPath:destination])
                [[NSFileManager defaultManager] copyItemAtPath:source toPath:destination error:nil];
        }
        if (![[NSFileManager defaultManager] fileExistsAtPath:[game stringByAppendingPathComponent:@"maps/gm_construct.bsp"]])
            SDL_Log("gm_construct may be inside a VPK; content must be imported into %s", game.UTF8String);
        std::vector<std::string> args={"GModPort","-basedir",documents.UTF8String,"-game","garrysmod","-novid","-insecure","-console",
                                       "+touch_enable","1","+exec","touch_default.cfg","+map","gm_construct"};
        std::vector<char*> argv;
        for (auto& arg:args) argv.push_back(&arg[0]);
        return LauncherMain(static_cast<int>(argv.size()),argv.data());
    }
}
