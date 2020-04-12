

// ios only allows a single window so we can keep globals
var
  GlobalInitFlags: GLPT_InitFlags = [];
  GlobalWindow: pGLPTWindow = nil;

//#########################################################
// COCOA UTILS
//#########################################################

operator := (right: array of NSObject): NSArray;
var
  i: integer;
  arr: NSMutableArray;
begin
  arr := NSMutableArray.array_;
  for i := 0 to high(right) do
    arr.addObject(right[i]);
  result := NSArray.arrayWithArray(arr);
end;

{} operator = (left: NSString; right: string): boolean;
begin
  result := (left.UTF8String = right);
end;

operator := (right: TObject): id;
begin
  result := CocoaObject(right);
end;

operator := (right: string): NSString;
begin
  result := NSSTR(right);
end;

operator explicit (right: NSObject): string;
begin
  if assigned(right) then
    result := right.description.UTF8String
  else
    result := 'nil';
end;

{$if FPC_FULLVERSION <= 30004}
type
  objcbool = bool;
{$endif}
  
type
  NSFileManager_Additions = objccategory external (NSFileManager)
    function createDirectoryAtURL_withIntermediateDirectories_attributes_error (url: NSURL; createIntermediates: boolean; attributes: NSDictionary; error: NSErrorPtr): boolean; message 'createDirectoryAtURL:withIntermediateDirectories:attributes:error:'; { available in 10_7, 5_0 }
  end;

function NSSTR(str: string): NSString; overload;
begin
  result := NSString.stringWithCString_length(@str[1], length(str));
end;

// GLPT forwards
function IPhone_GetKeyboardShiftState: TShiftState; forward;
function IPhone_GetTime: double; forward;

type
  TGLPTWindow = objcclass (UIWindow)
    private
      ref: pGLPTwindow;
  end;

type
  TGLPT_UIView = objcclass (UIView)
    public
      function initWithFrame (frame: CGRect): id; override;

      procedure touchesBegan_withEvent (touches: NSSet; event: UIEvent); override;
      procedure touchesMoved_withEvent (touches: NSSet; event: UIEvent); override;
      procedure touchesEnded_withEvent (touches: NSSet; event: UIEvent); override;
      procedure touchesCancelled_withEvent (touches: NSSet; event: UIEvent); override;

      procedure waitForMainLoop; message 'waitForMainLoop';
      procedure addDefaultGestures; message 'addDefaultGestures';
    private
      firstFingerDown: UITouch;
      function owner: pGLPTwindow; message 'owner';

      procedure tapGesture (sender: UITapGestureRecognizer); message 'tapGesture:';
      procedure pinchGesture (sender: UIPinchGestureRecognizer); message 'pinchGesture:';
      procedure swipeGesture (sender: UISwipeGestureRecognizer); message 'swipeGesture:';
      procedure panGesture (sender: UIPanGestureRecognizer); message 'panGesture:';
      procedure rotationGesture (sender: UIRotationGestureRecognizer); message 'rotationGesture:';
      procedure longPressGesture (sender: UILongPressGestureRecognizer); message 'longPressGesture:';
  end;

type
  UITouch_Additions = objccategory external (UITouch)
    // iOS 9+
    function force: CGFloat; message 'force';
  end;

function PressureForTouch(touch: UITouch): single;
begin
  if touch.respondsToSelector(objcselector('force')) then
    result := touch.force
  else
    result := 1.0;
end;

function MessageParamsForTouch(view: UIView; touch: UITouch): GLPT_MessageParams;
var
  params: GLPT_MessageParams;
begin
  params := Default(GLPT_MessageParams);
  params.touch.x := Trunc(touch.locationInView(view).x);
  params.touch.y := Trunc(touch.locationInView(view).y);
  params.touch.pressure := PressureForTouch(touch);
  params.touch.timestamp := IPhone_GetTime;
  params.touch.tapCount := touch.tapCount;
  params.touch.radius := touch.majorRadius;
  result := params;
end;
function MessageParamsForTouches(view: UIView; touches: NSSet): GLPT_MessageParams;
var
  touch: UITouch;
  params: GLPT_MessageParams;
  fingerID: integer = 0;
