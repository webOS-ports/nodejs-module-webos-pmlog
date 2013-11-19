/* @@@LICENSE
*
*      Copyright (c) 2010-2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include <node.h>
#include <iostream>
#include <syslog.h>
#include <v8.h>

#if HAS_PMLOGLIB
#include <PmLogLib.h>
#endif

#include "pmloglib.js.h"

using namespace v8;
using namespace std;

#if !HAS_PMLOGLIB
void LogString(int level, const char* label, const char *msgId, const char* stringToLog)
{
    static bool sysLogOpened = false;
    if (!sysLogOpened) {
        openlog(label, 0, 0);
        sysLogOpened = true;
    }
    syslog(level, "%s", stringToLog);
    if (level == LOG_ERR) {
        cerr << stringToLog << endl;
    } else {
        cout << stringToLog << endl;
    }
}
static void LogKeyValueString(int level, const char *label, const char *msgId, const char *keyValues, const char *freeText)
{
    static bool sysLogOpened = false;
    if (!sysLogOpened) {
        openlog(label, 0, 0);
        sysLogOpened = true;
    }
    syslog(level, "%s", freeText);
    if (level == LOG_ERR) {
        cerr << msgId << " " << keyValues << " " << freeText << endl;
    } else {
        cout << msgId << " " << keyValues << " " << freeText << endl;        
    }
}
#else
static void LogString(int level, const char* label, const char *msgId, const char* stringToLog)
{
        PmLogContext jsContext;
        PmLogGetContext(label, &jsContext);

	switch(level) {
	default:
		PmLogPrintInfo(jsContext, "%s", stringToLog);
		break;
	case LOG_WARNING:
		PmLogPrintWarning(jsContext, "%s", stringToLog);
		break;
	case LOG_ERR:
		PmLogPrintError(jsContext, "%s", stringToLog);
		break;
	}
	cerr << stringToLog << endl;
}

static void LogKeyValueString(int level, const char *label, const char *msgId, const char *keyValues, const char *freeText)
{
        PmLogContext jsContext;
        PmLogGetContext(label, &jsContext);
        PmLogString(jsContext, level, msgId, keyValues, freeText);
        cerr << msgId << " " << keyValues << " " << freeText << endl;
}

#endif

static Handle<Value> LogWrapper(const Arguments& args)
{
    if (args.Length() != 4) {
        return ThrowException(v8::Exception::Error(
                                  v8::String::New("Invalid number of parameters, 3 expected.")));
    }
    
    String::Utf8Value label(args[0]);
    int logLevel = args[1]->IntegerValue();
    String::Utf8Value msgId(args[2]);
    String::Utf8Value stringToLog(args[3]);
    LogString(logLevel, *label, *msgId, *stringToLog);
    return args[2];
}

static Handle<Value> LogKeyValueWrapper(const Arguments& args)
{   
    if (args.Length() < 2) {
        return ThrowException(v8::Exception::Error(
                                  v8::String::New("Minimum 2 parameters expected")));
    }

    if (args.Length() > 5) {
        return ThrowException(v8::Exception::Error(
                                  v8::String::New("Not more than 5 parameters expected")));
    }
    
    String::Utf8Value label(args[0]);
    int logLevel = args[1]->IntegerValue();
    const char *mid = NULL;
    const char *kv = NULL;
    const char *ft = NULL;

    if (!args[1]->IsNumber()) {
        return ThrowException(v8::Exception::Error(
                                v8::String::New("Logging level must be an integer")));
    }

    if(logLevel != kPmLogLevel_Debug) {

	String::Utf8Value msgId(args[2]);
	String::Utf8Value keyValues(args[3]);
	String::Utf8Value freeText(args[4]);

        if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
            mid = *msgId;
        } else {
            return ThrowException(v8::Exception::Error(
                                  v8::String::New("msgId is required for info and higher log levels")));
        }
        if (!args[3]->IsNull() && !args[3]->IsUndefined()) {
            kv = *keyValues;
        }
        if (!args[4]->IsNull() && !args[4]->IsUndefined()) {
            ft = *freeText;
        }
	LogKeyValueString(logLevel, *label, mid, kv, ft);
	return args[4];
    }
    else {

	String::Utf8Value freeText(args[2]);
        if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
	    ft = *freeText;
	}
        LogKeyValueString(logLevel, *label, mid, kv, ft);
	return args[2];
    }
}

extern "C" void
init(Handle<Object> target)
{
    HandleScope scope;
    Local<FunctionTemplate> logFunction = FunctionTemplate::New(LogWrapper);
    target->Set(String::NewSymbol("_logString"), logFunction->GetFunction());
    Local<FunctionTemplate> logKeyValueFunction = FunctionTemplate::New(LogKeyValueWrapper);
    target->Set(String::NewSymbol("_logKeyValueString"), logKeyValueFunction->GetFunction());
    target->Set(String::NewSymbol("LOG_CRITICAL"), Integer::New(kPmLogLevel_Critical));
    target->Set(String::NewSymbol("LOG_ERR"), Integer::New(kPmLogLevel_Error));
    target->Set(String::NewSymbol("LOG_WARNING"), Integer::New(kPmLogLevel_Warning));
    target->Set(String::NewSymbol("LOG_INFO"), Integer::New(kPmLogLevel_Info));
    target->Set(String::NewSymbol("LOG_DEBUG"), Integer::New(kPmLogLevel_Debug));
    Local<String> scriptText = String::New((const char*)pmloglib_js, pmloglib_js_len);
    Local<Script> script = Script::New(scriptText, String::New("pmloglib.js"));
    if (!script.IsEmpty()) {
        Local<Value> v = script->Run();
        Local<Function> f = Local<Function>::Cast(v);
        Local<Context> current = Context::GetCurrent();
        Handle<Value> argv[1];
        argv[0] = target;
        f->Call(current->Global(), 1, &argv[0]);
    } else {
        cerr << "Script was empty." << endl;
    }
}

NODE_MODULE(pmloglib, init)

