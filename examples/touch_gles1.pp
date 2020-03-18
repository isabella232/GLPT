program touch;

{$mode objfpc}

uses
  CThreads, SysUtils,
  GLES11, GLPT, CocoaUtils;

var
  window: pGLPTWindow;
  context: GLPT_Context;
  rotate: double;
  event: pGLPT_MessageRec;

  procedure error_callback(error: integer; description: string);
  begin
    NSLog([error, ' ', description]);
  end;

  var
    nbFrames: longint = 0;
    lastTime: double = 0;

  procedure write_FPS;
  var
    currentTime: double;
    fps: string;
  begin
    // measure FPS
    currentTime := GLPT_GetTime;
    inc(nbFrames);

    if currentTime - lastTime >= 1 then
    begin
      fps := format('[FPS: %3.0f]', [nbFrames / (currentTime - lastTime)]);
      writeln(fps);
      nbFrames := 0;
      lastTime := GLPT_GetTime;
    end;
  end;

  procedure Reshape; 
  var
    width, height: integer;
    ratio: double;
  begin
    GLPT_GetFrameBufferSize(window, width, height);
    ratio := width / height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity;
    glOrthof(-ratio, ratio, -1, 1, 1, -1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity;
  end;

var
  colors: array of GLfloat = (
    1, 0, 0, 1,
    0, 1, 0, 1,
    0, 0, 1, 1
  );
  verts: array of GLfloat = (
    -0.6, -0.4, 0,
    0.6,  -0.4, 0,
    0.0,   0.6, 0
  );
begin
  GLPT_SetErrorCallback(@error_callback);

  if not GLPT_Init then
    halt(-1);

  context := GLPT_GetDefaultContext;
  context.glesVersion := 1;
  context.vsync := true;

  window := GLPT_CreateWindow(0, 0, 0, 0, 'Touch example', context);
  if window = nil then
  begin
    GLPT_Terminate;
    halt(-1);
  end;


  writeln('GLPT version: ', GLPT_GetVersionString);
  writeln('OpenGL version: ', glGetString(GL_VERSION)^);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glEnable(GL_SMOOTH);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  Reshape;

  while not GLPT_WindowShouldClose(window) do
  begin
    write_FPS;

    if GLPT_PollEvents(event) then
      case event^.mcode of
        GLPT_MESSAGE_MOUSEDOWN:
          ;
        GLPT_MESSAGE_TOUCH_DOWN:
          writeln('touch down: ', event^.params.touch.x, 'x', event^.params.touch.y);
        GLPT_MESSAGE_TOUCH_UP:
          writeln('touch up: ', event^.params.touch.x, 'x', event^.params.touch.y);
        GLPT_MESSAGE_TOUCH_MOTION:
          writeln('touch motion: ', event^.params.touch.x, 'x', event^.params.touch.y);
        GLPT_MESSAGE_RESIZE:
          Reshape;
      end;

    glClearColor(0.1, 0.1, 0.3, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity;

    rotate := (GLPT_GetTime * 10);
    rotate := rotate - int(rotate / 360) * 360;
    glRotatef(rotate, 0, 0, 1);

    // draw triangle
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, @verts[0]);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, @colors[0]);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    GLPT_SwapBuffers(window);
  end;

  GLPT_DestroyWindow(window);
  GLPT_Terminate;
end.