begin
  params := Default(GLPT_MessageParams);
  for touch in touches do
    begin
      if fingerID = 0 then
        begin
          params.touch.x := Trunc(touch.locationInView(view).x);
          params.touch.y := Trunc(touch.locationInView(view).y);
          params.touch.pressure := PressureForTouch(touch);
          params.touch.timestamp := IPhone_GetTime;
          params.touch.tapCount := touch.tapCount;
          params.touch.radius := touch.majorRadius;
          params.touch.fingerCount := touches.count;
        end;
      params.touch.fingers[fingerID].x := Trunc(touch.locationInView(view).x);
      params.touch.fingers[fingerID].y := Trunc(touch.locationInView(view).y);
      params.touch.fingers[fingerID].pressure := PressureForTouch(touch);
      params.touch.fingers[fingerID].radius := touch.majorRadius;
      inc(fingerID);
    end;
  result := params;
end;

function MessageParamsForGesture(view: UIView; gesture: UIGestureRecognizer): GLPT_MessageParams;
var
  params: GLPT_MessageParams;
begin
  params := Default(GLPT_MessageParams);
  params.gesture.x := Trunc(gesture.locationInView(view).x);
  params.gesture.y := Trunc(gesture.locationInView(view).y);
  params.gesture.timestamp := IPhone_GetTime;

  case gesture.state of
    UIGestureRecognizerStateBegan:
      params.gesture.state := gsBegan;
    UIGestureRecognizerStateChanged:
      params.gesture.state := gsChanged;
    UIGestureRecognizerStateEnded:
      params.gesture.state := gsEnded;
    UIGestureRecognizerStateCancelled:
      params.gesture.state := gsCancelled;
    UIGestureRecognizerStateFailed:
      params.gesture.state := gsFailed
    otherwise
      params.gesture.state := gsUnknown;
  end;

  result := params;
end;

function SimulatedMouseMessageParamsForTouch(view: UIView; touch: UITouch): GLPT_MessageParams;
var
  params: GLPT_MessageParams;
begin
  params := Default(GLPT_MessageParams);
  params.mouse.x := Trunc(touch.locationInView(view).x);
  params.mouse.y := Trunc(touch.locationInView(view).y);
  params.mouse.shiftstate := IPhone_GetKeyboardShiftState;
  params.mouse.timestamp := IPhone_GetTime;
  params.mouse.buttons := GLPT_MOUSE_BUTTON_TOUCH;
  result := params;
end;

function TGLPT_UIView.owner: pGLPTwindow;
begin
  result := TGLPTWindow(window).ref;
end;

function TGLPT_UIView.initWithFrame (frame: CGRect): id;
begin
  result := inherited initWithFrame(frame);
  if result <> nil then
    begin
      setMultipleTouchEnabled(true);
      if GLPT_Gestures in GlobalInitFlags then
        addDefaultGestures;
    end;
end;

procedure TGLPT_UIView.waitForMainLoop;
begin
end;

procedure TGLPT_UIView.addDefaultGestures;
var
  gestureRecognizer: UIGestureRecognizer;
  tapGestureRecognizer: UITapGestureRecognizer;
  longPressGestureRecognizer: UILongPressGestureRecognizer;
  swipeGestureRecognizer: UISwipeGestureRecognizer;
begin  
  // tap gesture
  tapGestureRecognizer := UITapGestureRecognizer.alloc.initWithTarget_action(self, objcselector('tapGesture:')).autorelease;
  addGestureRecognizer(tapGestureRecognizer);

  // pinch gesture
  gestureRecognizer := UIPinchGestureRecognizer.alloc.initWithTarget_action(self, objcselector('pinchGesture:')).autorelease;
  addGestureRecognizer(gestureRecognizer);

  // pan gesture
  gestureRecognizer := UIPanGestureRecognizer.alloc.initWithTarget_action(self, objcselector('panGesture:')).autorelease;
  addGestureRecognizer(gestureRecognizer);

  // swipe gesture
  swipeGestureRecognizer := UISwipeGestureRecognizer.alloc.initWithTarget_action(self, objcselector('swipeGesture:')).autorelease;
  swipeGestureRecognizer.setDirection(UISwipeGestureRecognizerDirectionRight + 
                                      UISwipeGestureRecognizerDirectionLeft + 
                                      UISwipeGestureRecognizerDirectionUp + 
                                      UISwipeGestureRecognizerDirectionDown);
  addGestureRecognizer(swipeGestureRecognizer);

  // rotation gesture
  gestureRecognizer := UIRotationGestureRecognizer.alloc.initWithTarget_action(self, objcselector('rotationGesture:')).autorelease;
  addGestureRecognizer(gestureRecognizer);

  // long press gesture
  longPressGestureRecognizer := UILongPressGestureRecognizer.alloc.initWithTarget_action(self, objcselector('longPressGesture:')).autorelease;
  addGestureRecognizer(longPressGestureRecognizer);
