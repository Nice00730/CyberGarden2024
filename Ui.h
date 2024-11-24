#include <AppCore/CAPI.h>
#include <AppCore/AppCore.h>
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <windows.h>
#include <thread>
#include "back.h"

using ultralight::JSObject;
using ultralight::JSArgs;
using ultralight::JSFunction;
using namespace ultralight;

///
///  Welcome to Sample 6!
///
///  In this sample we'll demonstrate how to set up a simple JavaScript app using only the C API.
///
///  We will bind a JavaScript function to a C callback and use it to display a welcome message
///  in our HTML view.
///
///  __About the C API__
///
///  The C API is useful for porting the library to other languages or using it in scenarios that
///  are unsuitable for C++.
///
///  Most of the C++ API functionality is currently available via the CAPI headers with the
///  exception of some of the Platform API.
///
///  JavaScriptCore's native API is already in C so we can use it directly.
///  
///  Both Ultralight and JavaScriptCore follow the same paradigm when it comes to
///  ownership/destruction: You should explicitly Destroy/Release anything you Create.
///

std::wstring openFileDialogue()
{
    OPENFILENAME ofn;       // common dialog box structure
    wchar_t szFile[260];       // buffer for file name
    HANDLE hf;              // file handle
    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Video\0*\0";//"All\0*.*\0Text\0*.TXT\0"
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    GetOpenFileName(&ofn);
    // Display the Open dialog box. 
    /*if (GetOpenFileName(&ofn) == TRUE)
    {
        std::wcout << std::wstring(szFile);
    }*/

    return std::wstring(szFile);
}

std::wstring saveFileDialogue()
{
    OPENFILENAME ofn;       // common dialog box structure
    wchar_t szFile[260];       // buffer for file name
    HANDLE hf;              // file handle
    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Video\0*.\0";//"All\0*.*\0Text\0*.TXT\0"
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    GetSaveFileName(&ofn);
    // Display the Open dialog box. 
    /*if (GetOpenFileName(&ofn) == TRUE)
    {
        std::wcout << std::wstring(szFile);
    }*/

    return std::wstring(szFile);
}

/// Various globals
ULApp app = 0;
ULWindow window = 0;
ULOverlay overlay = 0;
ULView view = 0;

std::thread* matThread;
std::wstring openedFileName = L"in.mp4";
std::wstring savedFileName = L"out.mp4";

/// Forward declaration of our OnUpdate callback.
void OnUpdate(void* user_data);

/// Forward declaration of our OnClose callback.
void OnClose(void* user_data, ULWindow window);

/// Forward declaration of our OnResize callback.
void OnResize(void* user_data, ULWindow window, unsigned int width, unsigned int height);

/// Forward declaration of our OnDOMReady callback.
void OnDOMReady(void* user_data, ULView caller, unsigned long long frame_id,
    bool is_main_frame, ULString url);

///
/// We set up our application here.
///
void Init() 
{
    ///
    /// Create default settings/config
    ///
    ULSettings settings = ulCreateSettings();
    ulSettingsSetForceCPURenderer(settings, true);
    ULConfig config = ulCreateConfig();

    ///
    /// Create our App
    ///
    app = ulCreateApp(settings, config);

    ///
    /// Register a callback to handle app update logic.
    ///
    ulAppSetUpdateCallback(app, OnUpdate, 0);

    ///
    /// Done using settings/config, make sure to destroy anything we create
    ///
    ulDestroySettings(settings);
    ulDestroyConfig(config);

    ///
    /// Create our window, make it 500x500 with a titlebar and resize handles.
    ///
    window = ulCreateWindow(ulAppGetMainMonitor(app), 680, 480, false, ultralight::kWindowFlags_Titled);

    ///
    /// Set our window title.
    ///
    ulWindowSetTitle(window, "GUI");

    ///
    /// Register a callback to handle window close.
    /// 
    ulWindowSetCloseCallback(window, OnClose, 0);

    ///
    /// Register a callback to handle window resize.
    ///
    ulWindowSetResizeCallback(window, OnResize, 0);

    ///
    /// Create an overlay same size as our window at 0,0 (top-left) origin. Overlays also create an
    /// HTML view for us to display content in.
    ///
    /// **Note**:
    ///     Ownership of the view remains with the overlay since we don't explicitly create it.
    ///
    overlay = ulCreateOverlay(window, ulWindowGetWidth(window), ulWindowGetHeight(window), 0, 0);

    ///
    /// Get the overlay's view.
    ///
    view = ulOverlayGetView(overlay);

    ///
    /// Register a callback to handle our view's DOMReady event. We will use this event to setup any
    /// JavaScript <-> C bindings and initialize our page.
    ///
    ulViewSetDOMReadyCallback(view, OnDOMReady, 0);

    ///
    /// Load a file from the FileSystem.
    ///
    ///  **IMPORTANT**: Make sure `file:///` has three (3) forward slashes.
    ///
    ///  **Note**: You can configure the base path for the FileSystem in the Settings we passed to
    ///            ulCreateApp earlier.
    ///
    ULString url = ulCreateString("file:///app.html");
    ulViewLoadURL(view, url);
    ulDestroyString(url);
}

