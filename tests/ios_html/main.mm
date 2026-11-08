#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include "../../platform/html/html_panel.h"
#include <cstdlib>
#include <cstring>
#include <cmath>

// A UIKit transport fixture, not a Source/gameplay compatibility test.
static const char page[]=R"HTML(<!doctype html><html><head><meta charset="utf-8"></head>
<body style="margin:0"><div id="fill" style="position:fixed;inset:0;background:#183b68">
<button id="resume" onclick="lua.Run('gui.HideGameUI()')">Resume Game</button></div>
<script>var gScope={},scope={};</script></body></html>)HTML";
static int Asset(const char *path,void **bytes,size_t *count){
    if(strcmp(path,"html/menu.html"))return 0;
    *count=sizeof(page)-1;*bytes=malloc(*count);memcpy(*bytes,page,*count);return 1;
}
static void Finish(BOOL passed,NSString *detail){
    NSString *root=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    NSData *json=[NSJSONSerialization dataWithJSONObject:@{@"passed":@(passed),@"detail":detail} options:NSJSONWritingPrettyPrinted error:nil];
    [json writeToFile:[root stringByAppendingPathComponent:@"html-result.json"] atomically:YES];
    NSLog(@"HTML transport test: %@",detail);
}
@interface HarnessController:UIViewController
@end
@implementation HarnessController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIInterfaceOrientationMaskAll;}
@end
@interface HarnessScene:NSObject<UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic,strong) NSTimer *timer;
@property(nonatomic,assign) uint64_t token;
@property(nonatomic,assign) int phase;
@property(nonatomic,assign) int ticks;
@end
@implementation HarnessScene
- (BOOL)fullFrame:(WKWebView *)view{
    CGRect a=view.frame,b=self.window.bounds;
    UIEdgeInsets inset=view.scrollView.adjustedContentInset;
    return view.superview==self.window&&fabs(a.origin.x-b.origin.x)<.5&&fabs(a.origin.y-b.origin.y)<.5&&fabs(a.size.width-b.size.width)<.5&&fabs(a.size.height-b.size.height)<.5&&UIEdgeInsetsEqualToEdgeInsets(inset,UIEdgeInsetsZero);
}
- (void)done:(BOOL)success reason:(NSString *)reason{[self.timer invalidate];self.timer=nil;Finish(success,reason);GMPHTML_Shutdown();}
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options{
    self.window=[[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.window.rootViewController=[HarnessController new];
    // Force a nontrivial safe area. The HTML overlay must still cover the window.
    self.window.rootViewController.additionalSafeAreaInsets=UIEdgeInsetsMake(10,17,23,19);
    self.window.rootViewController.view.backgroundColor=UIColor.blueColor;
    [self.window makeKeyAndVisible];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{
        GMPHTML_SetAssetReader(Asset);self.token=GMPHTML_Create("asset://garrysmod/html/menu.html");
        GMPHTML_AddFunction(self.token,"lua","Run");GMPHTML_Viewport(self.token,1280,720);GMPHTML_Show(self.token,1);
        self.timer=[NSTimer scheduledTimerWithTimeInterval:.05 target:self selector:@selector(tick) userInfo:nil repeats:YES];
    });
}
- (void)tick{
    if(++self.ticks>800){[self done:NO reason:[NSString stringWithFormat:@"Timed out in phase %d",self.phase]];return;}
    WKWebView *view=nil;for(UIView *child in self.window.subviews)if([child isKindOfClass:WKWebView.class])view=(WKWebView *)child;
    if(!view)return;
    char name[256],json[65536];
    while(GMPHTML_Poll(self.token,name,sizeof(name),json,sizeof(json))==1){
        if(!strcmp(name,"_gmp.Error")){[self done:NO reason:[NSString stringWithUTF8String:json]];return;}
        if(self.phase==0&&!strcmp(name,"_gmp.Ready")){
            if(![self fullFrame:view]){[self done:NO reason:@"WKWebView leaves a frame or safe-area gap"];return;}
            self.phase=1;
            [view evaluateJavaScript:@"document.querySelector('meta[name=viewport]').content.includes('viewport-fit=cover') && Math.abs(document.getElementById('fill').getBoundingClientRect().width-innerWidth)<1" completionHandler:^(id value,NSError *error){
                if(error||![value boolValue]){[self done:NO reason:@"Viewport does not fill its CSS canvas"];return;}
                GMPHTML_Call(self.token,"document.getElementById('resume').click()");
            }];
        }else if(self.phase==1&&!strcmp(name,"lua.Run")){
            NSData *data=[[NSString stringWithUTF8String:json] dataUsingEncoding:NSUTF8StringEncoding];
            NSDictionary *packet=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
            if(![packet[@"args"] isEqual:@[@"gui.HideGameUI()"]]){[self done:NO reason:@"Resume callback lost its arguments"];return;}
            GMPHTML_Show(self.token,0);self.phase=2;
        }
    }
    if(self.phase==2&&view.hidden){
        if(view.userInteractionEnabled){[self done:NO reason:@"Hidden overlay still accepts input"];return;}
        GMPHTML_Show(self.token,1);self.phase=3;
    }else if(self.phase==3&&!view.hidden){
        if(!view.userInteractionEnabled){[self done:NO reason:@"Reopened overlay cannot receive input"];return;}
        self.phase=4;
        UIWindowSceneGeometryPreferencesIOS *geometry=[[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscapeRight];
        [self.window.windowScene requestGeometryUpdateWithPreferences:geometry errorHandler:^(NSError *error){[self done:NO reason:error.localizedDescription];}];
    }else if(self.phase==4&&self.window.bounds.size.width>self.window.bounds.size.height){
        if(![self fullFrame:view]){[self done:NO reason:@"Rotated overlay no longer fills the window"];return;}
        [self done:YES reason:@"Actual iOS WebKit: full window, zero safe-area gaps, Resume callback, hide/input release, reopen and landscape resize passed"];
    }
}
@end
@interface HarnessApp:NSObject<UIApplicationDelegate>
@end
@implementation HarnessApp
- (UISceneConfiguration *)application:(UIApplication *)app configurationForConnectingSceneSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options{
    UISceneConfiguration *config=[[UISceneConfiguration alloc] initWithName:@"Harness" sessionRole:session.role];config.delegateClass=HarnessScene.class;return config;
}
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(HarnessApp.class));}}