end;

procedure TGLPT_UIView.touchesBegan_withEvent (touches: NSSet; event: UIEvent);
var
  params: GLPT_MessageParams;
  touch: UITouch;
begin
  firstFingerDown := nil;

  for touch in touches do
    begin
      // simulate mouse moved
      params := SimulatedMouseMessageParamsForTouch(self, touch);
      glptPostMessage(owner, GLPT_MESSAGE_MOUSEMOVE, params);

      // simulate mouse down
      params := SimulatedMouseMessageParamsForTouch(self, touch);
      params.mouse.buttons := GLPT_MOUSE_BUTTON_LEFT;
      glptPostMessage(owner, GLPT_MESSAGE_MOUSEDOWN, params);

      firstFingerDown := touch;
      break;
    end;

  params := MessageParamsForTouches(self, touches);
  glptPostMessage(owner, GLPT_MESSAGE_TOUCH_DOWN, params);
end;

procedure TGLPT_UIView.touchesMoved_withEvent (touches: NSSet; event: UIEvent);
var
  params: GLPT_MessageParams;
  touch: UITouch;
begin
  for touch in touches do
    begin
      if touch = firstFingerDown then
        begin
          params := SimulatedMouseMessageParamsForTouch(self, touch);
          glptPostMessage(owner, GLPT_MESSAGE_MOUSEMOVE, params);
        end;
    end;

  params := MessageParamsForTouches(self, touches);
  glptPostMessage(owner, GLPT_MESSAGE_TOUCH_MOTION, params);
  waitForMainLoop;
end;

procedure TGLPT_UIView.touchesEnded_withEvent (touches: NSSet; event: UIEvent);
var
  params: GLPT_MessageParams;
  touch: UITouch;
begin
  for touch in touches do
    if touch = firstFingerDown then
      begin
        params := SimulatedMouseMessageParamsForTouch(self, touch);
        params.mouse.buttons := GLPT_MOUSE_BUTTON_LEFT;
        glptPostMessage(owner, GLPT_MESSAGE_MOUSEUP, params);
        firstFingerDown := nil;
      end;

  params := MessageParamsForTouches(self, touches);
  glptPostMessage(owner, GLPT_MESSAGE_TOUCH_UP, params);
end;

procedure TGLPT_UIView.touchesCancelled_withEvent (touches: NSSet; event: UIEvent);
begin
  touchesEnded_withEvent(touches, event);
end;

procedure TGLPT_UIView.tapGesture (sender: UITapGestureRecognizer);
var
  params: GLPT_MessageParams;
begin
  params := MessageParamsForGesture(self, sender);
  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_TAP, params);
end;

procedure TGLPT_UIView.pinchGesture (sender: UIPinchGestureRecognizer);
var
  params: GLPT_MessageParams;
begin
  // https://developer.apple.com/documentation/uikit/touches_presses_and_gestures/handling_uikit_gestures/handling_pinch_gestures
  params := MessageParamsForGesture(self, sender);
  params.gesture.scale := sender.scale;
  params.gesture.velocity := sender.velocity;
  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_PINCH, params);
  waitForMainLoop;
end;

procedure TGLPT_UIView.swipeGesture (sender: UISwipeGestureRecognizer);
var
  params: GLPT_MessageParams;
begin  
  params := MessageParamsForGesture(self, sender);
  case sender.direction of
    UISwipeGestureRecognizerDirectionRight:
      params.gesture.direction := 3;
    UISwipeGestureRecognizerDirectionLeft:
      params.gesture.direction := 2;
    UISwipeGestureRecognizerDirectionUp:
      params.gesture.direction := 0;
    UISwipeGestureRecognizerDirectionDown:
      params.gesture.direction := 1;
  end;
  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_SWIPE, params);
  waitForMainLoop;
end;

procedure TGLPT_UIView.panGesture (sender: UIPanGestureRecognizer);
var
  params: GLPT_MessageParams;
begin  
  params := MessageParamsForGesture(self, sender);

  with params.gesture do
    begin
      translationX := trunc(sender.translationInView(self).x);
      translationY := trunc(sender.translationInView(self).y);
      velocityX := sender.velocityInView(self).x;
      velocityY := sender.velocityInView(self).y;
    end;

  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_PAN, params);
  waitForMainLoop;