void hide();
void complete();
void postprocess();

///
/// This is called continuously from the app's main run loop. You should update any app logic
/// inside this callback.
///
void OnUpdate(void* user_data) 
{
    if (startRender == 3)
    {
        if (openedFileName != L"" && savedFileName != L"")
        {
            matThread = new std::thread(renderVid, openedFileName, savedFileName);
        }
        startRender++;
    }
    else if (!startRender && percent == 101)
    {
        complete();
        percent = -1;
    }
    else if (percent == 100)
    {
        postprocess();
    }
    else if (startRender < 3 && startRender > 0)
    {
        startRender++;
    }
    else if (startRender == 4 && percent != -1)
    {
        setprogress(std::to_string(percent).c_str());
    }
}

///
/// This is called when the window is closed.
///
void OnClose(void* user_data, ULWindow window) 
{
    ulAppQuit(app);
}

///
/// This is called whenever the window resizes. Width and height are in DPI-independent logical
/// coordinates (not pixels).
///
void OnResize(void* user_data, ULWindow window, unsigned int width, unsigned int height) {
    ulOverlayResize(overlay, width, height);
}

JSValueRef openFileUI(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) 
{
    /*const char* str = "document.getElementById('btn').innerHTML = 'Jerk Me!';";

    // Create our string of JavaScript
    JSStringRef script = JSStringCreateWithUTF8CString(str);

    // Execute it with JSEvaluateScript, ignoring other parameters for now
    JSEvaluateScript(ctx, script, 0, 0, 0, 0);   

    // Release our string (we only Release what we Create)
    JSStringRelease(script);*/

    if (!startRender)
    {
        hide();

        openedFileName = L"";

        openedFileName = openFileDialogue();

        std::wstring str = L"document.getElementById('open').innerHTML = 'Open: " + openedFileName + L"';";
        std::replace(str.begin(), str.end(), '\\', '/');
        JSChar* utfarr = new JSChar[str.length()];
        for (unsigned int i = 0; i < str.length(); i++)
        {
            utfarr[i] = str[i];
        }
        JSStringRef script = JSStringCreateWithCharacters(utfarr, str.length());
        JSEvaluateScript(ctx, script, 0, 0, 0, 0);
        JSStringRelease(script);
    }

    return JSValueMakeNull(ctx);
}

JSValueRef saveFileUI(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!startRender)
    {
        hide();

        savedFileName = L"";

        savedFileName = saveFileDialogue();

        std::wstring str = L"document.getElementById('save').innerHTML = 'Save: " + savedFileName + L"';";
        std::replace(str.begin(), str.end(), '\\', '/');
        JSChar* utfarr = new JSChar[str.length()];
        for (unsigned int i = 0; i < str.length(); i++)
        {
            utfarr[i] = str[i];
        }
        JSStringRef script = JSStringCreateWithCharacters(utfarr, str.length());
        JSEvaluateScript(ctx, script, 0, 0, 0, 0);
        JSStringRelease(script);
    }
    return JSValueMakeNull(ctx);
}

JSValueRef startUI(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!startRender && openedFileName != L"" && savedFileName != L"")
    {
        short int percent = -1;

        hide();

        std::wstring str = L"document.getElementById('progress_cont').className = 'progress-container';document.getElementById('progress_status').innerHTML = 'Preprocessing...';document.getElementById('progress').value = 0;";
        JSChar* utfarr = new JSChar[str.length()];
        for (unsigned int i = 0; i < str.length(); i++)
        {
            utfarr[i] = str[i];
        }
        JSStringRef script = JSStringCreateWithCharacters(utfarr, str.length());
        JSEvaluateScript(ctx, script, 0, 0, 0, 0);
        JSStringRelease(script);

        startRender = 1;
    }
    return JSValueMakeNull(ctx);
}

