/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <ole.andre.ravnas@tillitech.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumscriptplatform.h"

#include "gumscript-debug.h"
#include "gumscript-runtime.h"

using namespace v8;

GumScriptPlatform::GumScriptPlatform ()
  : scheduler (gum_script_scheduler_new ()),
    start_time (g_get_monotonic_time ())
{
  V8::InitializePlatform (this);
  V8::Initialize ();

  isolate = Isolate::New ();
  isolate->SetFatalErrorHandler (OnFatalError);

  InitRuntime ();
}

void
GumScriptPlatform::InitRuntime ()
{
  Locker locker (isolate);
  Isolate::Scope isolate_scope (isolate);
  HandleScope handle_scope (isolate);
  Local<Context> context (Context::New (isolate));
  Context::Scope context_scope (context);

  user_runtime = gum_script_bundle_new (isolate, gum_script_runtime_sources);
  debug_runtime = gum_script_bundle_new (isolate, gum_script_debug_sources);
}

void
GumScriptPlatform::OnFatalError (const char * location,
                                 const char * message)
{
  g_log ("V8", G_LOG_LEVEL_ERROR, "%s: %s", location, message);
}

GumScriptPlatform::~GumScriptPlatform ()
{
  {
    Locker locker (isolate);
    Isolate::Scope isolate_scope (isolate);
    HandleScope handle_scope (isolate);

    gum_script_bundle_free (debug_runtime);
    gum_script_bundle_free (user_runtime);
  }

  isolate->Dispose ();

  V8::Dispose ();
  V8::ShutdownPlatform ();

  g_object_unref (scheduler);
}

void
GumScriptPlatform::CallOnBackgroundThread (Task * task,
                                           ExpectedRuntime expected_runtime)
{
  (void) expected_runtime;

  gum_script_scheduler_push_job_on_thread_pool (scheduler,
      (GumScriptJobFunc) PerformTask, task, (GDestroyNotify) DisposeTask, NULL);
}

void
GumScriptPlatform::CallOnForegroundThread (Isolate * isolate,
                                           Task * task)
{
  (void) isolate;

  gum_script_scheduler_push_job_on_v8_thread (scheduler, G_PRIORITY_HIGH,
      (GumScriptJobFunc) PerformTask, task, (GDestroyNotify) DisposeTask, NULL);
}

double
GumScriptPlatform::MonotonicallyIncreasingTime ()
{
  gint64 delta = g_get_monotonic_time () - start_time;
  return ((double) (delta / G_GINT64_CONSTANT (1000))) / 1000.0;
}

void
GumScriptPlatform::PerformTask (Task * task)
{
  task->Run ();
}

void
GumScriptPlatform::DisposeTask (Task * task)
{
  delete task;
}

