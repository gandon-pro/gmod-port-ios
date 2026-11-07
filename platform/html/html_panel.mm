#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include "html_panel.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <set>
#include <string>

static std::atomic<GMPHTML_ReadAsset> reader{nullptr};
static std::atomic<uint64_t> nextId{1};
struct Message {uint64_t id;std::string name,json;};
static std::mutex queueLock;
static std::deque<Message> messages;
static std::set<uint64_t> alive;
@class GMPHTMLPanel;
@interface GMPHTMLProxy : NSObject<WKScriptMessageHandler,WKURLSchemeHandler,WKNavigationDelegate>
@property(nonatomic,weak) GMPHTMLPanel *owner;
@end
@interface GMPHTMLPanel : NSObject
@property(nonatomic,strong) WKWebView *view;
@property(nonatomic,strong) GMPHTMLProxy *proxy;
@property(nonatomic,strong) NSMutableSet<NSString *> *functions;
@property(nonatomic,assign) uint64_t token;
@property(nonatomic,copy) NSString *address;
@property(nonatomic,assign) BOOL loaded;
@end
@implementation GMPHTMLPanel
@end
static NSMutableDictionary<NSNumber *,GMPHTMLPanel *> *panels;
static BOOL Active(uint64_t token){std::lock_guard<std::mutex> guard(queueLock);return alive.count(token)!=0;}
static NSString *JSONString(NSString *text){NSData *data=[NSJSONSerialization dataWithJSONObject:@[text] options:0 error:nil];NSString *value=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];return [value substringWithRange:NSMakeRange(1,value.length-2)];}
static UIView *HostView(){
    for(UIScene *scene in UIApplication.sharedApplication.connectedScenes)if(scene.activationState==UISceneActivationStateForegroundActive&&[scene isKindOfClass:UIWindowScene.class])
        for(UIWindow *window in ((UIWindowScene *)scene).windows)if(window.isKeyWindow)return window.rootViewController.view;
    return nil;
}
@implementation GMPHTMLProxy
- (void)userContentController:(WKUserContentController *)controller didReceiveScriptMessage:(WKScriptMessage *)message {
    GMPHTMLPanel *panel=self.owner;if(!panel||!message.frameInfo.isMainFrame||![message.body isKindOfClass:NSDictionary.class])return;
    NSDictionary *body=message.body;NSString *name=body[@"name"];id args=body[@"args"];
    if(![name isKindOfClass:NSString.class]||![panel.functions containsObject:name]||![args isKindOfClass:NSArray.class])return;
    NSData *json=[NSJSONSerialization dataWithJSONObject:args options:0 error:nil];if(!json||json.length>65535||[name lengthOfBytesUsingEncoding:NSUTF8StringEncoding]>255)return;
    std::lock_guard<std::mutex> guard(queueLock);
    if(alive.count(panel.token)&&messages.size()<128)messages.push_back({panel.token,name.UTF8String,std::string(static_cast<const char *>(json.bytes),json.length)});
}
- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)task {
    NSURL *url=task.request.URL;GMPHTML_ReadAsset read=reader.load();
    NSString *path=url.path.stringByRemovingPercentEncoding;
    BOOL valid=self.owner&&[url.host isEqualToString:@"garrysmod"]&&path&&read;
    if(valid)for(NSString *part in path.pathComponents)if([part isEqualToString:@".."]||[part containsString:@"\\"]||[part containsString:@":"])valid=NO;
    if(!valid){[task didFailWithError:[NSError errorWithDomain:@"GModPort.HTML" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Invalid game asset URL"}]];return;}
    while([path hasPrefix:@"/"])path=[path substringFromIndex:1];
    void *bytes=nullptr;size_t count=0;
    if(!read(path.UTF8String,&bytes,&count)||(!bytes&&count)){free(bytes);[task didFailWithError:[NSError errorWithDomain:@"GModPort.HTML" code:2 userInfo:@{NSLocalizedDescriptionKey:@"Game asset not found"}]];return;}
    NSDictionary *types=@{@"html":@"text/html",@"css":@"text/css",@"js":@"application/javascript",@"json":@"application/json",@"png":@"image/png",@"jpg":@"image/jpeg",@"jpeg":@"image/jpeg",@"svg":@"image/svg+xml",@"woff":@"font/woff",@"woff2":@"font/woff2",@"ttf":@"font/ttf"};
    NSString *mime=types[path.pathExtension.lowercaseString]?:@"application/octet-stream";
    NSData *data=[NSData dataWithBytesNoCopy:bytes length:count freeWhenDone:YES];
    [task didReceiveResponse:[[NSURLResponse alloc] initWithURL:url MIMEType:mime expectedContentLength:count textEncodingName:nil]];
    [task didReceiveData:data];[task didFinish];
}
- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)task { /* Requests are completed synchronously. */ }
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)action decisionHandler:(void (^)(WKNavigationActionPolicy))handler {
    NSURL *url=action.request.URL;
    handler(([url.scheme isEqualToString:@"asset"]&&[url.host isEqualToString:@"garrysmod"])||[url.absoluteString isEqualToString:@"about:blank"]?WKNavigationActionPolicyAllow:WKNavigationActionPolicyCancel);
}
- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {NSLog(@"GModPort HTML navigation: %@",error.localizedDescription);}
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {NSLog(@"GModPort HTML load: %@",error.localizedDescription);}
@end
extern "C" void GMPHTML_SetAssetReader(GMPHTML_ReadAsset callback){reader.store(callback);}
extern "C" uint64_t GMPHTML_Create(const char *address){
    NSString *url=[NSString stringWithUTF8String:address?address:""];NSURL *parsed=[NSURL URLWithString:url?:@""];if(!parsed||![parsed.scheme isEqualToString:@"asset"]||![parsed.host isEqualToString:@"garrysmod"])return 0;
    const uint64_t token=nextId.fetch_add(1);{std::lock_guard<std::mutex> guard(queueLock);alive.insert(token);}
    dispatch_async(dispatch_get_main_queue(),^{
        if(!Active(token))return;
        UIView *host=HostView();if(!host){NSLog(@"GModPort HTML: no active SDL host view");std::lock_guard<std::mutex> guard(queueLock);alive.erase(token);return;}
        if(!panels)panels=[NSMutableDictionary dictionary];
        GMPHTMLPanel *panel=[GMPHTMLPanel new];panel.token=token;panel.address=url;panel.functions=[NSMutableSet set];
        GMPHTMLProxy *proxy=[GMPHTMLProxy new];proxy.owner=panel;panel.proxy=proxy;
        WKWebViewConfiguration *config=[WKWebViewConfiguration new];[config setURLSchemeHandler:proxy forURLScheme:@"asset"];
        [config.userContentController addScriptMessageHandler:proxy name:@"gmp"];
        panel.view=[[WKWebView alloc] initWithFrame:host.bounds configuration:config];panel.view.navigationDelegate=proxy;
        panel.view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;panel.view.opaque=NO;panel.view.backgroundColor=UIColor.clearColor;
        panel.view.hidden=YES;panels[@(token)]=panel;[host addSubview:panel.view];
    });return token;
}
extern "C" void GMPHTML_Destroy(uint64_t token){
    {std::lock_guard<std::mutex> guard(queueLock);alive.erase(token);for(auto it=messages.begin();it!=messages.end();)if(it->id==token)it=messages.erase(it);else ++it;}
    dispatch_async(dispatch_get_main_queue(),^{GMPHTMLPanel *panel=panels[@(token)];[panel.view stopLoading];[panel.view.configuration.userContentController removeScriptMessageHandlerForName:@"gmp"];[panel.view removeFromSuperview];[panels removeObjectForKey:@(token)];});
}
extern "C" void GMPHTML_Shutdown(){
    reader.store(nullptr);
    {std::lock_guard<std::mutex> guard(queueLock);alive.clear();messages.clear();}
    dispatch_async(dispatch_get_main_queue(),^{
        for(GMPHTMLPanel *panel in panels.allValues){
            panel.proxy.owner=nil;
            [panel.view stopLoading];
            [panel.view.configuration.userContentController removeScriptMessageHandlerForName:@"gmp"];
            [panel.view removeFromSuperview];
        }
        [panels removeAllObjects];
    });
}
extern "C" void GMPHTML_Show(uint64_t token,int visible){dispatch_async(dispatch_get_main_queue(),^{GMPHTMLPanel *p=panels[@(token)];if(!p)return;p.view.hidden=!visible;if(visible){if(!p.loaded){p.loaded=YES;[p.view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:p.address]]];}[p.view.superview bringSubviewToFront:p.view];}});}
extern "C" void GMPHTML_Bounds(uint64_t token,double x,double y,double w,double h){
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||w<0||h<0)return;
    dispatch_async(dispatch_get_main_queue(),^{GMPHTMLPanel *p=panels[@(token)];CGSize size=p.view.superview.bounds.size;p.view.frame=CGRectMake(x*size.width,y*size.height,w*size.width,h*size.height);});
}
extern "C" void GMPHTML_Call(uint64_t token,const char *code){NSString *js=[NSString stringWithUTF8String:code?code:""];if(!js)return;dispatch_async(dispatch_get_main_queue(),^{[panels[@(token)].view evaluateJavaScript:js completionHandler:^(id result,NSError *error){if(error)NSLog(@"GModPort HTML JS: %@",error.localizedDescription);}];});}
extern "C" void GMPHTML_AddFunction(uint64_t token,const char *object,const char *function){
    NSString *obj=[NSString stringWithUTF8String:object?object:""];NSString *fn=[NSString stringWithUTF8String:function?function:""];if(!obj.length||!fn.length)return;
    dispatch_async(dispatch_get_main_queue(),^{GMPHTMLPanel *p=panels[@(token)];if(!p)return;NSString *name=[NSString stringWithFormat:@"%@.%@",obj,fn];[p.functions addObject:name];
        NSString *js=[NSString stringWithFormat:@"window[%@]=window[%@]||{};window[%@][%@]=function(){window.webkit.messageHandlers.gmp.postMessage({name:%@,args:Array.from(arguments)});};",JSONString(obj),JSONString(obj),JSONString(obj),JSONString(fn),JSONString(name)];
        WKUserScript *script=[[WKUserScript alloc] initWithSource:js injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES];[p.view.configuration.userContentController addUserScript:script];
        [p.view evaluateJavaScript:js completionHandler:nil];
    });
}
extern "C" int GMPHTML_Poll(uint64_t token,char *name,size_t nameSize,char *json,size_t jsonSize){
    std::lock_guard<std::mutex> guard(queueLock);if(!alive.count(token))return 0;
    for(auto it=messages.begin();it!=messages.end();++it)if(it->id==token){if(!name||!json||it->name.size()+1>nameSize||it->json.size()+1>jsonSize)return -1;memcpy(name,it->name.c_str(),it->name.size()+1);memcpy(json,it->json.c_str(),it->json.size()+1);messages.erase(it);return 1;}return 0;
}