///
/// This is called when the page has finished parsing the document and is ready to execute scripts.
///
/// We will use this event to set up our JavaScript <-> C callback.
///
void OnDOMReady(void* user_data, ULView caller, unsigned long long frame_id, bool is_main_frame, ULString url) 
{
    ///
    /// Acquire the page's JavaScript execution context.
    ///
    /// This locks the JavaScript context so we can modify it safely on this thread, we need to
    /// unlock it when we're done via ulViewUnlockJSContext().
    ///
    JSContextRef ctx = ulViewLockJSContext(view);
    
    JSStringRef name = JSStringCreateWithUTF8CString("openFileUI");
    JSObjectRef func = JSObjectMakeFunctionWithCallback(ctx, name, openFileUI);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name, func, 0, 0);
    JSStringRelease(name);

    name = JSStringCreateWithUTF8CString("saveFileUI");
    func = JSObjectMakeFunctionWithCallback(ctx, name, saveFileUI);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name, func, 0, 0);
    JSStringRelease(name);

    name = JSStringCreateWithUTF8CString("startUI");
    func = JSObjectMakeFunctionWithCallback(ctx, name, startUI);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name, func, 0, 0);
    JSStringRelease(name);

    ///
    /// Unlock the JS context so other threads can modify JavaScript state.
    ///
    ulViewUnlockJSContext(view);
}

void hide()
{
    JSContextRef ctx = ulViewLockJSContext(view);
    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString("hide"));
    JSValueRef etc = JSEvaluateScript(ctx, str.get(), 0, 0, 0, 0);
    if (JSValueIsObject(ctx, etc))
    {
        JSObjectRef funcObj = JSValueToObject(ctx, etc, 0);
        if (funcObj && JSObjectIsFunction(ctx, funcObj))
        {
            JSObjectCallAsFunction(ctx, funcObj, 0, 0, 0, 0);
        }
    }
    ulViewUnlockJSContext(view);

    return;
}

void postprocess()
{
    JSContextRef ctx = ulViewLockJSContext(view);
    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString("postprocess"));
    JSValueRef etc = JSEvaluateScript(ctx, str.get(), 0, 0, 0, 0);
    if (JSValueIsObject(ctx, etc))
    {
        JSObjectRef funcObj = JSValueToObject(ctx, etc, 0);
        if (funcObj && JSObjectIsFunction(ctx, funcObj))
        {
            JSObjectCallAsFunction(ctx, funcObj, 0, 0, 0, 0);
        }
    }
    ulViewUnlockJSContext(view);

    return;
}

void complete()
{
    JSContextRef ctx = ulViewLockJSContext(view);
    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString("complete"));
    JSValueRef etc = JSEvaluateScript(ctx, str.get(), 0, 0, 0, 0);
    if (JSValueIsObject(ctx, etc))
    {
        JSObjectRef funcObj = JSValueToObject(ctx, etc, 0);
        if (funcObj && JSObjectIsFunction(ctx, funcObj))
        {
            JSObjectCallAsFunction(ctx, funcObj, 0, 0, 0, 0);
        }
    }
    ulViewUnlockJSContext(view);

    return;
}

void setprogress(const char* inp)
{
    JSContextRef ctx = ulViewLockJSContext(view);
    JSRetainPtr<JSStringRef> str = adopt(JSStringCreateWithUTF8CString("progressBarProg"));
    JSValueRef etc = JSEvaluateScript(ctx, str.get(), 0, 0, 0, 0);
    if (JSValueIsObject(ctx, etc))
    {
        JSObjectRef funcObj = JSValueToObject(ctx, etc, 0);
        if (funcObj && JSObjectIsFunction(ctx, funcObj))
        {
            JSRetainPtr<JSStringRef> msg = adopt(JSStringCreateWithUTF8CString(inp));
            const JSValueRef args[] = { JSValueMakeString(ctx, msg.get()) };
            size_t num_args = sizeof(args) / sizeof(JSValueRef*);
            JSObjectCallAsFunction(ctx, funcObj, 0, num_args, args, 0);
        }
    }
    ulViewUnlockJSContext(view);

    return;
}

///
/// We tear down our application here.
///
void Shutdown() 
{
    ///
    /// Explicitly destroy everything we created in Init().
    ///
    ulDestroyOverlay(overlay);
    ulDestroyWindow(window);
    ulDestroyApp(app);
}

int main_ui() 
{
    ///
    /// Initialize the app.
    ///
    Init();

    ///
    /// Run the app until the window is closed. (This is a modal operation)
    ///
    ulAppRun(app);

    ///
    /// Shutdown the app.
    ///
    Shutdown();

    return 0;
}
