program touch;

{$mode objfpc}
{$assertions on}

uses
  CThreads, SysUtils,
  GLES20, GLPT, CocoaUtils;

const
  VertexShader: pchar =   'attribute vec4 vPosition;'+#10+
                          'attribute vec4 vColor;'+
                          'varying vec4 fColor;'+
                          'void main() {'+
                          '  gl_Position = vPosition;'+
                          '  fColor = vColor;'+
                          '}'#0;
  FragmentShader: pchar = 'precision mediump float;'+#10+
                          'varying vec4 fColor;'+
                          'void main() {'+
                          '  gl_FragColor = fColor;'+
                          '}'#0;

var
  shader: GLint;
  positionAttrib,
  colorAttrib: integer;

var
  colors: array of GLfloat = (
    1, 0, 0, 1,
    0, 1, 0, 1,
    0, 0, 1, 1
  );
  verts: array of GLfloat = (
    0.0, 0.5, 0.0, 
    -0.5, -0.5, 0.0, 
    0.5, -0.5, 0.0
  );

var
  window: pGLPTWindow;
  context: GLPT_Context;
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

  function CreateShader (vertexShaderSource, fragmentShaderSource: pchar): GLuint;
  var
    programID: GLuint;
    vertexShaderID: GLuint;
    fragmentShaderID: GLuint;
  var
    success: GLint;
    logLength: GLint;
    logArray: array of char;
    i: integer;
  begin
    // create shader
    vertexShaderID := glCreateShader(GL_VERTEX_SHADER);
    fragmentShaderID := glCreateShader(GL_FRAGMENT_SHADER);

    // shader source
    glShaderSource(vertexShaderID, 1, @vertexShaderSource, nil);
    glShaderSource(fragmentShaderID, 1, @fragmentShaderSource, nil);  

    // compile shader
    glCompileShader(vertexShaderID);
    glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, @success);
    glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, @logLength);
    if success = GL_FALSE then
      begin
        SetLength(logArray, logLength+1);
        glGetShaderInfoLog(vertexShaderID, logLength, nil, @logArray[0]);
        for i := 0 to logLength do
          write(logArray[i]);
        Assert(success = GL_TRUE, 'Vertex shader failed to compile');
      end;
    
    glCompileShader(fragmentShaderID);
    glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, @success);
    glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, @logLength);
    if success = GL_FALSE then
      begin
        SetLength(logArray, logLength+1);
        glGetShaderInfoLog(fragmentShaderID, logLength, nil, @logArray[0]);
        for i := 0 to logLength do
          write(logArray[i]);
        Assert(success = GL_TRUE, 'Fragment shader failed to compile');
      end;
      
    // create program
    programID := glCreateProgram();
    glAttachShader(programID, vertexShaderID);
    glAttachShader(programID, fragmentShaderID);

    // link
    glLinkProgram(programID);
    glGetProgramiv(programID, GL_LINK_STATUS, @success);
    Assert(success = GL_TRUE, 'Error with linking shader program'); 

    result := programID;
  end;

  procedure Prepare;
  begin
    glClearColor(0.1, 0.1, 0.3, 1);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader := CreateShader(VertexShader, FragmentShader);
    glUseProgram(shader);

    // we can't assume the attributes are in the same order
    // as they were defined in the shader
    positionAttrib := glGetAttribLocation(shader, 'vPosition');
    colorAttrib := glGetAttribLocation(shader, 'vColor');
  end;

  procedure Reshape;
  var
    width, height: integer;
  begin
    GLPT_GetFrameBufferSize(window, width, height);
    glViewport(0, 0, width, height);
  end;

begin
  GLPT_SetErrorCallback(@error_callback);

  if not GLPT_Init then
    halt(-1);

  context := GLPT_GetDefaultContext;
  context.glesVersion := 2;

  window := GLPT_CreateWindow(0, 0, 0, 0, 'Touch example', context);
  if window = nil then
  begin
    GLPT_Terminate;
    halt(-1);
  end;

  writeln('GLPT version: ', GLPT_GetVersionString);
  writeln('OpenGL version: ', glGetString(GL_VERSION)^);
  writeln('GLSL version: ', glGetString(GL_SHADING_LANGUAGE_VERSION)^);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  Prepare;
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

    glClear(GL_COLOR_BUFFER_BIT);

    // position attributes
    glEnableVertexAttribArray(positionAttrib);
    glVertexAttribPointer(positionAttrib, 3, GL_FLOAT, GL_FALSE, 0, @verts[0]);

    // color attributes
    glEnableVertexAttribArray(colorAttrib);
    glVertexAttribPointer(colorAttrib, 4, GL_FLOAT, GL_FALSE, 0, @colors[0]);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    GLPT_SwapBuffers(window);
  end;

  GLPT_DestroyWindow(window);
  GLPT_Terminate;
end.
