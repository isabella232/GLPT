
function Cocoa_CreateSemaphore(sem: pGLPT_Semaphore; initial_value: integer): boolean;
begin
  // sem_init doesn't exist on macOS so we need to rely on named semaphores
  sem^.name := '/'+HexStr(sem)+#0;
  sem_unlink(@sem^.name[1]);
  sem^.handle := sem_open(@sem^.name[1], O_CREAT, cint(S_IWUSR), cint(initial_value));
  if (sem^.handle = SEM_FAILED) then
    begin
      GLPTError(GLPT_ERROR_THREADS, 'sem_open() failed '+IntToStr(fpGetErrno));
      result := false;
    end
  else
    result := true;
end;

procedure Cocoa_DestroySemaphore(sem: pGLPT_Semaphore);
begin
  if sem_close(sem^.handle) <> 0 then
    GLPTError(GLPT_ERROR_THREADS, 'sem_close() failed.');
  if sem_unlink(@sem^.name[1]) <> 0 then
    GLPTError(GLPT_ERROR_THREADS, 'sem_unlink() failed.');
end;

function Cocoa_SemaphoreTryWait(sem: pGLPT_Semaphore): integer;
begin
  if sem_trywait(sem^.handle) = 0 then
    result := 0
  else
    result := GLPT_ERROR_MUTEX_TIMEDOUT;
end;

function Cocoa_SemaphoreWait(sem: pGLPT_Semaphore): integer;
begin
  result := sem_wait(sem^.handle);
  if result < 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'sem_wait() failed '+IntToStr(fpGetErrno));
end;

function Cocoa_SemaphoreWaitTimeout(sem: pGLPT_Semaphore; timeout: longint): integer;
var
  endTime: longint;
begin
  // sem_timedwait is not implemented for macOS
  // https://stackoverflow.com/questions/641126/posix-semaphores-on-mac-os-x-sem-timedwait-alternative
  endTime := GLPT_GetTicks + timeout;

  while true do
    begin
      result := GLPT_SemaphoreTryWait(sem);
      if result = 0 then
        break;
      if GLPT_GetTicks - endTime <= 0 then
        break;
      GLPT_Delay(1);
    end;
end;

function Cocoa_SemaphoreValue(sem: pGLPT_Semaphore): integer;
begin
  result := -1;
  GLPTError(GLPT_ERROR_THREADS, 'Cocoa_SemValue() is not implemented.');
  // TODO: sem_getvalue not implemented in macOS! we need to make a whole new semaphore
  // implemention for this to work. see /generic/SDL_syssem.c for example
  //sem_getvalue(@sem^.handle, @result);
  //if result < 0 then
  //  result := 0;
end;

function Cocoa_SemaphorePost(sem: pGLPT_Semaphore): integer;
begin
  result := sem_post(sem^.handle);
  if result < 0 then
    GLPTError(GLPT_ERROR_THREADS, 'sem_post() failed '+IntToStr(fpGetErrno));
end;


function Cocoa_ConditionWait(condition: pGLPT_Condition; mutex: pGLPT_Mutex): integer;
begin
  if pthread_cond_wait(@condition^.handle, @mutex^.handle) <> 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'pthread_cond_wait() failed')
  else
    result := 0;
end;

function GetTimeoutTime (ms: longint): timespec;
var
  time: timeval;
begin
  fpGetTimeOfDay(@time,nil);
  result.tv_sec := time.tv_sec + (ms div 1000);
  result.tv_nsec := (time.tv_usec + (ms mod 1000) * 1000) * 1000;
  if result.tv_nsec > 1000000000 then
    begin
      result.tv_sec += 1;
      result.tv_nsec -= 1000000000;
    end;
end;

function Cocoa_ConditionWaitTimeout(condition: pGLPT_Condition; mutex: pGLPT_Mutex; ms: longint): integer;
var
  ts_timeout: timespec;
label
  TryAgain;