end;

procedure TGLPT_UIView.rotationGesture (sender: UIRotationGestureRecognizer);
var
  params: GLPT_MessageParams;
begin
  params := MessageParamsForGesture(self, sender);
  params.gesture.rotation := sender.rotation;
  params.gesture.velocity := sender.velocity;
  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_ROTATE, params);
  waitForMainLoop;
end;

procedure TGLPT_UIView.longPressGesture (sender: UILongPressGestureRecognizer);
var
  params: GLPT_MessageParams;
begin
  params := MessageParamsForGesture(self, sender);
  glptPostMessage(owner, GLPT_MESSAGE_GESTURE_LONG_PRESS, params);
  waitForMainLoop;
end;

type
  TOpenGLESView = objcclass (TGLPT_UIView)
    public
      class function layerClass: pobjc_class; override;
      procedure layoutSubviews; override;
      procedure didMoveToWindow; override;
      procedure dealloc; override;
      procedure waitForMainLoop; override;
    private     
      eaglLayer: CAEAGLLayer;
      context: EAGLContext;
      colorRenderBuffer: GLuint;
      displayLink: CADisplayLink;
      displayLinkSemaphore,
      mainLoopSemaphore: pGLPT_Semaphore;
      frameBuffer: GLuint;
      displayLinkSetup: boolean;

      procedure setupDisplayLink; message 'setupDisplayLink';
      procedure renderDisplayLink(link: CADisplayLink); message 'renderDisplayLink:';
  end;

class function TOpenGLESView.layerClass: pobjc_class;
begin
  result := CAEAGLLayer.classClass;
end;

procedure TOpenGLESView.waitForMainLoop;
begin
  GLPT_SemaphoreWait(mainLoopSemaphore);
end;

procedure TOpenGLESView.renderDisplayLink(link: CADisplayLink);
begin
  //NSLog(['display link fired']);
  GLPT_SemaphorePost(displayLinkSemaphore);
end;

procedure TOpenGLESView.dealloc;
begin
  context.release;
  context := nil;

  if assigned(displayLink) then
    begin
      displayLink.invalidate;
      displayLink.release;
      displayLink := nil;
    end;

  if assigned(displayLinkSemaphore) then
    GLPT_DestroySemaphore(displayLinkSemaphore);

  inherited dealloc;
end;

procedure TOpenGLESView.layoutSubviews;
var
  params: GLPT_MessageParams;
begin
  params.rect.top := 0;
  params.rect.left := 0;
  params.rect.width  := Trunc(bounds.size.width);
  params.rect.height := Trunc(bounds.size.height);
  glptPostMessage(TGLPTWindow(window).ref, GLPT_MESSAGE_RESIZE, params);      

  // TODO: if you remove "layoutSubviews" we get Internal error 200303171
  // layoutSubviews is part of UIView category is probably why
  inherited layoutSubviews;
end;

procedure TOpenGLESView.setupDisplayLink;
var
  pool: NSAutoreleasePool;
begin
  pool := NSAutoreleasePool.alloc.init;

  displayLink := CADisplayLink.displayLinkWithTarget_selector(self, objcselector('renderDisplayLink:'));
  displayLink.addToRunLoop_forMode(NSRunLoop.currentRunLoop, NSDefaultRunLoopMode);
  displayLinkSetup := true;

  // TODO: do we need this?
  NSRunLoop.currentRunLoop.run;
  pool.release;
end;

procedure TOpenGLESView.didMoveToWindow;
var
  major, minor: cuint;
