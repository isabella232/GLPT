program touch;

{$mode objfpc}
{$assertions on}

uses
  CThreads, SysUtils,
  GLES30, GLPT, CocoaUtils;

// https://www.shaderific.com/blog/2014/3/13/tutorial-how-to-update-a-shader-for-opengl-es-30

const
  VertexShader: pchar =   '#version 300 es'+#10+
                          'layout (location=0) in vec4 vPosition;'+
                          'layout (location=1) in vec4 vColor;'+
                          'out vec4 fColor;'+
                          'void main() {'+
                          '  gl_Position = vPosition;'+
                          '  fColor = vec4(vColor.rgb, 1);'+
                          '}'#0;
  FragmentShader: pchar = '#version 300 es'+#10+
                          'precision mediump float;'+
                          'in vec4 fColor;'+
                          'out vec4 fragColor;'+
                          'void main() {'+
                          '  fragColor = fColor;'+
                          '}'#0;

var
  shader: GLint;
  vao: GLuint;
  vbo: GLuint;

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

type
  TModel = record
    verts: array[0..8] of GLfloat;
    colors: array[0..11] of GLfloat;
  end;

  generic procedure Move<T>(source: array of T; var dest);
  var
    bytes: SizeInt;
  begin
    bytes := length(source) * sizeof(T);
    System.Move(source, dest, bytes);
  end;

  procedure Prepare;
  var
    data: TModel;
  begin
    specialize Move<GLfloat>([
      0.0, 0.5, 0.0, 
      -0.5, -0.5, 0.0, 
      0.5, -0.5, 0.0], 
      data.verts);
    
    specialize Move<GLfloat>([
      1, 0, 0, 1,
      0, 1, 0, 1,
      0, 0, 1, 1
      ], 
      data.colors);

    glClearColor(0.1, 0.1, 0.3, 1);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // create the shader from sources
    shader := CreateShader(VertexShader, FragmentShader);

    glUseProgram(shader);

    // generate vertex array object
    // this will store the vertex attributes until a subsequent call to glBindVertexArray
    glGenVertexArrays(1, @vao);
    glBindVertexArray(vao);

    // generate vertex buffer object
    // we could use client side vertex arrays in GLES 3.0 but VAO's require a VBO
    glGenBuffers(1, @vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data.verts) + sizeof(data.colors), @data.verts, GL_STATIC_DRAW);

    // enable vertex attributes in the order they were specified in the vertex shader
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, PSizeInt(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, PSizeInt(sizeof(data.verts)));

    // clean up
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
  context.glesVersion := 3;

  window := GLPT_CreateWindow(0, 0, 0, 0, 'Touch example', context);
  if window = nil then
  begin
    GLPT_Terminate;
    halt(-1);
  end;

  // TODO: make a GLPT function that returns the version numbers instead of using
  // glGetString/EAGLGetVersion
  //procedure EAGLGetVersion(major: pcuint; minor: pcuint); cdecl; external;
  writeln('context.glesVersion: ', context.glesVersion);

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
    //write_FPS;

    if GLPT_PollEvents(event) then
      case event^.mcode of
        GLPT_MESSAGE_RESIZE:
          Reshape;
      end;

    glClear(GL_COLOR_BUFFER_BIT);

    // bind the VAO (for shader vertex attributes) and the VBO (for vertex data)
    // then issue the draw call using glDrawArrays
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    GLPT_SwapBuffers(window);
  end;

  GLPT_DestroyWindow(window);
  GLPT_Terminate;
end.