begin
  ts_timeout := GetTimeoutTime(ms);
  TryAgain:
  result := pthread_cond_timedwait(@condition^.handle, @mutex^.handle, @ts_timeout);
  case result of
    ESysEINTR:
      goto TryAgain;
    EsysETIMEDOUT:
      result := GLPT_ERROR_MUTEX_TIMEDOUT;
    otherwise
      result := GLPTError(GLPT_ERROR_THREADS, 'pthread_cond_timedwait() failed');
  end;
end;

function Cocoa_ConditionBroadcast(condition: pGLPT_Condition): integer;
begin
  if pthread_cond_broadcast(@condition^.handle) <> 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'pthread_cond_broadcast() failed')
  else
    result := 0;
end;

function Cocoa_ConditionSignal(condition: pGLPT_Condition): integer;
begin
  if pthread_cond_signal(@condition^.handle) <> 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'pthread_cond_signal() failed')
  else
    result := 0;
end;

procedure Cocoa_DestroyCondition(condition: pGLPT_Condition);
begin
  pthread_cond_destroy(@condition^.handle);
end;

function Cocoa_CreateCondition(condition: pGLPT_Condition): boolean;
begin
  if pthread_cond_init(@condition^.handle, nil) < 0 then
    begin
      GLPTError(GLPT_ERROR_THREADS, 'pthread_cond_init() failed');
      result := false;
    end
  else
    result := true;
end;

function Cocoa_LockMutex(mutex: pGLPT_Mutex): integer;
begin
  result := pthread_mutex_lock(@mutex^.handle);
  if result < 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'pthread_mutex_lock() failed');
end;

function Cocoa_UnlockMutex(mutex: pGLPT_Mutex): integer;
begin
  result := pthread_mutex_unlock(@mutex^.handle);
  if result < 0 then
    result := GLPTError(GLPT_ERROR_THREADS, 'pthread_mutex_unlock() failed');
end;

function Cocoa_TryLockMutex(mutex: pGLPT_Mutex): integer;
begin
  result := pthread_mutex_trylock(@mutex^.handle);
  if result <> 0 then
    begin
      if fpGetErrno = ESysEBUSY then
        result := GLPT_ERROR_MUTEX_TIMEDOUT
      else
        result := GLPTError(GLPT_ERROR_THREADS, 'pthread_mutex_trylock() failed');
    end;
end;

procedure Cocoa_DestroyMutex(mutex: pGLPT_Mutex);
begin
  pthread_mutex_destroy(@mutex^.handle);
end;

function Cocoa_CreateMutex(mutex: pGLPT_Mutex): boolean;
var
  attr: pthread_mutexattr_t;
begin
  pthread_mutexattr_init(@attr);
  // GLPT mutexes can be nested
  pthread_mutexattr_settype(@attr, PTHREAD_MUTEX_RECURSIVE);
  if pthread_mutex_init(@mutex^.handle, @attr) <> 0 then
    begin
      GLPTError(GLPT_ERROR_THREADS, 'pthread_mutex_init() failed');
      result := false;
    end
  else
    result := true;
end;

function Cocoa_ThreadID: GLPT_Thread_ID;
begin
  result := GLPT_Thread_ID(pthread_self);
end;

procedure Cocoa_DetachThread(thread: pGLPT_Thread);
begin
  pthread_detach(thread^.handle);
end;

procedure Cocoa_WaitThread(thread: pGLPT_Thread);
begin
  pthread_join(thread^.handle, nil);
end;

function Cocoa_CreateThread(thread: pGLPT_Thread): boolean;
var
  attr: pthread_attr_t;
begin
  if pthread_attr_init(@attr) <> 0 then
    GLPTError(GLPT_ERROR_THREADS, 'Couldn''t initialize pthread attributes');

  pthread_attr_setdetachstate(@attr, PTHREAD_CREATE_JOINABLE);
  pthread_attr_setstacksize(@attr, thread^.stacksize);

  if pthread_create(@thread^.handle, @attr, __startroutine_t(@GLPT_RunThread), thread) <> 0 then
    begin
      GLPTError(GLPT_ERROR_THREADS, 'Not enough resources to create thread');
      result := false;
    end
  else
    result := true;
end;