begin
  inherited didMoveToWindow;

  // setup EAGLContext
  eaglLayer := CAEAGLLayer(layer);
  eaglLayer.setOpaque(true);

  case owner^.context.glesVersion of
    1:
      context := EAGLContext.alloc.initWithAPI(kEAGLRenderingAPIOpenGLES1);
    2:
      context := EAGLContext.alloc.initWithAPI(kEAGLRenderingAPIOpenGLES2);
    3:
      context := EAGLContext.alloc.initWithAPI(kEAGLRenderingAPIOpenGLES3);
    otherwise
      glptError(GLPT_ERROR_PLATFORM, 'invalid OpenGLES version', GLPT_ERROR_FATAL);
  end;

  if context = nil then
    glptError(GLPT_ERROR_PLATFORM, 'EAGLContext for API '+IntToStr(owner^.context.glesVersion)+' couldn''t be initialized.', GLPT_ERROR_FATAL);

  EAGLContext.setCurrentContext(context);

  EAGLGetVersion(@major, @minor);
  //writeln('Requested API: ', owner^.context.glesVersion, ' (got ', context.API, ')');
  writeln('EAGL version: ', major,'.',minor);

  // setup render buffer
  glGenRenderbuffersOES(1, @colorRenderBuffer);
  glBindRenderbufferOES(GL_RENDERBUFFER_OES, colorRenderBuffer);
  context.renderbufferStorage_fromDrawable(GL_RENDERBUFFER_OES, eaglLayer);

  // setup default frame buffer
  glGenFramebuffersOES(1, @frameBuffer);
  glBindFramebufferOES(GL_FRAMEBUFFER_OES, frameBuffer);
  glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, colorRenderBuffer);

  owner^.glcontext := context;

  if owner^.context.vsync then
    begin
      NSThread.detachNewThreadSelector_toTarget_withObject(objcselector('setupDisplayLink'), self, nil);
      displayLinkSemaphore := GLPT_CreateSemaphore(0);
    end;

  mainLoopSemaphore := GLPT_CreateSemaphore(0);  
end;

type
  TAppDelegate = objcclass(UIResponder, UIApplicationDelegateProtocol)
    private
      procedure enterUserProgram; message 'enterUserProgram';
    public
      function application_didFinishLaunchingWithOptions(application: UIApplication; launchOptions: NSDictionary): boolean; message 'application:didFinishLaunchingWithOptions:';
      procedure applicationWillResignActive(application: UIApplication); message 'applicationWillResignActive:';
      procedure applicationDidEnterBackground(application: UIApplication); message 'applicationDidEnterBackground:';
      procedure applicationWillEnterForeground(application: UIApplication); message 'applicationWillEnterForeground:';
      procedure applicationDidBecomeActive(application: UIApplication); message 'applicationDidBecomeActive:';
      procedure applicationWillTerminate(application: UIApplication); message 'applicationWillTerminate:';
  end;

procedure TAppDelegate.applicationWillResignActive(application: UIApplication);
begin
end;

procedure TAppDelegate.applicationDidEnterBackground(application: UIApplication);
begin
end;

procedure TAppDelegate.applicationWillEnterForeground(application: UIApplication);
begin
end;

procedure TAppDelegate.applicationDidBecomeActive(application: UIApplication);
begin
end;

procedure TAppDelegate.applicationWillTerminate(application: UIApplication);
begin
end;

procedure TAppDelegate.enterUserProgram;
var
  res: integer;
begin
  // TODO: we need to keep argc and argv now
  res := GLPT_Main(0, '');
  if res <> 0 then
    halt(res);
end;

function TAppDelegate.application_didFinishLaunchingWithOptions (application: UIApplication; launchOptions: NSDictionary): boolean;
begin
  NSFileManager.defaultManager.changeCurrentDirectoryPath(NSBundle.mainBundle.resourcePath);
  performSelector_withObject_afterDelay(objcselector('enterUserProgram'), nil, 0);
  result := true;
end;

//#########################################################
// GLPT FUNCTIONS
//#########################################################

procedure IPhone_Delay(ms: longint);
begin
  NSThread.sleepForTimeInterval(ms/1000);
end;

procedure IPhone_Delay(ms: double);
begin
  NSThread.sleepForTimeInterval(ms);
end;

function IPhone_GetTime: double;
begin
  result := NSDate.date.timeIntervalSince1970;
end;

function IPhone_Terminate: boolean;
begin
  // TODO: what do we do here?
  //UIApp.terminate(nil);
  exit(True);
end;

procedure IPhone_DestroyWindow(var win: pGLPTwindow);
begin
  win^.ref.release;
  win^.ref := nil;
end;

function IPhone_CreateWindow(win: pGLPTwindow; posx, posy, sizex, sizey: integer; title: PChar): boolean;
var
  controller: UIViewController;
  rootView: TOpenGLESView;
  window: TGLPTWindow;
begin
  // only one window is allowed on ios
  if GlobalWindow <> nil then
    exit(false);

  // create window
  window := TGLPTWindow.alloc.initWithFrame(UIScreen.mainScreen.bounds);
  window.ref := win;
  win^.ref := window;
  
  GlobalWindow := win;

  // create root view controller
  controller := UIViewController.alloc.init.autorelease;
  //controller.view.setBackgroundColor(UIColor.blueColor);

  // create openGLES view
  rootView := TOpenGLESView.alloc.initWithFrame(window.bounds);
  controller.view.addSubView(rootView);

  window.setRootViewController(controller);
  window.makeKeyAndVisible;

  result := true;
end;

function GetRootView(win: pGLPTwindow): TOpenGLESView;
begin
  result := TOpenGLESView(win^.ref.rootViewController.view.subviews.objectAtIndex(0));
end;

function IPhone_MakeCurrent(win: pGLPTwindow): boolean;
begin
  EAGLContext.setCurrentContext(win^.glcontext);

  exit(True);
end;

procedure IPhone_SwapBuffers(win: pGLPTwindow);
var
  view: TOpenGLESView;
begin
  view := GetRootView(win);

  // once the display link has been setup we can block waiting for it to fire
  if view.displayLinkSetup then
    GLPT_SemaphoreWait(view.displayLinkSemaphore);
  view.context.presentRenderbuffer(GL_RENDERBUFFER_OES);
end;

procedure IPhone_GetFrameBufferSize(win: pGLPTwindow; out width, height: integer);
begin
  width := Trunc(win^.ref.bounds.size.width);
  height := Trunc(win^.ref.bounds.size.height);
end;

procedure IPhone_PollEvents;
const
  kSeconds = 0.000002;
var
  view: TOpenGLESView;
begin
  view := GetRootView(GlobalWindow);
  GLPT_SemaphorePost(view.mainLoopSemaphore);

  { Let the run loop run for a short amount of time: long enough for
    touch events to get processed (which is important to get certain
    elements of Game Center's GKLeaderboardViewController to respond
    to touch input), but not long enough to introduce a significant
    delay in the rest of the app. }
  while CFRunLoopRunInMode(kCFRunLoopDefaultMode, kSeconds, true) = kCFRunLoopRunHandledSource do 
    ;

  { Make sure UIScrollView objects scroll properly. }
  while CFRunLoopRunInMode(CFStringRef(UITrackingRunLoopMode), kSeconds, true) = kCFRunLoopRunHandledSource do 
    ;

end;

procedure IPhone_GetDisplayCoords(var dr: GLPTRect);
var
  screenFrame: CGRect;
begin
  screenFrame := UIScreen.mainScreen.bounds;

  dr.top := Trunc(screenFrame.origin.x);
  dr.left := Trunc(screenFrame.origin.y);
  dr.bottom := Trunc(screenFrame.origin.y + screenFrame.size.height);
  dr.right := Trunc(screenFrame.origin.x + screenFrame.size.width);
end;

function IPhone_GetBasePath: string;
begin
  result := NSBundle.mainBundle.resourcePath.UTF8String+'/';
end;

function IPhone_GetPrefPath (org: string; app: string): string;
var
  applicationSupport: NSURL;
  folder: NSURL;
begin
  applicationSupport := NSFileManager.defaultManager.URLForDirectory_inDomain_appropriateForURL_create_error(NSApplicationSupportDirectory, NSUserDomainMask, nil, false, nil);
  result := applicationSupport.path.UTF8String+'/'+org+'/'+app+'/';
  folder := NSURL.fileURLWithPath(NSSTR(result));
  NSFileManager.defaultManager.createDirectoryAtURL_withIntermediateDirectories_attributes_error(folder, true, nil, nil);
end;

function IPhone_GetKeyboardShiftState: TShiftState;
begin
  result := [];
end;

function IPhone_Init(flags: GLPT_InitFlags): boolean;
begin  
  // nothing to init for iphone right now ...
  GlobalInitFlags := flags;
  result := true;
end;

function IPhone_Main: integer;
var
  pool: NSAutoreleasePool;
  argc: LongInt = 0;
  argv: PChar = '';
begin  
  // TODO: do I need this for iOS? it's crashing in ios simulator
  // init the fpc heap manager
  // TThread.Create(false).WaitFor;

  pool := NSAutoreleasePool.alloc.init;
  UIApplicationMain(argc, argv, nil, NSSTR('TAppDelegate'));
  pool.release;

  result := 0;
end;

procedure IPhone_SetCursor(cursor: byte);
begin
  // no cursor in ios, do nothing
end;
