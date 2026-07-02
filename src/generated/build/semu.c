#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/types.h>

static inline char* __btrc_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (!copy) { fprintf(stderr, "btrc: out of memory (strdup %zu bytes)\n", len); exit(1); }
    memcpy(copy, s, len);
    return copy;
}

static inline void* __btrc_safe_realloc(void* ptr, size_t size) {
    void* result = realloc(ptr, size);
    if (!result && size > 0) { fprintf(stderr, "btrc: out of memory (realloc %zu bytes)\n", size); exit(1); }
    return result;
}

static inline void* __btrc_safe_calloc(size_t count, size_t size) {
    void* result = calloc(count, size);
    if (!result && count > 0) { fprintf(stderr, "btrc: out of memory (calloc %zu bytes)\n", count * size); exit(1); }
    return result;
}

static inline int __btrc_div_int(int a, int b) {
    if (b == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
    return a / b;
}

static inline double __btrc_div_double(double a, double b) {
    if (b == 0.0) { fprintf(stderr, "Division by zero\n"); exit(1); }
    return a / b;
}

/* btrc string temp pool (dynamic) */
static int __btrc_str_pool_cap = 256;
static char** __btrc_str_pool = NULL;
static int __btrc_str_pool_top = 0;

static inline char* __btrc_str_track(char* s) {
    if (!__btrc_str_pool) {
        __btrc_str_pool = (char**)malloc(sizeof(char*) * __btrc_str_pool_cap);
    }
    if (__btrc_str_pool_top >= __btrc_str_pool_cap) {
        __btrc_str_pool_cap *= 2;
        __btrc_str_pool = (char**)realloc(__btrc_str_pool, sizeof(char*) * __btrc_str_pool_cap);
        if (!__btrc_str_pool) { fprintf(stderr, "btrc: string pool OOM\n"); exit(1); }
    }
    __btrc_str_pool[__btrc_str_pool_top++] = s;
    return s;
}

static inline char* __btrc_substring(const char* s, int start, int len) {
    if (!s) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    int slen = (int)strlen(s);
    if (start < 0) start = 0;
    if (start > slen) start = slen;
    if (start + len > slen) len = slen - start;
    if (len < 0) len = 0;
    char* result = (char*)malloc(len + 1);
    strncpy(result, s + start, len);
    result[len] = '\0';
    return result;
}

static inline char* __btrc_trim(const char* s) {
    if (!s) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') { char* r = (char*)malloc(1); r[0]='\0'; return r; }
    const char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    int len = (int)(end - s + 1);
    char* result = (char*)malloc(len + 1);
    strncpy(result, s, len);
    result[len] = '\0';
    return result;
}

static inline char* __btrc_toUpper(const char* s) {
    if (!s) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    int len = (int)strlen(s);
    char* result = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++) result[i] = (char)toupper((unsigned char)s[i]);
    result[len] = '\0';
    return result;
}

static inline char* __btrc_toLower(const char* s) {
    if (!s) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    int len = (int)strlen(s);
    char* result = (char*)malloc(len + 1);
    for (int i = 0; i < len; i++) result[i] = (char)tolower((unsigned char)s[i]);
    result[len] = '\0';
    return result;
}

static inline char* __btrc_replace(const char* s, const char* old, const char* rep) {
    if (!s) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    if (!old || !old[0]) return __btrc_strdup(s);
    if (!rep) rep = "";
    int slen = (int)strlen(s);
    int oldlen = (int)strlen(old);
    int replen = (int)strlen(rep);
    int cap = slen * 2 + 1;
    char* result = (char*)malloc(cap);
    int rlen = 0, i = 0;
    while (i < slen) {
        if (i + oldlen <= slen && strncmp(s + i, old, oldlen) == 0) {
            while (rlen + replen >= cap) { cap *= 2; result = (char*)__btrc_safe_realloc(result, cap); }
            memcpy(result + rlen, rep, replen);
            rlen += replen; i += oldlen;
        } else {
            if (rlen + 1 >= cap) { cap *= 2; result = (char*)__btrc_safe_realloc(result, cap); }
            result[rlen++] = s[i++];
        }
    }
    result[rlen] = '\0';
    return result;
}

static inline char* __btrc_strcat(const char* a, const char* b) {
    if (!a && !b) { char* r = (char*)malloc(1); r[0] = '\0'; return r; }
    if (!a) return __btrc_strdup(b);
    if (!b) return __btrc_strdup(a);
    int la = (int)strlen(a), lb = (int)strlen(b);
    char* r = (char*)malloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

static inline char __btrc_charAt(const char* s, int idx) {
    if (!s) { fprintf(stderr, "String index on NULL\n"); exit(1); }
    int len = (int)strlen(s);
    if (idx < 0 || idx >= len) { fprintf(stderr, "String index out of bounds: %d (length %d)\n", idx, len); exit(1); }
    return s[idx];
}

static inline int __btrc_indexOf(const char* s, const char* sub) {
    if (!s || !sub) return -1;
    char* p = strstr(s, sub);
    return p ? (int)(p - s) : -1;
}

static inline bool __btrc_isEmpty(const char* s) {
    if (!s) return true;
    return s[0] == '\0';
}

static inline bool __btrc_startsWith(const char* s, const char* prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static inline bool __btrc_endsWith(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    int slen = (int)strlen(s);
    int suflen = (int)strlen(suffix);
    if (suflen > slen) return false;
    return strcmp(s + slen - suflen, suffix) == 0;
}

static inline bool __btrc_strContains(const char* s, const char* sub) {
    if (!s || !sub) return false;
    return strstr(s, sub) != NULL;
}

/* btrc try/catch runtime (dynamic) */
static __thread int __btrc_try_cap = 16;
static __thread jmp_buf* __btrc_try_stack = NULL;
static __thread volatile int __btrc_try_top = -1;
static __thread char __btrc_error_msg[1024] = "";

/* Cleanup stack: tracks heap resources to free on exception */
typedef void (*__btrc_cleanup_fn)(void*);
typedef struct { void** ptr_ref; __btrc_cleanup_fn fn; void* visit; int try_level; } __btrc_cleanup_entry;
static __thread int __btrc_cleanup_cap = 64;
static __thread __btrc_cleanup_entry* __btrc_cleanup_stack = NULL;
static __thread int __btrc_cleanup_top = -1;

static inline unsigned int __btrc_hash_str(const char* s) {
    unsigned int h = 5381;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s++; }
    return h;
}

/* ARC cascade-destroy tracking: avoid reading freed memory */
static int __btrc_tracking = 0;
static void** __btrc_destroyed = NULL;
static int __btrc_destroyed_count = 0;
static int __btrc_destroyed_cap = 0;
static void __btrc_mark_destroyed(void* ptr) {
    if (__btrc_destroyed_count >= __btrc_destroyed_cap) {
        __btrc_destroyed_cap = __btrc_destroyed_cap ? __btrc_destroyed_cap * 2 : 256;
        __btrc_destroyed = (void**)__btrc_safe_realloc(__btrc_destroyed, sizeof(void*) * __btrc_destroyed_cap);
    }
    __btrc_destroyed[__btrc_destroyed_count++] = ptr;
}
static int __btrc_is_destroyed(void* ptr) {
    for (int i = 0; i < __btrc_destroyed_count; i++)
        if (__btrc_destroyed[i] == ptr) return 1;
    return 0;
}

/* ARC cycle detection: suspect buffer */
static void** __btrc_suspects = NULL;
static int __btrc_suspect_count = 0;
static int __btrc_suspect_cap = 0;
typedef void (*__btrc_visit_fn)(void*, void (*)(void**));
typedef void (*__btrc_destroy_fn)(void*);
static __btrc_visit_fn* __btrc_visit_table = NULL;
static __btrc_destroy_fn* __btrc_destroy_table = NULL;
static void __btrc_suspect(void* obj, __btrc_visit_fn visit,
                           __btrc_destroy_fn destroy) {
    if (__btrc_suspect_count >= __btrc_suspect_cap) {
        __btrc_suspect_cap = __btrc_suspect_cap ? __btrc_suspect_cap * 2 : 256;
        __btrc_suspects = (void**)__btrc_safe_realloc(__btrc_suspects, sizeof(void*) * __btrc_suspect_cap);
        __btrc_visit_table = (__btrc_visit_fn*)__btrc_safe_realloc(__btrc_visit_table, sizeof(__btrc_visit_fn) * __btrc_suspect_cap);
        __btrc_destroy_table = (__btrc_destroy_fn*)__btrc_safe_realloc(__btrc_destroy_table, sizeof(__btrc_destroy_fn) * __btrc_suspect_cap);
    }
    __btrc_suspects[__btrc_suspect_count] = obj;
    __btrc_visit_table[__btrc_suspect_count] = visit;
    __btrc_destroy_table[__btrc_suspect_count] = destroy;
    __btrc_suspect_count++;
}

/* ARC cycle collector: trial deletion with cycle-breaking */
static void __btrc_trial_dec(void** fp) {
    if (*fp) { int* rc = (int*)*fp; (*rc)--; }
}
static void __btrc_trial_restore(void** fp) {
    if (*fp) { int* rc = (int*)*fp; (*rc)++; }
}
static void __btrc_clear_field(void** fp) {
    *fp = NULL;
}
static void __btrc_collect_cycles(void) {
    int n = __btrc_suspect_count;
    if (n == 0) return;
    /* Phase 1: trial decrement all suspects' cyclable children */
    for (int i = 0; i < n; i++) {
        if (__btrc_suspects[i] && __btrc_visit_table[i])
            __btrc_visit_table[i](__btrc_suspects[i], __btrc_trial_dec);
    }
    /* Phase 2: break cycles by NULLing cyclable fields, then destroy */
    for (int i = 0; i < n; i++) {
        void* obj = __btrc_suspects[i];
        if (!obj) continue;
        int rc = *(int*)obj;
        if (rc <= 0) {
            /* NULL cyclable fields to prevent cascade recursion */
            if (__btrc_visit_table[i])
                __btrc_visit_table[i](obj, __btrc_clear_field);
            /* Restore rc for destroy to work, then destroy */
            *(int*)obj = 1;
            if (__btrc_destroy_table[i])
                __btrc_destroy_table[i](obj);
            __btrc_suspects[i] = NULL;
        } else {
            /* Restore trial decrements for still-live objects */
            if (__btrc_visit_table[i])
                __btrc_visit_table[i](obj, __btrc_trial_restore);
        }
    }
    __btrc_suspect_count = 0;
}

/* Exception cleanup. Non-cyclable managed locals keep the original
 * unconditional-destroy semantics. Cyclable ones (those with a
 * registered visitor) go through the same phased release +
 * trial-deletion cycle collection as a normal scope exit, so
 * unwinding a scope that holds a reference cycle reclaims it without
 * a use-after-free: a plain per-node destroy would cascade-free one
 * node then read freed memory through the other node's back-edge.
 * __btrc_tracking + the destroyed log guard every cross-entry read. */
static inline void __btrc_run_cleanups(int level) {
    int base = __btrc_cleanup_top;
    while (base >= 0 && __btrc_cleanup_stack[base].try_level >= level) { base--; }
    base++;
    if (base > __btrc_cleanup_top) { return; }
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    /* Non-cyclable entries: unconditional destroy (original behavior),
     * but skip any object already cascade-freed by an earlier destroy. */
    for (int i = __btrc_cleanup_top; i >= base; i--) {
        __btrc_cleanup_entry* e = &__btrc_cleanup_stack[i];
        if (e->visit) { continue; }
        if (e->fn && e->ptr_ref && *e->ptr_ref && !__btrc_is_destroyed(*e->ptr_ref)) {
            e->fn(*e->ptr_ref);
            *e->ptr_ref = NULL;
        }
    }
    /* Cyclable entries: phased release + cycle collection.
     * Phase 1 -- drop each managed local's own reference (rc--). */
    for (int i = __btrc_cleanup_top; i >= base; i--) {
        __btrc_cleanup_entry* e = &__btrc_cleanup_stack[i];
        if (e->visit && e->ptr_ref && *e->ptr_ref) { (*(int*)*e->ptr_ref)--; }
    }
    /* Phase 2 -- destroy objects whose rc hit zero (destroyed-log guarded). */
    for (int i = __btrc_cleanup_top; i >= base; i--) {
        __btrc_cleanup_entry* e = &__btrc_cleanup_stack[i];
        if (e->visit && e->fn && e->ptr_ref && *e->ptr_ref
                && !__btrc_is_destroyed(*e->ptr_ref)
                && *(int*)*e->ptr_ref <= 0) {
            e->fn(*e->ptr_ref);
            *e->ptr_ref = NULL;
        }
    }
    /* Phase 3 -- still-referenced survivors are cycle suspects. */
    for (int i = __btrc_cleanup_top; i >= base; i--) {
        __btrc_cleanup_entry* e = &__btrc_cleanup_stack[i];
        if (e->visit && e->fn && e->ptr_ref && *e->ptr_ref
                && !__btrc_is_destroyed(*e->ptr_ref)
                && *(int*)*e->ptr_ref > 0) {
            __btrc_suspect(*e->ptr_ref, (__btrc_visit_fn)e->visit,
                           (__btrc_destroy_fn)e->fn);
        }
    }
    /* Phase 4 -- trial-deletion cycle collection over the suspects. */
    if (__btrc_suspect_count > 0) { __btrc_collect_cycles(); }
    __btrc_tracking = 0;
    __btrc_cleanup_top = base - 1;
}

static inline void __btrc_throw(const char* msg) {
    if (__btrc_try_top < 0) {
        fprintf(stderr, "Unhandled exception: %s\n", msg);
        exit(1);
    }
    strncpy(__btrc_error_msg, msg, 1023);
    __btrc_error_msg[1023] = '\0';
    __btrc_run_cleanups(__btrc_try_top);
    longjmp(__btrc_try_stack[__btrc_try_top--], 1);
}

typedef struct CliArgs CliArgs;
void CliArgs_destroy(CliArgs* self);
typedef struct File File;
void File_destroy(File* self);
typedef struct Path Path;
typedef struct ProcessStatus ProcessStatus;
typedef struct CommandOutput CommandOutput;
typedef struct ExecResult ExecResult;
typedef struct Command Command;
typedef struct UnixShell UnixShell;
void UnixShell_destroy(UnixShell* self);
typedef struct FileStatus FileStatus;
void FileStatus_destroy(FileStatus* self);
typedef struct Directory Directory;
typedef struct ParseLog ParseLog;
void ParseLog_destroy(ParseLog* self);
typedef struct JsonValue JsonValue;
void JsonValue_destroy(JsonValue* self);
typedef struct JsonParser JsonParser;
void JsonParser_destroy(JsonParser* self);
typedef struct NormalizedRect NormalizedRect;
void NormalizedRect_destroy(NormalizedRect* self);
typedef struct PixelSize PixelSize;
void PixelSize_destroy(PixelSize* self);
typedef struct AspectRatio AspectRatio;
void AspectRatio_destroy(AspectRatio* self);
typedef struct RgbColor RgbColor;
void RgbColor_destroy(RgbColor* self);
typedef struct SystemScreen SystemScreen;
void SystemScreen_destroy(SystemScreen* self);
typedef struct WidescreenPolicy WidescreenPolicy;
void WidescreenPolicy_destroy(WidescreenPolicy* self);
typedef struct DisplayPolicy DisplayPolicy;
void DisplayPolicy_destroy(DisplayPolicy* self);
typedef struct RenderPolicy RenderPolicy;
void RenderPolicy_destroy(RenderPolicy* self);
typedef struct EmulatorBinding EmulatorBinding;
void EmulatorBinding_destroy(EmulatorBinding* self);
typedef struct BiosFile BiosFile;
void BiosFile_destroy(BiosFile* self);
typedef struct BiosRequirement BiosRequirement;
void BiosRequirement_destroy(BiosRequirement* self);
typedef struct ScreenHole ScreenHole;
void ScreenHole_destroy(ScreenHole* self);
typedef struct BezelVariant BezelVariant;
void BezelVariant_destroy(BezelVariant* self);
typedef struct BezelCollection BezelCollection;
void BezelCollection_destroy(BezelCollection* self);
typedef struct ShaderCollection ShaderCollection;
void ShaderCollection_destroy(ShaderCollection* self);
typedef struct SystemDefinition SystemDefinition;
void SystemDefinition_destroy(SystemDefinition* self);
typedef struct SandboxFilesystem SandboxFilesystem;
void SandboxFilesystem_destroy(SandboxFilesystem* self);
typedef struct SandboxPolicy SandboxPolicy;
void SandboxPolicy_destroy(SandboxPolicy* self);
typedef struct GraphicsPolicy GraphicsPolicy;
void GraphicsPolicy_destroy(GraphicsPolicy* self);
typedef struct AspectProbe AspectProbe;
void AspectProbe_destroy(AspectProbe* self);
typedef struct StateSeed StateSeed;
void StateSeed_destroy(StateSeed* self);
typedef struct StatePolicy StatePolicy;
void StatePolicy_destroy(StatePolicy* self);
typedef struct FirmwarePolicy FirmwarePolicy;
void FirmwarePolicy_destroy(FirmwarePolicy* self);
typedef struct SessionOverride SessionOverride;
void SessionOverride_destroy(SessionOverride* self);
typedef struct EmulatorPlatform EmulatorPlatform;
void EmulatorPlatform_destroy(EmulatorPlatform* self);
typedef struct EmulatorInput EmulatorInput;
void EmulatorInput_destroy(EmulatorInput* self);
typedef struct EmulatorDefinition EmulatorDefinition;
void EmulatorDefinition_destroy(EmulatorDefinition* self);
typedef struct SemuContracts SemuContracts;
void SemuContracts_destroy(SemuContracts* self);
typedef struct KeymapAction KeymapAction;
void KeymapAction_destroy(KeymapAction* self);
typedef struct KeymapBinding KeymapBinding;
void KeymapBinding_destroy(KeymapBinding* self);
typedef struct Keymap Keymap;
void Keymap_destroy(Keymap* self);
typedef struct SteamInputTarget SteamInputTarget;
void SteamInputTarget_destroy(SteamInputTarget* self);
typedef struct RetroArchConfiguration RetroArchConfiguration;
void RetroArchConfiguration_destroy(RetroArchConfiguration* self);
typedef struct RetroArchEmulator RetroArchEmulator;
typedef struct DolphinEmulator DolphinEmulator;
typedef struct TemplateContext TemplateContext;
void TemplateContext_destroy(TemplateContext* self);
typedef struct Pcsx2Emulator Pcsx2Emulator;
typedef struct CemuEmulator CemuEmulator;
typedef struct RyujinxEmulator RyujinxEmulator;
typedef struct PpssppEmulator PpssppEmulator;
typedef struct FlycastEmulator FlycastEmulator;
typedef struct AzaharEmulator AzaharEmulator;
typedef struct MelonDsEmulator MelonDsEmulator;
typedef struct SystemEmitter SystemEmitter;
typedef struct LaunchPlan LaunchPlan;
void LaunchPlan_destroy(LaunchPlan* self);
typedef struct EmulatorExecution EmulatorExecution;
typedef struct SteamdeckHotkeyAssignments SteamdeckHotkeyAssignments;
void SteamdeckHotkeyAssignments_destroy(SteamdeckHotkeyAssignments* self);
typedef struct ResolvedBezel ResolvedBezel;
void ResolvedBezel_destroy(ResolvedBezel* self);
typedef struct ShaderSelector ShaderSelector;
typedef struct TapEnvironment TapEnvironment;
void TapEnvironment_destroy(TapEnvironment* self);
typedef struct GithubPin GithubPin;
void GithubPin_destroy(GithubPin* self);
typedef struct NixPackage NixPackage;
typedef struct btrc_Vector_string btrc_Vector_string;
typedef struct btrc_Vector_bool btrc_Vector_bool;
typedef struct btrc_Vector_JsonValue_p1 btrc_Vector_JsonValue_p1;
typedef struct btrc_Vector_SystemScreen_p1 btrc_Vector_SystemScreen_p1;
typedef struct btrc_Vector_BiosFile_p1 btrc_Vector_BiosFile_p1;
typedef struct btrc_Vector_ScreenHole_p1 btrc_Vector_ScreenHole_p1;
typedef struct btrc_Vector_BezelVariant_p1 btrc_Vector_BezelVariant_p1;
typedef struct btrc_Vector_EmulatorBinding_p1 btrc_Vector_EmulatorBinding_p1;
typedef struct btrc_Vector_SandboxFilesystem_p1 btrc_Vector_SandboxFilesystem_p1;
typedef struct btrc_Vector_StateSeed_p1 btrc_Vector_StateSeed_p1;
typedef struct btrc_Vector_SessionOverride_p1 btrc_Vector_SessionOverride_p1;
typedef struct btrc_Vector_EmulatorPlatform_p1 btrc_Vector_EmulatorPlatform_p1;
typedef struct btrc_Vector_SystemDefinition_p1 btrc_Vector_SystemDefinition_p1;
typedef struct btrc_Vector_EmulatorDefinition_p1 btrc_Vector_EmulatorDefinition_p1;
typedef struct btrc_Vector_KeymapAction_p1 btrc_Vector_KeymapAction_p1;
typedef struct btrc_Vector_KeymapBinding_p1 btrc_Vector_KeymapBinding_p1;
typedef struct btrc_Vector_NormalizedRect_p1 btrc_Vector_NormalizedRect_p1;
typedef struct btrc_Vector_GithubPin_p1 btrc_Vector_GithubPin_p1;
char* Strings_copy(char* s);
char* Strings_replace(char* s, char* old, char* replacement);
btrc_Vector_string* Strings_split(char* s, char* delim);
int Strings_find(char* s, char* sub, int start);
void CliArgs_init(CliArgs* self, int argc, char** argv);
CliArgs* CliArgs_new(int argc, char** argv);
int CliArgs_count(CliArgs* self);
char* CliArgs_get(CliArgs* self, int index);
char* CliArgs_command(CliArgs* self);
char* CliArgs_valueAfter(CliArgs* self, char* flag, char* fallback);
void File_init(File* self, char* path, char* mode);
File* File_new(char* path, char* mode);
bool File_ok(File* self);
char* File_read(File* self);
bool File_write(File* self, char* text);
void File_close(File* self);
char* Path_readAll(char* path);
bool Path_writeAll(char* path, char* content);
int UnixPlatform_pid(void);
int Platform_pid(void);
char* Environment_get(char* name, char* fallback);
int mkstemp(char* templatePath);
void ProcessStatus_init(ProcessStatus* self, int raw);
ProcessStatus* ProcessStatus_new(int raw);
int ProcessStatus_code(ProcessStatus* self);
ProcessStatus* UnixProcess_system(char* command);
bool ShellWords_isEnvNameStart(char c);
bool ShellWords_isEnvNameChar(char c);
bool ShellWords_isEnvName(char* name);
bool ShellWords_isSafeArgChar(char c);
bool ShellWords_isSafeArg(char* raw);
char* ShellWords_quote(char* raw);
char* ShellWords_redact(char* text, char* sensitive);
char* ShellWords_envAssignment(char* item);
char* CommandOutput_collect(void);
char* CommandOutput_stream(void);
char* CommandOutput_combine(void);
char* CommandOutput_suppress(void);
bool CommandOutput_valid(char* mode);
btrc_Vector_string* CommandEnvironment_empty(void);
void ExecResult_init(ExecResult* self, int code, char* out, char* err, char* command);
ExecResult* ExecResult_new(int code, char* out, char* err, char* command);
void UnixShell_init(UnixShell* self);
UnixShell* UnixShell_new(void);
char* UnixShell_redactText(char* text, char* sensitive);
void UnixShell_logError(char* message);
char* UnixShell_tempPath(UnixShell* self, char* name);
char* UnixShell_renderEnv(UnixShell* self, btrc_Vector_string* env);
char* UnixShell_withContext(UnixShell* self, char* command, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot);
char* UnixShell_withRedirections(UnixShell* self, char* rendered, char* stdout, char* stderr, char* outFile, char* errFile, char* stdinFile);
ExecResult* UnixShell_run(UnixShell* self, char* command, char* stdout, char* stderr, bool logCommand, bool logFailure, bool throwOnFailure, char* redactSubstring, char* stdin, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot);
void FileStatus_init(FileStatus* self, char* path);
FileStatus* FileStatus_new(char* path);
bool FileStatus_exists(FileStatus* self);
bool FileStatus_isDir(FileStatus* self);
bool FileStatus_isFile(FileStatus* self);
void Directory_init(Directory* self, char* path);
Directory* Directory_new(char* path);
btrc_Vector_string* Directory_entries(Directory* self);
int UnixFileSystem_chmodPath(char* path, int mode);
int UnixFileSystem_mkdirPath(char* path, int mode);
int UnixFileSystem_mkdirOne(char* path, int mode);
int UnixFileSystem_mkdirp(char* path);
char* UnixFileSystem_currentDirectory(void);
char* PathTools_basename(char* path);
char* PathTools_dirname(char* path);
char* PathTools_join(char* left, char* right);
bool FileSystem_exists(char* path);
bool FileSystem_isDir(char* path);
bool FileSystem_isFile(char* path);
int FileSystem_chmod(char* path, int mode);
char* FileSystem_currentDirectory(void);
void ParseLog_init(ParseLog* self);
ParseLog* ParseLog_new(void);
void ParseLog_add(ParseLog* self, char* file, char* field, char* message);
bool ParseLog_ok(ParseLog* self);
void JsonValue_init(JsonValue* self);
JsonValue* JsonValue_new(void);
JsonValue* JsonValue_makeNull(void);
JsonValue* JsonValue_makeError(void);
JsonValue* JsonValue_makeBool(bool b);
JsonValue* JsonValue_makeInt(int n);
JsonValue* JsonValue_makeFloat(double d);
JsonValue* JsonValue_makeString(char* s);
JsonValue* JsonValue_makeArray(void);
JsonValue* JsonValue_makeObject(void);
void JsonValue_push(JsonValue* self, JsonValue* child);
void JsonValue_set(JsonValue* self, char* key, JsonValue* value);
bool JsonValue_isNull(JsonValue* self);
bool JsonValue_isError(JsonValue* self);
bool JsonValue_isBool(JsonValue* self);
bool JsonValue_isNumber(JsonValue* self);
bool JsonValue_isString(JsonValue* self);
bool JsonValue_isArray(JsonValue* self);
bool JsonValue_isObject(JsonValue* self);
bool JsonValue_isInt(JsonValue* self);
bool JsonValue_asBool(JsonValue* self);
int JsonValue_asInt(JsonValue* self);
double JsonValue_asFloat(JsonValue* self);
char* JsonValue_asString(JsonValue* self);
int JsonValue_size(JsonValue* self);
JsonValue* JsonValue_at(JsonValue* self, int i);
JsonValue* JsonValue_get(JsonValue* self, char* key);
bool JsonValue_has(JsonValue* self, char* key);
btrc_Vector_string* JsonValue_keys(JsonValue* self);
JsonValue* JsonValue_parse(char* text);
JsonValue* JsonValue_readFile(char* path);
void JsonParser_init(JsonParser* self, char* text);
JsonParser* JsonParser_new(char* text);
void JsonParser_skipWs(JsonParser* self);
JsonValue* JsonParser_parseValue(JsonParser* self);
JsonValue* JsonParser_parseObject(JsonParser* self);
JsonValue* JsonParser_parseArray(JsonParser* self);
JsonValue* JsonParser_parseStringValue(JsonParser* self);
char* JsonParser_readString(JsonParser* self);
int JsonParser_readHex4(JsonParser* self);
JsonValue* JsonParser_parseBool(JsonParser* self);
JsonValue* JsonParser_parseNull(JsonParser* self);
bool JsonParser_matchLiteral(JsonParser* self, char* word);
JsonValue* JsonParser_parseNumber(JsonParser* self);
double JsonNum_toDouble(char* token);
char* JsonNum_charFromCode(int code);
char* JsonNum_utf8(int cp);
void NormalizedRect_init(NormalizedRect* self);
NormalizedRect* NormalizedRect_new(void);
void PixelSize_init(PixelSize* self);
PixelSize* PixelSize_new(void);
void AspectRatio_init(AspectRatio* self);
AspectRatio* AspectRatio_new(void);
void RgbColor_init(RgbColor* self);
RgbColor* RgbColor_new(void);
char* JsonContract_fieldLabel(char* context, char* key);
void JsonContract_checkKeys(JsonValue* jsonObject, btrc_Vector_string* allowedKeys, char* file, char* context, ParseLog* log);
void JsonContract_checkSchemaVersion(JsonValue* root, int expectedVersion, char* file, ParseLog* log);
char* JsonContract_requiredString(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log);
char* JsonContract_optionalString(JsonValue* jsonObject, char* key, char* fallback, char* file, char* context, ParseLog* log);
bool JsonContract_requiredBool(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log);
bool JsonContract_optionalBool(JsonValue* jsonObject, char* key, bool fallback, char* file, char* context, ParseLog* log);
double JsonContract_requiredNumber(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log);
double JsonContract_optionalNumber(JsonValue* jsonObject, char* key, double fallback, char* file, char* context, ParseLog* log);
int JsonContract_requiredInt(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log);
void JsonContract_checkEnum(char* value, btrc_Vector_string* allowedValues, char* file, char* context, ParseLog* log);
btrc_Vector_string* JsonContract_stringArray(JsonValue* jsonObject, char* key, bool required, char* file, char* context, ParseLog* log);
void JsonContract_checkAssetPath(char* path, char* file, char* context, ParseLog* log);
NormalizedRect* JsonContract_rect(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
PixelSize* JsonContract_pixelSize(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
AspectRatio* JsonContract_aspectRatio(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
RgbColor* JsonContract_rgbColor(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void JsonContract_checkContainedRect(NormalizedRect* inner, NormalizedRect* outer, char* file, char* context, ParseLog* log);
bool SourceDirectories_nameGreater(char* left, char* right);
btrc_Vector_string* SourceDirectories_sortedChildDirectories(char* root);
void SystemScreen_init(SystemScreen* self);
SystemScreen* SystemScreen_new(void);
void WidescreenPolicy_init(WidescreenPolicy* self);
WidescreenPolicy* WidescreenPolicy_new(void);
void DisplayPolicy_init(DisplayPolicy* self);
DisplayPolicy* DisplayPolicy_new(void);
void DisplayPolicyParser_parseScreens(DisplayPolicy* displayPolicy, JsonValue* jsonValue, char* file, ParseLog* log);
void DisplayPolicyParser_parseWidescreen(DisplayPolicy* displayPolicy, JsonValue* jsonValue, char* file, ParseLog* log);
DisplayPolicy* DisplayPolicyParser_parse(JsonValue* jsonValue, char* file, ParseLog* log);
void RenderPolicy_init(RenderPolicy* self);
RenderPolicy* RenderPolicy_new(void);
RenderPolicy* RenderPolicyParser_parse(JsonValue* jsonValue, char* file, int screenCount, ParseLog* log);
void EmulatorBinding_init(EmulatorBinding* self);
EmulatorBinding* EmulatorBinding_new(void);
EmulatorBinding* EmulatorBindingParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void BiosFile_init(BiosFile* self);
BiosFile* BiosFile_new(void);
void BiosRequirement_init(BiosRequirement* self);
BiosRequirement* BiosRequirement_new(void);
BiosRequirement* BiosRequirementParser_parse(JsonValue* jsonValue, char* file, ParseLog* log);
void ScreenHole_init(ScreenHole* self);
ScreenHole* ScreenHole_new(void);
void BezelVariant_init(BezelVariant* self);
BezelVariant* BezelVariant_new(void);
void BezelCollection_init(BezelCollection* self);
BezelCollection* BezelCollection_new(void);
btrc_Vector_ScreenHole_p1* BezelCollectionParser_parseScreenHoles(JsonValue* jsonValue, btrc_Vector_string* screenIds, char* file, char* context, ParseLog* log);
BezelVariant* BezelCollectionParser_parseVariant(JsonValue* jsonValue, btrc_Vector_string* screenIds, char* file, char* context, ParseLog* log);
void BezelCollectionParser_parseVariants(BezelCollection* bezelCollection, JsonValue* root, btrc_Vector_string* screenIds, char* path, ParseLog* log);
BezelCollection* BezelCollectionParser_parseFile(char* path, btrc_Vector_string* screenIds, ParseLog* log);
void ShaderCollection_init(ShaderCollection* self);
ShaderCollection* ShaderCollection_new(void);
void ShaderCollectionParser_parseWidescreen(ShaderCollection* shaderCollection, JsonValue* root, char* path, ParseLog* log);
ShaderCollection* ShaderCollectionParser_parseFile(char* path, ParseLog* log);
void SystemDefinition_init(SystemDefinition* self);
SystemDefinition* SystemDefinition_new(void);
btrc_Vector_string* SystemDefinition_screenIds(SystemDefinition* self);
bool TemplateTokens_allowed(char* token, bool allowHostPortable);
void TemplateTokens_check(char* value, char* file, char* context, ParseLog* log, bool allowHostPortable);
void SandboxFilesystem_init(SandboxFilesystem* self);
SandboxFilesystem* SandboxFilesystem_new(void);
void SandboxPolicy_init(SandboxPolicy* self);
SandboxPolicy* SandboxPolicy_new(void);
void SandboxPolicyParser_parseFilesystems(SandboxPolicy* sandbox, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void SandboxPolicyParser_parseSymlinks(SandboxPolicy* sandbox, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
SandboxPolicy* SandboxPolicyParser_parse(JsonValue* jsonValue, char* operatingSystemName, char* file, char* context, ParseLog* log);
void GraphicsPolicy_init(GraphicsPolicy* self);
GraphicsPolicy* GraphicsPolicy_new(void);
void GraphicsPolicyParser_parseConfigFileFields(GraphicsPolicy* graphics, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
GraphicsPolicy* GraphicsPolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void AspectProbe_init(AspectProbe* self);
AspectProbe* AspectProbe_new(void);
AspectProbe* AspectProbeParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void StateSeed_init(StateSeed* self);
StateSeed* StateSeed_new(void);
void StatePolicy_init(StatePolicy* self);
StatePolicy* StatePolicy_new(void);
void StatePolicyParser_parseSeeds(StatePolicy* state, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
StatePolicy* StatePolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void FirmwarePolicy_init(FirmwarePolicy* self);
FirmwarePolicy* FirmwarePolicy_new(void);
FirmwarePolicy* FirmwarePolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void SessionOverride_init(SessionOverride* self);
SessionOverride* SessionOverride_new(void);
void EmulatorPlatform_init(EmulatorPlatform* self);
EmulatorPlatform* EmulatorPlatform_new(void);
bool EmulatorPlatform_hasExtension(EmulatorPlatform* self);
SessionOverride* EmulatorSessions_effective(EmulatorPlatform* platform, char* session);
void EmulatorInput_init(EmulatorInput* self);
EmulatorInput* EmulatorInput_new(void);
void EmulatorDefinition_init(EmulatorDefinition* self);
EmulatorDefinition* EmulatorDefinition_new(void);
bool EmulatorDefinition_hasOperatingSystem(EmulatorDefinition* self, char* name);
EmulatorPlatform* EmulatorDefinition_platformFor(EmulatorDefinition* self, char* name);
void SemuContracts_init(SemuContracts* self);
SemuContracts* SemuContracts_new(void);
bool SemuContracts_hasSystem(SemuContracts* self, char* id);
bool SemuContracts_hasEmulator(SemuContracts* self, char* id);
void ContractValidation_checkUniqueIdentity(SemuContracts* contracts, ParseLog* log);
EmulatorDefinition* ContractValidation_findEmulator(SemuContracts* contracts, char* emulatorId);
void ContractValidation_checkBindings(SemuContracts* contracts, ParseLog* log);
void ContractValidation_checkDefaultSystems(SemuContracts* contracts, ParseLog* log);
void ContractValidation_checkWidescreenSubstitutions(SemuContracts* contracts, ParseLog* log);
bool ContractValidation_copyVariantInto(BezelCollection* bezels, char* variantId, BezelVariant* destination);
SystemDefinition* ContractValidation_findSystem(SemuContracts* contracts, char* systemId, SystemDefinition* fallback);
void ContractValidation_resolveVariantGeometry(SemuContracts* contracts, SystemDefinition* systemDefinition, BezelVariant* variant, ParseLog* log);
void ContractValidation_resolveGeometry(SemuContracts* contracts, ParseLog* log);
void ContractValidation_crossValidate(SemuContracts* contracts, ParseLog* log);
int ChordSyntax_lastPlusIndex(char* chord);
char* ChordSyntax_keyPart(char* chord);
char* ChordSyntax_modifierPart(char* chord);
void KeymapAction_init(KeymapAction* self);
KeymapAction* KeymapAction_new(void);
void KeymapBinding_init(KeymapBinding* self);
KeymapBinding* KeymapBinding_new(void);
void Keymap_init(Keymap* self);
Keymap* Keymap_new(void);
bool Keymap_hasAction(Keymap* self, char* id);
char* Keymap_chordFor(Keymap* self, char* id);
bool Keymap_hasBindingButton(Keymap* self, char* button);
bool KeymapParser_modifierAllowed(char* modifier);
void KeymapParser_checkChord(char* actionId, char* chord, char* file, ParseLog* log);
void KeymapParser_parseActionLine(Keymap* keymap, char* rest, int lineNumber, char* file, ParseLog* log);
void KeymapParser_parseBindingLine(Keymap* keymap, char* rest, int lineNumber, char* file, ParseLog* log);
Keymap* KeymapParser_parse(char* text, ParseLog* log);
void SteamInputTarget_init(SteamInputTarget* self);
SteamInputTarget* SteamInputTarget_new(void);
bool SteamInputTarget_hasAction(SteamInputTarget* self, char* id);
char* SteamInputTarget_actionLabel(SteamInputTarget* self, char* id);
char* SteamInputTarget_keyName(SteamInputTarget* self, char* token);
char* SteamInputTarget_deviceIdentity(SteamInputTarget* self, char* key);
void SteamInputParser_parseActions(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log);
void SteamInputParser_parseController(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log);
void SteamInputParser_parseRadial(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log);
void SteamInputParser_parseTemplates(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log);
void SteamInputParser_parseStringTable(JsonValue* root, char* key, btrc_Vector_string* outputKeys, btrc_Vector_string* outputValues, char* path, ParseLog* log);
void SteamInputParser_parsePlatforms(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log);
void SteamInputParser_checkKeymapConsistency(SteamInputTarget* target, char* path, ParseLog* log);
SteamInputTarget* SteamInputParser_parse(char* filePath, ParseLog* log);
char* ChordTranslator_keyPart(char* chord);
btrc_Vector_string* ChordTranslator_modifierList(char* chord);
char* ChordTranslator_vdfTokens(char* chord, SteamInputTarget* target);
char* RetroArchEmitter_hotkeyToken(char* chord);
char* RetroArchEmitter_configurationLine(char* key, char* value);
char* RetroArchEmitter_runtimeContentRoot(char* outputRoot);
btrc_Vector_string* RetroArchEmitter_hotkeyLines(Keymap* keymap);
char* RetroArchEmitter_profileText(char* inputDriver, char* audioDriver, char* contentRoot, Keymap* keymap);
void RetroArchEmitter_emitProfile(char* profileName, char* inputDriver, char* audioDriver, Keymap* keymap, char* outputRoot);
void RetroArchConfiguration_init(RetroArchConfiguration* self);
RetroArchConfiguration* RetroArchConfiguration_new(void);
void RetroArchEmulator_init(RetroArchEmulator* self);
RetroArchEmulator* RetroArchEmulator_new(void);
char* RetroArchEmulator_id(RetroArchEmulator* self);
char* RetroArchEmulator_contractFile(RetroArchEmulator* self);
RetroArchConfiguration* RetroArchEmulator_parseConfiguration(RetroArchEmulator* self, EmulatorPlatform* platform, ParseLog* log);
void RetroArchEmulator_parseSettingsOverrides(RetroArchEmulator* self, RetroArchConfiguration* configuration, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void RetroArchEmulator_parseCoreOptions(RetroArchEmulator* self, RetroArchConfiguration* configuration, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void RetroArchEmulator_parseExtension(RetroArchEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* RetroArchEmulator_commandFragments(RetroArchEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
char* RetroArchEmulator_bundledCorePathEntry(RetroArchEmulator* self);
btrc_Vector_string* RetroArchEmulator_esDeCorePathDirectories(RetroArchEmulator* self, EmulatorPlatform* platform);
EmulatorPlatform* RetroArchEmulator_profilePlatform(RetroArchEmulator* self, EmulatorDefinition* definition);
char* RetroArchEmulator_audioDriver(RetroArchEmulator* self, EmulatorPlatform* platform);
void RetroArchEmulator_emitProfiles(RetroArchEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void DolphinWiimoteProfiles_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions);
char* DolphinWiimoteProfiles_qualifiedControl(char* deviceIdentity, char* control);
btrc_Vector_string* DolphinWiimoteProfiles_wiimoteProfileCommon(char* wiimoteIdentity, char* pointerIdentity);
char* DolphinWiimoteProfiles_wiimoteNunchukProfile(char* wiimoteIdentity, char* pointerIdentity);
char* DolphinWiimoteProfiles_wiimoteClassicProfile(char* wiimoteIdentity, char* pointerIdentity);
char* DolphinWiimoteProfiles_wiimoteNewFile(char* wiimoteIdentity, char* pointerIdentity);
void DolphinEmitter_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions);
char* DolphinEmitter_nativeChord(char* chord);
char* DolphinEmitter_dolphinIni(void);
btrc_Vector_string* DolphinEmitter_gamecubePadBody(char* padIdentity);
char* DolphinEmitter_gamecubePadFile(char* padIdentity);
char* DolphinEmitter_gamecubePadProfile(char* padIdentity);
void DolphinEmitter_pushHotkeyLine(btrc_Vector_string* lines, EmulatorDefinition* definition, Keymap* keymap, char* settingName, char* actionId);
char* DolphinEmitter_speedLimitExpression(Keymap* keymap);
char* DolphinEmitter_hotkeysProfile(EmulatorDefinition* definition, Keymap* keymap, char* pointerIdentity);
void DolphinEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void DolphinEmulator_init(DolphinEmulator* self);
DolphinEmulator* DolphinEmulator_new(void);
char* DolphinEmulator_id(DolphinEmulator* self);
void DolphinEmulator_parseExtension(DolphinEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* DolphinEmulator_commandFragments(DolphinEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void DolphinEmulator_emitProfiles(DolphinEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void TemplateContext_init(TemplateContext* self);
TemplateContext* TemplateContext_new(void);
char* TemplatePaths_home(void);
char* TemplatePaths_generatedRoot(char* project);
char* TemplatePaths_runtimeRoot(char* project);
char* TemplatePaths_contentRoot(char* project);
char* TemplatePaths_biosRoot(char* project);
char* TemplatePaths_normalizedRomsRoot(char* romsDirectory);
char* TemplatePaths_romsRoot(char* project);
char* TemplatePaths_stateRoot(TemplateContext* context);
char* TemplatePaths_emulationRoot(char* project);
char* TemplatePaths_flatpakHome(char* flatpakId);
char* TemplatePaths_nixResult(char* project);
char* TemplatePaths_assetRoot(char* project);
char* TemplatePaths_resolveEnvironmentTokens(char* value);
char* TemplatePaths_resolve(char* value, TemplateContext* context);
btrc_Vector_string* TemplatePaths_resolveAll(btrc_Vector_string* values, TemplateContext* context);
char* Pcsx2Rendering_automaticRendererValue(void);
char* Pcsx2Rendering_rendererKey(GraphicsPolicy* graphics);
btrc_Vector_string* Pcsx2Rendering_graphicsSectionLines(GraphicsPolicy* graphics);
void Pcsx2Emitter_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions);
char* Pcsx2Emitter_hotkeyChord(char* chord);
btrc_Vector_string* Pcsx2Emitter_userInterfaceSectionLines(void);
btrc_Vector_string* Pcsx2Emitter_foldersSectionLines(char* biosDirectory);
btrc_Vector_string* Pcsx2Emitter_coreSectionLines(void);
btrc_Vector_string* Pcsx2Emitter_padSectionsLines(char* padIdentity);
void Pcsx2Emitter_pushHotkeyLine(btrc_Vector_string* lines, EmulatorDefinition* definition, Keymap* keymap, char* settingName, char* actionId);
btrc_Vector_string* Pcsx2Emitter_hotkeysSectionLines(EmulatorDefinition* definition, Keymap* keymap);
char* Pcsx2Emitter_projectRootFor(char* outputRoot);
char* Pcsx2Emitter_biosDirectory(EmulatorPlatform* platform, char* outputRoot);
EmulatorPlatform* Pcsx2Emitter_profilePlatform(EmulatorDefinition* definition);
char* Pcsx2Emitter_baseConfigurationText(EmulatorPlatform* platform, char* padIdentity, char* biosDirectory);
char* Pcsx2Emitter_inputProfileText(EmulatorDefinition* definition, Keymap* keymap, char* padIdentity);
void Pcsx2Emitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void Pcsx2Emulator_init(Pcsx2Emulator* self);
Pcsx2Emulator* Pcsx2Emulator_new(void);
char* Pcsx2Emulator_id(Pcsx2Emulator* self);
void Pcsx2Emulator_parseExtension(Pcsx2Emulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* Pcsx2Emulator_commandFragments(Pcsx2Emulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void Pcsx2Emulator_emitProfiles(Pcsx2Emulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
char* CemuEmitter_stagedRootMarker(void);
char* CemuEmitter_stagedRelativePath(char* declaredPath);
btrc_Vector_string* CemuEmitter_stagedSeedDirectories(EmulatorDefinition* definition);
char* CemuEmitter_controllerProfileXml(SteamInputTarget* steamInput);
char* CemuEmitter_controllerProfilePath(EmulatorDefinition* definition, SteamInputTarget* steamInput, char* outputRoot);
void CemuEmitter_emit(EmulatorDefinition* definition, SteamInputTarget* steamInput, char* outputRoot);
void CemuEmulator_init(CemuEmulator* self);
CemuEmulator* CemuEmulator_new(void);
char* CemuEmulator_id(CemuEmulator* self);
char* CemuEmulator_contractFile(CemuEmulator* self);
void CemuEmulator_parseExtension(CemuEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* CemuEmulator_commandFragments(CemuEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void CemuEmulator_emitProfiles(CemuEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
char* RyujinxEmitter_textField(char* fieldKey, char* fieldValue);
char* RyujinxEmitter_literalField(char* fieldKey, char* literalValue);
char* RyujinxEmitter_booleanField(char* fieldKey, bool fieldValue);
char* RyujinxEmitter_objectText(btrc_Vector_string* fields);
char* RyujinxEmitter_objectField(char* fieldKey, btrc_Vector_string* fields);
btrc_Vector_string* RyujinxEmitter_leftJoyconStickFields(void);
btrc_Vector_string* RyujinxEmitter_rightJoyconStickFields(void);
btrc_Vector_string* RyujinxEmitter_motionFields(void);
btrc_Vector_string* RyujinxEmitter_rumbleFields(void);
btrc_Vector_string* RyujinxEmitter_leftJoyconFields(void);
btrc_Vector_string* RyujinxEmitter_rightJoyconFields(void);
char* RyujinxEmitter_controllerProfileText(char* controllerIdentity);
char* RyujinxEmitter_controllerIdentityKey(char* emulatorId);
char* RyujinxEmitter_controllerProfileRelativePath(char* emulatorName);
void RyujinxEmulator_init(RyujinxEmulator* self);
RyujinxEmulator* RyujinxEmulator_new(void);
char* RyujinxEmulator_id(RyujinxEmulator* self);
char* RyujinxEmulator_contractFile(RyujinxEmulator* self);
void RyujinxEmulator_parseExtension(RyujinxEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* RyujinxEmulator_commandFragments(RyujinxEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void RyujinxEmulator_emitProfiles(RyujinxEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void PpssppEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void PpssppEmulator_init(PpssppEmulator* self);
PpssppEmulator* PpssppEmulator_new(void);
char* PpssppEmulator_id(PpssppEmulator* self);
char* PpssppEmulator_contractFile(PpssppEmulator* self);
void PpssppEmulator_parseExtension(PpssppEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* PpssppEmulator_commandFragments(PpssppEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void PpssppEmulator_emitProfiles(PpssppEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void FlycastEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void FlycastEmulator_init(FlycastEmulator* self);
FlycastEmulator* FlycastEmulator_new(void);
char* FlycastEmulator_id(FlycastEmulator* self);
char* FlycastEmulator_contractFile(FlycastEmulator* self);
void FlycastEmulator_parseExtension(FlycastEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* FlycastEmulator_commandFragments(FlycastEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void FlycastEmulator_emitProfiles(FlycastEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
char* AzaharRendering_graphicsApiValue(GraphicsPolicy* graphics, char* graphicsApi);
char* AzaharRendering_overlayGraphicsValue(GraphicsPolicy* graphics);
char* AzaharEmitter_configurationFileName(GraphicsPolicy* graphics);
char* AzaharEmitter_configurationDirectoryName(GraphicsPolicy* graphics);
char* AzaharEmitter_profileRelativePath(EmulatorDefinition* definition, EmulatorPlatform* platform);
char* AzaharEmitter_runtimeDataRoot(char* outputRoot, char* emulatorId);
void AzaharEmitter_pushSetting(btrc_Vector_string* lines, char* key, char* value, bool valueIsQtDefault);
char* AzaharEmitter_qtConfigurationText(char* dataRoot, GraphicsPolicy* graphics);
bool AzaharEmitter_emitPlatformProfile(EmulatorDefinition* definition, EmulatorPlatform* platform, char* outputRoot);
void AzaharEmulator_init(AzaharEmulator* self);
AzaharEmulator* AzaharEmulator_new(void);
char* AzaharEmulator_id(AzaharEmulator* self);
char* AzaharEmulator_contractFile(AzaharEmulator* self);
void AzaharEmulator_parseExtension(AzaharEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* AzaharEmulator_commandFragments(AzaharEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void AzaharEmulator_emitProfiles(AzaharEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void MelonDsEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void MelonDsEmulator_init(MelonDsEmulator* self);
MelonDsEmulator* MelonDsEmulator_new(void);
char* MelonDsEmulator_id(MelonDsEmulator* self);
char* MelonDsEmulator_contractFile(MelonDsEmulator* self);
void MelonDsEmulator_parseExtension(MelonDsEmulator* self, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* MelonDsEmulator_commandFragments(MelonDsEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform);
void MelonDsEmulator_emitProfiles(MelonDsEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
void EmulatorRegistry_parseExtension(char* emulatorId, EmulatorPlatform* platform, ParseLog* log);
btrc_Vector_string* EmulatorRegistry_commandFragments(char* emulatorId, EmulatorBinding* binding, EmulatorPlatform* platform);
void EmulatorRegistry_emitProfiles(char* emulatorId, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot);
btrc_Vector_string* EmulatorRegistry_esDeCorePathDirectories(char* emulatorId, EmulatorPlatform* platform);
void EmulatorParser_parseSessions(EmulatorPlatform* platform, JsonValue* jsonValue, bool isLinux, char* file, char* context, ParseLog* log);
void EmulatorParser_parseEnvironment(EmulatorPlatform* platform, JsonValue* jsonValue, char* file, char* context, ParseLog* log);
void EmulatorParser_checkPlatformAxes(EmulatorPlatform* platform, JsonValue* jsonValue, bool isLinux, char* file, char* context, ParseLog* log);
EmulatorPlatform* EmulatorParser_parsePlatform(JsonValue* jsonValue, char* operatingSystemName, char* emulatorId, char* file, ParseLog* log);
void EmulatorParser_parseInput(EmulatorDefinition* emulatorDefinition, JsonValue* root, char* filePath, ParseLog* log);
EmulatorDefinition* EmulatorParser_parseFile(char* emulatorId, char* filePath, ParseLog* log);
btrc_Vector_EmulatorDefinition_p1* EmulatorParser_parseAll(char* sourceRoot, ParseLog* log);
void SystemParser_parseEsDe(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log);
void SystemParser_parseRom(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log);
void SystemParser_parseBindings(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log);
SystemDefinition* SystemParser_parseSystemFile(char* systemId, char* path, ParseLog* log);
BezelCollection* SystemParser_parseBezelsFile(char* path, btrc_Vector_string* screenIds, ParseLog* log);
ShaderCollection* SystemParser_parseShadersFile(char* path, ParseLog* log);
SemuContracts* SystemParser_loadAll(char* sourceRoot, ParseLog* log);
char* EmulatorEmitter_quoteArgument(char* argument);
char* EmulatorEmitter_esDeCommand(EmulatorBinding* binding, EmulatorDefinition* emulator, char* operatingSystem);
char* EmulatorEmitter_shimScript(EmulatorDefinition* emulator);
char* EmulatorEmitter_shimName(EmulatorDefinition* emulator);
void EmulatorEmitter_emitShims(btrc_Vector_EmulatorDefinition_p1* emulators, char* outputDirectory);
char* SystemEmitter_xmlEscape(char* value);
char* SystemEmitter_jsonEscape(char* value);
char* SystemEmitter_jsonString(char* value);
char* SystemEmitter_jsonStringArray(btrc_Vector_string* values);
EmulatorDefinition* SystemEmitter_emulatorForId(btrc_Vector_EmulatorDefinition_p1* emulators, char* emulatorId);
bool SystemEmitter_bindingApplies(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem);
char* SystemEmitter_extensionList(btrc_Vector_string* extensions);
char* SystemEmitter_commandLabel(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators);
char* SystemEmitter_systemEntryXml(SystemDefinition* systemDefinition, btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem);
char* SystemEmitter_esSystemsXml(SemuContracts* contracts, char* operatingSystem);
char* SystemEmitter_launcherRuleXml(EmulatorDefinition* emulatorDefinition);
char* SystemEmitter_corePathRuleXml(EmulatorDefinition* emulatorDefinition, char* operatingSystem);
char* SystemEmitter_esFindRulesXml(btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem);
char* SystemEmitter_manifestBindingJson(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators);
char* SystemEmitter_manifestSystemJson(SystemDefinition* systemDefinition, btrc_Vector_EmulatorDefinition_p1* emulators);
char* SystemEmitter_manifestEmulatorJson(EmulatorDefinition* emulatorDefinition);
char* SystemEmitter_manifestJson(SemuContracts* contracts);
void SystemEmitter_emitAll(SemuContracts* contracts, char* generatedRoot);
void LaunchPlan_init(LaunchPlan* self);
LaunchPlan* LaunchPlan_new(void);
void EmulatorLauncher_setEnvironment(LaunchPlan* launchPlan, char* key, char* value);
void EmulatorLauncher_unsetEnvironment(LaunchPlan* launchPlan, char* key);
void EmulatorLauncher_bundleEnvironment(LaunchPlan* launchPlan, char* key, char* value);
void EmulatorLauncher_bundleUnsetEnvironment(LaunchPlan* launchPlan, char* key);
void EmulatorLauncher_applyDisplayServer(LaunchPlan* launchPlan, char* displayServer, char* displayFallback);
void EmulatorLauncher_applyStateEnvironment(LaunchPlan* launchPlan, StatePolicy* state);
void EmulatorLauncher_applyContractEnvironment(LaunchPlan* launchPlan, EmulatorPlatform* platform);
void EmulatorLauncher_pushAllArguments(LaunchPlan* launchPlan, btrc_Vector_string* values);
void EmulatorLauncher_pushMissingPlatformArguments(LaunchPlan* launchPlan, EmulatorPlatform* platform, btrc_Vector_string* systemArguments);
void EmulatorLauncher_pushTailArguments(LaunchPlan* launchPlan, EmulatorPlatform* platform, btrc_Vector_string* systemArguments, char* romPath);
void EmulatorLauncher_assembleFlatpak(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath);
void EmulatorLauncher_assembleExecutable(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath);
void EmulatorLauncher_assembleBwrap(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath);
LaunchPlan* EmulatorLauncher_plan(EmulatorDefinition* emulator, char* operatingSystem, char* session, char* romPath, btrc_Vector_string* systemArguments);
char* EmulatorLauncher_shellCommand(LaunchPlan* launchPlan);
TemplateContext* EmulatorExecution_contextFor(EmulatorDefinition* emulator, EmulatorPlatform* platform, char* project);
void EmulatorExecution_prepareStateRoot(EmulatorPlatform* platform, TemplateContext* context);
void EmulatorExecution_copyFile(char* sourcePath, char* targetPath);
void EmulatorExecution_applySeeds(EmulatorPlatform* platform, TemplateContext* context);
void EmulatorExecution_materializeSymlinks(EmulatorPlatform* platform, TemplateContext* context);
char* EmulatorExecution_environmentValueFor(LaunchPlan* launchPlan, char* key);
LaunchPlan* EmulatorExecution_resolvePlan(LaunchPlan* launchPlan, TemplateContext* context);
void EmulatorExecution_removeEnvironmentKey(LaunchPlan* launchPlan, char* key);
char* EmulatorExecution_x11DisplayPreamble(char* displayFallback);
char* EmulatorExecution_applyHostDisplayPrecedence(LaunchPlan* resolvedPlan);
int EmulatorExecution_run(EmulatorDefinition* emulator, char* operatingSystem, LaunchPlan* launchPlan, char* project);
void SteamdeckHotkeyAssignments_init(SteamdeckHotkeyAssignments* self);
SteamdeckHotkeyAssignments* SteamdeckHotkeyAssignments_new(void);
char* SteamdeckHotkeyAssignments_hotkeyButtonToken(char* combo);
SteamdeckHotkeyAssignments* SteamdeckHotkeyAssignments_collect(Keymap* keymap);
char* SteamdeckHotkeyAssignments_actionFor(SteamdeckHotkeyAssignments* self, char* button);
bool SteamdeckHotkeyAssignments_anyBound(SteamdeckHotkeyAssignments* self, btrc_Vector_string* candidateButtons);
char* SteamdeckInputEmitter_keyPressBindings(SteamInputTarget* target, char* chord, char* label);
char* SteamdeckInputEmitter_xinputBinding(char* button, char* label);
char* SteamdeckInputEmitter_quitExtraBindings(SteamInputTarget* target, char* label);
char* SteamdeckInputEmitter_actionBindings(SteamInputTarget* target, Keymap* keymap, char* actionId);
char* SteamdeckInputEmitter_presetHotkeyBinding(void);
char* SteamdeckInputEmitter_presetDefaultBinding(void);
char* SteamdeckInputEmitter_inputEntry(char* inputName, char* activator, char* bindings);
char* SteamdeckInputEmitter_singleInputGroup(char* groupId, char* mode, char* inputName, char* bindings);
void SteamdeckInputEmitter_pushGroupHead(btrc_Vector_string* lines, char* groupId, char* mode);
void SteamdeckInputEmitter_pushGroupTail(btrc_Vector_string* lines);
void SteamdeckInputEmitter_pushBoundEntry(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, SteamdeckHotkeyAssignments* assignments, char* button, char* inputName);
void SteamdeckInputEmitter_pushGamepadGroups(btrc_Vector_string* lines, SteamInputTarget* target);
void SteamdeckInputEmitter_pushHotkeyGroups(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, SteamdeckHotkeyAssignments* assignments);
void SteamdeckInputEmitter_pushRadialGroup(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, char* radialName);
char* SteamdeckInputEmitter_presetLine(char* presetId, char* presetName, btrc_Vector_string* groupIds, SteamInputTarget* target);
btrc_Vector_string* SteamdeckInputEmitter_hotkeyPresetGroupIds(SteamdeckHotkeyAssignments* assignments);
char* SteamdeckInputEmitter_templateVdf(SteamInputTarget* target, Keymap* keymap, char* title, char* radialName);
void SteamdeckInputEmitter_emitTemplates(SteamInputTarget* target, Keymap* keymap, char* outputDirectory);
char* AppImagePackage_templateDirectory(char* repoRoot);
char* AppImagePackage_renderTemplate(char* repoRoot, char* templateName);
bool AppImagePackage_emitTemplate(char* repoRoot, char* templateName, char* outputPath, bool executable);
void AppImagePackage_stageCliBinary(char* repoRoot, char* binDirectory);
void AppImagePackage_emitForEmulators(btrc_Vector_EmulatorDefinition_p1* emulators, char* repoRoot, char* outputRoot);
char* SystemContractTest_aspectLabel(SystemDefinition* systemDefinition);
char* SystemContractTest_defaultEmulatorLabel(SystemDefinition* systemDefinition);
int SystemContractTest_run(char* repoRoot);
int EmulatorContractTest_fail(char* message);
bool EmulatorContractTest_argumentVectorHas(LaunchPlan* launchPlan, char* expected);
char* EmulatorContractTest_environmentValue(LaunchPlan* launchPlan, char* key);
EmulatorDefinition* EmulatorContractTest_findEmulator(SemuContracts* contracts, char* emulatorId);
SystemDefinition* EmulatorContractTest_findSystem(SemuContracts* contracts, char* systemId);
EmulatorBinding* EmulatorContractTest_findBinding(SystemDefinition* systemDefinition, char* emulatorId);
int EmulatorContractTest_checkDolphinPlan(SemuContracts* contracts);
int EmulatorContractTest_countInArgumentVector(LaunchPlan* launchPlan, char* expected);
int EmulatorContractTest_checkDolphinEsDeTailPlan(SemuContracts* contracts);
int EmulatorContractTest_checkRetroArchLinuxPlan(SemuContracts* contracts);
int EmulatorContractTest_checkRetroArchMacosPlan(SemuContracts* contracts);
int EmulatorContractTest_checkAresPlans(SemuContracts* contracts);
int EmulatorContractTest_checkEsDeCommands(SemuContracts* contracts);
int EmulatorContractTest_comparePcsx2GoldenFile(char* emittedPath, char* goldenPath, EmulatorDefinition* pcsx2, char* name);
int EmulatorContractTest_checkPcsx2Profiles(SemuContracts* contracts, char* repoRoot);
int EmulatorContractTest_run(char* repoRoot);
void ResolvedBezel_init(ResolvedBezel* self);
ResolvedBezel* ResolvedBezel_new(void);
char* BezelResolver_assetRoot(char* repoRoot);
char* BezelResolver_resolveAssetPath(char* canonicalPath, char* assetRoot);
char* BezelResolver_fallbackProfile(SystemDefinition* system);
ResolvedBezel* BezelResolver_genericFallback(char* assetProfile);
int BezelResolver_variantIndex(BezelCollection* bezels, char* variantId);
void BezelResolver_applyRenderDefaults(ResolvedBezel* resolved, SystemDefinition* system);
ResolvedBezel* BezelResolver_resolveFallback(SystemDefinition* system, char* assetRoot);
void BezelResolver_applyVariantOverrides(ResolvedBezel* resolved, BezelVariant* variant);
ResolvedBezel* BezelResolver_resolve(SystemDefinition* system, char* variantId, char* assetRoot);
char* ShaderSelector_select(SystemDefinition* system, bool visualsEnabled, bool tapActive, bool bezelsEnabled, bool crtShadersEnabled, bool widescreenActive);
char* ShaderSelector_resolvePath(char* canonicalPath, char* shaderRoot);
void TapEnvironment_init(TapEnvironment* self);
TapEnvironment* TapEnvironment_new(void);
void TapEnvironment_put(TapEnvironment* self, char* key, char* value);
char* TapEnvironment_valueOf(TapEnvironment* self, char* key);
int RenderPlanner_systemKindWireValue(char* kind);
char* RenderPlanner_wireFloat(double value);
PixelSize* RenderPlanner_combinedNativeSize(SystemDefinition* system);
char* RenderPlanner_aspectWireValue(SystemDefinition* system, PixelSize* nativeSize);
TapEnvironment* RenderPlanner_tapEnvironment(SystemDefinition* system, ResolvedBezel* bezel, char* renderMode);
char* RenderPlanner_targetResolution(SystemDefinition* system, char* renderMode, int screenWidth, int screenHeight);
int RenderingContractTest_fail(char* message);
bool RenderingContractTest_nearlyEqual(double actualValue, double expectedValue);
SystemDefinition* RenderingContractTest_findSystem(SemuContracts* contracts, char* systemId);
int RenderingContractTest_checkHandheldBezel(SystemDefinition* gameBoy, char* assetRoot);
int RenderingContractTest_checkGeometryIndirection(SystemDefinition* gameCube, char* assetRoot);
int RenderingContractTest_checkGenericFallback(SystemDefinition* switchSystem, char* assetRoot);
int RenderingContractTest_checkSystemKindWire(SystemDefinition* dualScreenSystem);
int RenderingContractTest_checkShaderMatrix(SystemDefinition* gameBoy, SystemDefinition* gameCube);
int RenderingContractTest_checkTargetResolution(SystemDefinition* gameBoy, SystemDefinition* dualScreenSystem, char* assetRoot);
int RenderingContractTest_run(char* repoRoot);
int InputContractTest_check(bool condition, char* what);
int InputContractTest_checkEquals(char* got, char* want, char* what);
int InputContractTest_checkChordTokens(SteamInputTarget* target);
btrc_Vector_string* InputContractTest_normalizedBindings(char* line);
int InputContractTest_countOf(btrc_Vector_string* values, char* value);
int InputContractTest_checkBindingSets(char* emittedLine, char* goldenLine, char* what);
char* InputContractTest_lineContaining(char* text, char* needle);
int InputContractTest_compareGolden(char* emittedVdf, char* goldenVdf, char* name);
int InputContractTest_checkVdfStructure(char* vdfText, char* name, SteamInputTarget* target, Keymap* keymap, char* title, char* radialName);
int InputContractTest_run(char* repoRoot);
void GithubPin_init(GithubPin* self);
GithubPin* GithubPin_new(void);
char* NixPackage_sourcesPath(char* repoRoot);
char* NixPackage_bezelsNixPath(char* repoRoot);
char* NixPackage_stringField(JsonValue* objectValue, char* key);
int NixPackage_checkUpstreamPins(JsonValue* root, char* file, btrc_Vector_GithubPin_p1* pins, ParseLog* log);
int NixPackage_checkAssetRecipe(char* assetPath, btrc_Vector_string* recipeKeys, char* file, char* context, ParseLog* log);
int NixPackage_checkSystemAssets(SystemDefinition* systemDefinition, btrc_Vector_string* recipeKeys, ParseLog* log);
int NixPackage_checkReferencedAssets(char* repoRoot, btrc_Vector_string* recipeKeys, char* sourcesFile, ParseLog* log);
void NixPackage_crossCheckBezelsNix(char* repoRoot, btrc_Vector_GithubPin_p1* pins, ParseLog* log);
int NixPackage_verify(char* repoRoot, ParseLog* log);
int PackageContractTest_fail(char* message);
int PackageContractTest_require(char* text, char* needle, char* what);
int PackageContractTest_occurrenceCount(char* text, char* needle);
bool PackageContractTest_isExecutable(char* path);
int PackageContractTest_checkEsSystems(SemuContracts* contracts);
int PackageContractTest_checkFindRules(SemuContracts* contracts, char* repoRoot);
int PackageContractTest_checkAppImage(SemuContracts* contracts, char* repoRoot, char* outputRoot);
int PackageContractTest_checkManifest(SemuContracts* contracts);
int PackageContractTest_checkNixLayer(char* repoRoot);
int PackageContractTest_run(char* repoRoot);
int GeneratedTreeTest_fail(char* message);
int GeneratedTreeTest_requireFile(char* root, char* relativePath);
int GeneratedTreeTest_checkArtifactTree(SemuContracts* contracts, char* generatedRoot);
int GeneratedTreeTest_checkTreeAudit(char* repoRoot);
int GeneratedTreeTest_run(char* repoRoot);
char* SemuCli_projectRoot(CliArgs* arguments);
char* SemuCli_sourceRoot(char* project);
char* SemuCli_generatedRoot(char* project);
char* SemuCli_detectOperatingSystem(void);
char* SemuCli_detectSession(void);
SemuContracts* SemuCli_loadContracts(char* project, ParseLog* log);
bool SemuCli_reportErrors(ParseLog* log);
int SemuCli_runManifest(CliArgs* arguments, char* project);
char* SemuCli_inputTargetDirectory(char* project);
int SemuCli_runBootstrap(CliArgs* arguments, char* project);
EmulatorDefinition* SemuCli_emulatorForId(SemuContracts* contracts, char* emulatorId);
int SemuCli_printPlan(LaunchPlan* launchPlan);
int SemuCli_runLauncher(CliArgs* arguments, char* project);
int SemuCli_runTestCore(char* project);
int SemuCli_printUsage(void);
int SemuCli_run(CliArgs* arguments, char* repoRoot);
typedef bool (*__btrc_fn_bool_string_string)(char*, char*);
typedef bool (*__btrc_fn_bool_string)(char*);
typedef void (*__btrc_fn_void_string)(char*);
typedef char* (*__btrc_fn_string_string)(char*);
typedef char* (*__btrc_fn_string_string_string)(char*, char*);
typedef bool (*__btrc_fn_bool_bool_bool)(bool, bool);
typedef bool (*__btrc_fn_bool_bool)(bool);
typedef void (*__btrc_fn_void_bool)(bool);
typedef bool (*__btrc_fn_bool_JsonValue_p1_JsonValue_p1)(JsonValue*, JsonValue*);
typedef bool (*__btrc_fn_bool_JsonValue_p1)(JsonValue*);
typedef void (*__btrc_fn_void_JsonValue_p1)(JsonValue*);
typedef JsonValue* (*__btrc_fn_JsonValue_p1_JsonValue_p1)(JsonValue*);
typedef JsonValue* (*__btrc_fn_JsonValue_p1_JsonValue_p1_JsonValue_p1)(JsonValue*, JsonValue*);
typedef bool (*__btrc_fn_bool_SystemScreen_p1_SystemScreen_p1)(SystemScreen*, SystemScreen*);
typedef bool (*__btrc_fn_bool_SystemScreen_p1)(SystemScreen*);
typedef void (*__btrc_fn_void_SystemScreen_p1)(SystemScreen*);
typedef SystemScreen* (*__btrc_fn_SystemScreen_p1_SystemScreen_p1)(SystemScreen*);
typedef SystemScreen* (*__btrc_fn_SystemScreen_p1_SystemScreen_p1_SystemScreen_p1)(SystemScreen*, SystemScreen*);
typedef bool (*__btrc_fn_bool_BiosFile_p1_BiosFile_p1)(BiosFile*, BiosFile*);
typedef bool (*__btrc_fn_bool_BiosFile_p1)(BiosFile*);
typedef void (*__btrc_fn_void_BiosFile_p1)(BiosFile*);
typedef BiosFile* (*__btrc_fn_BiosFile_p1_BiosFile_p1)(BiosFile*);
typedef BiosFile* (*__btrc_fn_BiosFile_p1_BiosFile_p1_BiosFile_p1)(BiosFile*, BiosFile*);
typedef bool (*__btrc_fn_bool_ScreenHole_p1_ScreenHole_p1)(ScreenHole*, ScreenHole*);
typedef bool (*__btrc_fn_bool_ScreenHole_p1)(ScreenHole*);
typedef void (*__btrc_fn_void_ScreenHole_p1)(ScreenHole*);
typedef ScreenHole* (*__btrc_fn_ScreenHole_p1_ScreenHole_p1)(ScreenHole*);
typedef ScreenHole* (*__btrc_fn_ScreenHole_p1_ScreenHole_p1_ScreenHole_p1)(ScreenHole*, ScreenHole*);
typedef bool (*__btrc_fn_bool_BezelVariant_p1_BezelVariant_p1)(BezelVariant*, BezelVariant*);
typedef bool (*__btrc_fn_bool_BezelVariant_p1)(BezelVariant*);
typedef void (*__btrc_fn_void_BezelVariant_p1)(BezelVariant*);
typedef BezelVariant* (*__btrc_fn_BezelVariant_p1_BezelVariant_p1)(BezelVariant*);
typedef BezelVariant* (*__btrc_fn_BezelVariant_p1_BezelVariant_p1_BezelVariant_p1)(BezelVariant*, BezelVariant*);
typedef bool (*__btrc_fn_bool_EmulatorBinding_p1_EmulatorBinding_p1)(EmulatorBinding*, EmulatorBinding*);
typedef bool (*__btrc_fn_bool_EmulatorBinding_p1)(EmulatorBinding*);
typedef void (*__btrc_fn_void_EmulatorBinding_p1)(EmulatorBinding*);
typedef EmulatorBinding* (*__btrc_fn_EmulatorBinding_p1_EmulatorBinding_p1)(EmulatorBinding*);
typedef EmulatorBinding* (*__btrc_fn_EmulatorBinding_p1_EmulatorBinding_p1_EmulatorBinding_p1)(EmulatorBinding*, EmulatorBinding*);
typedef bool (*__btrc_fn_bool_SandboxFilesystem_p1_SandboxFilesystem_p1)(SandboxFilesystem*, SandboxFilesystem*);
typedef bool (*__btrc_fn_bool_SandboxFilesystem_p1)(SandboxFilesystem*);
typedef void (*__btrc_fn_void_SandboxFilesystem_p1)(SandboxFilesystem*);
typedef SandboxFilesystem* (*__btrc_fn_SandboxFilesystem_p1_SandboxFilesystem_p1)(SandboxFilesystem*);
typedef SandboxFilesystem* (*__btrc_fn_SandboxFilesystem_p1_SandboxFilesystem_p1_SandboxFilesystem_p1)(SandboxFilesystem*, SandboxFilesystem*);
typedef bool (*__btrc_fn_bool_StateSeed_p1_StateSeed_p1)(StateSeed*, StateSeed*);
typedef bool (*__btrc_fn_bool_StateSeed_p1)(StateSeed*);
typedef void (*__btrc_fn_void_StateSeed_p1)(StateSeed*);
typedef StateSeed* (*__btrc_fn_StateSeed_p1_StateSeed_p1)(StateSeed*);
typedef StateSeed* (*__btrc_fn_StateSeed_p1_StateSeed_p1_StateSeed_p1)(StateSeed*, StateSeed*);
typedef bool (*__btrc_fn_bool_SessionOverride_p1_SessionOverride_p1)(SessionOverride*, SessionOverride*);
typedef bool (*__btrc_fn_bool_SessionOverride_p1)(SessionOverride*);
typedef void (*__btrc_fn_void_SessionOverride_p1)(SessionOverride*);
typedef SessionOverride* (*__btrc_fn_SessionOverride_p1_SessionOverride_p1)(SessionOverride*);
typedef SessionOverride* (*__btrc_fn_SessionOverride_p1_SessionOverride_p1_SessionOverride_p1)(SessionOverride*, SessionOverride*);
typedef bool (*__btrc_fn_bool_EmulatorPlatform_p1_EmulatorPlatform_p1)(EmulatorPlatform*, EmulatorPlatform*);
typedef bool (*__btrc_fn_bool_EmulatorPlatform_p1)(EmulatorPlatform*);
typedef void (*__btrc_fn_void_EmulatorPlatform_p1)(EmulatorPlatform*);
typedef EmulatorPlatform* (*__btrc_fn_EmulatorPlatform_p1_EmulatorPlatform_p1)(EmulatorPlatform*);
typedef EmulatorPlatform* (*__btrc_fn_EmulatorPlatform_p1_EmulatorPlatform_p1_EmulatorPlatform_p1)(EmulatorPlatform*, EmulatorPlatform*);
typedef bool (*__btrc_fn_bool_SystemDefinition_p1_SystemDefinition_p1)(SystemDefinition*, SystemDefinition*);
typedef bool (*__btrc_fn_bool_SystemDefinition_p1)(SystemDefinition*);
typedef void (*__btrc_fn_void_SystemDefinition_p1)(SystemDefinition*);
typedef SystemDefinition* (*__btrc_fn_SystemDefinition_p1_SystemDefinition_p1)(SystemDefinition*);
typedef SystemDefinition* (*__btrc_fn_SystemDefinition_p1_SystemDefinition_p1_SystemDefinition_p1)(SystemDefinition*, SystemDefinition*);
typedef bool (*__btrc_fn_bool_EmulatorDefinition_p1_EmulatorDefinition_p1)(EmulatorDefinition*, EmulatorDefinition*);
typedef bool (*__btrc_fn_bool_EmulatorDefinition_p1)(EmulatorDefinition*);
typedef void (*__btrc_fn_void_EmulatorDefinition_p1)(EmulatorDefinition*);
typedef EmulatorDefinition* (*__btrc_fn_EmulatorDefinition_p1_EmulatorDefinition_p1)(EmulatorDefinition*);
typedef EmulatorDefinition* (*__btrc_fn_EmulatorDefinition_p1_EmulatorDefinition_p1_EmulatorDefinition_p1)(EmulatorDefinition*, EmulatorDefinition*);
typedef bool (*__btrc_fn_bool_KeymapAction_p1_KeymapAction_p1)(KeymapAction*, KeymapAction*);
typedef bool (*__btrc_fn_bool_KeymapAction_p1)(KeymapAction*);
typedef void (*__btrc_fn_void_KeymapAction_p1)(KeymapAction*);
typedef KeymapAction* (*__btrc_fn_KeymapAction_p1_KeymapAction_p1)(KeymapAction*);
typedef KeymapAction* (*__btrc_fn_KeymapAction_p1_KeymapAction_p1_KeymapAction_p1)(KeymapAction*, KeymapAction*);
typedef bool (*__btrc_fn_bool_KeymapBinding_p1_KeymapBinding_p1)(KeymapBinding*, KeymapBinding*);
typedef bool (*__btrc_fn_bool_KeymapBinding_p1)(KeymapBinding*);
typedef void (*__btrc_fn_void_KeymapBinding_p1)(KeymapBinding*);
typedef KeymapBinding* (*__btrc_fn_KeymapBinding_p1_KeymapBinding_p1)(KeymapBinding*);
typedef KeymapBinding* (*__btrc_fn_KeymapBinding_p1_KeymapBinding_p1_KeymapBinding_p1)(KeymapBinding*, KeymapBinding*);
typedef bool (*__btrc_fn_bool_NormalizedRect_p1_NormalizedRect_p1)(NormalizedRect*, NormalizedRect*);
typedef bool (*__btrc_fn_bool_NormalizedRect_p1)(NormalizedRect*);
typedef void (*__btrc_fn_void_NormalizedRect_p1)(NormalizedRect*);
typedef NormalizedRect* (*__btrc_fn_NormalizedRect_p1_NormalizedRect_p1)(NormalizedRect*);
typedef NormalizedRect* (*__btrc_fn_NormalizedRect_p1_NormalizedRect_p1_NormalizedRect_p1)(NormalizedRect*, NormalizedRect*);
typedef bool (*__btrc_fn_bool_GithubPin_p1_GithubPin_p1)(GithubPin*, GithubPin*);
typedef bool (*__btrc_fn_bool_GithubPin_p1)(GithubPin*);
typedef void (*__btrc_fn_void_GithubPin_p1)(GithubPin*);
typedef GithubPin* (*__btrc_fn_GithubPin_p1_GithubPin_p1)(GithubPin*);
typedef GithubPin* (*__btrc_fn_GithubPin_p1_GithubPin_p1_GithubPin_p1)(GithubPin*, GithubPin*);

struct btrc_Vector_string {
    int __rc;
    char** data;
    int len;
    int cap;
};

struct btrc_Vector_bool {
    int __rc;
    bool* data;
    int len;
    int cap;
};

struct btrc_Vector_JsonValue_p1 {
    int __rc;
    JsonValue** data;
    int len;
    int cap;
};

struct btrc_Vector_SystemScreen_p1 {
    int __rc;
    SystemScreen** data;
    int len;
    int cap;
};

struct btrc_Vector_BiosFile_p1 {
    int __rc;
    BiosFile** data;
    int len;
    int cap;
};

struct btrc_Vector_ScreenHole_p1 {
    int __rc;
    ScreenHole** data;
    int len;
    int cap;
};

struct btrc_Vector_BezelVariant_p1 {
    int __rc;
    BezelVariant** data;
    int len;
    int cap;
};

struct btrc_Vector_EmulatorBinding_p1 {
    int __rc;
    EmulatorBinding** data;
    int len;
    int cap;
};

struct btrc_Vector_SandboxFilesystem_p1 {
    int __rc;
    SandboxFilesystem** data;
    int len;
    int cap;
};

struct btrc_Vector_StateSeed_p1 {
    int __rc;
    StateSeed** data;
    int len;
    int cap;
};

struct btrc_Vector_SessionOverride_p1 {
    int __rc;
    SessionOverride** data;
    int len;
    int cap;
};

struct btrc_Vector_EmulatorPlatform_p1 {
    int __rc;
    EmulatorPlatform** data;
    int len;
    int cap;
};

struct btrc_Vector_SystemDefinition_p1 {
    int __rc;
    SystemDefinition** data;
    int len;
    int cap;
};

struct btrc_Vector_EmulatorDefinition_p1 {
    int __rc;
    EmulatorDefinition** data;
    int len;
    int cap;
};

struct btrc_Vector_KeymapAction_p1 {
    int __rc;
    KeymapAction** data;
    int len;
    int cap;
};

struct btrc_Vector_KeymapBinding_p1 {
    int __rc;
    KeymapBinding** data;
    int len;
    int cap;
};

struct btrc_Vector_NormalizedRect_p1 {
    int __rc;
    NormalizedRect** data;
    int len;
    int cap;
};

struct btrc_Vector_GithubPin_p1 {
    int __rc;
    GithubPin** data;
    int len;
    int cap;
};

struct CliArgs {
    int __rc;
    char* program;
    btrc_Vector_string* values;
};

struct File {
    int __rc;
    FILE* handle;
    char* path;
    char* mode;
    bool is_open;
};

struct Path {
    int __rc;
};

struct ProcessStatus {
    int __rc;
    int raw;
};

struct CommandOutput {
    int __rc;
};

struct ExecResult {
    int __rc;
    int code;
    char* out;
    char* err;
    char* command;
};

struct Command {
    int __rc;
    char* command;
    btrc_Vector_string* arguments;
};

struct UnixShell {
    int __rc;
    bool logCommands;
    char* chrootPath;
    int tempId;
};

struct FileStatus {
    int __rc;
    char* path;
    int mode;
    int linkMode;
    bool found;
    bool linkFound;
};

struct Directory {
    int __rc;
    char* path;
};

struct ParseLog {
    int __rc;
    btrc_Vector_string* errors;
};

struct JsonValue {
    int __rc;
    int tag;
    bool boolVal;
    double numVal;
    bool numIsInt;
    char* strVal;
    btrc_Vector_JsonValue_p1* arr;
    btrc_Vector_string* objKeys;
    btrc_Vector_JsonValue_p1* objVals;
};

struct JsonParser {
    int __rc;
    char* text;
    int pos;
    int len;
    bool ok;
};

struct NormalizedRect {
    int __rc;
    double x;
    double y;
    double w;
    double h;
    bool set;
};

struct PixelSize {
    int __rc;
    int w;
    int h;
};

struct AspectRatio {
    int __rc;
    int w;
    int h;
    bool set;
};

struct RgbColor {
    int __rc;
    double r;
    double g;
    double b;
    bool set;
};

struct SystemScreen {
    int __rc;
    char* id;
    PixelSize* native;
};

struct WidescreenPolicy {
    int __rc;
    AspectRatio* aspect;
    char* detect;
    bool set;
};

struct DisplayPolicy {
    int __rc;
    btrc_Vector_SystemScreen_p1* screens;
    char* arrangement;
    AspectRatio* aspect;
    WidescreenPolicy* widescreen;
};

struct RenderPolicy {
    int __rc;
    char* style;
    char* priority;
    char* kind;
    bool fill;
    double reflect;
    double curvature;
    double cornerRadius;
    RgbColor* shellTint;
    bool retroDefault;
    char* assetProfile;
};

struct EmulatorBinding {
    int __rc;
    char* emulator;
    char* label;
    char* core;
    btrc_Vector_string* platforms;
    btrc_Vector_string* arguments;
};

struct BiosFile {
    int __rc;
    char* name;
    char* note;
};

struct BiosRequirement {
    int __rc;
    bool present;
    bool required;
    char* match;
    btrc_Vector_BiosFile_p1* files;
    char* directory;
};

struct ScreenHole {
    int __rc;
    char* screenId;
    NormalizedRect* hole;
};

struct BezelVariant {
    int __rc;
    char* id;
    char* label;
    char* art;
    char* glass;
    NormalizedRect* hole;
    btrc_Vector_ScreenHole_p1* screenHoles;
    bool hasGeometryFrom;
    char* geometrySystem;
    char* geometryVariant;
    RgbColor* shellTint;
    bool hasReflect;
    double reflect;
    bool hasCurvature;
    double curvature;
    bool hasCornerRadius;
    double cornerRadius;
};

struct BezelCollection {
    int __rc;
    bool present;
    char* file;
    bool enabled;
    char* defaultVariant;
    char* widescreenVariant;
    NormalizedRect* hole;
    btrc_Vector_ScreenHole_p1* screenHoles;
    btrc_Vector_BezelVariant_p1* variants;
};

struct ShaderCollection {
    int __rc;
    bool present;
    char* file;
    bool enabled;
    char* screen;
    char* composite;
    bool hasWidescreen;
    char* widescreenScreen;
    char* widescreenComposite;
};

struct SystemDefinition {
    int __rc;
    char* id;
    char* file;
    char* name;
    btrc_Vector_string* aliases;
    btrc_Vector_string* esDePlatforms;
    char* esDeTheme;
    char* romDirectory;
    btrc_Vector_string* extensions;
    btrc_Vector_EmulatorBinding_p1* emulators;
    BiosRequirement* bios;
    char* controllerProfile;
    DisplayPolicy* display;
    RenderPolicy* render;
    BezelCollection* bezels;
    ShaderCollection* shaders;
};

struct SandboxFilesystem {
    int __rc;
    char* path;
    char* mode;
};

struct SandboxPolicy {
    int __rc;
    bool present;
    btrc_Vector_string* sockets;
    btrc_Vector_string* devices;
    btrc_Vector_string* share;
    btrc_Vector_SandboxFilesystem_p1* filesystems;
    btrc_Vector_string* extraArguments;
    char* runtime;
    char* command;
    btrc_Vector_string* symlinkLinks;
    btrc_Vector_string* symlinkTargets;
};

struct GraphicsPolicy {
    int __rc;
    bool present;
    char* method;
    char* file;
    char* format;
    char* key;
    char* valueOpenGl;
    char* valueVulkan;
    char* tapApi;
    char* overlayApi;
};

struct AspectProbe {
    int __rc;
    bool present;
    btrc_Vector_string* files;
    btrc_Vector_string* widescreenMarkers;
    btrc_Vector_string* standardMarkers;
    char* fallback;
};

struct StateSeed {
    int __rc;
    char* purpose;
    char* target;
    btrc_Vector_string* sources;
};

struct StatePolicy {
    int __rc;
    bool present;
    char* configHome;
    char* dataHome;
    btrc_Vector_string* extraArguments;
    btrc_Vector_StateSeed_p1* seeds;
};

struct FirmwarePolicy {
    int __rc;
    bool present;
    char* directory;
    btrc_Vector_string* fallbackDirectories;
};

struct SessionOverride {
    int __rc;
    char* id;
    char* displayServer;
    char* displayFallback;
    char* overlay;
};

struct EmulatorPlatform {
    int __rc;
    char* operatingSystem;
    char* backend;
    char* flatpakId;
    char* executable;
    char* displayServer;
    char* displayFallback;
    btrc_Vector_SessionOverride_p1* sessions;
    btrc_Vector_string* arguments;
    char* glWrapper;
    btrc_Vector_string* environmentSetKeys;
    btrc_Vector_string* environmentSetValues;
    btrc_Vector_string* environmentUnset;
    SandboxPolicy* sandbox;
    GraphicsPolicy* graphics;
    char* overlayAspect;
    AspectProbe* aspectProbe;
    StatePolicy* state;
    FirmwarePolicy* firmware;
    JsonValue* extensionJson;
};

struct EmulatorInput {
    int __rc;
    bool present;
    btrc_Vector_string* actions;
};

struct EmulatorDefinition {
    int __rc;
    char* id;
    char* file;
    char* name;
    char* kind;
    char* defaultSystem;
    EmulatorInput* input;
    btrc_Vector_EmulatorPlatform_p1* platforms;
};

struct SemuContracts {
    int __rc;
    btrc_Vector_SystemDefinition_p1* systems;
    btrc_Vector_EmulatorDefinition_p1* emulators;
};

struct KeymapAction {
    int __rc;
    char* id;
    char* chord;
};

struct KeymapBinding {
    int __rc;
    char* button;
    char* actionId;
};

struct Keymap {
    int __rc;
    btrc_Vector_KeymapAction_p1* actions;
    btrc_Vector_KeymapBinding_p1* bindings;
};

struct SteamInputTarget {
    int __rc;
    char* file;
    char* keymap;
    btrc_Vector_string* modifiers;
    btrc_Vector_string* actionIds;
    btrc_Vector_string* actionAliases;
    btrc_Vector_string* actionLabels;
    char* controllerModel;
    bool gyro;
    char* trackpadLeft;
    char* trackpadRight;
    char* hotkeyButton;
    btrc_Vector_string* radialSlots;
    btrc_Vector_string* quitExtraBindings;
    btrc_Vector_string* templateIds;
    btrc_Vector_string* templateTitles;
    btrc_Vector_string* templateRadialNames;
    btrc_Vector_string* templateOutputs;
    char* defaultTemplateId;
    btrc_Vector_string* keyNameKeys;
    btrc_Vector_string* keyNameValues;
    btrc_Vector_string* deviceIdentityKeys;
    btrc_Vector_string* deviceIdentityValues;
    char* templatesDirectory;
};

struct RetroArchConfiguration {
    int __rc;
    bool present;
    char* videoDriver;
    char* inputDriver;
    char* coreSuffix;
    btrc_Vector_string* coreDirectories;
    btrc_Vector_string* settingsOverrideKeys;
    btrc_Vector_string* settingsOverrideValues;
    btrc_Vector_string* coreOptionEntries;
    btrc_Vector_string* tapBinaries;
};

struct RetroArchEmulator {
    int __rc;
};

struct DolphinEmulator {
    int __rc;
};

struct TemplateContext {
    int __rc;
    char* project;
    char* emulatorId;
    char* backend;
    char* flatpakId;
};

struct Pcsx2Emulator {
    int __rc;
};

struct CemuEmulator {
    int __rc;
};

struct RyujinxEmulator {
    int __rc;
};

struct PpssppEmulator {
    int __rc;
};

struct FlycastEmulator {
    int __rc;
};

struct AzaharEmulator {
    int __rc;
};

struct MelonDsEmulator {
    int __rc;
};

struct SystemEmitter {
    int __rc;
};

struct LaunchPlan {
    int __rc;
    btrc_Vector_string* argumentVector;
    btrc_Vector_string* environmentSetKeys;
    btrc_Vector_string* environmentSetValues;
    btrc_Vector_string* environmentUnset;
    char* backend;
    char* note;
};

struct EmulatorExecution {
    int __rc;
};

struct SteamdeckHotkeyAssignments {
    int __rc;
    btrc_Vector_string* buttons;
    btrc_Vector_string* actionIds;
};

struct ResolvedBezel {
    int __rc;
    char* art;
    char* glass;
    NormalizedRect* hole;
    btrc_Vector_string* screenHoleIds;
    btrc_Vector_NormalizedRect_p1* screenHoleRects;
    double reflect;
    double curvature;
    double cornerRadius;
    RgbColor* shellTint;
    bool fill;
};

struct ShaderSelector {
    int __rc;
};

struct TapEnvironment {
    int __rc;
    btrc_Vector_string* keys;
    btrc_Vector_string* values;
};

struct GithubPin {
    int __rc;
    char* key;
    char* owner;
    char* repository;
    char* revision;
    char* narHash;
};

struct NixPackage {
    int __rc;
};

/* Type-dependent comparison/hashing macros for generic collections.
 * Uses __builtin_choose_expr — unselected branch is NOT evaluated.
 * Cast chain (void*)(intptr_t) avoids float-to-pointer hard errors. */
#define __btrc_eq(a, b) __builtin_choose_expr( \
    __builtin_types_compatible_p(__typeof__(a), char*), \
    strcmp((const char*)(void*)(intptr_t)(a), (const char*)(void*)(intptr_t)(b)) == 0, \
    (a) == (b))
#define __btrc_lt(a, b) __builtin_choose_expr( \
    __builtin_types_compatible_p(__typeof__(a), char*), \
    strcmp((const char*)(void*)(intptr_t)(a), (const char*)(void*)(intptr_t)(b)) < 0, \
    (a) < (b))
#define __btrc_gt(a, b) __builtin_choose_expr( \
    __builtin_types_compatible_p(__typeof__(a), char*), \
    strcmp((const char*)(void*)(intptr_t)(a), (const char*)(void*)(intptr_t)(b)) > 0, \
    (a) > (b))
#define __btrc_hash(k) __builtin_choose_expr( \
    __builtin_types_compatible_p(__typeof__(k), char*), \
    __btrc_hash_str((const char*)(void*)(intptr_t)(k)), \
    (unsigned int)(intptr_t)(k))

static void btrc_Vector_string_init(btrc_Vector_string* self);
static btrc_Vector_string* btrc_Vector_string_new(void);
static void btrc_Vector_string_destroy(btrc_Vector_string* self);
static void btrc_Vector_string_push(btrc_Vector_string* self, char* val);
static char* btrc_Vector_string_get(btrc_Vector_string* self, int i);
static void btrc_Vector_string_set(btrc_Vector_string* self, int i, char* val);
static void btrc_Vector_string_free(btrc_Vector_string* self);
static int btrc_Vector_string_size(btrc_Vector_string* self);
static bool btrc_Vector_string_isEmpty(btrc_Vector_string* self);
static bool btrc_Vector_string_contains(btrc_Vector_string* self, char* val);
static char* btrc_Vector_string_sum(btrc_Vector_string* self);
static char* btrc_Vector_string_join(btrc_Vector_string* self, char* sep);
static int btrc_Vector_string_iterLen(btrc_Vector_string* self);
static char* btrc_Vector_string_iterGet(btrc_Vector_string* self, int i);


static char* btrc_Vector_bool_join(btrc_Vector_bool* self, char* sep);
static char* btrc_Vector_bool_joinToString(btrc_Vector_bool* self, char* sep);

static void btrc_Vector_JsonValue_p1_init(btrc_Vector_JsonValue_p1* self);
static btrc_Vector_JsonValue_p1* btrc_Vector_JsonValue_p1_new(void);
static void btrc_Vector_JsonValue_p1_push(btrc_Vector_JsonValue_p1* self, JsonValue* val);
static JsonValue* btrc_Vector_JsonValue_p1_get(btrc_Vector_JsonValue_p1* self, int i);
static void btrc_Vector_JsonValue_p1_set(btrc_Vector_JsonValue_p1* self, int i, JsonValue* val);
static void btrc_Vector_JsonValue_p1_free(btrc_Vector_JsonValue_p1* self);
static JsonValue* btrc_Vector_JsonValue_p1_sum(btrc_Vector_JsonValue_p1* self);
static char* btrc_Vector_JsonValue_p1_join(btrc_Vector_JsonValue_p1* self, char* sep);
static char* btrc_Vector_JsonValue_p1_joinToString(btrc_Vector_JsonValue_p1* self, char* sep);

static void btrc_Vector_SystemScreen_p1_init(btrc_Vector_SystemScreen_p1* self);
static btrc_Vector_SystemScreen_p1* btrc_Vector_SystemScreen_p1_new(void);
static void btrc_Vector_SystemScreen_p1_push(btrc_Vector_SystemScreen_p1* self, SystemScreen* val);
static void btrc_Vector_SystemScreen_p1_free(btrc_Vector_SystemScreen_p1* self);
static int btrc_Vector_SystemScreen_p1_size(btrc_Vector_SystemScreen_p1* self);
static SystemScreen* btrc_Vector_SystemScreen_p1_sum(btrc_Vector_SystemScreen_p1* self);
static char* btrc_Vector_SystemScreen_p1_join(btrc_Vector_SystemScreen_p1* self, char* sep);
static char* btrc_Vector_SystemScreen_p1_joinToString(btrc_Vector_SystemScreen_p1* self, char* sep);
static int btrc_Vector_SystemScreen_p1_iterLen(btrc_Vector_SystemScreen_p1* self);
static SystemScreen* btrc_Vector_SystemScreen_p1_iterGet(btrc_Vector_SystemScreen_p1* self, int i);

static void btrc_Vector_BiosFile_p1_init(btrc_Vector_BiosFile_p1* self);
static btrc_Vector_BiosFile_p1* btrc_Vector_BiosFile_p1_new(void);
static void btrc_Vector_BiosFile_p1_push(btrc_Vector_BiosFile_p1* self, BiosFile* val);
static void btrc_Vector_BiosFile_p1_free(btrc_Vector_BiosFile_p1* self);
static BiosFile* btrc_Vector_BiosFile_p1_sum(btrc_Vector_BiosFile_p1* self);
static char* btrc_Vector_BiosFile_p1_join(btrc_Vector_BiosFile_p1* self, char* sep);
static char* btrc_Vector_BiosFile_p1_joinToString(btrc_Vector_BiosFile_p1* self, char* sep);
static int btrc_Vector_BiosFile_p1_iterLen(btrc_Vector_BiosFile_p1* self);
static BiosFile* btrc_Vector_BiosFile_p1_iterGet(btrc_Vector_BiosFile_p1* self, int i);

static void btrc_Vector_ScreenHole_p1_init(btrc_Vector_ScreenHole_p1* self);
static btrc_Vector_ScreenHole_p1* btrc_Vector_ScreenHole_p1_new(void);
static void btrc_Vector_ScreenHole_p1_destroy(btrc_Vector_ScreenHole_p1* self);
static void btrc_Vector_ScreenHole_p1_push(btrc_Vector_ScreenHole_p1* self, ScreenHole* val);
static void btrc_Vector_ScreenHole_p1_free(btrc_Vector_ScreenHole_p1* self);
static int btrc_Vector_ScreenHole_p1_size(btrc_Vector_ScreenHole_p1* self);
static ScreenHole* btrc_Vector_ScreenHole_p1_sum(btrc_Vector_ScreenHole_p1* self);
static char* btrc_Vector_ScreenHole_p1_join(btrc_Vector_ScreenHole_p1* self, char* sep);
static char* btrc_Vector_ScreenHole_p1_joinToString(btrc_Vector_ScreenHole_p1* self, char* sep);
static int btrc_Vector_ScreenHole_p1_iterLen(btrc_Vector_ScreenHole_p1* self);
static ScreenHole* btrc_Vector_ScreenHole_p1_iterGet(btrc_Vector_ScreenHole_p1* self, int i);

static void btrc_Vector_BezelVariant_p1_init(btrc_Vector_BezelVariant_p1* self);
static btrc_Vector_BezelVariant_p1* btrc_Vector_BezelVariant_p1_new(void);
static void btrc_Vector_BezelVariant_p1_push(btrc_Vector_BezelVariant_p1* self, BezelVariant* val);
static BezelVariant* btrc_Vector_BezelVariant_p1_get(btrc_Vector_BezelVariant_p1* self, int i);
static void btrc_Vector_BezelVariant_p1_free(btrc_Vector_BezelVariant_p1* self);
static int btrc_Vector_BezelVariant_p1_size(btrc_Vector_BezelVariant_p1* self);
static BezelVariant* btrc_Vector_BezelVariant_p1_sum(btrc_Vector_BezelVariant_p1* self);
static char* btrc_Vector_BezelVariant_p1_join(btrc_Vector_BezelVariant_p1* self, char* sep);
static char* btrc_Vector_BezelVariant_p1_joinToString(btrc_Vector_BezelVariant_p1* self, char* sep);
static int btrc_Vector_BezelVariant_p1_iterLen(btrc_Vector_BezelVariant_p1* self);
static BezelVariant* btrc_Vector_BezelVariant_p1_iterGet(btrc_Vector_BezelVariant_p1* self, int i);

static void btrc_Vector_EmulatorBinding_p1_init(btrc_Vector_EmulatorBinding_p1* self);
static btrc_Vector_EmulatorBinding_p1* btrc_Vector_EmulatorBinding_p1_new(void);
static void btrc_Vector_EmulatorBinding_p1_push(btrc_Vector_EmulatorBinding_p1* self, EmulatorBinding* val);
static EmulatorBinding* btrc_Vector_EmulatorBinding_p1_get(btrc_Vector_EmulatorBinding_p1* self, int i);
static void btrc_Vector_EmulatorBinding_p1_free(btrc_Vector_EmulatorBinding_p1* self);
static int btrc_Vector_EmulatorBinding_p1_size(btrc_Vector_EmulatorBinding_p1* self);
static EmulatorBinding* btrc_Vector_EmulatorBinding_p1_sum(btrc_Vector_EmulatorBinding_p1* self);
static char* btrc_Vector_EmulatorBinding_p1_join(btrc_Vector_EmulatorBinding_p1* self, char* sep);
static char* btrc_Vector_EmulatorBinding_p1_joinToString(btrc_Vector_EmulatorBinding_p1* self, char* sep);
static int btrc_Vector_EmulatorBinding_p1_iterLen(btrc_Vector_EmulatorBinding_p1* self);
static EmulatorBinding* btrc_Vector_EmulatorBinding_p1_iterGet(btrc_Vector_EmulatorBinding_p1* self, int i);

static void btrc_Vector_SandboxFilesystem_p1_init(btrc_Vector_SandboxFilesystem_p1* self);
static btrc_Vector_SandboxFilesystem_p1* btrc_Vector_SandboxFilesystem_p1_new(void);
static void btrc_Vector_SandboxFilesystem_p1_push(btrc_Vector_SandboxFilesystem_p1* self, SandboxFilesystem* val);
static void btrc_Vector_SandboxFilesystem_p1_free(btrc_Vector_SandboxFilesystem_p1* self);
static SandboxFilesystem* btrc_Vector_SandboxFilesystem_p1_sum(btrc_Vector_SandboxFilesystem_p1* self);
static char* btrc_Vector_SandboxFilesystem_p1_join(btrc_Vector_SandboxFilesystem_p1* self, char* sep);
static char* btrc_Vector_SandboxFilesystem_p1_joinToString(btrc_Vector_SandboxFilesystem_p1* self, char* sep);
static int btrc_Vector_SandboxFilesystem_p1_iterLen(btrc_Vector_SandboxFilesystem_p1* self);
static SandboxFilesystem* btrc_Vector_SandboxFilesystem_p1_iterGet(btrc_Vector_SandboxFilesystem_p1* self, int i);

static void btrc_Vector_StateSeed_p1_init(btrc_Vector_StateSeed_p1* self);
static btrc_Vector_StateSeed_p1* btrc_Vector_StateSeed_p1_new(void);
static void btrc_Vector_StateSeed_p1_push(btrc_Vector_StateSeed_p1* self, StateSeed* val);
static void btrc_Vector_StateSeed_p1_free(btrc_Vector_StateSeed_p1* self);
static StateSeed* btrc_Vector_StateSeed_p1_sum(btrc_Vector_StateSeed_p1* self);
static char* btrc_Vector_StateSeed_p1_join(btrc_Vector_StateSeed_p1* self, char* sep);
static char* btrc_Vector_StateSeed_p1_joinToString(btrc_Vector_StateSeed_p1* self, char* sep);
static int btrc_Vector_StateSeed_p1_iterLen(btrc_Vector_StateSeed_p1* self);
static StateSeed* btrc_Vector_StateSeed_p1_iterGet(btrc_Vector_StateSeed_p1* self, int i);

static void btrc_Vector_SessionOverride_p1_init(btrc_Vector_SessionOverride_p1* self);
static btrc_Vector_SessionOverride_p1* btrc_Vector_SessionOverride_p1_new(void);
static void btrc_Vector_SessionOverride_p1_push(btrc_Vector_SessionOverride_p1* self, SessionOverride* val);
static void btrc_Vector_SessionOverride_p1_free(btrc_Vector_SessionOverride_p1* self);
static SessionOverride* btrc_Vector_SessionOverride_p1_sum(btrc_Vector_SessionOverride_p1* self);
static char* btrc_Vector_SessionOverride_p1_join(btrc_Vector_SessionOverride_p1* self, char* sep);
static char* btrc_Vector_SessionOverride_p1_joinToString(btrc_Vector_SessionOverride_p1* self, char* sep);
static int btrc_Vector_SessionOverride_p1_iterLen(btrc_Vector_SessionOverride_p1* self);
static SessionOverride* btrc_Vector_SessionOverride_p1_iterGet(btrc_Vector_SessionOverride_p1* self, int i);

static void btrc_Vector_EmulatorPlatform_p1_init(btrc_Vector_EmulatorPlatform_p1* self);
static btrc_Vector_EmulatorPlatform_p1* btrc_Vector_EmulatorPlatform_p1_new(void);
static void btrc_Vector_EmulatorPlatform_p1_push(btrc_Vector_EmulatorPlatform_p1* self, EmulatorPlatform* val);
static EmulatorPlatform* btrc_Vector_EmulatorPlatform_p1_get(btrc_Vector_EmulatorPlatform_p1* self, int i);
static void btrc_Vector_EmulatorPlatform_p1_free(btrc_Vector_EmulatorPlatform_p1* self);
static int btrc_Vector_EmulatorPlatform_p1_size(btrc_Vector_EmulatorPlatform_p1* self);
static EmulatorPlatform* btrc_Vector_EmulatorPlatform_p1_sum(btrc_Vector_EmulatorPlatform_p1* self);
static char* btrc_Vector_EmulatorPlatform_p1_join(btrc_Vector_EmulatorPlatform_p1* self, char* sep);
static char* btrc_Vector_EmulatorPlatform_p1_joinToString(btrc_Vector_EmulatorPlatform_p1* self, char* sep);
static int btrc_Vector_EmulatorPlatform_p1_iterLen(btrc_Vector_EmulatorPlatform_p1* self);
static EmulatorPlatform* btrc_Vector_EmulatorPlatform_p1_iterGet(btrc_Vector_EmulatorPlatform_p1* self, int i);

static void btrc_Vector_SystemDefinition_p1_init(btrc_Vector_SystemDefinition_p1* self);
static btrc_Vector_SystemDefinition_p1* btrc_Vector_SystemDefinition_p1_new(void);
static void btrc_Vector_SystemDefinition_p1_push(btrc_Vector_SystemDefinition_p1* self, SystemDefinition* val);
static void btrc_Vector_SystemDefinition_p1_free(btrc_Vector_SystemDefinition_p1* self);
static int btrc_Vector_SystemDefinition_p1_size(btrc_Vector_SystemDefinition_p1* self);
static SystemDefinition* btrc_Vector_SystemDefinition_p1_sum(btrc_Vector_SystemDefinition_p1* self);
static char* btrc_Vector_SystemDefinition_p1_join(btrc_Vector_SystemDefinition_p1* self, char* sep);
static char* btrc_Vector_SystemDefinition_p1_joinToString(btrc_Vector_SystemDefinition_p1* self, char* sep);
static int btrc_Vector_SystemDefinition_p1_iterLen(btrc_Vector_SystemDefinition_p1* self);
static SystemDefinition* btrc_Vector_SystemDefinition_p1_iterGet(btrc_Vector_SystemDefinition_p1* self, int i);

static void btrc_Vector_EmulatorDefinition_p1_init(btrc_Vector_EmulatorDefinition_p1* self);
static btrc_Vector_EmulatorDefinition_p1* btrc_Vector_EmulatorDefinition_p1_new(void);
static void btrc_Vector_EmulatorDefinition_p1_destroy(btrc_Vector_EmulatorDefinition_p1* self);
static void btrc_Vector_EmulatorDefinition_p1_push(btrc_Vector_EmulatorDefinition_p1* self, EmulatorDefinition* val);
static void btrc_Vector_EmulatorDefinition_p1_free(btrc_Vector_EmulatorDefinition_p1* self);
static int btrc_Vector_EmulatorDefinition_p1_size(btrc_Vector_EmulatorDefinition_p1* self);
static EmulatorDefinition* btrc_Vector_EmulatorDefinition_p1_sum(btrc_Vector_EmulatorDefinition_p1* self);
static char* btrc_Vector_EmulatorDefinition_p1_join(btrc_Vector_EmulatorDefinition_p1* self, char* sep);
static char* btrc_Vector_EmulatorDefinition_p1_joinToString(btrc_Vector_EmulatorDefinition_p1* self, char* sep);
static int btrc_Vector_EmulatorDefinition_p1_iterLen(btrc_Vector_EmulatorDefinition_p1* self);
static EmulatorDefinition* btrc_Vector_EmulatorDefinition_p1_iterGet(btrc_Vector_EmulatorDefinition_p1* self, int i);

static void btrc_Vector_KeymapAction_p1_init(btrc_Vector_KeymapAction_p1* self);
static btrc_Vector_KeymapAction_p1* btrc_Vector_KeymapAction_p1_new(void);
static void btrc_Vector_KeymapAction_p1_push(btrc_Vector_KeymapAction_p1* self, KeymapAction* val);
static void btrc_Vector_KeymapAction_p1_free(btrc_Vector_KeymapAction_p1* self);
static int btrc_Vector_KeymapAction_p1_size(btrc_Vector_KeymapAction_p1* self);
static KeymapAction* btrc_Vector_KeymapAction_p1_sum(btrc_Vector_KeymapAction_p1* self);
static char* btrc_Vector_KeymapAction_p1_join(btrc_Vector_KeymapAction_p1* self, char* sep);
static char* btrc_Vector_KeymapAction_p1_joinToString(btrc_Vector_KeymapAction_p1* self, char* sep);
static int btrc_Vector_KeymapAction_p1_iterLen(btrc_Vector_KeymapAction_p1* self);
static KeymapAction* btrc_Vector_KeymapAction_p1_iterGet(btrc_Vector_KeymapAction_p1* self, int i);

static void btrc_Vector_KeymapBinding_p1_init(btrc_Vector_KeymapBinding_p1* self);
static btrc_Vector_KeymapBinding_p1* btrc_Vector_KeymapBinding_p1_new(void);
static void btrc_Vector_KeymapBinding_p1_push(btrc_Vector_KeymapBinding_p1* self, KeymapBinding* val);
static void btrc_Vector_KeymapBinding_p1_free(btrc_Vector_KeymapBinding_p1* self);
static int btrc_Vector_KeymapBinding_p1_size(btrc_Vector_KeymapBinding_p1* self);
static KeymapBinding* btrc_Vector_KeymapBinding_p1_sum(btrc_Vector_KeymapBinding_p1* self);
static char* btrc_Vector_KeymapBinding_p1_join(btrc_Vector_KeymapBinding_p1* self, char* sep);
static char* btrc_Vector_KeymapBinding_p1_joinToString(btrc_Vector_KeymapBinding_p1* self, char* sep);
static int btrc_Vector_KeymapBinding_p1_iterLen(btrc_Vector_KeymapBinding_p1* self);
static KeymapBinding* btrc_Vector_KeymapBinding_p1_iterGet(btrc_Vector_KeymapBinding_p1* self, int i);

static void btrc_Vector_NormalizedRect_p1_init(btrc_Vector_NormalizedRect_p1* self);
static btrc_Vector_NormalizedRect_p1* btrc_Vector_NormalizedRect_p1_new(void);
static void btrc_Vector_NormalizedRect_p1_push(btrc_Vector_NormalizedRect_p1* self, NormalizedRect* val);
static void btrc_Vector_NormalizedRect_p1_free(btrc_Vector_NormalizedRect_p1* self);
static NormalizedRect* btrc_Vector_NormalizedRect_p1_sum(btrc_Vector_NormalizedRect_p1* self);
static char* btrc_Vector_NormalizedRect_p1_join(btrc_Vector_NormalizedRect_p1* self, char* sep);
static char* btrc_Vector_NormalizedRect_p1_joinToString(btrc_Vector_NormalizedRect_p1* self, char* sep);

static void btrc_Vector_GithubPin_p1_init(btrc_Vector_GithubPin_p1* self);
static btrc_Vector_GithubPin_p1* btrc_Vector_GithubPin_p1_new(void);
static void btrc_Vector_GithubPin_p1_destroy(btrc_Vector_GithubPin_p1* self);
static void btrc_Vector_GithubPin_p1_push(btrc_Vector_GithubPin_p1* self, GithubPin* val);
static void btrc_Vector_GithubPin_p1_free(btrc_Vector_GithubPin_p1* self);
static GithubPin* btrc_Vector_GithubPin_p1_sum(btrc_Vector_GithubPin_p1* self);
static char* btrc_Vector_GithubPin_p1_join(btrc_Vector_GithubPin_p1* self, char* sep);
static char* btrc_Vector_GithubPin_p1_joinToString(btrc_Vector_GithubPin_p1* self, char* sep);
static int btrc_Vector_GithubPin_p1_iterLen(btrc_Vector_GithubPin_p1* self);
static GithubPin* btrc_Vector_GithubPin_p1_iterGet(btrc_Vector_GithubPin_p1* self, int i);

static void JsonValue_visit(JsonValue* self, void (*fn)(void**)) {
    (void)self;
    (void)fn;
}

static void btrc_Vector_string_init(btrc_Vector_string* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_string* btrc_Vector_string_new(void) {
    btrc_Vector_string* self = ((btrc_Vector_string*)malloc(sizeof(btrc_Vector_string)));
    memset(self, 0, sizeof(btrc_Vector_string));
    btrc_Vector_string_init(self);
    return self;
}

static void btrc_Vector_string_destroy(btrc_Vector_string* self) {
    btrc_Vector_string_free(self);
    free(self);
}

static void btrc_Vector_string_push(btrc_Vector_string* self, char* val) {
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((char**)__btrc_safe_realloc(self->data, (sizeof(char*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static char* btrc_Vector_string_get(btrc_Vector_string* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_string_set(btrc_Vector_string* self, int i, char* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (self->data[i] = val);
}

static void btrc_Vector_string_free(btrc_Vector_string* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_string_size(btrc_Vector_string* self) {
    return self->len;
}

static bool btrc_Vector_string_isEmpty(btrc_Vector_string* self) {
    return (self->len == 0);
}

static bool btrc_Vector_string_contains(btrc_Vector_string* self, char* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static char* btrc_Vector_string_join(btrc_Vector_string* self, char* sep) {
    int total = 0;
    int sep_len = ((int)strlen(sep));
    for (int i = 0; (i < self->len); (i++)) {
        (total = (total + ((int)strlen(self->data[i]))));
        if (i < (self->len - 1)) {
            (total = (total + sep_len));
        }
    }
    char* result = ((char*)malloc((total + 1)));
    int pos = 0;
    for (int i = 0; (i < self->len); (i++)) {
        int slen = ((int)strlen(self->data[i]));
        memcpy((result + pos), self->data[i], slen);
        (pos = (pos + slen));
        if (i < (self->len - 1)) {
            memcpy((result + pos), sep, sep_len);
            (pos = (pos + sep_len));
        }
    }
    (result[pos] = '\0');
    return result;
}

static int btrc_Vector_string_iterLen(btrc_Vector_string* self) {
    return self->len;
}

static char* btrc_Vector_string_iterGet(btrc_Vector_string* self, int i) {
    return self->data[i];
}

static void btrc_Vector_JsonValue_p1_init(btrc_Vector_JsonValue_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_JsonValue_p1* btrc_Vector_JsonValue_p1_new(void) {
    btrc_Vector_JsonValue_p1* self = ((btrc_Vector_JsonValue_p1*)malloc(sizeof(btrc_Vector_JsonValue_p1)));
    memset(self, 0, sizeof(btrc_Vector_JsonValue_p1));
    btrc_Vector_JsonValue_p1_init(self);
    return self;
}

static void btrc_Vector_JsonValue_p1_push(btrc_Vector_JsonValue_p1* self, JsonValue* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((JsonValue**)__btrc_safe_realloc(self->data, (sizeof(JsonValue*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static JsonValue* btrc_Vector_JsonValue_p1_get(btrc_Vector_JsonValue_p1* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_JsonValue_p1_set(btrc_Vector_JsonValue_p1* self, int i, JsonValue* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            JsonValue_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_JsonValue_p1_free(btrc_Vector_JsonValue_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                JsonValue_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SystemScreen_p1_init(btrc_Vector_SystemScreen_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SystemScreen_p1* btrc_Vector_SystemScreen_p1_new(void) {
    btrc_Vector_SystemScreen_p1* self = ((btrc_Vector_SystemScreen_p1*)malloc(sizeof(btrc_Vector_SystemScreen_p1)));
    memset(self, 0, sizeof(btrc_Vector_SystemScreen_p1));
    btrc_Vector_SystemScreen_p1_init(self);
    return self;
}

static void btrc_Vector_SystemScreen_p1_push(btrc_Vector_SystemScreen_p1* self, SystemScreen* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SystemScreen**)__btrc_safe_realloc(self->data, (sizeof(SystemScreen*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_SystemScreen_p1_free(btrc_Vector_SystemScreen_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SystemScreen_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_SystemScreen_p1_size(btrc_Vector_SystemScreen_p1* self) {
    return self->len;
}

static int btrc_Vector_SystemScreen_p1_iterLen(btrc_Vector_SystemScreen_p1* self) {
    return self->len;
}

static SystemScreen* btrc_Vector_SystemScreen_p1_iterGet(btrc_Vector_SystemScreen_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_BiosFile_p1_init(btrc_Vector_BiosFile_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_BiosFile_p1* btrc_Vector_BiosFile_p1_new(void) {
    btrc_Vector_BiosFile_p1* self = ((btrc_Vector_BiosFile_p1*)malloc(sizeof(btrc_Vector_BiosFile_p1)));
    memset(self, 0, sizeof(btrc_Vector_BiosFile_p1));
    btrc_Vector_BiosFile_p1_init(self);
    return self;
}

static void btrc_Vector_BiosFile_p1_push(btrc_Vector_BiosFile_p1* self, BiosFile* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((BiosFile**)__btrc_safe_realloc(self->data, (sizeof(BiosFile*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_BiosFile_p1_free(btrc_Vector_BiosFile_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                BiosFile_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_BiosFile_p1_iterLen(btrc_Vector_BiosFile_p1* self) {
    return self->len;
}

static BiosFile* btrc_Vector_BiosFile_p1_iterGet(btrc_Vector_BiosFile_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_ScreenHole_p1_init(btrc_Vector_ScreenHole_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_ScreenHole_p1* btrc_Vector_ScreenHole_p1_new(void) {
    btrc_Vector_ScreenHole_p1* self = ((btrc_Vector_ScreenHole_p1*)malloc(sizeof(btrc_Vector_ScreenHole_p1)));
    memset(self, 0, sizeof(btrc_Vector_ScreenHole_p1));
    btrc_Vector_ScreenHole_p1_init(self);
    return self;
}

static void btrc_Vector_ScreenHole_p1_destroy(btrc_Vector_ScreenHole_p1* self) {
    btrc_Vector_ScreenHole_p1_free(self);
    free(self);
}

static void btrc_Vector_ScreenHole_p1_push(btrc_Vector_ScreenHole_p1* self, ScreenHole* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((ScreenHole**)__btrc_safe_realloc(self->data, (sizeof(ScreenHole*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_ScreenHole_p1_free(btrc_Vector_ScreenHole_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                ScreenHole_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_ScreenHole_p1_size(btrc_Vector_ScreenHole_p1* self) {
    return self->len;
}

static int btrc_Vector_ScreenHole_p1_iterLen(btrc_Vector_ScreenHole_p1* self) {
    return self->len;
}

static ScreenHole* btrc_Vector_ScreenHole_p1_iterGet(btrc_Vector_ScreenHole_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_BezelVariant_p1_init(btrc_Vector_BezelVariant_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_BezelVariant_p1* btrc_Vector_BezelVariant_p1_new(void) {
    btrc_Vector_BezelVariant_p1* self = ((btrc_Vector_BezelVariant_p1*)malloc(sizeof(btrc_Vector_BezelVariant_p1)));
    memset(self, 0, sizeof(btrc_Vector_BezelVariant_p1));
    btrc_Vector_BezelVariant_p1_init(self);
    return self;
}

static void btrc_Vector_BezelVariant_p1_push(btrc_Vector_BezelVariant_p1* self, BezelVariant* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((BezelVariant**)__btrc_safe_realloc(self->data, (sizeof(BezelVariant*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static BezelVariant* btrc_Vector_BezelVariant_p1_get(btrc_Vector_BezelVariant_p1* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_BezelVariant_p1_free(btrc_Vector_BezelVariant_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                BezelVariant_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_BezelVariant_p1_size(btrc_Vector_BezelVariant_p1* self) {
    return self->len;
}

static int btrc_Vector_BezelVariant_p1_iterLen(btrc_Vector_BezelVariant_p1* self) {
    return self->len;
}

static BezelVariant* btrc_Vector_BezelVariant_p1_iterGet(btrc_Vector_BezelVariant_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_EmulatorBinding_p1_init(btrc_Vector_EmulatorBinding_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_EmulatorBinding_p1* btrc_Vector_EmulatorBinding_p1_new(void) {
    btrc_Vector_EmulatorBinding_p1* self = ((btrc_Vector_EmulatorBinding_p1*)malloc(sizeof(btrc_Vector_EmulatorBinding_p1)));
    memset(self, 0, sizeof(btrc_Vector_EmulatorBinding_p1));
    btrc_Vector_EmulatorBinding_p1_init(self);
    return self;
}

static void btrc_Vector_EmulatorBinding_p1_push(btrc_Vector_EmulatorBinding_p1* self, EmulatorBinding* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((EmulatorBinding**)__btrc_safe_realloc(self->data, (sizeof(EmulatorBinding*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static EmulatorBinding* btrc_Vector_EmulatorBinding_p1_get(btrc_Vector_EmulatorBinding_p1* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_EmulatorBinding_p1_free(btrc_Vector_EmulatorBinding_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                EmulatorBinding_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_EmulatorBinding_p1_size(btrc_Vector_EmulatorBinding_p1* self) {
    return self->len;
}

static int btrc_Vector_EmulatorBinding_p1_iterLen(btrc_Vector_EmulatorBinding_p1* self) {
    return self->len;
}

static EmulatorBinding* btrc_Vector_EmulatorBinding_p1_iterGet(btrc_Vector_EmulatorBinding_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SandboxFilesystem_p1_init(btrc_Vector_SandboxFilesystem_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SandboxFilesystem_p1* btrc_Vector_SandboxFilesystem_p1_new(void) {
    btrc_Vector_SandboxFilesystem_p1* self = ((btrc_Vector_SandboxFilesystem_p1*)malloc(sizeof(btrc_Vector_SandboxFilesystem_p1)));
    memset(self, 0, sizeof(btrc_Vector_SandboxFilesystem_p1));
    btrc_Vector_SandboxFilesystem_p1_init(self);
    return self;
}

static void btrc_Vector_SandboxFilesystem_p1_push(btrc_Vector_SandboxFilesystem_p1* self, SandboxFilesystem* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SandboxFilesystem**)__btrc_safe_realloc(self->data, (sizeof(SandboxFilesystem*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_SandboxFilesystem_p1_free(btrc_Vector_SandboxFilesystem_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SandboxFilesystem_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_SandboxFilesystem_p1_iterLen(btrc_Vector_SandboxFilesystem_p1* self) {
    return self->len;
}

static SandboxFilesystem* btrc_Vector_SandboxFilesystem_p1_iterGet(btrc_Vector_SandboxFilesystem_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_StateSeed_p1_init(btrc_Vector_StateSeed_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_StateSeed_p1* btrc_Vector_StateSeed_p1_new(void) {
    btrc_Vector_StateSeed_p1* self = ((btrc_Vector_StateSeed_p1*)malloc(sizeof(btrc_Vector_StateSeed_p1)));
    memset(self, 0, sizeof(btrc_Vector_StateSeed_p1));
    btrc_Vector_StateSeed_p1_init(self);
    return self;
}

static void btrc_Vector_StateSeed_p1_push(btrc_Vector_StateSeed_p1* self, StateSeed* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((StateSeed**)__btrc_safe_realloc(self->data, (sizeof(StateSeed*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_StateSeed_p1_free(btrc_Vector_StateSeed_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                StateSeed_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_StateSeed_p1_iterLen(btrc_Vector_StateSeed_p1* self) {
    return self->len;
}

static StateSeed* btrc_Vector_StateSeed_p1_iterGet(btrc_Vector_StateSeed_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SessionOverride_p1_init(btrc_Vector_SessionOverride_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SessionOverride_p1* btrc_Vector_SessionOverride_p1_new(void) {
    btrc_Vector_SessionOverride_p1* self = ((btrc_Vector_SessionOverride_p1*)malloc(sizeof(btrc_Vector_SessionOverride_p1)));
    memset(self, 0, sizeof(btrc_Vector_SessionOverride_p1));
    btrc_Vector_SessionOverride_p1_init(self);
    return self;
}

static void btrc_Vector_SessionOverride_p1_push(btrc_Vector_SessionOverride_p1* self, SessionOverride* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SessionOverride**)__btrc_safe_realloc(self->data, (sizeof(SessionOverride*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_SessionOverride_p1_free(btrc_Vector_SessionOverride_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SessionOverride_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_SessionOverride_p1_iterLen(btrc_Vector_SessionOverride_p1* self) {
    return self->len;
}

static SessionOverride* btrc_Vector_SessionOverride_p1_iterGet(btrc_Vector_SessionOverride_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_EmulatorPlatform_p1_init(btrc_Vector_EmulatorPlatform_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_EmulatorPlatform_p1* btrc_Vector_EmulatorPlatform_p1_new(void) {
    btrc_Vector_EmulatorPlatform_p1* self = ((btrc_Vector_EmulatorPlatform_p1*)malloc(sizeof(btrc_Vector_EmulatorPlatform_p1)));
    memset(self, 0, sizeof(btrc_Vector_EmulatorPlatform_p1));
    btrc_Vector_EmulatorPlatform_p1_init(self);
    return self;
}

static void btrc_Vector_EmulatorPlatform_p1_push(btrc_Vector_EmulatorPlatform_p1* self, EmulatorPlatform* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((EmulatorPlatform**)__btrc_safe_realloc(self->data, (sizeof(EmulatorPlatform*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static EmulatorPlatform* btrc_Vector_EmulatorPlatform_p1_get(btrc_Vector_EmulatorPlatform_p1* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_EmulatorPlatform_p1_free(btrc_Vector_EmulatorPlatform_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                EmulatorPlatform_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_EmulatorPlatform_p1_size(btrc_Vector_EmulatorPlatform_p1* self) {
    return self->len;
}

static int btrc_Vector_EmulatorPlatform_p1_iterLen(btrc_Vector_EmulatorPlatform_p1* self) {
    return self->len;
}

static EmulatorPlatform* btrc_Vector_EmulatorPlatform_p1_iterGet(btrc_Vector_EmulatorPlatform_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SystemDefinition_p1_init(btrc_Vector_SystemDefinition_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SystemDefinition_p1* btrc_Vector_SystemDefinition_p1_new(void) {
    btrc_Vector_SystemDefinition_p1* self = ((btrc_Vector_SystemDefinition_p1*)malloc(sizeof(btrc_Vector_SystemDefinition_p1)));
    memset(self, 0, sizeof(btrc_Vector_SystemDefinition_p1));
    btrc_Vector_SystemDefinition_p1_init(self);
    return self;
}

static void btrc_Vector_SystemDefinition_p1_push(btrc_Vector_SystemDefinition_p1* self, SystemDefinition* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SystemDefinition**)__btrc_safe_realloc(self->data, (sizeof(SystemDefinition*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_SystemDefinition_p1_free(btrc_Vector_SystemDefinition_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SystemDefinition_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_SystemDefinition_p1_size(btrc_Vector_SystemDefinition_p1* self) {
    return self->len;
}

static int btrc_Vector_SystemDefinition_p1_iterLen(btrc_Vector_SystemDefinition_p1* self) {
    return self->len;
}

static SystemDefinition* btrc_Vector_SystemDefinition_p1_iterGet(btrc_Vector_SystemDefinition_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_EmulatorDefinition_p1_init(btrc_Vector_EmulatorDefinition_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_EmulatorDefinition_p1* btrc_Vector_EmulatorDefinition_p1_new(void) {
    btrc_Vector_EmulatorDefinition_p1* self = ((btrc_Vector_EmulatorDefinition_p1*)malloc(sizeof(btrc_Vector_EmulatorDefinition_p1)));
    memset(self, 0, sizeof(btrc_Vector_EmulatorDefinition_p1));
    btrc_Vector_EmulatorDefinition_p1_init(self);
    return self;
}

static void btrc_Vector_EmulatorDefinition_p1_destroy(btrc_Vector_EmulatorDefinition_p1* self) {
    btrc_Vector_EmulatorDefinition_p1_free(self);
    free(self);
}

static void btrc_Vector_EmulatorDefinition_p1_push(btrc_Vector_EmulatorDefinition_p1* self, EmulatorDefinition* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((EmulatorDefinition**)__btrc_safe_realloc(self->data, (sizeof(EmulatorDefinition*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_EmulatorDefinition_p1_free(btrc_Vector_EmulatorDefinition_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                EmulatorDefinition_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_EmulatorDefinition_p1_size(btrc_Vector_EmulatorDefinition_p1* self) {
    return self->len;
}

static int btrc_Vector_EmulatorDefinition_p1_iterLen(btrc_Vector_EmulatorDefinition_p1* self) {
    return self->len;
}

static EmulatorDefinition* btrc_Vector_EmulatorDefinition_p1_iterGet(btrc_Vector_EmulatorDefinition_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_KeymapAction_p1_init(btrc_Vector_KeymapAction_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_KeymapAction_p1* btrc_Vector_KeymapAction_p1_new(void) {
    btrc_Vector_KeymapAction_p1* self = ((btrc_Vector_KeymapAction_p1*)malloc(sizeof(btrc_Vector_KeymapAction_p1)));
    memset(self, 0, sizeof(btrc_Vector_KeymapAction_p1));
    btrc_Vector_KeymapAction_p1_init(self);
    return self;
}

static void btrc_Vector_KeymapAction_p1_push(btrc_Vector_KeymapAction_p1* self, KeymapAction* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((KeymapAction**)__btrc_safe_realloc(self->data, (sizeof(KeymapAction*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_KeymapAction_p1_free(btrc_Vector_KeymapAction_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                KeymapAction_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_KeymapAction_p1_size(btrc_Vector_KeymapAction_p1* self) {
    return self->len;
}

static int btrc_Vector_KeymapAction_p1_iterLen(btrc_Vector_KeymapAction_p1* self) {
    return self->len;
}

static KeymapAction* btrc_Vector_KeymapAction_p1_iterGet(btrc_Vector_KeymapAction_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_KeymapBinding_p1_init(btrc_Vector_KeymapBinding_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_KeymapBinding_p1* btrc_Vector_KeymapBinding_p1_new(void) {
    btrc_Vector_KeymapBinding_p1* self = ((btrc_Vector_KeymapBinding_p1*)malloc(sizeof(btrc_Vector_KeymapBinding_p1)));
    memset(self, 0, sizeof(btrc_Vector_KeymapBinding_p1));
    btrc_Vector_KeymapBinding_p1_init(self);
    return self;
}

static void btrc_Vector_KeymapBinding_p1_push(btrc_Vector_KeymapBinding_p1* self, KeymapBinding* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((KeymapBinding**)__btrc_safe_realloc(self->data, (sizeof(KeymapBinding*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_KeymapBinding_p1_free(btrc_Vector_KeymapBinding_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                KeymapBinding_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_KeymapBinding_p1_size(btrc_Vector_KeymapBinding_p1* self) {
    return self->len;
}

static int btrc_Vector_KeymapBinding_p1_iterLen(btrc_Vector_KeymapBinding_p1* self) {
    return self->len;
}

static KeymapBinding* btrc_Vector_KeymapBinding_p1_iterGet(btrc_Vector_KeymapBinding_p1* self, int i) {
    return self->data[i];
}

static void btrc_Vector_NormalizedRect_p1_init(btrc_Vector_NormalizedRect_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_NormalizedRect_p1* btrc_Vector_NormalizedRect_p1_new(void) {
    btrc_Vector_NormalizedRect_p1* self = ((btrc_Vector_NormalizedRect_p1*)malloc(sizeof(btrc_Vector_NormalizedRect_p1)));
    memset(self, 0, sizeof(btrc_Vector_NormalizedRect_p1));
    btrc_Vector_NormalizedRect_p1_init(self);
    return self;
}

static void btrc_Vector_NormalizedRect_p1_push(btrc_Vector_NormalizedRect_p1* self, NormalizedRect* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((NormalizedRect**)__btrc_safe_realloc(self->data, (sizeof(NormalizedRect*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_NormalizedRect_p1_free(btrc_Vector_NormalizedRect_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                NormalizedRect_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_GithubPin_p1_init(btrc_Vector_GithubPin_p1* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_GithubPin_p1* btrc_Vector_GithubPin_p1_new(void) {
    btrc_Vector_GithubPin_p1* self = ((btrc_Vector_GithubPin_p1*)malloc(sizeof(btrc_Vector_GithubPin_p1)));
    memset(self, 0, sizeof(btrc_Vector_GithubPin_p1));
    btrc_Vector_GithubPin_p1_init(self);
    return self;
}

static void btrc_Vector_GithubPin_p1_destroy(btrc_Vector_GithubPin_p1* self) {
    btrc_Vector_GithubPin_p1_free(self);
    free(self);
}

static void btrc_Vector_GithubPin_p1_push(btrc_Vector_GithubPin_p1* self, GithubPin* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((GithubPin**)__btrc_safe_realloc(self->data, (sizeof(GithubPin*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static void btrc_Vector_GithubPin_p1_free(btrc_Vector_GithubPin_p1* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                GithubPin_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static int btrc_Vector_GithubPin_p1_iterLen(btrc_Vector_GithubPin_p1* self) {
    return self->len;
}

static GithubPin* btrc_Vector_GithubPin_p1_iterGet(btrc_Vector_GithubPin_p1* self, int i) {
    return self->data[i];
}

char* Strings_copy(char* s) {
    char* __fstr_1_arg0 = s;
    int __fstr_1_len = snprintf(NULL, 0, "%s", __fstr_1_arg0);
    char* __fstr_1_buf = __btrc_str_track(((char*)malloc((__fstr_1_len + 1))));
    snprintf(__fstr_1_buf, (__fstr_1_len + 1), "%s", __fstr_1_arg0);
    return __fstr_1_buf;
}

char* Strings_replace(char* s, char* old, char* replacement) {
    if (s == NULL) {
        return "";
    }
    if ((old == NULL) || (replacement == NULL)) {
        return Strings_copy(s);
    }
    int slen = ((int)strlen(s));
    int oldlen = ((int)strlen(old));
    if (oldlen == 0) {
        return Strings_copy(s);
    }
    int replen = ((int)strlen(replacement));
    int cap = ((slen * 2) + 1);
    char* result = ((char*)malloc(cap));
    int rlen = 0;
    int i = 0;
    while (i < slen) {
        if (((i + oldlen) <= slen) && (strncmp((s + i), old, oldlen) == 0)) {
            while ((rlen + replen) >= cap) {
                (cap = (cap * 2));
                (result = ((char*)realloc(result, cap)));
            }
            memcpy((result + rlen), replacement, replen);
            (rlen = (rlen + replen));
            (i = (i + oldlen));
        } else {
            if ((rlen + 1) >= cap) {
                (cap = (cap * 2));
                (result = ((char*)realloc(result, cap)));
            }
            (result[rlen] = s[i]);
            (rlen++);
            (i++);
        }
    }
    (result[rlen] = '\0');
    return result;
}

btrc_Vector_string* Strings_split(char* s, char* delim) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    if ((s == NULL) || (delim == NULL)) {
        return result;
    }
    int dlen = ((int)strlen(delim));
    if (dlen == 0) {
        return result;
    }
    char* p = s;
    while (*p) {
        char* found = strstr(p, delim);
        int seglen = ((found != NULL) ? ((int)(found - p)) : ((int)strlen(p)));
        char* item = ((char*)malloc((seglen + 1)));
        memcpy(item, p, seglen);
        (item[seglen] = '\0');
        btrc_Vector_string_push(result, item);
        if (found == NULL) {
            break;
        }
        (p = (found + dlen));
    }
    return result;
    if (result != NULL) {
        if ((--result->__rc) <= 0) {
            btrc_Vector_string_destroy(result);
        }
    }
}

int Strings_find(char* s, char* sub, int start) {
    int slen = ((int)strlen(s));
    int sublen = ((int)strlen(sub));
    if (start < 0) {
        (start = 0);
    }
    if (sublen == 0) {
        return start;
    }
    int i = start;
    while ((i + sublen) <= slen) {
        if (strncmp((s + i), sub, sublen) == 0) {
            return i;
        }
        (i++);
    }
    return (-1);
}

void CliArgs_init(CliArgs* self, int argc, char** argv) {
    self->__rc = 1;
    (self->program = ((argc > 0) ? Strings_copy(argv[0]) : ""));
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    btrc_Vector_string* __list_2 = btrc_Vector_string_new();
    (self->values = __list_2);
    (self->values->__rc++);
    for (int i = 1; (i < argc); (i++)) {
        btrc_Vector_string_push(self->values, Strings_copy(argv[i]));
    }
}

CliArgs* CliArgs_new(int argc, char** argv) {
    CliArgs* self = ((CliArgs*)malloc(sizeof(CliArgs)));
    memset(self, 0, sizeof(CliArgs));
    CliArgs_init(self, argc, argv);
    return self;
}

void CliArgs_destroy(CliArgs* self) {
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

int CliArgs_count(CliArgs* self) {
    return self->values->len;
}

char* CliArgs_get(CliArgs* self, int index) {
    return btrc_Vector_string_get(self->values, index);
}

char* CliArgs_command(CliArgs* self) {
    if (self->values->len == 0) {
        return "";
    }
    return btrc_Vector_string_get(self->values, 0);
}

char* CliArgs_valueAfter(CliArgs* self, char* flag, char* fallback) {
    for (int i = 0; (i < (self->values->len - 1)); (i++)) {
        if (strcmp(btrc_Vector_string_get(self->values, i), flag) == 0) {
            return btrc_Vector_string_get(self->values, (i + 1));
        }
    }
    return fallback;
}

void File_init(File* self, char* path, char* mode) {
    self->__rc = 1;
    (self->path = path);
    (self->mode = mode);
    (self->handle = fopen(path, mode));
    (self->is_open = (self->handle != NULL));
}

File* File_new(char* path, char* mode) {
    File* self = ((File*)malloc(sizeof(File)));
    memset(self, 0, sizeof(File));
    File_init(self, path, mode);
    return self;
}

void File_destroy(File* self) {
    File_close(self);
    free(self);
}

bool File_ok(File* self) {
    return self->is_open;
}

char* File_read(File* self) {
    if (!self->is_open) {
        return "";
    }
    int cap = 4096;
    int len = 0;
    char* buffer = ((char*)malloc(cap));
    if (buffer == NULL) {
        fprintf(stderr, "File.read: out of memory\n");
        exit(1);
    }
    int ch = fgetc(self->handle);
    while (ch != EOF) {
        if ((len + 2) >= cap) {
            (cap = (cap * 2));
            char* grown = ((char*)realloc(buffer, cap));
            if (grown == NULL) {
                fprintf(stderr, "File.read: out of memory\n");
                exit(1);
            }
            (buffer = grown);
        }
        (buffer[len] = ((char)ch));
        (len++);
        (ch = fgetc(self->handle));
    }
    (buffer[len] = '\0');
    return buffer;
}

bool File_write(File* self, char* text) {
    if (!self->is_open) {
        return false;
    }
    return (fputs(text, self->handle) != EOF);
}

void File_close(File* self) {
    if (self->is_open) {
        if (((int)strlen(self->path)) > 0) {
            fclose(self->handle);
        }
        (self->is_open = false);
    }
}

char* Path_readAll(char* path) {
    File* f = File_new(path, "r");
    if (!File_ok(f)) {
        char* __btrc_ret_23 = "";
        if (f != NULL) {
            if ((--f->__rc) <= 0) {
                File_destroy(f);
            }
        }
        return __btrc_ret_23;
    }
    char* content = File_read(f);
    File_close(f);
    if (f != NULL) {
        if ((--f->__rc) <= 0) {
            File_destroy(f);
        }
    }
    return content;
    if (f != NULL) {
        if ((--f->__rc) <= 0) {
            File_destroy(f);
        }
    }
}

bool Path_writeAll(char* path, char* content) {
    File* f = File_new(path, "w");
    if (!File_ok(f)) {
        fprintf(stderr, "Path.writeAll: cannot open '%s' for writing\n", path);
        bool __btrc_ret_24 = false;
        if (f != NULL) {
            if ((--f->__rc) <= 0) {
                File_destroy(f);
            }
        }
        return __btrc_ret_24;
    }
    bool wrote = File_write(f, content);
    File_close(f);
    if (!wrote) {
        fprintf(stderr, "Path.writeAll: write to '%s' failed\n", path);
    }
    if (f != NULL) {
        if ((--f->__rc) <= 0) {
            File_destroy(f);
        }
    }
    return wrote;
    if (f != NULL) {
        if ((--f->__rc) <= 0) {
            File_destroy(f);
        }
    }
}

int UnixPlatform_pid(void) {
    return ((int)getpid());
}

int Platform_pid(void) {
    return UnixPlatform_pid();
}

char* Environment_get(char* name, char* fallback) {
    char* value = getenv(name);
    if ((value == NULL) || __btrc_isEmpty(value)) {
        return fallback;
    }
    return Strings_copy(value);
}

void ProcessStatus_init(ProcessStatus* self, int raw) {
    self->__rc = 1;
    (self->raw = raw);
}

ProcessStatus* ProcessStatus_new(int raw) {
    ProcessStatus* self = ((ProcessStatus*)malloc(sizeof(ProcessStatus)));
    memset(self, 0, sizeof(ProcessStatus));
    ProcessStatus_init(self, raw);
    return self;
}

int ProcessStatus_code(ProcessStatus* self) {
    if (self->raw == (-1)) {
        return 127;
    }
    if (WIFEXITED(self->raw)) {
        return WEXITSTATUS(self->raw);
    }
    if (WIFSIGNALED(self->raw)) {
        return (128 + WTERMSIG(self->raw));
    }
    return self->raw;
}

ProcessStatus* UnixProcess_system(char* command) {
    return ProcessStatus_new(system(command));
}

bool ShellWords_isEnvNameStart(char c) {
    if ((c >= 'a') && (c <= 'z')) {
        return true;
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return true;
    }
    return (c == '_');
}

bool ShellWords_isEnvNameChar(char c) {
    return (ShellWords_isEnvNameStart(c) || ((c >= '0') && (c <= '9')));
}

bool ShellWords_isEnvName(char* name) {
    if (__btrc_isEmpty(name)) {
        return false;
    }
    if (!ShellWords_isEnvNameStart(name[0])) {
        return false;
    }
    for (int __i_25 = 0; (name[__i_25] != '\0'); (__i_25++)) {
        char ch = name[__i_25];
        if (!ShellWords_isEnvNameChar(ch)) {
            return false;
        }
    }
    return true;
}

bool ShellWords_isSafeArgChar(char c) {
    if ((c >= 'a') && (c <= 'z')) {
        return true;
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return true;
    }
    if ((c >= '0') && (c <= '9')) {
        return true;
    }
    return ((((((((c == '_') || (c == '-')) || (c == '.')) || (c == '/')) || (c == ':')) || (c == '=')) || (c == ',')) || (c == '+'));
}

bool ShellWords_isSafeArg(char* raw) {
    int len = ((int)strlen(raw));
    if (len == 0) {
        return false;
    }
    for (int __i_26 = 0; (raw[__i_26] != '\0'); (__i_26++)) {
        char ch = raw[__i_26];
        if (!ShellWords_isSafeArgChar(ch)) {
            return false;
        }
    }
    return true;
}

char* ShellWords_quote(char* raw) {
    if (ShellWords_isSafeArg(raw)) {
        return Strings_copy(raw);
    }
    char* escaped = Strings_replace(raw, "'", "'\\''");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", escaped)), "'"));
}

char* ShellWords_redact(char* text, char* sensitive) {
    if (__btrc_isEmpty(sensitive)) {
        return text;
    }
    return Strings_replace(text, sensitive, "***");
}

char* ShellWords_envAssignment(char* item) {
    int split = Strings_find(item, "=", 0);
    if (split <= 0) {
        char* __fstr_27_arg0 = item;
        int __fstr_27_len = snprintf(NULL, 0, "invalid environment assignment: %s", __fstr_27_arg0);
        char* __fstr_27_buf = __btrc_str_track(((char*)malloc((__fstr_27_len + 1))));
        snprintf(__fstr_27_buf, (__fstr_27_len + 1), "invalid environment assignment: %s", __fstr_27_arg0);
        __btrc_throw(__fstr_27_buf);
    }
    char* name = __btrc_str_track(__btrc_substring(item, 0, split));
    if (!ShellWords_isEnvName(name)) {
        char* __fstr_28_arg0 = name;
        int __fstr_28_len = snprintf(NULL, 0, "invalid environment variable name: %s", __fstr_28_arg0);
        char* __fstr_28_buf = __btrc_str_track(((char*)malloc((__fstr_28_len + 1))));
        snprintf(__fstr_28_buf, (__fstr_28_len + 1), "invalid environment variable name: %s", __fstr_28_arg0);
        __btrc_throw(__fstr_28_buf);
    }
    char* value = __btrc_str_track(__btrc_substring(item, (split + 1), ((((int)strlen(item)) - split) - 1)));
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, "=")), ShellWords_quote(value)));
}

char* CommandOutput_collect(void) {
    return "collect";
}

char* CommandOutput_stream(void) {
    return "stream";
}

char* CommandOutput_combine(void) {
    return "combine";
}

char* CommandOutput_suppress(void) {
    return "suppress";
}

bool CommandOutput_valid(char* mode) {
    return ((((strcmp(mode, CommandOutput_collect()) == 0) || (strcmp(mode, CommandOutput_stream()) == 0)) || (strcmp(mode, CommandOutput_combine()) == 0)) || (strcmp(mode, CommandOutput_suppress()) == 0));
}

btrc_Vector_string* CommandEnvironment_empty(void) {
    btrc_Vector_string* env = btrc_Vector_string_new();
    return env;
    if (env != NULL) {
        if ((--env->__rc) <= 0) {
            btrc_Vector_string_destroy(env);
        }
    }
}

void ExecResult_init(ExecResult* self, int code, char* out, char* err, char* command) {
    self->__rc = 1;
    (self->code = code);
    (self->out = out);
    (self->err = err);
    (self->command = command);
}

ExecResult* ExecResult_new(int code, char* out, char* err, char* command) {
    ExecResult* self = ((ExecResult*)malloc(sizeof(ExecResult)));
    memset(self, 0, sizeof(ExecResult));
    ExecResult_init(self, code, out, err, command);
    return self;
}

void UnixShell_init(UnixShell* self) {
    self->__rc = 1;
    (self->logCommands = false);
    (self->chrootPath = "");
    (self->tempId = 0);
}

UnixShell* UnixShell_new(void) {
    UnixShell* self = ((UnixShell*)malloc(sizeof(UnixShell)));
    memset(self, 0, sizeof(UnixShell));
    UnixShell_init(self);
    return self;
}

void UnixShell_destroy(UnixShell* self) {
    free(self);
}

char* UnixShell_redactText(char* text, char* sensitive) {
    return ShellWords_redact(text, sensitive);
}

void UnixShell_logError(char* message) {
    fprintf(stderr, "%s\n", message);
}

char* UnixShell_tempPath(UnixShell* self, char* name) {
    (self->tempId++);
    char* base = Environment_get("TMPDIR", "/tmp");
    char* separator = (__btrc_endsWith(base, "/") ? "" : "/");
    char* __fstr_36_arg0 = base;
    char* __fstr_36_arg1 = separator;
    int __fstr_36_arg2 = Platform_pid();
    int __fstr_36_arg3 = self->tempId;
    char* __fstr_36_arg4 = name;
    int __fstr_36_len = snprintf(NULL, 0, "%s%sbtrc-process-%d-%d.%s.XXXXXX", __fstr_36_arg0, __fstr_36_arg1, __fstr_36_arg2, __fstr_36_arg3, __fstr_36_arg4);
    char* __fstr_36_buf = __btrc_str_track(((char*)malloc((__fstr_36_len + 1))));
    snprintf(__fstr_36_buf, (__fstr_36_len + 1), "%s%sbtrc-process-%d-%d.%s.XXXXXX", __fstr_36_arg0, __fstr_36_arg1, __fstr_36_arg2, __fstr_36_arg3, __fstr_36_arg4);
    char* templatePath = __fstr_36_buf;
    char* raw = Strings_copy(templatePath);
    int fd = mkstemp(raw);
    if (fd < 0) {
        return "";
    }
    close(fd);
    return Strings_copy(raw);
}

char* UnixShell_renderEnv(UnixShell* self, btrc_Vector_string* env) {
    btrc_Vector_string* parts = btrc_Vector_string_new();
    int __n_38 = btrc_Vector_string_iterLen(env);
    for (int __i_37 = 0; (__i_37 < __n_38); (__i_37++)) {
        char* item = btrc_Vector_string_iterGet(env, __i_37);
        btrc_Vector_string_push(parts, ShellWords_envAssignment(item));
    }
    char* __btrc_ret_39 = btrc_Vector_string_join(parts, " ");
    if (parts != NULL) {
        if ((--parts->__rc) <= 0) {
            btrc_Vector_string_destroy(parts);
        }
    }
    return __btrc_ret_39;
    if (parts != NULL) {
        if ((--parts->__rc) <= 0) {
            btrc_Vector_string_destroy(parts);
        }
    }
}

char* UnixShell_withContext(UnixShell* self, char* command, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot) {
    char* rendered = command;
    char* envPrefix = UnixShell_renderEnv(self, env);
    if (!__btrc_isEmpty(envPrefix)) {
        char* __fstr_40_arg0 = envPrefix;
        char* __fstr_40_arg1 = rendered;
        int __fstr_40_len = snprintf(NULL, 0, "%s %s", __fstr_40_arg0, __fstr_40_arg1);
        char* __fstr_40_buf = __btrc_str_track(((char*)malloc((__fstr_40_len + 1))));
        snprintf(__fstr_40_buf, (__fstr_40_len + 1), "%s %s", __fstr_40_arg0, __fstr_40_arg1);
        (rendered = __fstr_40_buf);
    }
    if (sudo) {
        char* __fstr_41_arg0 = rendered;
        int __fstr_41_len = snprintf(NULL, 0, "sudo %s", __fstr_41_arg0);
        char* __fstr_41_buf = __btrc_str_track(((char*)malloc((__fstr_41_len + 1))));
        snprintf(__fstr_41_buf, (__fstr_41_len + 1), "sudo %s", __fstr_41_arg0);
        (rendered = __fstr_41_buf);
    }
    if (!__btrc_isEmpty(cwd)) {
        char* __fstr_42_arg0 = ShellWords_quote(cwd);
        char* __fstr_42_arg1 = rendered;
        int __fstr_42_len = snprintf(NULL, 0, "cd %s && %s", __fstr_42_arg0, __fstr_42_arg1);
        char* __fstr_42_buf = __btrc_str_track(((char*)malloc((__fstr_42_len + 1))));
        snprintf(__fstr_42_buf, (__fstr_42_len + 1), "cd %s && %s", __fstr_42_arg0, __fstr_42_arg1);
        (rendered = __fstr_42_buf);
    }
    char* root = (__btrc_isEmpty(chroot) ? self->chrootPath : chroot);
    if (!__btrc_isEmpty(root)) {
        char* __fstr_43_arg0 = ShellWords_quote(root);
        char* __fstr_43_arg1 = ShellWords_quote(rendered);
        int __fstr_43_len = snprintf(NULL, 0, "nixos-enter --root %s --command %s", __fstr_43_arg0, __fstr_43_arg1);
        char* __fstr_43_buf = __btrc_str_track(((char*)malloc((__fstr_43_len + 1))));
        snprintf(__fstr_43_buf, (__fstr_43_len + 1), "nixos-enter --root %s --command %s", __fstr_43_arg0, __fstr_43_arg1);
        (rendered = __fstr_43_buf);
    }
    return rendered;
}

char* UnixShell_withRedirections(UnixShell* self, char* rendered, char* stdout, char* stderr, char* outFile, char* errFile, char* stdinFile) {
    char* __fstr_44_arg0 = rendered;
    int __fstr_44_len = snprintf(NULL, 0, "( %s )", __fstr_44_arg0);
    char* __fstr_44_buf = __btrc_str_track(((char*)malloc((__fstr_44_len + 1))));
    snprintf(__fstr_44_buf, (__fstr_44_len + 1), "( %s )", __fstr_44_arg0);
    char* command = __fstr_44_buf;
    if (!__btrc_isEmpty(stdinFile)) {
        char* __fstr_45_arg0 = command;
        char* __fstr_45_arg1 = ShellWords_quote(stdinFile);
        int __fstr_45_len = snprintf(NULL, 0, "%s < %s", __fstr_45_arg0, __fstr_45_arg1);
        char* __fstr_45_buf = __btrc_str_track(((char*)malloc((__fstr_45_len + 1))));
        snprintf(__fstr_45_buf, (__fstr_45_len + 1), "%s < %s", __fstr_45_arg0, __fstr_45_arg1);
        (command = __fstr_45_buf);
    }
    if (strcmp(stdout, CommandOutput_collect()) == 0) {
        char* __fstr_46_arg0 = command;
        char* __fstr_46_arg1 = ShellWords_quote(outFile);
        int __fstr_46_len = snprintf(NULL, 0, "%s > %s", __fstr_46_arg0, __fstr_46_arg1);
        char* __fstr_46_buf = __btrc_str_track(((char*)malloc((__fstr_46_len + 1))));
        snprintf(__fstr_46_buf, (__fstr_46_len + 1), "%s > %s", __fstr_46_arg0, __fstr_46_arg1);
        (command = __fstr_46_buf);
    } else if (strcmp(stdout, CommandOutput_suppress()) == 0) {
        char* __fstr_47_arg0 = command;
        int __fstr_47_len = snprintf(NULL, 0, "%s > /dev/null", __fstr_47_arg0);
        char* __fstr_47_buf = __btrc_str_track(((char*)malloc((__fstr_47_len + 1))));
        snprintf(__fstr_47_buf, (__fstr_47_len + 1), "%s > /dev/null", __fstr_47_arg0);
        (command = __fstr_47_buf);
    }
    if (strcmp(stderr, CommandOutput_combine()) == 0) {
        char* __fstr_48_arg0 = command;
        int __fstr_48_len = snprintf(NULL, 0, "%s 2>&1", __fstr_48_arg0);
        char* __fstr_48_buf = __btrc_str_track(((char*)malloc((__fstr_48_len + 1))));
        snprintf(__fstr_48_buf, (__fstr_48_len + 1), "%s 2>&1", __fstr_48_arg0);
        (command = __fstr_48_buf);
    } else if (strcmp(stderr, CommandOutput_collect()) == 0) {
        char* __fstr_49_arg0 = command;
        char* __fstr_49_arg1 = ShellWords_quote(errFile);
        int __fstr_49_len = snprintf(NULL, 0, "%s 2> %s", __fstr_49_arg0, __fstr_49_arg1);
        char* __fstr_49_buf = __btrc_str_track(((char*)malloc((__fstr_49_len + 1))));
        snprintf(__fstr_49_buf, (__fstr_49_len + 1), "%s 2> %s", __fstr_49_arg0, __fstr_49_arg1);
        (command = __fstr_49_buf);
    } else if (strcmp(stderr, CommandOutput_suppress()) == 0) {
        char* __fstr_50_arg0 = command;
        int __fstr_50_len = snprintf(NULL, 0, "%s 2>/dev/null", __fstr_50_arg0);
        char* __fstr_50_buf = __btrc_str_track(((char*)malloc((__fstr_50_len + 1))));
        snprintf(__fstr_50_buf, (__fstr_50_len + 1), "%s 2>/dev/null", __fstr_50_arg0);
        (command = __fstr_50_buf);
    }
    return command;
}

ExecResult* UnixShell_run(UnixShell* self, char* command, char* stdout, char* stderr, bool logCommand, bool logFailure, bool throwOnFailure, char* redactSubstring, char* stdin, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot) {
    if (!CommandOutput_valid(stdout)) {
        char* __fstr_51_arg0 = stdout;
        int __fstr_51_len = snprintf(NULL, 0, "invalid stdout mode: %s", __fstr_51_arg0);
        char* __fstr_51_buf = __btrc_str_track(((char*)malloc((__fstr_51_len + 1))));
        snprintf(__fstr_51_buf, (__fstr_51_len + 1), "invalid stdout mode: %s", __fstr_51_arg0);
        __btrc_throw(__fstr_51_buf);
    }
    if (!CommandOutput_valid(stderr)) {
        char* __fstr_52_arg0 = stderr;
        int __fstr_52_len = snprintf(NULL, 0, "invalid stderr mode: %s", __fstr_52_arg0);
        char* __fstr_52_buf = __btrc_str_track(((char*)malloc((__fstr_52_len + 1))));
        snprintf(__fstr_52_buf, (__fstr_52_len + 1), "invalid stderr mode: %s", __fstr_52_arg0);
        __btrc_throw(__fstr_52_buf);
    }
    if (strcmp(stdout, CommandOutput_combine()) == 0) {
        __btrc_throw("stdout cannot use CommandOutput.combine");
    }
    if ((strcmp(stderr, CommandOutput_combine()) == 0) && (!(strcmp(stdout, CommandOutput_collect()) == 0))) {
        __btrc_throw("stderr can only combine with collected stdout");
    }
    char* rendered = UnixShell_withContext(self, command, cwd, env, sudo, chroot);
    char* outFile = UnixShell_tempPath(self, "stdout");
    char* errFile = UnixShell_tempPath(self, "stderr");
    if (__btrc_isEmpty(outFile) || __btrc_isEmpty(errFile)) {
        __btrc_throw("failed to create temporary process files");
    }
    char* stdinFile = "";
    if (!__btrc_isEmpty(stdin)) {
        (stdinFile = UnixShell_tempPath(self, "stdin"));
        if (__btrc_isEmpty(stdinFile)) {
            unlink(outFile);
            unlink(errFile);
            __btrc_throw("failed to create temporary stdin file");
        }
        Path_writeAll(stdinFile, stdin);
        chmod(stdinFile, 384);
    }
    char* executed = UnixShell_withRedirections(self, rendered, stdout, stderr, outFile, errFile, stdinFile);
    if (self->logCommands || logCommand) {
        char* visible = UnixShell_redactText(executed, redactSubstring);
        char* __fstr_53_arg0 = visible;
        int __fstr_53_len = snprintf(NULL, 0, "LOG: %s", __fstr_53_arg0);
        char* __fstr_53_buf = __btrc_str_track(((char*)malloc((__fstr_53_len + 1))));
        snprintf(__fstr_53_buf, (__fstr_53_len + 1), "LOG: %s", __fstr_53_arg0);
        UnixShell_logError(__fstr_53_buf);
    }
    ProcessStatus* status = UnixProcess_system(executed);
    int code = ProcessStatus_code(status);
    char* output = ((strcmp(stdout, CommandOutput_collect()) == 0) ? Path_readAll(outFile) : "");
    char* error = ((strcmp(stderr, CommandOutput_collect()) == 0) ? Path_readAll(errFile) : "");
    unlink(outFile);
    unlink(errFile);
    if (!__btrc_isEmpty(stdinFile)) {
        unlink(stdinFile);
    }
    if ((logFailure || throwOnFailure) && (code != 0)) {
        int __fstr_54_arg0 = code;
        char* __fstr_54_arg1 = UnixShell_redactText(rendered, redactSubstring);
        int __fstr_54_len = snprintf(NULL, 0, "Command failed (%d): %s", __fstr_54_arg0, __fstr_54_arg1);
        char* __fstr_54_buf = __btrc_str_track(((char*)malloc((__fstr_54_len + 1))));
        snprintf(__fstr_54_buf, (__fstr_54_len + 1), "Command failed (%d): %s", __fstr_54_arg0, __fstr_54_arg1);
        UnixShell_logError(__fstr_54_buf);
    }
    if (throwOnFailure && (code != 0)) {
        int __fstr_55_arg0 = code;
        char* __fstr_55_arg1 = UnixShell_redactText(rendered, redactSubstring);
        int __fstr_55_len = snprintf(NULL, 0, "Command failed (%d): %s", __fstr_55_arg0, __fstr_55_arg1);
        char* __fstr_55_buf = __btrc_str_track(((char*)malloc((__fstr_55_len + 1))));
        snprintf(__fstr_55_buf, (__fstr_55_len + 1), "Command failed (%d): %s", __fstr_55_arg0, __fstr_55_arg1);
        __btrc_throw(__fstr_55_buf);
    }
    return ExecResult_new(code, output, error, rendered);
}

void FileStatus_init(FileStatus* self, char* path) {
    self->__rc = 1;
    (self->path = path);
    struct stat st;
    (self->found = (stat(path, (&st)) == 0));
    (self->mode = (self->found ? ((int)st.st_mode) : 0));
    struct stat lst;
    (self->linkFound = (lstat(path, (&lst)) == 0));
    (self->linkMode = (self->linkFound ? ((int)lst.st_mode) : 0));
}

FileStatus* FileStatus_new(char* path) {
    FileStatus* self = ((FileStatus*)malloc(sizeof(FileStatus)));
    memset(self, 0, sizeof(FileStatus));
    FileStatus_init(self, path);
    return self;
}

void FileStatus_destroy(FileStatus* self) {
    free(self);
}

bool FileStatus_exists(FileStatus* self) {
    return self->found;
}

bool FileStatus_isDir(FileStatus* self) {
    return (self->found && S_ISDIR(self->mode));
}

bool FileStatus_isFile(FileStatus* self) {
    return (self->found && S_ISREG(self->mode));
}

void Directory_init(Directory* self, char* path) {
    self->__rc = 1;
    (self->path = path);
}

Directory* Directory_new(char* path) {
    Directory* self = ((Directory*)malloc(sizeof(Directory)));
    memset(self, 0, sizeof(Directory));
    Directory_init(self, path);
    return self;
}

btrc_Vector_string* Directory_entries(Directory* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    DIR* dir = opendir(self->path);
    if (dir == NULL) {
        return result;
    }
    struct dirent* entry = readdir(dir);
    while (entry != NULL) {
        char* name = entry->d_name;
        if ((!(strcmp(name, ".") == 0)) && (!(strcmp(name, "..") == 0))) {
            btrc_Vector_string_push(result, Strings_copy(name));
        }
        (entry = readdir(dir));
    }
    closedir(dir);
    return result;
    if (result != NULL) {
        if ((--result->__rc) <= 0) {
            btrc_Vector_string_destroy(result);
        }
    }
}

int UnixFileSystem_chmodPath(char* path, int mode) {
    return chmod(path, ((mode_t)mode));
}

int UnixFileSystem_mkdirPath(char* path, int mode) {
    return mkdir(path, ((mode_t)mode));
}

int UnixFileSystem_mkdirOne(char* path, int mode) {
    if (__btrc_isEmpty(path) || FileSystem_isDir(path)) {
        return 0;
    }
    if (FileSystem_exists(path)) {
        return (-1);
    }
    int created = UnixFileSystem_mkdirPath(path, mode);
    if ((created == 0) || FileSystem_isDir(path)) {
        return 0;
    }
    return created;
}

int UnixFileSystem_mkdirp(char* path) {
    if (__btrc_isEmpty(path)) {
        return (-1);
    }
    if (FileSystem_isDir(path)) {
        return 0;
    }
    int mode = 493;
    for (int i = 1; (i < ((int)strlen(path))); (i++)) {
        if (path[i] == '/') {
            char* parent = __btrc_str_track(__btrc_substring(path, 0, i));
            if ((!__btrc_isEmpty(parent)) && (UnixFileSystem_mkdirOne(parent, mode) != 0)) {
                return (-1);
            }
        }
    }
    return UnixFileSystem_mkdirOne(path, mode);
}

char* UnixFileSystem_currentDirectory(void) {
    char buffer[4096];
    char* result = getcwd(buffer, 4096);
    if (result == NULL) {
        return "";
    }
    return Strings_copy(buffer);
}

char* PathTools_basename(char* path) {
    int len = ((int)strlen(path));
    if (len == 0) {
        return "";
    }
    int end = (len - 1);
    while ((end > 0) && (path[end] == '/')) {
        (end--);
    }
    int start = end;
    while ((start > 0) && (path[(start - 1)] != '/')) {
        (start--);
    }
    int outLen = ((end - start) + 1);
    char* result = ((char*)malloc((outLen + 1)));
    memcpy(result, (path + start), outLen);
    (result[outLen] = '\0');
    return result;
}

char* PathTools_dirname(char* path) {
    int len = ((int)strlen(path));
    if (len == 0) {
        return ".";
    }
    int end = (len - 1);
    while ((end > 0) && (path[end] == '/')) {
        (end--);
    }
    while ((end > 0) && (path[end] != '/')) {
        (end--);
    }
    if (end == 0) {
        if (path[0] == '/') {
            return "/";
        }
        return ".";
    }
    char* result = ((char*)malloc((end + 1)));
    memcpy(result, path, end);
    (result[end] = '\0');
    return result;
}

char* PathTools_join(char* left, char* right) {
    if (((int)strlen(left)) == 0) {
        return Strings_copy(right);
    }
    if (((int)strlen(right)) == 0) {
        return Strings_copy(left);
    }
    if (left[(((int)strlen(left)) - 1)] == '/') {
        char* __fstr_62_arg0 = left;
        char* __fstr_62_arg1 = right;
        int __fstr_62_len = snprintf(NULL, 0, "%s%s", __fstr_62_arg0, __fstr_62_arg1);
        char* __fstr_62_buf = __btrc_str_track(((char*)malloc((__fstr_62_len + 1))));
        snprintf(__fstr_62_buf, (__fstr_62_len + 1), "%s%s", __fstr_62_arg0, __fstr_62_arg1);
        return __fstr_62_buf;
    }
    char* __fstr_63_arg0 = left;
    char* __fstr_63_arg1 = right;
    int __fstr_63_len = snprintf(NULL, 0, "%s/%s", __fstr_63_arg0, __fstr_63_arg1);
    char* __fstr_63_buf = __btrc_str_track(((char*)malloc((__fstr_63_len + 1))));
    snprintf(__fstr_63_buf, (__fstr_63_len + 1), "%s/%s", __fstr_63_arg0, __fstr_63_arg1);
    return __fstr_63_buf;
}

bool FileSystem_exists(char* path) {
    FileStatus* status = FileStatus_new(path);
    bool result = FileStatus_exists(status);
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
    return result;
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
}

bool FileSystem_isDir(char* path) {
    FileStatus* status = FileStatus_new(path);
    bool result = FileStatus_isDir(status);
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
    return result;
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
}

bool FileSystem_isFile(char* path) {
    FileStatus* status = FileStatus_new(path);
    bool result = FileStatus_isFile(status);
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
    return result;
    if (status != NULL) {
        if ((--status->__rc) <= 0) {
            FileStatus_destroy(status);
        }
    }
}

int FileSystem_chmod(char* path, int mode) {
    return UnixFileSystem_chmodPath(path, mode);
}

char* FileSystem_currentDirectory(void) {
    return UnixFileSystem_currentDirectory();
}

void ParseLog_init(ParseLog* self) {
    self->__rc = 1;
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            btrc_Vector_string_free(self->errors);
        }
    }
    btrc_Vector_string* __list_64 = btrc_Vector_string_new();
    (self->errors = __list_64);
    (self->errors->__rc++);
}

ParseLog* ParseLog_new(void) {
    ParseLog* self = ((ParseLog*)malloc(sizeof(ParseLog)));
    memset(self, 0, sizeof(ParseLog));
    ParseLog_init(self);
    return self;
}

void ParseLog_destroy(ParseLog* self) {
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            btrc_Vector_string_free(self->errors);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void ParseLog_add(ParseLog* self, char* file, char* field, char* message) {
    btrc_Vector_string_push(self->errors, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(file, ": ")), field)), ": ")), message)));
}

bool ParseLog_ok(ParseLog* self) {
    return (btrc_Vector_string_size(self->errors) == 0);
}

void JsonValue_init(JsonValue* self) {
    self->__rc = 1;
    (self->tag = 0);
    (self->boolVal = false);
    (self->numVal = 0.0);
    (self->numIsInt = true);
    (self->strVal = "");
    if (self->arr != NULL) {
        if ((--self->arr->__rc) <= 0) {
            btrc_Vector_JsonValue_p1_free(self->arr);
        }
    }
    btrc_Vector_JsonValue_p1* __list_69 = btrc_Vector_JsonValue_p1_new();
    (self->arr = __list_69);
    (self->arr->__rc++);
    if (self->objKeys != NULL) {
        if ((--self->objKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->objKeys);
        }
    }
    btrc_Vector_string* __list_70 = btrc_Vector_string_new();
    (self->objKeys = __list_70);
    (self->objKeys->__rc++);
    if (self->objVals != NULL) {
        if ((--self->objVals->__rc) <= 0) {
            btrc_Vector_JsonValue_p1_free(self->objVals);
        }
    }
    btrc_Vector_JsonValue_p1* __list_71 = btrc_Vector_JsonValue_p1_new();
    (self->objVals = __list_71);
    (self->objVals->__rc++);
}

JsonValue* JsonValue_new(void) {
    JsonValue* self = ((JsonValue*)malloc(sizeof(JsonValue)));
    memset(self, 0, sizeof(JsonValue));
    JsonValue_init(self);
    return self;
}

void JsonValue_destroy(JsonValue* self) {
    if (self->arr != NULL) {
        if ((--self->arr->__rc) <= 0) {
            btrc_Vector_JsonValue_p1_free(self->arr);
        }
    }
    if (self->objKeys != NULL) {
        if ((--self->objKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->objKeys);
        }
    }
    if (self->objVals != NULL) {
        if ((--self->objVals->__rc) <= 0) {
            btrc_Vector_JsonValue_p1_free(self->objVals);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

JsonValue* JsonValue_makeNull(void) {
    JsonValue* v = JsonValue_new();
    (v->tag = 0);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeError(void) {
    JsonValue* v = JsonValue_new();
    (v->tag = 6);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeBool(bool b) {
    JsonValue* v = JsonValue_new();
    (v->tag = 1);
    (v->boolVal = b);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeInt(int n) {
    JsonValue* v = JsonValue_new();
    (v->tag = 2);
    (v->numVal = ((double)n));
    (v->numIsInt = true);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeFloat(double d) {
    JsonValue* v = JsonValue_new();
    (v->tag = 2);
    (v->numVal = d);
    (v->numIsInt = false);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeString(char* s) {
    JsonValue* v = JsonValue_new();
    (v->tag = 3);
    (v->strVal = ((s == NULL) ? "" : s));
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeArray(void) {
    JsonValue* v = JsonValue_new();
    (v->tag = 4);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

JsonValue* JsonValue_makeObject(void) {
    JsonValue* v = JsonValue_new();
    (v->tag = 5);
    return v;
    __btrc_tracking = 1;
    __btrc_destroyed_count = 0;
    if (v != NULL) {
        (--v->__rc);
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc <= 0) {
                JsonValue_destroy(v);
                v = NULL;
            }
        }
    }
    if (v != NULL) {
        if (__btrc_is_destroyed(v) == 0) {
            if (v->__rc > 0) {
                __btrc_suspect(v, (__btrc_visit_fn)JsonValue_visit, (__btrc_destroy_fn)JsonValue_destroy);
            }
        }
    }
    if (__btrc_suspect_count > 0) {
        __btrc_collect_cycles();
    }
    __btrc_tracking = 0;
}

void JsonValue_push(JsonValue* self, JsonValue* child) {
    btrc_Vector_JsonValue_p1_push(self->arr, child);
}

void JsonValue_set(JsonValue* self, char* key, JsonValue* value) {
    for (int i = 0; (i < self->objKeys->len); (i++)) {
        if (strcmp(btrc_Vector_string_get(self->objKeys, i), key) == 0) {
            btrc_Vector_JsonValue_p1_set(self->objVals, i, value);
            return;
        }
    }
    btrc_Vector_string_push(self->objKeys, key);
    btrc_Vector_JsonValue_p1_push(self->objVals, value);
}

bool JsonValue_isNull(JsonValue* self) {
    return ((self->tag == 0) || (self->tag == 6));
}

bool JsonValue_isError(JsonValue* self) {
    return (self->tag == 6);
}

bool JsonValue_isBool(JsonValue* self) {
    return (self->tag == 1);
}

bool JsonValue_isNumber(JsonValue* self) {
    return (self->tag == 2);
}

bool JsonValue_isString(JsonValue* self) {
    return (self->tag == 3);
}

bool JsonValue_isArray(JsonValue* self) {
    return (self->tag == 4);
}

bool JsonValue_isObject(JsonValue* self) {
    return (self->tag == 5);
}

bool JsonValue_isInt(JsonValue* self) {
    return ((self->tag == 2) && self->numIsInt);
}

bool JsonValue_asBool(JsonValue* self) {
    if (self->tag == 1) {
        return self->boolVal;
    }
    return false;
}

int JsonValue_asInt(JsonValue* self) {
    if (self->tag == 2) {
        return ((int)self->numVal);
    }
    return 0;
}

double JsonValue_asFloat(JsonValue* self) {
    if (self->tag == 2) {
        return self->numVal;
    }
    return 0.0;
}

char* JsonValue_asString(JsonValue* self) {
    if (self->tag == 3) {
        return self->strVal;
    }
    return "";
}

int JsonValue_size(JsonValue* self) {
    if (self->tag == 4) {
        return self->arr->len;
    }
    if (self->tag == 5) {
        return self->objKeys->len;
    }
    return 0;
}

JsonValue* JsonValue_at(JsonValue* self, int i) {
    if (((self->tag == 4) && (i >= 0)) && (i < self->arr->len)) {
        return btrc_Vector_JsonValue_p1_get(self->arr, i);
    }
    return JsonValue_makeNull();
}

JsonValue* JsonValue_get(JsonValue* self, char* key) {
    if (self->tag == 5) {
        for (int i = 0; (i < self->objKeys->len); (i++)) {
            if (strcmp(btrc_Vector_string_get(self->objKeys, i), key) == 0) {
                return btrc_Vector_JsonValue_p1_get(self->objVals, i);
            }
        }
    }
    return JsonValue_makeNull();
}

bool JsonValue_has(JsonValue* self, char* key) {
    if (self->tag != 5) {
        return false;
    }
    for (int i = 0; (i < self->objKeys->len); (i++)) {
        if (strcmp(btrc_Vector_string_get(self->objKeys, i), key) == 0) {
            return true;
        }
    }
    return false;
}

btrc_Vector_string* JsonValue_keys(JsonValue* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    if (self->tag == 5) {
        for (int i = 0; (i < self->objKeys->len); (i++)) {
            btrc_Vector_string_push(result, btrc_Vector_string_get(self->objKeys, i));
        }
    }
    return result;
    if (result != NULL) {
        if ((--result->__rc) <= 0) {
            btrc_Vector_string_destroy(result);
        }
    }
}

JsonValue* JsonValue_parse(char* text) {
    if (text == NULL) {
        return JsonValue_makeError();
    }
    JsonParser* p = JsonParser_new(text);
    JsonValue* value = JsonParser_parseValue(p);
    if (!p->ok) {
        JsonValue* __btrc_ret_72 = JsonValue_makeError();
        if (p != NULL) {
            if ((--p->__rc) <= 0) {
                JsonParser_destroy(p);
            }
        }
        return __btrc_ret_72;
    }
    JsonParser_skipWs(p);
    if (p->pos < p->len) {
        JsonValue* __btrc_ret_73 = JsonValue_makeError();
        if (p != NULL) {
            if ((--p->__rc) <= 0) {
                JsonParser_destroy(p);
            }
        }
        return __btrc_ret_73;
    }
    if (p != NULL) {
        if ((--p->__rc) <= 0) {
            JsonParser_destroy(p);
        }
    }
    return value;
    if (p != NULL) {
        if ((--p->__rc) <= 0) {
            JsonParser_destroy(p);
        }
    }
}

JsonValue* JsonValue_readFile(char* path) {
    return JsonValue_parse(Path_readAll(path));
}

void JsonParser_init(JsonParser* self, char* text) {
    self->__rc = 1;
    (self->text = text);
    (self->pos = 0);
    (self->len = ((int)strlen(text)));
    (self->ok = true);
}

JsonParser* JsonParser_new(char* text) {
    JsonParser* self = ((JsonParser*)malloc(sizeof(JsonParser)));
    memset(self, 0, sizeof(JsonParser));
    JsonParser_init(self, text);
    return self;
}

void JsonParser_destroy(JsonParser* self) {
    free(self);
}

void JsonParser_skipWs(JsonParser* self) {
    while (self->pos < self->len) {
        char c = self->text[self->pos];
        if ((((c == ' ') || (c == '\n')) || (c == '\t')) || (c == '\r')) {
            (self->pos++);
        } else {
            break;
        }
    }
}

JsonValue* JsonParser_parseValue(JsonParser* self) {
    JsonParser_skipWs(self);
    if (self->pos >= self->len) {
        (self->ok = false);
        return JsonValue_makeError();
    }
    char c = self->text[self->pos];
    if (c == '{') {
        return JsonParser_parseObject(self);
    }
    if (c == '[') {
        return JsonParser_parseArray(self);
    }
    if (c == ((char)34)) {
        return JsonParser_parseStringValue(self);
    }
    if ((c == 't') || (c == 'f')) {
        return JsonParser_parseBool(self);
    }
    if (c == 'n') {
        return JsonParser_parseNull(self);
    }
    if ((c == '-') || ((c >= '0') && (c <= '9'))) {
        return JsonParser_parseNumber(self);
    }
    (self->ok = false);
    return JsonValue_makeError();
}

JsonValue* JsonParser_parseObject(JsonParser* self) {
    JsonValue* obj = JsonValue_makeObject();
    (self->pos++);
    JsonParser_skipWs(self);
    if ((self->pos < self->len) && (self->text[self->pos] == '}')) {
        (self->pos++);
        return obj;
    }
    while (true) {
        JsonParser_skipWs(self);
        if ((self->pos >= self->len) || (self->text[self->pos] != ((char)34))) {
            (self->ok = false);
            return obj;
        }
        char* key = JsonParser_readString(self);
        if (!self->ok) {
            return obj;
        }
        JsonParser_skipWs(self);
        if ((self->pos >= self->len) || (self->text[self->pos] != ':')) {
            (self->ok = false);
            return obj;
        }
        (self->pos++);
        JsonValue* value = JsonParser_parseValue(self);
        if (!self->ok) {
            return obj;
        }
        JsonValue_set(obj, key, value);
        JsonParser_skipWs(self);
        if (self->pos >= self->len) {
            (self->ok = false);
            return obj;
        }
        char d = self->text[self->pos];
        if (d == ',') {
            (self->pos++);
            continue;
        }
        if (d == '}') {
            (self->pos++);
            return obj;
        }
        (self->ok = false);
        return obj;
    }
    return obj;
}

JsonValue* JsonParser_parseArray(JsonParser* self) {
    JsonValue* arr = JsonValue_makeArray();
    (self->pos++);
    JsonParser_skipWs(self);
    if ((self->pos < self->len) && (self->text[self->pos] == ']')) {
        (self->pos++);
        return arr;
    }
    while (true) {
        JsonValue* value = JsonParser_parseValue(self);
        if (!self->ok) {
            return arr;
        }
        JsonValue_push(arr, value);
        JsonParser_skipWs(self);
        if (self->pos >= self->len) {
            (self->ok = false);
            return arr;
        }
        char d = self->text[self->pos];
        if (d == ',') {
            (self->pos++);
            continue;
        }
        if (d == ']') {
            (self->pos++);
            return arr;
        }
        (self->ok = false);
        return arr;
    }
    return arr;
}

JsonValue* JsonParser_parseStringValue(JsonParser* self) {
    char* s = JsonParser_readString(self);
    if (!self->ok) {
        return JsonValue_makeError();
    }
    return JsonValue_makeString(s);
}

char* JsonParser_readString(JsonParser* self) {
    (self->pos++);
    char* out = "";
    while (self->pos < self->len) {
        char c = self->text[self->pos];
        if (c == ((char)34)) {
            (self->pos++);
            return out;
        }
        if (c == '\\') {
            (self->pos++);
            if (self->pos >= self->len) {
                (self->ok = false);
                return out;
            }
            char e = self->text[self->pos];
            if (e == ((char)34)) {
                (out = __btrc_str_track(__btrc_strcat(out, "\"")));
            } else if (e == '\\') {
                (out = __btrc_str_track(__btrc_strcat(out, "\\")));
            } else if (e == '/') {
                (out = __btrc_str_track(__btrc_strcat(out, "/")));
            } else if (e == 'n') {
                (out = __btrc_str_track(__btrc_strcat(out, "\n")));
            } else if (e == 'r') {
                (out = __btrc_str_track(__btrc_strcat(out, "\r")));
            } else if (e == 't') {
                (out = __btrc_str_track(__btrc_strcat(out, "\t")));
            } else if (e == 'b') {
                (out = __btrc_str_track(__btrc_strcat(out, JsonNum_charFromCode(8))));
            } else if (e == 'f') {
                (out = __btrc_str_track(__btrc_strcat(out, JsonNum_charFromCode(12))));
            } else if (e == 'u') {
                int cp = JsonParser_readHex4(self);
                if (!self->ok) {
                    return out;
                }
                if ((cp >= 0xD800) && (cp <= 0xDBFF)) {
                    if ((((self->pos + 2) < self->len) && (self->text[(self->pos + 1)] == '\\')) && (self->text[(self->pos + 2)] == 'u')) {
                        (self->pos += 2);
                        int lo = JsonParser_readHex4(self);
                        if (!self->ok) {
                            return out;
                        }
                        if ((lo >= 0xDC00) && (lo <= 0xDFFF)) {
                            (cp = ((0x10000 + ((cp - 0xD800) * 0x400)) + (lo - 0xDC00)));
                        } else {
                            (out = __btrc_str_track(__btrc_strcat(out, JsonNum_utf8(0xFFFD))));
                            (cp = lo);
                        }
                    } else {
                        (cp = 0xFFFD);
                    }
                } else if ((cp >= 0xDC00) && (cp <= 0xDFFF)) {
                    (cp = 0xFFFD);
                }
                (out = __btrc_str_track(__btrc_strcat(out, JsonNum_utf8(cp))));
            } else {
                (self->ok = false);
                return out;
            }
            (self->pos++);
            continue;
        }
        (out = __btrc_str_track(__btrc_strcat(out, __btrc_str_track(__btrc_substring(self->text, self->pos, 1)))));
        (self->pos++);
    }
    (self->ok = false);
    return out;
}

int JsonParser_readHex4(JsonParser* self) {
    int value = 0;
    for (int k = 0; (k < 4); (k++)) {
        (self->pos++);
        if (self->pos >= self->len) {
            (self->ok = false);
            return 0;
        }
        char h = self->text[self->pos];
        int digit = (-1);
        if ((h >= '0') && (h <= '9')) {
            (digit = ((int)(h - '0')));
        } else if ((h >= 'a') && (h <= 'f')) {
            (digit = (((int)(h - 'a')) + 10));
        } else if ((h >= 'A') && (h <= 'F')) {
            (digit = (((int)(h - 'A')) + 10));
        } else {
            (self->ok = false);
            return 0;
        }
        (value = ((value * 16) + digit));
    }
    return value;
}

JsonValue* JsonParser_parseBool(JsonParser* self) {
    if (JsonParser_matchLiteral(self, "true")) {
        return JsonValue_makeBool(true);
    }
    if (JsonParser_matchLiteral(self, "false")) {
        return JsonValue_makeBool(false);
    }
    (self->ok = false);
    return JsonValue_makeError();
}

JsonValue* JsonParser_parseNull(JsonParser* self) {
    if (JsonParser_matchLiteral(self, "null")) {
        return JsonValue_makeNull();
    }
    (self->ok = false);
    return JsonValue_makeError();
}

bool JsonParser_matchLiteral(JsonParser* self, char* word) {
    int n = ((int)strlen(word));
    if ((self->pos + n) > self->len) {
        return false;
    }
    for (int k = 0; (k < n); (k++)) {
        if (self->text[(self->pos + k)] != word[k]) {
            return false;
        }
    }
    (self->pos += n);
    return true;
}

JsonValue* JsonParser_parseNumber(JsonParser* self) {
    int start = self->pos;
    bool isFloat = false;
    if ((self->pos < self->len) && (self->text[self->pos] == '-')) {
        (self->pos++);
    }
    if (self->pos >= self->len) {
        (self->ok = false);
        return JsonValue_makeError();
    }
    if (self->text[self->pos] == '0') {
        (self->pos++);
    } else if ((self->text[self->pos] >= '1') && (self->text[self->pos] <= '9')) {
        while (((self->pos < self->len) && (self->text[self->pos] >= '0')) && (self->text[self->pos] <= '9')) {
            (self->pos++);
        }
    } else {
        (self->ok = false);
        return JsonValue_makeError();
    }
    if ((self->pos < self->len) && (self->text[self->pos] == '.')) {
        (isFloat = true);
        (self->pos++);
        if (((self->pos >= self->len) || (self->text[self->pos] < '0')) || (self->text[self->pos] > '9')) {
            (self->ok = false);
            return JsonValue_makeError();
        }
        while (((self->pos < self->len) && (self->text[self->pos] >= '0')) && (self->text[self->pos] <= '9')) {
            (self->pos++);
        }
    }
    if ((self->pos < self->len) && ((self->text[self->pos] == 'e') || (self->text[self->pos] == 'E'))) {
        (isFloat = true);
        (self->pos++);
        if ((self->pos < self->len) && ((self->text[self->pos] == '+') || (self->text[self->pos] == '-'))) {
            (self->pos++);
        }
        if (((self->pos >= self->len) || (self->text[self->pos] < '0')) || (self->text[self->pos] > '9')) {
            (self->ok = false);
            return JsonValue_makeError();
        }
        while (((self->pos < self->len) && (self->text[self->pos] >= '0')) && (self->text[self->pos] <= '9')) {
            (self->pos++);
        }
    }
    char* token = __btrc_str_track(__btrc_substring(self->text, start, (self->pos - start)));
    double d = JsonNum_toDouble(token);
    if (isFloat) {
        return JsonValue_makeFloat(d);
    }
    JsonValue* v = JsonValue_makeInt(0);
    (v->numVal = d);
    (v->numIsInt = true);
    return v;
}

double JsonNum_toDouble(char* token) {
    double result = strtod(token, NULL);
    return result;
}

char* JsonNum_charFromCode(int code) {
    char* buf = ((char*)malloc(2));
    (buf[0] = ((char)code));
    (buf[1] = ((char)0));
    return buf;
}

char* JsonNum_utf8(int cp) {
    if (cp < 0) {
        (cp = 0xFFFD);
    }
    if (cp < 0x80) {
        char* b = ((char*)malloc(2));
        (b[0] = ((char)cp));
        (b[1] = ((char)0));
        return b;
    }
    if (cp < 0x800) {
        char* b = ((char*)malloc(3));
        (b[0] = ((char)(0xC0 | (cp >> 6))));
        (b[1] = ((char)(0x80 | (cp & 0x3F))));
        (b[2] = ((char)0));
        return b;
    }
    if (cp < 0x10000) {
        char* b = ((char*)malloc(4));
        (b[0] = ((char)(0xE0 | (cp >> 12))));
        (b[1] = ((char)(0x80 | ((cp >> 6) & 0x3F))));
        (b[2] = ((char)(0x80 | (cp & 0x3F))));
        (b[3] = ((char)0));
        return b;
    }
    char* b = ((char*)malloc(5));
    (b[0] = ((char)(0xF0 | (cp >> 18))));
    (b[1] = ((char)(0x80 | ((cp >> 12) & 0x3F))));
    (b[2] = ((char)(0x80 | ((cp >> 6) & 0x3F))));
    (b[3] = ((char)(0x80 | (cp & 0x3F))));
    (b[4] = ((char)0));
    return b;
}

void NormalizedRect_init(NormalizedRect* self) {
    self->__rc = 1;
    (self->x = 0.0);
    (self->y = 0.0);
    (self->w = 0.0);
    (self->h = 0.0);
    (self->set = false);
}

NormalizedRect* NormalizedRect_new(void) {
    NormalizedRect* self = ((NormalizedRect*)malloc(sizeof(NormalizedRect)));
    memset(self, 0, sizeof(NormalizedRect));
    NormalizedRect_init(self);
    return self;
}

void NormalizedRect_destroy(NormalizedRect* self) {
    free(self);
}

void PixelSize_init(PixelSize* self) {
    self->__rc = 1;
    (self->w = 0);
    (self->h = 0);
}

PixelSize* PixelSize_new(void) {
    PixelSize* self = ((PixelSize*)malloc(sizeof(PixelSize)));
    memset(self, 0, sizeof(PixelSize));
    PixelSize_init(self);
    return self;
}

void PixelSize_destroy(PixelSize* self) {
    free(self);
}

void AspectRatio_init(AspectRatio* self) {
    self->__rc = 1;
    (self->w = 0);
    (self->h = 0);
    (self->set = false);
}

AspectRatio* AspectRatio_new(void) {
    AspectRatio* self = ((AspectRatio*)malloc(sizeof(AspectRatio)));
    memset(self, 0, sizeof(AspectRatio));
    AspectRatio_init(self);
    return self;
}

void AspectRatio_destroy(AspectRatio* self) {
    free(self);
}

void RgbColor_init(RgbColor* self) {
    self->__rc = 1;
    (self->r = 0.0);
    (self->g = 0.0);
    (self->b = 0.0);
    (self->set = false);
}

RgbColor* RgbColor_new(void) {
    RgbColor* self = ((RgbColor*)malloc(sizeof(RgbColor)));
    memset(self, 0, sizeof(RgbColor));
    RgbColor_init(self);
    return self;
}

void RgbColor_destroy(RgbColor* self) {
    free(self);
}

char* JsonContract_fieldLabel(char* context, char* key) {
    return (__btrc_isEmpty(context) ? key : __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".")), key)));
}

void JsonContract_checkKeys(JsonValue* jsonObject, btrc_Vector_string* allowedKeys, char* file, char* context, ParseLog* log) {
    if (!JsonValue_isObject(jsonObject)) {
        return;
    }
    btrc_Vector_string* __iter_85 = JsonValue_keys(jsonObject);
    int __n_87 = btrc_Vector_string_iterLen(__iter_85);
    for (int __i_86 = 0; (__i_86 < __n_87); (__i_86++)) {
        char* key = btrc_Vector_string_iterGet(__iter_85, __i_86);
        if ((!(strcmp(key, "doc") == 0)) && (!btrc_Vector_string_contains(allowedKeys, key))) {
            ParseLog_add(log, file, JsonContract_fieldLabel(context, key), "unknown key");
        }
    }
    if (JsonValue_has(jsonObject, "doc") && (!JsonValue_isObject(JsonValue_get(jsonObject, "doc")))) {
        ParseLog_add(log, file, JsonContract_fieldLabel(context, "doc"), "doc must be an object");
    }
}

void JsonContract_checkSchemaVersion(JsonValue* root, int expectedVersion, char* file, ParseLog* log) {
    if (!JsonValue_isObject(root)) {
        ParseLog_add(log, file, "(root)", "expected a JSON object");
        return;
    }
    if (!JsonValue_has(root, "schema_version")) {
        ParseLog_add(log, file, "schema_version", "missing required field");
        return;
    }
    JsonValue* declaredVersion = JsonValue_get(root, "schema_version");
    if (!JsonValue_isInt(declaredVersion)) {
        ParseLog_add(log, file, "schema_version", "expected integer");
        return;
    }
    if (JsonValue_asInt(declaredVersion) != expectedVersion) {
        int __fstr_88_arg0 = JsonValue_asInt(declaredVersion);
        int __fstr_88_arg1 = expectedVersion;
        int __fstr_88_len = snprintf(NULL, 0, "version %d unsupported (want %d)", __fstr_88_arg0, __fstr_88_arg1);
        char* __fstr_88_buf = __btrc_str_track(((char*)malloc((__fstr_88_len + 1))));
        snprintf(__fstr_88_buf, (__fstr_88_len + 1), "version %d unsupported (want %d)", __fstr_88_arg0, __fstr_88_arg1);
        ParseLog_add(log, file, "schema_version", __fstr_88_buf);
    }
    btrc_Vector_string* keys = JsonValue_keys(root);
    if ((btrc_Vector_string_size(keys) > 0) && (!(strcmp(btrc_Vector_string_get(keys, 0), "schema_version") == 0))) {
        ParseLog_add(log, file, "schema_version", "must be the first key");
    }
}

char* JsonContract_requiredString(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log) {
    char* label = JsonContract_fieldLabel(context, key);
    if (!JsonValue_has(jsonObject, key)) {
        ParseLog_add(log, file, label, "missing required field");
        return "";
    }
    JsonValue* jsonValue = JsonValue_get(jsonObject, key);
    if (!JsonValue_isString(jsonValue)) {
        ParseLog_add(log, file, label, "expected string");
        return "";
    }
    char* value = JsonValue_asString(jsonValue);
    if (__btrc_isEmpty(value)) {
        ParseLog_add(log, file, label, "empty string not allowed (omit the field instead)");
    }
    return value;
}

char* JsonContract_optionalString(JsonValue* jsonObject, char* key, char* fallback, char* file, char* context, ParseLog* log) {
    if (!JsonValue_has(jsonObject, key)) {
        return fallback;
    }
    return JsonContract_requiredString(jsonObject, key, file, context, log);
}

bool JsonContract_requiredBool(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log) {
    char* label = JsonContract_fieldLabel(context, key);
    if (!JsonValue_has(jsonObject, key)) {
        ParseLog_add(log, file, label, "missing required field");
        return false;
    }
    JsonValue* jsonValue = JsonValue_get(jsonObject, key);
    if (!JsonValue_isBool(jsonValue)) {
        ParseLog_add(log, file, label, "expected boolean");
        return false;
    }
    return JsonValue_asBool(jsonValue);
}

bool JsonContract_optionalBool(JsonValue* jsonObject, char* key, bool fallback, char* file, char* context, ParseLog* log) {
    if (!JsonValue_has(jsonObject, key)) {
        return fallback;
    }
    return JsonContract_requiredBool(jsonObject, key, file, context, log);
}

double JsonContract_requiredNumber(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log) {
    char* label = JsonContract_fieldLabel(context, key);
    if (!JsonValue_has(jsonObject, key)) {
        ParseLog_add(log, file, label, "missing required field");
        return 0.0;
    }
    JsonValue* jsonValue = JsonValue_get(jsonObject, key);
    if (!JsonValue_isNumber(jsonValue)) {
        ParseLog_add(log, file, label, "expected number");
        return 0.0;
    }
    return JsonValue_asFloat(jsonValue);
}

double JsonContract_optionalNumber(JsonValue* jsonObject, char* key, double fallback, char* file, char* context, ParseLog* log) {
    if (!JsonValue_has(jsonObject, key)) {
        return fallback;
    }
    return JsonContract_requiredNumber(jsonObject, key, file, context, log);
}

int JsonContract_requiredInt(JsonValue* jsonObject, char* key, char* file, char* context, ParseLog* log) {
    char* label = JsonContract_fieldLabel(context, key);
    if (!JsonValue_has(jsonObject, key)) {
        ParseLog_add(log, file, label, "missing required field");
        return 0;
    }
    JsonValue* jsonValue = JsonValue_get(jsonObject, key);
    if (!JsonValue_isInt(jsonValue)) {
        ParseLog_add(log, file, label, "expected integer");
        return 0;
    }
    return JsonValue_asInt(jsonValue);
}

void JsonContract_checkEnum(char* value, btrc_Vector_string* allowedValues, char* file, char* context, ParseLog* log) {
    if (__btrc_isEmpty(value)) {
        return;
    }
    if (!btrc_Vector_string_contains(allowedValues, value)) {
        ParseLog_add(log, file, context, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("invalid value '", value)), "' (allowed: ")), btrc_Vector_string_join(allowedValues, ", "))), ")")));
    }
}

btrc_Vector_string* JsonContract_stringArray(JsonValue* jsonObject, char* key, bool required, char* file, char* context, ParseLog* log) {
    char* label = JsonContract_fieldLabel(context, key);
    btrc_Vector_string* result = btrc_Vector_string_new();
    if (!JsonValue_has(jsonObject, key)) {
        if (required) {
            ParseLog_add(log, file, label, "missing required field");
        }
        return result;
    }
    JsonValue* jsonValue = JsonValue_get(jsonObject, key);
    if (!JsonValue_isArray(jsonValue)) {
        ParseLog_add(log, file, label, "expected array of strings");
        return result;
    }
    for (int entryIndex = 0; (entryIndex < JsonValue_size(jsonValue)); (entryIndex++)) {
        JsonValue* entry = JsonValue_at(jsonValue, entryIndex);
        if ((!JsonValue_isString(entry)) || __btrc_isEmpty(JsonValue_asString(entry))) {
            char* __fstr_89_arg0 = label;
            int __fstr_89_arg1 = entryIndex;
            int __fstr_89_len = snprintf(NULL, 0, "%s[%d]", __fstr_89_arg0, __fstr_89_arg1);
            char* __fstr_89_buf = __btrc_str_track(((char*)malloc((__fstr_89_len + 1))));
            snprintf(__fstr_89_buf, (__fstr_89_len + 1), "%s[%d]", __fstr_89_arg0, __fstr_89_arg1);
            ParseLog_add(log, file, __fstr_89_buf, "expected non-empty string");
        } else {
            btrc_Vector_string_push(result, JsonValue_asString(entry));
        }
    }
    return result;
    if (result != NULL) {
        if ((--result->__rc) <= 0) {
            btrc_Vector_string_destroy(result);
        }
    }
}

void JsonContract_checkAssetPath(char* path, char* file, char* context, ParseLog* log) {
    if ((!__btrc_isEmpty(path)) && (!__btrc_startsWith(path, "assets/"))) {
        ParseLog_add(log, file, context, __btrc_str_track(__btrc_strcat("must be a canonical asset path starting with 'assets/'", " (upstream pack paths live only in assets/sources.json)")));
    }
}

NormalizedRect* JsonContract_rect(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    NormalizedRect* normalizedRect = NormalizedRect_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected rect object {x, y, w, h}");
        return normalizedRect;
    }
    btrc_Vector_string* __list_90 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_90, "x");
    btrc_Vector_string_push(__list_90, "y");
    btrc_Vector_string_push(__list_90, "w");
    btrc_Vector_string_push(__list_90, "h");
    btrc_Vector_string* allowedKeys = __list_90;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (normalizedRect->x = JsonContract_requiredNumber(jsonValue, "x", file, context, log));
    (normalizedRect->y = JsonContract_requiredNumber(jsonValue, "y", file, context, log));
    (normalizedRect->w = JsonContract_requiredNumber(jsonValue, "w", file, context, log));
    (normalizedRect->h = JsonContract_requiredNumber(jsonValue, "h", file, context, log));
    if ((((((((normalizedRect->x < 0.0) || (normalizedRect->x > 1.0)) || (normalizedRect->y < 0.0)) || (normalizedRect->y > 1.0)) || (normalizedRect->w <= 0.0)) || (normalizedRect->w > 1.0)) || (normalizedRect->h <= 0.0)) || (normalizedRect->h > 1.0)) {
        ParseLog_add(log, file, context, "rect values must be normalized to 0..1 with positive w/h");
    }
    (normalizedRect->set = true);
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return normalizedRect;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (normalizedRect != NULL) {
        if ((--normalizedRect->__rc) <= 0) {
            NormalizedRect_destroy(normalizedRect);
        }
    }
}

PixelSize* JsonContract_pixelSize(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    PixelSize* parsedSize = PixelSize_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected size object {w, h}");
        return parsedSize;
    }
    btrc_Vector_string* __list_91 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_91, "w");
    btrc_Vector_string_push(__list_91, "h");
    btrc_Vector_string* allowedKeys = __list_91;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (parsedSize->w = JsonContract_requiredInt(jsonValue, "w", file, context, log));
    (parsedSize->h = JsonContract_requiredInt(jsonValue, "h", file, context, log));
    if ((parsedSize->w < 1) || (parsedSize->h < 1)) {
        ParseLog_add(log, file, context, "size values must be positive integers");
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return parsedSize;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (parsedSize != NULL) {
        if ((--parsedSize->__rc) <= 0) {
            PixelSize_destroy(parsedSize);
        }
    }
}

AspectRatio* JsonContract_aspectRatio(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    AspectRatio* parsedAspect = AspectRatio_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected aspect object {w, h}");
        return parsedAspect;
    }
    btrc_Vector_string* __list_92 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_92, "w");
    btrc_Vector_string_push(__list_92, "h");
    btrc_Vector_string* allowedKeys = __list_92;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (parsedAspect->w = JsonContract_requiredInt(jsonValue, "w", file, context, log));
    (parsedAspect->h = JsonContract_requiredInt(jsonValue, "h", file, context, log));
    if ((parsedAspect->w < 1) || (parsedAspect->h < 1)) {
        ParseLog_add(log, file, context, "aspect values must be positive integers");
    }
    (parsedAspect->set = true);
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return parsedAspect;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (parsedAspect != NULL) {
        if ((--parsedAspect->__rc) <= 0) {
            AspectRatio_destroy(parsedAspect);
        }
    }
}

RgbColor* JsonContract_rgbColor(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    RgbColor* parsedColor = RgbColor_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected color object {r, g, b}");
        return parsedColor;
    }
    btrc_Vector_string* __list_93 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_93, "r");
    btrc_Vector_string_push(__list_93, "g");
    btrc_Vector_string_push(__list_93, "b");
    btrc_Vector_string* allowedKeys = __list_93;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (parsedColor->r = JsonContract_requiredNumber(jsonValue, "r", file, context, log));
    (parsedColor->g = JsonContract_requiredNumber(jsonValue, "g", file, context, log));
    (parsedColor->b = JsonContract_requiredNumber(jsonValue, "b", file, context, log));
    if ((((((parsedColor->r < 0.0) || (parsedColor->r > 1.0)) || (parsedColor->g < 0.0)) || (parsedColor->g > 1.0)) || (parsedColor->b < 0.0)) || (parsedColor->b > 1.0)) {
        ParseLog_add(log, file, context, "color values must be 0..1 floats");
    }
    (parsedColor->set = true);
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return parsedColor;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (parsedColor != NULL) {
        if ((--parsedColor->__rc) <= 0) {
            RgbColor_destroy(parsedColor);
        }
    }
}

void JsonContract_checkContainedRect(NormalizedRect* inner, NormalizedRect* outer, char* file, char* context, ParseLog* log) {
    if ((!inner->set) || (!outer->set)) {
        return;
    }
    double epsilon = 0.0001;
    if (((((inner->x + epsilon) < outer->x) || ((inner->y + epsilon) < outer->y)) || ((inner->x + inner->w) > ((outer->x + outer->w) + epsilon))) || ((inner->y + inner->h) > ((outer->y + outer->h) + epsilon))) {
        ParseLog_add(log, file, context, "screen hole must lie within the combined hole");
    }
}

bool SourceDirectories_nameGreater(char* left, char* right) {
    int limit = ((((int)strlen(left)) < ((int)strlen(right))) ? ((int)strlen(left)) : ((int)strlen(right)));
    for (int charIndex = 0; (charIndex < limit); (charIndex++)) {
        if (__btrc_charAt(left, charIndex) > __btrc_charAt(right, charIndex)) {
            return true;
        }
        if (__btrc_charAt(left, charIndex) < __btrc_charAt(right, charIndex)) {
            return false;
        }
    }
    return (((int)strlen(left)) > ((int)strlen(right)));
}

btrc_Vector_string* SourceDirectories_sortedChildDirectories(char* root) {
    btrc_Vector_string* directories = btrc_Vector_string_new();
    if (!FileSystem_isDir(root)) {
        return directories;
    }
    btrc_Vector_string* __iter_94 = Directory_entries(Directory_new(root));
    int __n_96 = btrc_Vector_string_iterLen(__iter_94);
    for (int __i_95 = 0; (__i_95 < __n_96); (__i_95++)) {
        char* entry = btrc_Vector_string_iterGet(__iter_94, __i_95);
        if (FileSystem_isDir(PathTools_join(root, entry))) {
            btrc_Vector_string_push(directories, entry);
        }
    }
    for (int sortIndex = 1; (sortIndex < btrc_Vector_string_size(directories)); (sortIndex++)) {
        char* current = btrc_Vector_string_get(directories, sortIndex);
        int compareIndex = (sortIndex - 1);
        while ((compareIndex >= 0) && SourceDirectories_nameGreater(btrc_Vector_string_get(directories, compareIndex), current)) {
            btrc_Vector_string_set(directories, (compareIndex + 1), btrc_Vector_string_get(directories, compareIndex));
            (compareIndex--);
        }
        btrc_Vector_string_set(directories, (compareIndex + 1), current);
    }
    return directories;
    if (directories != NULL) {
        if ((--directories->__rc) <= 0) {
            btrc_Vector_string_destroy(directories);
        }
    }
}

void SystemScreen_init(SystemScreen* self) {
    self->__rc = 1;
    (self->id = "");
    if (self->native != NULL) {
        if ((--self->native->__rc) <= 0) {
            PixelSize_destroy(self->native);
        }
    }
    (self->native = PixelSize_new());
    (self->native->__rc++);
}

SystemScreen* SystemScreen_new(void) {
    SystemScreen* self = ((SystemScreen*)malloc(sizeof(SystemScreen)));
    memset(self, 0, sizeof(SystemScreen));
    SystemScreen_init(self);
    return self;
}

void SystemScreen_destroy(SystemScreen* self) {
    if (self->native != NULL) {
        if ((--self->native->__rc) <= 0) {
            PixelSize_destroy(self->native);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void WidescreenPolicy_init(WidescreenPolicy* self) {
    self->__rc = 1;
    if (self->aspect != NULL) {
        if ((--self->aspect->__rc) <= 0) {
            AspectRatio_destroy(self->aspect);
        }
    }
    (self->aspect = AspectRatio_new());
    (self->aspect->__rc++);
    (self->detect = "");
    (self->set = false);
}

WidescreenPolicy* WidescreenPolicy_new(void) {
    WidescreenPolicy* self = ((WidescreenPolicy*)malloc(sizeof(WidescreenPolicy)));
    memset(self, 0, sizeof(WidescreenPolicy));
    WidescreenPolicy_init(self);
    return self;
}

void WidescreenPolicy_destroy(WidescreenPolicy* self) {
    if (self->aspect != NULL) {
        if ((--self->aspect->__rc) <= 0) {
            AspectRatio_destroy(self->aspect);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void DisplayPolicy_init(DisplayPolicy* self) {
    self->__rc = 1;
    if (self->screens != NULL) {
        if ((--self->screens->__rc) <= 0) {
            btrc_Vector_SystemScreen_p1_free(self->screens);
        }
    }
    btrc_Vector_SystemScreen_p1* __list_97 = btrc_Vector_SystemScreen_p1_new();
    (self->screens = __list_97);
    (self->screens->__rc++);
    (self->arrangement = "");
    if (self->aspect != NULL) {
        if ((--self->aspect->__rc) <= 0) {
            AspectRatio_destroy(self->aspect);
        }
    }
    (self->aspect = AspectRatio_new());
    (self->aspect->__rc++);
    if (self->widescreen != NULL) {
        if ((--self->widescreen->__rc) <= 0) {
            WidescreenPolicy_destroy(self->widescreen);
        }
    }
    (self->widescreen = WidescreenPolicy_new());
    (self->widescreen->__rc++);
}

DisplayPolicy* DisplayPolicy_new(void) {
    DisplayPolicy* self = ((DisplayPolicy*)malloc(sizeof(DisplayPolicy)));
    memset(self, 0, sizeof(DisplayPolicy));
    DisplayPolicy_init(self);
    return self;
}

void DisplayPolicy_destroy(DisplayPolicy* self) {
    if (self->screens != NULL) {
        if ((--self->screens->__rc) <= 0) {
            btrc_Vector_SystemScreen_p1_free(self->screens);
        }
    }
    if (self->aspect != NULL) {
        if ((--self->aspect->__rc) <= 0) {
            AspectRatio_destroy(self->aspect);
        }
    }
    if (self->widescreen != NULL) {
        if ((--self->widescreen->__rc) <= 0) {
            WidescreenPolicy_destroy(self->widescreen);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void DisplayPolicyParser_parseScreens(DisplayPolicy* displayPolicy, JsonValue* jsonValue, char* file, ParseLog* log) {
    if (((!JsonValue_has(jsonValue, "screens")) || (!JsonValue_isArray(JsonValue_get(jsonValue, "screens")))) || (JsonValue_size(JsonValue_get(jsonValue, "screens")) < 1)) {
        ParseLog_add(log, file, "display.screens", "expected non-empty array");
        return;
    }
    JsonValue* screens = JsonValue_get(jsonValue, "screens");
    for (int screenIndex = 0; (screenIndex < JsonValue_size(screens)); (screenIndex++)) {
        JsonValue* entry = JsonValue_at(screens, screenIndex);
        int __fstr_98_arg0 = screenIndex;
        int __fstr_98_len = snprintf(NULL, 0, "display.screens[%d]", __fstr_98_arg0);
        char* __fstr_98_buf = __btrc_str_track(((char*)malloc((__fstr_98_len + 1))));
        snprintf(__fstr_98_buf, (__fstr_98_len + 1), "display.screens[%d]", __fstr_98_arg0);
        char* context = __fstr_98_buf;
        SystemScreen* screen = SystemScreen_new();
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, file, context, "expected object {id, native}");
        } else {
            btrc_Vector_string* __list_99 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_99, "id");
            btrc_Vector_string_push(__list_99, "native");
            btrc_Vector_string* screenKeys = __list_99;
            JsonContract_checkKeys(entry, screenKeys, file, context, log);
            (screen->id = JsonContract_requiredString(entry, "id", file, context, log));
            if (JsonValue_has(entry, "native")) {
                if (screen->native != NULL) {
                    if ((--screen->native->__rc) <= 0) {
                        PixelSize_destroy(screen->native);
                    }
                }
                (screen->native = JsonContract_pixelSize(JsonValue_get(entry, "native"), file, __btrc_str_track(__btrc_strcat(context, ".native")), log));
                (screen->native->__rc++);
            } else {
                ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".native")), "missing required field");
            }
            if (screenKeys != NULL) {
                if ((--screenKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(screenKeys);
                }
            }
        }
        btrc_Vector_SystemScreen_p1_push(displayPolicy->screens, screen);
        if (screen != NULL) {
            if ((--screen->__rc) <= 0) {
                SystemScreen_destroy(screen);
            }
        }
    }
}

void DisplayPolicyParser_parseWidescreen(DisplayPolicy* displayPolicy, JsonValue* jsonValue, char* file, ParseLog* log) {
    JsonValue* widescreen = JsonValue_get(jsonValue, "widescreen");
    if (!JsonValue_isObject(widescreen)) {
        ParseLog_add(log, file, "display.widescreen", "expected object");
        return;
    }
    btrc_Vector_string* __list_100 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_100, "aspect");
    btrc_Vector_string_push(__list_100, "detect");
    btrc_Vector_string* widescreenKeys = __list_100;
    JsonContract_checkKeys(widescreen, widescreenKeys, file, "display.widescreen", log);
    if (JsonValue_has(widescreen, "aspect")) {
        if (displayPolicy->widescreen->aspect != NULL) {
            if ((--displayPolicy->widescreen->aspect->__rc) <= 0) {
                AspectRatio_destroy(displayPolicy->widescreen->aspect);
            }
        }
        (displayPolicy->widescreen->aspect = JsonContract_aspectRatio(JsonValue_get(widescreen, "aspect"), file, "display.widescreen.aspect", log));
        (displayPolicy->widescreen->aspect->__rc++);
    } else {
        ParseLog_add(log, file, "display.widescreen.aspect", "missing required field");
    }
    (displayPolicy->widescreen->detect = JsonContract_requiredString(widescreen, "detect", file, "display.widescreen", log));
    btrc_Vector_string* __list_101 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_101, "emulator_config");
    btrc_Vector_string_push(__list_101, "manual");
    btrc_Vector_string* detectModes = __list_101;
    JsonContract_checkEnum(displayPolicy->widescreen->detect, detectModes, file, "display.widescreen.detect", log);
    (displayPolicy->widescreen->set = true);
    if (detectModes != NULL) {
        if ((--detectModes->__rc) <= 0) {
            btrc_Vector_string_destroy(detectModes);
        }
    }
    if (widescreenKeys != NULL) {
        if ((--widescreenKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(widescreenKeys);
        }
    }
}

DisplayPolicy* DisplayPolicyParser_parse(JsonValue* jsonValue, char* file, ParseLog* log) {
    DisplayPolicy* displayPolicy = DisplayPolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, "display", "expected object");
        return displayPolicy;
    }
    btrc_Vector_string* __list_102 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_102, "screens");
    btrc_Vector_string_push(__list_102, "arrangement");
    btrc_Vector_string_push(__list_102, "aspect");
    btrc_Vector_string_push(__list_102, "widescreen");
    btrc_Vector_string* allowedKeys = __list_102;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, "display", log);
    DisplayPolicyParser_parseScreens(displayPolicy, jsonValue, file, log);
    bool multiScreen = (btrc_Vector_SystemScreen_p1_size(displayPolicy->screens) > 1);
    (displayPolicy->arrangement = JsonContract_optionalString(jsonValue, "arrangement", "", file, "display", log));
    btrc_Vector_string* __list_103 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_103, "vertical");
    btrc_Vector_string_push(__list_103, "horizontal");
    btrc_Vector_string* arrangements = __list_103;
    JsonContract_checkEnum(displayPolicy->arrangement, arrangements, file, "display.arrangement", log);
    if (multiScreen && __btrc_isEmpty(displayPolicy->arrangement)) {
        ParseLog_add(log, file, "display.arrangement", "required for multi-screen systems");
    }
    if ((!multiScreen) && (!__btrc_isEmpty(displayPolicy->arrangement))) {
        ParseLog_add(log, file, "display.arrangement", "forbidden for single-screen systems");
    }
    if (JsonValue_has(jsonValue, "aspect")) {
        if (multiScreen) {
            ParseLog_add(log, file, "display.aspect", "forbidden for multi-screen systems (derived from screens)");
        }
        if (displayPolicy->aspect != NULL) {
            if ((--displayPolicy->aspect->__rc) <= 0) {
                AspectRatio_destroy(displayPolicy->aspect);
            }
        }
        (displayPolicy->aspect = JsonContract_aspectRatio(JsonValue_get(jsonValue, "aspect"), file, "display.aspect", log));
        (displayPolicy->aspect->__rc++);
    } else if (!multiScreen) {
        ParseLog_add(log, file, "display.aspect", "missing required field for single-screen systems");
    }
    if (JsonValue_has(jsonValue, "widescreen")) {
        DisplayPolicyParser_parseWidescreen(displayPolicy, jsonValue, file, log);
    }
    if (arrangements != NULL) {
        if ((--arrangements->__rc) <= 0) {
            btrc_Vector_string_destroy(arrangements);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return displayPolicy;
    if (arrangements != NULL) {
        if ((--arrangements->__rc) <= 0) {
            btrc_Vector_string_destroy(arrangements);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (displayPolicy != NULL) {
        if ((--displayPolicy->__rc) <= 0) {
            DisplayPolicy_destroy(displayPolicy);
        }
    }
}

void RenderPolicy_init(RenderPolicy* self) {
    self->__rc = 1;
    (self->style = "");
    (self->priority = "");
    (self->kind = "standard");
    (self->fill = false);
    (self->reflect = 0.0);
    (self->curvature = 0.0);
    (self->cornerRadius = 0.0);
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    (self->shellTint = RgbColor_new());
    (self->shellTint->__rc++);
    (self->retroDefault = false);
    (self->assetProfile = "");
}

RenderPolicy* RenderPolicy_new(void) {
    RenderPolicy* self = ((RenderPolicy*)malloc(sizeof(RenderPolicy)));
    memset(self, 0, sizeof(RenderPolicy));
    RenderPolicy_init(self);
    return self;
}

void RenderPolicy_destroy(RenderPolicy* self) {
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

RenderPolicy* RenderPolicyParser_parse(JsonValue* jsonValue, char* file, int screenCount, ParseLog* log) {
    RenderPolicy* renderPolicy = RenderPolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, "render", "expected object");
        return renderPolicy;
    }
    btrc_Vector_string* __list_104 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_104, "style");
    btrc_Vector_string_push(__list_104, "priority");
    btrc_Vector_string_push(__list_104, "kind");
    btrc_Vector_string_push(__list_104, "fill");
    btrc_Vector_string_push(__list_104, "reflect");
    btrc_Vector_string_push(__list_104, "curvature");
    btrc_Vector_string_push(__list_104, "corner_radius");
    btrc_Vector_string_push(__list_104, "shell_tint");
    btrc_Vector_string_push(__list_104, "retro_default");
    btrc_Vector_string_push(__list_104, "asset_profile");
    btrc_Vector_string* allowedKeys = __list_104;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, "render", log);
    (renderPolicy->style = JsonContract_requiredString(jsonValue, "style", file, "render", log));
    btrc_Vector_string* __list_105 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_105, "console");
    btrc_Vector_string_push(__list_105, "handheld");
    btrc_Vector_string* styles = __list_105;
    JsonContract_checkEnum(renderPolicy->style, styles, file, "render.style", log);
    (renderPolicy->priority = JsonContract_requiredString(jsonValue, "priority", file, "render", log));
    btrc_Vector_string* __list_106 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_106, "game");
    btrc_Vector_string_push(__list_106, "bezel");
    btrc_Vector_string* priorities = __list_106;
    JsonContract_checkEnum(renderPolicy->priority, priorities, file, "render.priority", log);
    (renderPolicy->kind = JsonContract_optionalString(jsonValue, "kind", "standard", file, "render", log));
    btrc_Vector_string* __list_107 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_107, "standard");
    btrc_Vector_string_push(__list_107, "dual");
    btrc_Vector_string_push(__list_107, "pointer");
    btrc_Vector_string* kinds = __list_107;
    JsonContract_checkEnum(renderPolicy->kind, kinds, file, "render.kind", log);
    bool multiScreen = (screenCount > 1);
    if ((strcmp(renderPolicy->kind, "dual") == 0) && (!multiScreen)) {
        ParseLog_add(log, file, "render.kind", "'dual' requires more than one display screen");
    }
    if (multiScreen && (!(strcmp(renderPolicy->kind, "dual") == 0))) {
        ParseLog_add(log, file, "render.kind", "multi-screen systems must declare kind 'dual'");
    }
    (renderPolicy->fill = JsonContract_optionalBool(jsonValue, "fill", false, file, "render", log));
    (renderPolicy->reflect = JsonContract_optionalNumber(jsonValue, "reflect", 0.0, file, "render", log));
    if ((renderPolicy->reflect < 0.0) || (renderPolicy->reflect > 1.0)) {
        ParseLog_add(log, file, "render.reflect", "must be 0..1");
    }
    (renderPolicy->curvature = JsonContract_optionalNumber(jsonValue, "curvature", 0.0, file, "render", log));
    (renderPolicy->cornerRadius = JsonContract_optionalNumber(jsonValue, "corner_radius", 0.0, file, "render", log));
    if (JsonValue_has(jsonValue, "shell_tint")) {
        if (renderPolicy->shellTint != NULL) {
            if ((--renderPolicy->shellTint->__rc) <= 0) {
                RgbColor_destroy(renderPolicy->shellTint);
            }
        }
        (renderPolicy->shellTint = JsonContract_rgbColor(JsonValue_get(jsonValue, "shell_tint"), file, "render.shell_tint", log));
        (renderPolicy->shellTint->__rc++);
    }
    (renderPolicy->retroDefault = JsonContract_optionalBool(jsonValue, "retro_default", false, file, "render", log));
    (renderPolicy->assetProfile = JsonContract_optionalString(jsonValue, "asset_profile", "", file, "render", log));
    btrc_Vector_string* __list_108 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_108, "handheld");
    btrc_Vector_string_push(__list_108, "crt4x3");
    btrc_Vector_string_push(__list_108, "wide16x9");
    btrc_Vector_string_push(__list_108, "dual");
    btrc_Vector_string* assetProfiles = __list_108;
    JsonContract_checkEnum(renderPolicy->assetProfile, assetProfiles, file, "render.asset_profile", log);
    if (assetProfiles != NULL) {
        if ((--assetProfiles->__rc) <= 0) {
            btrc_Vector_string_destroy(assetProfiles);
        }
    }
    if (kinds != NULL) {
        if ((--kinds->__rc) <= 0) {
            btrc_Vector_string_destroy(kinds);
        }
    }
    if (priorities != NULL) {
        if ((--priorities->__rc) <= 0) {
            btrc_Vector_string_destroy(priorities);
        }
    }
    if (styles != NULL) {
        if ((--styles->__rc) <= 0) {
            btrc_Vector_string_destroy(styles);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return renderPolicy;
    if (assetProfiles != NULL) {
        if ((--assetProfiles->__rc) <= 0) {
            btrc_Vector_string_destroy(assetProfiles);
        }
    }
    if (kinds != NULL) {
        if ((--kinds->__rc) <= 0) {
            btrc_Vector_string_destroy(kinds);
        }
    }
    if (priorities != NULL) {
        if ((--priorities->__rc) <= 0) {
            btrc_Vector_string_destroy(priorities);
        }
    }
    if (styles != NULL) {
        if ((--styles->__rc) <= 0) {
            btrc_Vector_string_destroy(styles);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (renderPolicy != NULL) {
        if ((--renderPolicy->__rc) <= 0) {
            RenderPolicy_destroy(renderPolicy);
        }
    }
}

void EmulatorBinding_init(EmulatorBinding* self) {
    self->__rc = 1;
    (self->emulator = "");
    (self->label = "");
    (self->core = "");
    if (self->platforms != NULL) {
        if ((--self->platforms->__rc) <= 0) {
            btrc_Vector_string_free(self->platforms);
        }
    }
    btrc_Vector_string* __list_109 = btrc_Vector_string_new();
    (self->platforms = __list_109);
    (self->platforms->__rc++);
    if (self->arguments != NULL) {
        if ((--self->arguments->__rc) <= 0) {
            btrc_Vector_string_free(self->arguments);
        }
    }
    btrc_Vector_string* __list_110 = btrc_Vector_string_new();
    (self->arguments = __list_110);
    (self->arguments->__rc++);
}

EmulatorBinding* EmulatorBinding_new(void) {
    EmulatorBinding* self = ((EmulatorBinding*)malloc(sizeof(EmulatorBinding)));
    memset(self, 0, sizeof(EmulatorBinding));
    EmulatorBinding_init(self);
    return self;
}

void EmulatorBinding_destroy(EmulatorBinding* self) {
    if (self->platforms != NULL) {
        if ((--self->platforms->__rc) <= 0) {
            btrc_Vector_string_free(self->platforms);
        }
    }
    if (self->arguments != NULL) {
        if ((--self->arguments->__rc) <= 0) {
            btrc_Vector_string_free(self->arguments);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

EmulatorBinding* EmulatorBindingParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    EmulatorBinding* binding = EmulatorBinding_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return binding;
    }
    btrc_Vector_string* __list_111 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_111, "emulator");
    btrc_Vector_string_push(__list_111, "label");
    btrc_Vector_string_push(__list_111, "core");
    btrc_Vector_string_push(__list_111, "platforms");
    btrc_Vector_string_push(__list_111, "args");
    btrc_Vector_string* allowedKeys = __list_111;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (binding->emulator = JsonContract_requiredString(jsonValue, "emulator", file, context, log));
    (binding->label = JsonContract_optionalString(jsonValue, "label", "", file, context, log));
    (binding->core = JsonContract_optionalString(jsonValue, "core", "", file, context, log));
    if ((!__btrc_isEmpty(binding->core)) && __btrc_strContains(binding->core, "libretro")) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".core")), "must be a bare core name (no _libretro suffix; the suffix is a platform rule)");
    }
    if (binding->platforms != NULL) {
        if ((--binding->platforms->__rc) <= 0) {
            btrc_Vector_string_free(binding->platforms);
        }
    }
    (binding->platforms = JsonContract_stringArray(jsonValue, "platforms", false, file, context, log));
    (binding->platforms->__rc++);
    btrc_Vector_string* __list_112 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_112, "linux");
    btrc_Vector_string_push(__list_112, "macos");
    btrc_Vector_string* operatingSystems = __list_112;
    btrc_Vector_string* __iter_113 = binding->platforms;
    int __n_115 = btrc_Vector_string_iterLen(__iter_113);
    for (int __i_114 = 0; (__i_114 < __n_115); (__i_114++)) {
        char* operatingSystemName = btrc_Vector_string_iterGet(__iter_113, __i_114);
        JsonContract_checkEnum(operatingSystemName, operatingSystems, file, __btrc_str_track(__btrc_strcat(context, ".platforms")), log);
    }
    if (JsonValue_has(jsonValue, "platforms") && (btrc_Vector_string_size(binding->platforms) == 0)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".platforms")), "must be non-empty when present (omit for all platforms)");
    }
    if (binding->arguments != NULL) {
        if ((--binding->arguments->__rc) <= 0) {
            btrc_Vector_string_free(binding->arguments);
        }
    }
    (binding->arguments = JsonContract_stringArray(jsonValue, "args", false, file, context, log));
    (binding->arguments->__rc++);
    if (operatingSystems != NULL) {
        if ((--operatingSystems->__rc) <= 0) {
            btrc_Vector_string_destroy(operatingSystems);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return binding;
    if (operatingSystems != NULL) {
        if ((--operatingSystems->__rc) <= 0) {
            btrc_Vector_string_destroy(operatingSystems);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (binding != NULL) {
        if ((--binding->__rc) <= 0) {
            EmulatorBinding_destroy(binding);
        }
    }
}

void BiosFile_init(BiosFile* self) {
    self->__rc = 1;
    (self->name = "");
    (self->note = "");
}

BiosFile* BiosFile_new(void) {
    BiosFile* self = ((BiosFile*)malloc(sizeof(BiosFile)));
    memset(self, 0, sizeof(BiosFile));
    BiosFile_init(self);
    return self;
}

void BiosFile_destroy(BiosFile* self) {
    free(self);
}

void BiosRequirement_init(BiosRequirement* self) {
    self->__rc = 1;
    (self->present = false);
    (self->required = false);
    (self->match = "");
    if (self->files != NULL) {
        if ((--self->files->__rc) <= 0) {
            btrc_Vector_BiosFile_p1_free(self->files);
        }
    }
    btrc_Vector_BiosFile_p1* __list_116 = btrc_Vector_BiosFile_p1_new();
    (self->files = __list_116);
    (self->files->__rc++);
    (self->directory = "");
}

BiosRequirement* BiosRequirement_new(void) {
    BiosRequirement* self = ((BiosRequirement*)malloc(sizeof(BiosRequirement)));
    memset(self, 0, sizeof(BiosRequirement));
    BiosRequirement_init(self);
    return self;
}

void BiosRequirement_destroy(BiosRequirement* self) {
    if (self->files != NULL) {
        if ((--self->files->__rc) <= 0) {
            btrc_Vector_BiosFile_p1_free(self->files);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

BiosRequirement* BiosRequirementParser_parse(JsonValue* jsonValue, char* file, ParseLog* log) {
    BiosRequirement* biosRequirement = BiosRequirement_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, "bios", "expected object");
        return biosRequirement;
    }
    (biosRequirement->present = true);
    btrc_Vector_string* __list_117 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_117, "required");
    btrc_Vector_string_push(__list_117, "match");
    btrc_Vector_string_push(__list_117, "files");
    btrc_Vector_string_push(__list_117, "dir");
    btrc_Vector_string* allowedKeys = __list_117;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, "bios", log);
    (biosRequirement->required = JsonContract_requiredBool(jsonValue, "required", file, "bios", log));
    (biosRequirement->match = JsonContract_requiredString(jsonValue, "match", file, "bios", log));
    btrc_Vector_string* __list_118 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_118, "all");
    btrc_Vector_string_push(__list_118, "any");
    btrc_Vector_string* matchModes = __list_118;
    JsonContract_checkEnum(biosRequirement->match, matchModes, file, "bios.match", log);
    (biosRequirement->directory = JsonContract_requiredString(jsonValue, "dir", file, "bios", log));
    if (((!JsonValue_has(jsonValue, "files")) || (!JsonValue_isArray(JsonValue_get(jsonValue, "files")))) || (JsonValue_size(JsonValue_get(jsonValue, "files")) < 1)) {
        ParseLog_add(log, file, "bios.files", "expected non-empty array");
        if (matchModes != NULL) {
            if ((--matchModes->__rc) <= 0) {
                btrc_Vector_string_destroy(matchModes);
            }
        }
        if (allowedKeys != NULL) {
            if ((--allowedKeys->__rc) <= 0) {
                btrc_Vector_string_destroy(allowedKeys);
            }
        }
        return biosRequirement;
    }
    JsonValue* files = JsonValue_get(jsonValue, "files");
    for (int fileIndex = 0; (fileIndex < JsonValue_size(files)); (fileIndex++)) {
        JsonValue* entry = JsonValue_at(files, fileIndex);
        int __fstr_119_arg0 = fileIndex;
        int __fstr_119_len = snprintf(NULL, 0, "bios.files[%d]", __fstr_119_arg0);
        char* __fstr_119_buf = __btrc_str_track(((char*)malloc((__fstr_119_len + 1))));
        snprintf(__fstr_119_buf, (__fstr_119_len + 1), "bios.files[%d]", __fstr_119_arg0);
        char* context = __fstr_119_buf;
        BiosFile* biosFile = BiosFile_new();
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, file, context, "expected object {name, note?}");
        } else {
            btrc_Vector_string* __list_120 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_120, "name");
            btrc_Vector_string_push(__list_120, "note");
            btrc_Vector_string* fileKeys = __list_120;
            JsonContract_checkKeys(entry, fileKeys, file, context, log);
            (biosFile->name = JsonContract_requiredString(entry, "name", file, context, log));
            (biosFile->note = JsonContract_optionalString(entry, "note", "", file, context, log));
            if (fileKeys != NULL) {
                if ((--fileKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(fileKeys);
                }
            }
        }
        btrc_Vector_BiosFile_p1_push(biosRequirement->files, biosFile);
        if (biosFile != NULL) {
            if ((--biosFile->__rc) <= 0) {
                BiosFile_destroy(biosFile);
            }
        }
    }
    if (matchModes != NULL) {
        if ((--matchModes->__rc) <= 0) {
            btrc_Vector_string_destroy(matchModes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return biosRequirement;
    if (matchModes != NULL) {
        if ((--matchModes->__rc) <= 0) {
            btrc_Vector_string_destroy(matchModes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (biosRequirement != NULL) {
        if ((--biosRequirement->__rc) <= 0) {
            BiosRequirement_destroy(biosRequirement);
        }
    }
}

void ScreenHole_init(ScreenHole* self) {
    self->__rc = 1;
    (self->screenId = "");
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    (self->hole = NormalizedRect_new());
    (self->hole->__rc++);
}

ScreenHole* ScreenHole_new(void) {
    ScreenHole* self = ((ScreenHole*)malloc(sizeof(ScreenHole)));
    memset(self, 0, sizeof(ScreenHole));
    ScreenHole_init(self);
    return self;
}

void ScreenHole_destroy(ScreenHole* self) {
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void BezelVariant_init(BezelVariant* self) {
    self->__rc = 1;
    (self->id = "");
    (self->label = "");
    (self->art = "");
    (self->glass = "");
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    (self->hole = NormalizedRect_new());
    (self->hole->__rc++);
    if (self->screenHoles != NULL) {
        if ((--self->screenHoles->__rc) <= 0) {
            btrc_Vector_ScreenHole_p1_free(self->screenHoles);
        }
    }
    btrc_Vector_ScreenHole_p1* __list_121 = btrc_Vector_ScreenHole_p1_new();
    (self->screenHoles = __list_121);
    (self->screenHoles->__rc++);
    (self->hasGeometryFrom = false);
    (self->geometrySystem = "");
    (self->geometryVariant = "");
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    (self->shellTint = RgbColor_new());
    (self->shellTint->__rc++);
    (self->hasReflect = false);
    (self->reflect = 0.0);
    (self->hasCurvature = false);
    (self->curvature = 0.0);
    (self->hasCornerRadius = false);
    (self->cornerRadius = 0.0);
}

BezelVariant* BezelVariant_new(void) {
    BezelVariant* self = ((BezelVariant*)malloc(sizeof(BezelVariant)));
    memset(self, 0, sizeof(BezelVariant));
    BezelVariant_init(self);
    return self;
}

void BezelVariant_destroy(BezelVariant* self) {
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    if (self->screenHoles != NULL) {
        if ((--self->screenHoles->__rc) <= 0) {
            btrc_Vector_ScreenHole_p1_free(self->screenHoles);
        }
    }
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void BezelCollection_init(BezelCollection* self) {
    self->__rc = 1;
    (self->present = false);
    (self->file = "");
    (self->enabled = false);
    (self->defaultVariant = "");
    (self->widescreenVariant = "");
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    (self->hole = NormalizedRect_new());
    (self->hole->__rc++);
    if (self->screenHoles != NULL) {
        if ((--self->screenHoles->__rc) <= 0) {
            btrc_Vector_ScreenHole_p1_free(self->screenHoles);
        }
    }
    btrc_Vector_ScreenHole_p1* __list_122 = btrc_Vector_ScreenHole_p1_new();
    (self->screenHoles = __list_122);
    (self->screenHoles->__rc++);
    if (self->variants != NULL) {
        if ((--self->variants->__rc) <= 0) {
            btrc_Vector_BezelVariant_p1_free(self->variants);
        }
    }
    btrc_Vector_BezelVariant_p1* __list_123 = btrc_Vector_BezelVariant_p1_new();
    (self->variants = __list_123);
    (self->variants->__rc++);
}

BezelCollection* BezelCollection_new(void) {
    BezelCollection* self = ((BezelCollection*)malloc(sizeof(BezelCollection)));
    memset(self, 0, sizeof(BezelCollection));
    BezelCollection_init(self);
    return self;
}

void BezelCollection_destroy(BezelCollection* self) {
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    if (self->screenHoles != NULL) {
        if ((--self->screenHoles->__rc) <= 0) {
            btrc_Vector_ScreenHole_p1_free(self->screenHoles);
        }
    }
    if (self->variants != NULL) {
        if ((--self->variants->__rc) <= 0) {
            btrc_Vector_BezelVariant_p1_free(self->variants);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

btrc_Vector_ScreenHole_p1* BezelCollectionParser_parseScreenHoles(JsonValue* jsonValue, btrc_Vector_string* screenIds, char* file, char* context, ParseLog* log) {
    btrc_Vector_ScreenHole_p1* holes = btrc_Vector_ScreenHole_p1_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object keyed by screen id");
        return holes;
    }
    btrc_Vector_string* __iter_124 = JsonValue_keys(jsonValue);
    int __n_126 = btrc_Vector_string_iterLen(__iter_124);
    for (int __i_125 = 0; (__i_125 < __n_126); (__i_125++)) {
        char* key = btrc_Vector_string_iterGet(__iter_124, __i_125);
        if (!btrc_Vector_string_contains(screenIds, key)) {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".")), key)), "unknown screen id (must match display.screens[].id)");
        }
        ScreenHole* screenHole = ScreenHole_new();
        (screenHole->screenId = key);
        if (screenHole->hole != NULL) {
            if ((--screenHole->hole->__rc) <= 0) {
                NormalizedRect_destroy(screenHole->hole);
            }
        }
        (screenHole->hole = JsonContract_rect(JsonValue_get(jsonValue, key), file, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".")), key)), log));
        (screenHole->hole->__rc++);
        btrc_Vector_ScreenHole_p1_push(holes, screenHole);
        if (screenHole != NULL) {
            if ((--screenHole->__rc) <= 0) {
                ScreenHole_destroy(screenHole);
            }
        }
    }
    return holes;
    if (holes != NULL) {
        if ((--holes->__rc) <= 0) {
            btrc_Vector_ScreenHole_p1_destroy(holes);
        }
    }
}

BezelVariant* BezelCollectionParser_parseVariant(JsonValue* jsonValue, btrc_Vector_string* screenIds, char* file, char* context, ParseLog* log) {
    BezelVariant* variant = BezelVariant_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return variant;
    }
    btrc_Vector_string* __list_127 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_127, "id");
    btrc_Vector_string_push(__list_127, "label");
    btrc_Vector_string_push(__list_127, "art");
    btrc_Vector_string_push(__list_127, "glass");
    btrc_Vector_string_push(__list_127, "hole");
    btrc_Vector_string_push(__list_127, "screen_holes");
    btrc_Vector_string_push(__list_127, "shell_tint");
    btrc_Vector_string_push(__list_127, "reflect");
    btrc_Vector_string_push(__list_127, "curvature");
    btrc_Vector_string_push(__list_127, "corner_radius");
    btrc_Vector_string_push(__list_127, "geometry_from");
    btrc_Vector_string* allowedKeys = __list_127;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (variant->id = JsonContract_requiredString(jsonValue, "id", file, context, log));
    (variant->label = JsonContract_requiredString(jsonValue, "label", file, context, log));
    (variant->art = JsonContract_optionalString(jsonValue, "art", "", file, context, log));
    JsonContract_checkAssetPath(variant->art, file, __btrc_str_track(__btrc_strcat(context, ".art")), log);
    (variant->glass = JsonContract_optionalString(jsonValue, "glass", "", file, context, log));
    JsonContract_checkAssetPath(variant->glass, file, __btrc_str_track(__btrc_strcat(context, ".glass")), log);
    if (JsonValue_has(jsonValue, "hole")) {
        if (variant->hole != NULL) {
            if ((--variant->hole->__rc) <= 0) {
                NormalizedRect_destroy(variant->hole);
            }
        }
        (variant->hole = JsonContract_rect(JsonValue_get(jsonValue, "hole"), file, __btrc_str_track(__btrc_strcat(context, ".hole")), log));
        (variant->hole->__rc++);
    }
    if (JsonValue_has(jsonValue, "screen_holes")) {
        if (variant->screenHoles != NULL) {
            if ((--variant->screenHoles->__rc) <= 0) {
                btrc_Vector_ScreenHole_p1_free(variant->screenHoles);
            }
        }
        (variant->screenHoles = BezelCollectionParser_parseScreenHoles(JsonValue_get(jsonValue, "screen_holes"), screenIds, file, __btrc_str_track(__btrc_strcat(context, ".screen_holes")), log));
        (variant->screenHoles->__rc++);
    }
    if (JsonValue_has(jsonValue, "shell_tint")) {
        if (variant->shellTint != NULL) {
            if ((--variant->shellTint->__rc) <= 0) {
                RgbColor_destroy(variant->shellTint);
            }
        }
        (variant->shellTint = JsonContract_rgbColor(JsonValue_get(jsonValue, "shell_tint"), file, __btrc_str_track(__btrc_strcat(context, ".shell_tint")), log));
        (variant->shellTint->__rc++);
    }
    if (JsonValue_has(jsonValue, "reflect")) {
        (variant->hasReflect = true);
        (variant->reflect = JsonContract_requiredNumber(jsonValue, "reflect", file, context, log));
    }
    if (JsonValue_has(jsonValue, "curvature")) {
        (variant->hasCurvature = true);
        (variant->curvature = JsonContract_requiredNumber(jsonValue, "curvature", file, context, log));
    }
    if (JsonValue_has(jsonValue, "corner_radius")) {
        (variant->hasCornerRadius = true);
        (variant->cornerRadius = JsonContract_requiredNumber(jsonValue, "corner_radius", file, context, log));
    }
    if (JsonValue_has(jsonValue, "geometry_from")) {
        JsonValue* geometryFrom = JsonValue_get(jsonValue, "geometry_from");
        if (!JsonValue_isObject(geometryFrom)) {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".geometry_from")), "expected object {system?, variant}");
        } else {
            btrc_Vector_string* __list_128 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_128, "system");
            btrc_Vector_string_push(__list_128, "variant");
            btrc_Vector_string* geometryKeys = __list_128;
            JsonContract_checkKeys(geometryFrom, geometryKeys, file, __btrc_str_track(__btrc_strcat(context, ".geometry_from")), log);
            (variant->hasGeometryFrom = true);
            (variant->geometrySystem = JsonContract_optionalString(geometryFrom, "system", "", file, __btrc_str_track(__btrc_strcat(context, ".geometry_from")), log));
            (variant->geometryVariant = JsonContract_requiredString(geometryFrom, "variant", file, __btrc_str_track(__btrc_strcat(context, ".geometry_from")), log));
            if (geometryKeys != NULL) {
                if ((--geometryKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(geometryKeys);
                }
            }
        }
    }
    if (__btrc_isEmpty(variant->art) && (!variant->hasGeometryFrom)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".art")), "missing (required unless geometry_from supplies it)");
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return variant;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (variant != NULL) {
        if ((--variant->__rc) <= 0) {
            BezelVariant_destroy(variant);
        }
    }
}

void BezelCollectionParser_parseVariants(BezelCollection* bezelCollection, JsonValue* root, btrc_Vector_string* screenIds, char* path, ParseLog* log) {
    if (((!JsonValue_has(root, "variants")) || (!JsonValue_isArray(JsonValue_get(root, "variants")))) || (JsonValue_size(JsonValue_get(root, "variants")) < 1)) {
        ParseLog_add(log, path, "variants", "expected non-empty array");
        return;
    }
    JsonValue* variants = JsonValue_get(root, "variants");
    btrc_Vector_string* seenVariantIds = btrc_Vector_string_new();
    for (int variantIndex = 0; (variantIndex < JsonValue_size(variants)); (variantIndex++)) {
        int __fstr_129_arg0 = variantIndex;
        int __fstr_129_len = snprintf(NULL, 0, "variants[%d]", __fstr_129_arg0);
        char* __fstr_129_buf = __btrc_str_track(((char*)malloc((__fstr_129_len + 1))));
        snprintf(__fstr_129_buf, (__fstr_129_len + 1), "variants[%d]", __fstr_129_arg0);
        BezelVariant* variant = BezelCollectionParser_parseVariant(JsonValue_at(variants, variantIndex), screenIds, path, __fstr_129_buf, log);
        if (btrc_Vector_string_contains(seenVariantIds, variant->id)) {
            int __fstr_130_arg0 = variantIndex;
            int __fstr_130_len = snprintf(NULL, 0, "variants[%d].id", __fstr_130_arg0);
            char* __fstr_130_buf = __btrc_str_track(((char*)malloc((__fstr_130_len + 1))));
            snprintf(__fstr_130_buf, (__fstr_130_len + 1), "variants[%d].id", __fstr_130_arg0);
            ParseLog_add(log, path, __fstr_130_buf, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate variant id '", variant->id)), "'")));
        }
        btrc_Vector_string_push(seenVariantIds, variant->id);
        btrc_Vector_BezelVariant_p1_push(bezelCollection->variants, variant);
    }
    if ((!__btrc_isEmpty(bezelCollection->defaultVariant)) && (!btrc_Vector_string_contains(seenVariantIds, bezelCollection->defaultVariant))) {
        ParseLog_add(log, path, "default_variant", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("references undeclared variant '", bezelCollection->defaultVariant)), "'")));
    }
    if ((!__btrc_isEmpty(bezelCollection->widescreenVariant)) && (!btrc_Vector_string_contains(seenVariantIds, bezelCollection->widescreenVariant))) {
        ParseLog_add(log, path, "widescreen_variant", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("references undeclared variant '", bezelCollection->widescreenVariant)), "'")));
    }
    if (seenVariantIds != NULL) {
        if ((--seenVariantIds->__rc) <= 0) {
            btrc_Vector_string_destroy(seenVariantIds);
        }
    }
}

BezelCollection* BezelCollectionParser_parseFile(char* path, btrc_Vector_string* screenIds, ParseLog* log) {
    BezelCollection* bezelCollection = BezelCollection_new();
    (bezelCollection->file = path);
    if (!FileSystem_isFile(path)) {
        return bezelCollection;
    }
    (bezelCollection->present = true);
    JsonValue* root = JsonValue_readFile(path);
    if (JsonValue_isError(root)) {
        ParseLog_add(log, path, "(root)", "invalid JSON");
        return bezelCollection;
    }
    JsonContract_checkSchemaVersion(root, 1, path, log);
    btrc_Vector_string* __list_131 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_131, "schema_version");
    btrc_Vector_string_push(__list_131, "enabled");
    btrc_Vector_string_push(__list_131, "default_variant");
    btrc_Vector_string_push(__list_131, "widescreen_variant");
    btrc_Vector_string_push(__list_131, "hole");
    btrc_Vector_string_push(__list_131, "screen_holes");
    btrc_Vector_string_push(__list_131, "variants");
    btrc_Vector_string* allowedKeys = __list_131;
    JsonContract_checkKeys(root, allowedKeys, path, "", log);
    (bezelCollection->enabled = JsonContract_requiredBool(root, "enabled", path, "", log));
    if (!bezelCollection->enabled) {
        if (allowedKeys != NULL) {
            if ((--allowedKeys->__rc) <= 0) {
                btrc_Vector_string_destroy(allowedKeys);
            }
        }
        return bezelCollection;
    }
    (bezelCollection->defaultVariant = JsonContract_requiredString(root, "default_variant", path, "", log));
    (bezelCollection->widescreenVariant = JsonContract_optionalString(root, "widescreen_variant", "", path, "", log));
    if (JsonValue_has(root, "hole")) {
        if (bezelCollection->hole != NULL) {
            if ((--bezelCollection->hole->__rc) <= 0) {
                NormalizedRect_destroy(bezelCollection->hole);
            }
        }
        (bezelCollection->hole = JsonContract_rect(JsonValue_get(root, "hole"), path, "hole", log));
        (bezelCollection->hole->__rc++);
    }
    if (JsonValue_has(root, "screen_holes")) {
        if (bezelCollection->screenHoles != NULL) {
            if ((--bezelCollection->screenHoles->__rc) <= 0) {
                btrc_Vector_ScreenHole_p1_free(bezelCollection->screenHoles);
            }
        }
        (bezelCollection->screenHoles = BezelCollectionParser_parseScreenHoles(JsonValue_get(root, "screen_holes"), screenIds, path, "screen_holes", log));
        (bezelCollection->screenHoles->__rc++);
    }
    BezelCollectionParser_parseVariants(bezelCollection, root, screenIds, path, log);
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return bezelCollection;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (bezelCollection != NULL) {
        if ((--bezelCollection->__rc) <= 0) {
            BezelCollection_destroy(bezelCollection);
        }
    }
}

void ShaderCollection_init(ShaderCollection* self) {
    self->__rc = 1;
    (self->present = false);
    (self->file = "");
    (self->enabled = false);
    (self->screen = "");
    (self->composite = "");
    (self->hasWidescreen = false);
    (self->widescreenScreen = "");
    (self->widescreenComposite = "");
}

ShaderCollection* ShaderCollection_new(void) {
    ShaderCollection* self = ((ShaderCollection*)malloc(sizeof(ShaderCollection)));
    memset(self, 0, sizeof(ShaderCollection));
    ShaderCollection_init(self);
    return self;
}

void ShaderCollection_destroy(ShaderCollection* self) {
    free(self);
}

void ShaderCollectionParser_parseWidescreen(ShaderCollection* shaderCollection, JsonValue* root, char* path, ParseLog* log) {
    JsonValue* widescreen = JsonValue_get(root, "widescreen");
    if (!JsonValue_isObject(widescreen)) {
        ParseLog_add(log, path, "widescreen", "expected object {screen?, composite?}");
        return;
    }
    btrc_Vector_string* __list_132 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_132, "screen");
    btrc_Vector_string_push(__list_132, "composite");
    btrc_Vector_string* widescreenKeys = __list_132;
    JsonContract_checkKeys(widescreen, widescreenKeys, path, "widescreen", log);
    (shaderCollection->hasWidescreen = true);
    (shaderCollection->widescreenScreen = JsonContract_optionalString(widescreen, "screen", "", path, "widescreen", log));
    JsonContract_checkAssetPath(shaderCollection->widescreenScreen, path, "widescreen.screen", log);
    (shaderCollection->widescreenComposite = JsonContract_optionalString(widescreen, "composite", "", path, "widescreen", log));
    JsonContract_checkAssetPath(shaderCollection->widescreenComposite, path, "widescreen.composite", log);
    if (widescreenKeys != NULL) {
        if ((--widescreenKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(widescreenKeys);
        }
    }
}

ShaderCollection* ShaderCollectionParser_parseFile(char* path, ParseLog* log) {
    ShaderCollection* shaderCollection = ShaderCollection_new();
    (shaderCollection->file = path);
    if (!FileSystem_isFile(path)) {
        return shaderCollection;
    }
    (shaderCollection->present = true);
    JsonValue* root = JsonValue_readFile(path);
    if (JsonValue_isError(root)) {
        ParseLog_add(log, path, "(root)", "invalid JSON");
        return shaderCollection;
    }
    JsonContract_checkSchemaVersion(root, 1, path, log);
    btrc_Vector_string* __list_133 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_133, "schema_version");
    btrc_Vector_string_push(__list_133, "enabled");
    btrc_Vector_string_push(__list_133, "screen");
    btrc_Vector_string_push(__list_133, "composite");
    btrc_Vector_string_push(__list_133, "widescreen");
    btrc_Vector_string* allowedKeys = __list_133;
    JsonContract_checkKeys(root, allowedKeys, path, "", log);
    (shaderCollection->enabled = JsonContract_requiredBool(root, "enabled", path, "", log));
    if (!shaderCollection->enabled) {
        if ((JsonValue_has(root, "screen") || JsonValue_has(root, "composite")) || JsonValue_has(root, "widescreen")) {
            ParseLog_add(log, path, "enabled", "disabled shaders must not declare presets");
        }
        if (allowedKeys != NULL) {
            if ((--allowedKeys->__rc) <= 0) {
                btrc_Vector_string_destroy(allowedKeys);
            }
        }
        return shaderCollection;
    }
    (shaderCollection->screen = JsonContract_requiredString(root, "screen", path, "", log));
    JsonContract_checkAssetPath(shaderCollection->screen, path, "screen", log);
    (shaderCollection->composite = JsonContract_optionalString(root, "composite", "", path, "", log));
    JsonContract_checkAssetPath(shaderCollection->composite, path, "composite", log);
    if (JsonValue_has(root, "widescreen")) {
        ShaderCollectionParser_parseWidescreen(shaderCollection, root, path, log);
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return shaderCollection;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (shaderCollection != NULL) {
        if ((--shaderCollection->__rc) <= 0) {
            ShaderCollection_destroy(shaderCollection);
        }
    }
}

void SystemDefinition_init(SystemDefinition* self) {
    self->__rc = 1;
    (self->id = "");
    (self->file = "");
    (self->name = "");
    if (self->aliases != NULL) {
        if ((--self->aliases->__rc) <= 0) {
            btrc_Vector_string_free(self->aliases);
        }
    }
    btrc_Vector_string* __list_134 = btrc_Vector_string_new();
    (self->aliases = __list_134);
    (self->aliases->__rc++);
    if (self->esDePlatforms != NULL) {
        if ((--self->esDePlatforms->__rc) <= 0) {
            btrc_Vector_string_free(self->esDePlatforms);
        }
    }
    btrc_Vector_string* __list_135 = btrc_Vector_string_new();
    (self->esDePlatforms = __list_135);
    (self->esDePlatforms->__rc++);
    (self->esDeTheme = "");
    (self->romDirectory = "");
    if (self->extensions != NULL) {
        if ((--self->extensions->__rc) <= 0) {
            btrc_Vector_string_free(self->extensions);
        }
    }
    btrc_Vector_string* __list_136 = btrc_Vector_string_new();
    (self->extensions = __list_136);
    (self->extensions->__rc++);
    if (self->emulators != NULL) {
        if ((--self->emulators->__rc) <= 0) {
            btrc_Vector_EmulatorBinding_p1_free(self->emulators);
        }
    }
    btrc_Vector_EmulatorBinding_p1* __list_137 = btrc_Vector_EmulatorBinding_p1_new();
    (self->emulators = __list_137);
    (self->emulators->__rc++);
    if (self->bios != NULL) {
        if ((--self->bios->__rc) <= 0) {
            BiosRequirement_destroy(self->bios);
        }
    }
    (self->bios = BiosRequirement_new());
    (self->bios->__rc++);
    (self->controllerProfile = "");
    if (self->display != NULL) {
        if ((--self->display->__rc) <= 0) {
            DisplayPolicy_destroy(self->display);
        }
    }
    (self->display = DisplayPolicy_new());
    (self->display->__rc++);
    if (self->render != NULL) {
        if ((--self->render->__rc) <= 0) {
            RenderPolicy_destroy(self->render);
        }
    }
    (self->render = RenderPolicy_new());
    (self->render->__rc++);
    if (self->bezels != NULL) {
        if ((--self->bezels->__rc) <= 0) {
            BezelCollection_destroy(self->bezels);
        }
    }
    (self->bezels = BezelCollection_new());
    (self->bezels->__rc++);
    if (self->shaders != NULL) {
        if ((--self->shaders->__rc) <= 0) {
            ShaderCollection_destroy(self->shaders);
        }
    }
    (self->shaders = ShaderCollection_new());
    (self->shaders->__rc++);
}

SystemDefinition* SystemDefinition_new(void) {
    SystemDefinition* self = ((SystemDefinition*)malloc(sizeof(SystemDefinition)));
    memset(self, 0, sizeof(SystemDefinition));
    SystemDefinition_init(self);
    return self;
}

void SystemDefinition_destroy(SystemDefinition* self) {
    if (self->aliases != NULL) {
        if ((--self->aliases->__rc) <= 0) {
            btrc_Vector_string_free(self->aliases);
        }
    }
    if (self->esDePlatforms != NULL) {
        if ((--self->esDePlatforms->__rc) <= 0) {
            btrc_Vector_string_free(self->esDePlatforms);
        }
    }
    if (self->extensions != NULL) {
        if ((--self->extensions->__rc) <= 0) {
            btrc_Vector_string_free(self->extensions);
        }
    }
    if (self->emulators != NULL) {
        if ((--self->emulators->__rc) <= 0) {
            btrc_Vector_EmulatorBinding_p1_free(self->emulators);
        }
    }
    if (self->bios != NULL) {
        if ((--self->bios->__rc) <= 0) {
            BiosRequirement_destroy(self->bios);
        }
    }
    if (self->display != NULL) {
        if ((--self->display->__rc) <= 0) {
            DisplayPolicy_destroy(self->display);
        }
    }
    if (self->render != NULL) {
        if ((--self->render->__rc) <= 0) {
            RenderPolicy_destroy(self->render);
        }
    }
    if (self->bezels != NULL) {
        if ((--self->bezels->__rc) <= 0) {
            BezelCollection_destroy(self->bezels);
        }
    }
    if (self->shaders != NULL) {
        if ((--self->shaders->__rc) <= 0) {
            ShaderCollection_destroy(self->shaders);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

btrc_Vector_string* SystemDefinition_screenIds(SystemDefinition* self) {
    btrc_Vector_string* ids = btrc_Vector_string_new();
    btrc_Vector_SystemScreen_p1* __iter_138 = self->display->screens;
    int __n_140 = btrc_Vector_SystemScreen_p1_iterLen(__iter_138);
    for (int __i_139 = 0; (__i_139 < __n_140); (__i_139++)) {
        SystemScreen* screen = btrc_Vector_SystemScreen_p1_iterGet(__iter_138, __i_139);
        btrc_Vector_string_push(ids, screen->id);
    }
    return ids;
    if (ids != NULL) {
        if ((--ids->__rc) <= 0) {
            btrc_Vector_string_destroy(ids);
        }
    }
}

bool TemplateTokens_allowed(char* token, bool allowHostPortable) {
    btrc_Vector_string* __list_141 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_141, "home");
    btrc_Vector_string_push(__list_141, "project");
    btrc_Vector_string_push(__list_141, "roms");
    btrc_Vector_string_push(__list_141, "state_root");
    btrc_Vector_string_push(__list_141, "emulation_root");
    btrc_Vector_string_push(__list_141, "flatpak_home");
    btrc_Vector_string_push(__list_141, "nix_result");
    btrc_Vector_string_push(__list_141, "asset_root");
    btrc_Vector_string_push(__list_141, "paths.project_roms");
    btrc_Vector_string_push(__list_141, "paths.project_bios");
    btrc_Vector_string_push(__list_141, "paths.project_saves");
    btrc_Vector_string_push(__list_141, "paths.project_states");
    btrc_Vector_string_push(__list_141, "paths.project_screenshots");
    btrc_Vector_string_push(__list_141, "paths.project_gamelists");
    btrc_Vector_string* exactTokens = __list_141;
    if (btrc_Vector_string_contains(exactTokens, token)) {
        bool __btrc_ret_142 = true;
        if (exactTokens != NULL) {
            if ((--exactTokens->__rc) <= 0) {
                btrc_Vector_string_destroy(exactTokens);
            }
        }
        return __btrc_ret_142;
    }
    if (__btrc_startsWith(token, "env:") && (((int)strlen(token)) > 4)) {
        bool __btrc_ret_143 = true;
        if (exactTokens != NULL) {
            if ((--exactTokens->__rc) <= 0) {
                btrc_Vector_string_destroy(exactTokens);
            }
        }
        return __btrc_ret_143;
    }
    if (allowHostPortable && ((strcmp(token, "host") == 0) || (strcmp(token, "portable") == 0))) {
        bool __btrc_ret_144 = true;
        if (exactTokens != NULL) {
            if ((--exactTokens->__rc) <= 0) {
                btrc_Vector_string_destroy(exactTokens);
            }
        }
        return __btrc_ret_144;
    }
    bool __btrc_ret_145 = false;
    if (exactTokens != NULL) {
        if ((--exactTokens->__rc) <= 0) {
            btrc_Vector_string_destroy(exactTokens);
        }
    }
    return __btrc_ret_145;
    if (exactTokens != NULL) {
        if ((--exactTokens->__rc) <= 0) {
            btrc_Vector_string_destroy(exactTokens);
        }
    }
}

void TemplateTokens_check(char* value, char* file, char* context, ParseLog* log, bool allowHostPortable) {
    char* rest = value;
    while (__btrc_strContains(rest, "${")) {
        int openIndex = __btrc_indexOf(rest, "${");
        char* after = __btrc_str_track(__btrc_substring(rest, (openIndex + 2), ((int)strlen(rest))));
        int closeIndex = __btrc_indexOf(after, "}");
        if (closeIndex < 0) {
            ParseLog_add(log, file, context, "unterminated template token");
            return;
        }
        char* token = __btrc_str_track(__btrc_substring(after, 0, closeIndex));
        if (!TemplateTokens_allowed(token, allowHostPortable)) {
            ParseLog_add(log, file, context, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown template token '", token)), "'")));
        }
        (rest = __btrc_str_track(__btrc_substring(after, (closeIndex + 1), ((int)strlen(after)))));
    }
}

void SandboxFilesystem_init(SandboxFilesystem* self) {
    self->__rc = 1;
    (self->path = "");
    (self->mode = "");
}

SandboxFilesystem* SandboxFilesystem_new(void) {
    SandboxFilesystem* self = ((SandboxFilesystem*)malloc(sizeof(SandboxFilesystem)));
    memset(self, 0, sizeof(SandboxFilesystem));
    SandboxFilesystem_init(self);
    return self;
}

void SandboxFilesystem_destroy(SandboxFilesystem* self) {
    free(self);
}

void SandboxPolicy_init(SandboxPolicy* self) {
    self->__rc = 1;
    (self->present = false);
    if (self->sockets != NULL) {
        if ((--self->sockets->__rc) <= 0) {
            btrc_Vector_string_free(self->sockets);
        }
    }
    btrc_Vector_string* __list_146 = btrc_Vector_string_new();
    (self->sockets = __list_146);
    (self->sockets->__rc++);
    if (self->devices != NULL) {
        if ((--self->devices->__rc) <= 0) {
            btrc_Vector_string_free(self->devices);
        }
    }
    btrc_Vector_string* __list_147 = btrc_Vector_string_new();
    (self->devices = __list_147);
    (self->devices->__rc++);
    if (self->share != NULL) {
        if ((--self->share->__rc) <= 0) {
            btrc_Vector_string_free(self->share);
        }
    }
    btrc_Vector_string* __list_148 = btrc_Vector_string_new();
    (self->share = __list_148);
    (self->share->__rc++);
    if (self->filesystems != NULL) {
        if ((--self->filesystems->__rc) <= 0) {
            btrc_Vector_SandboxFilesystem_p1_free(self->filesystems);
        }
    }
    btrc_Vector_SandboxFilesystem_p1* __list_149 = btrc_Vector_SandboxFilesystem_p1_new();
    (self->filesystems = __list_149);
    (self->filesystems->__rc++);
    if (self->extraArguments != NULL) {
        if ((--self->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(self->extraArguments);
        }
    }
    btrc_Vector_string* __list_150 = btrc_Vector_string_new();
    (self->extraArguments = __list_150);
    (self->extraArguments->__rc++);
    (self->runtime = "");
    (self->command = "");
    if (self->symlinkLinks != NULL) {
        if ((--self->symlinkLinks->__rc) <= 0) {
            btrc_Vector_string_free(self->symlinkLinks);
        }
    }
    btrc_Vector_string* __list_151 = btrc_Vector_string_new();
    (self->symlinkLinks = __list_151);
    (self->symlinkLinks->__rc++);
    if (self->symlinkTargets != NULL) {
        if ((--self->symlinkTargets->__rc) <= 0) {
            btrc_Vector_string_free(self->symlinkTargets);
        }
    }
    btrc_Vector_string* __list_152 = btrc_Vector_string_new();
    (self->symlinkTargets = __list_152);
    (self->symlinkTargets->__rc++);
}

SandboxPolicy* SandboxPolicy_new(void) {
    SandboxPolicy* self = ((SandboxPolicy*)malloc(sizeof(SandboxPolicy)));
    memset(self, 0, sizeof(SandboxPolicy));
    SandboxPolicy_init(self);
    return self;
}

void SandboxPolicy_destroy(SandboxPolicy* self) {
    if (self->sockets != NULL) {
        if ((--self->sockets->__rc) <= 0) {
            btrc_Vector_string_free(self->sockets);
        }
    }
    if (self->devices != NULL) {
        if ((--self->devices->__rc) <= 0) {
            btrc_Vector_string_free(self->devices);
        }
    }
    if (self->share != NULL) {
        if ((--self->share->__rc) <= 0) {
            btrc_Vector_string_free(self->share);
        }
    }
    if (self->filesystems != NULL) {
        if ((--self->filesystems->__rc) <= 0) {
            btrc_Vector_SandboxFilesystem_p1_free(self->filesystems);
        }
    }
    if (self->extraArguments != NULL) {
        if ((--self->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(self->extraArguments);
        }
    }
    if (self->symlinkLinks != NULL) {
        if ((--self->symlinkLinks->__rc) <= 0) {
            btrc_Vector_string_free(self->symlinkLinks);
        }
    }
    if (self->symlinkTargets != NULL) {
        if ((--self->symlinkTargets->__rc) <= 0) {
            btrc_Vector_string_free(self->symlinkTargets);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void SandboxPolicyParser_parseFilesystems(SandboxPolicy* sandbox, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    JsonValue* filesystems = JsonValue_get(jsonValue, "filesystems");
    if (!JsonValue_isArray(filesystems)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".filesystems")), "expected array");
        return;
    }
    for (int entryIndex = 0; (entryIndex < JsonValue_size(filesystems)); (entryIndex++)) {
        JsonValue* entry = JsonValue_at(filesystems, entryIndex);
        char* __fstr_153_arg0 = context;
        int __fstr_153_arg1 = entryIndex;
        int __fstr_153_len = snprintf(NULL, 0, "%s.filesystems[%d]", __fstr_153_arg0, __fstr_153_arg1);
        char* __fstr_153_buf = __btrc_str_track(((char*)malloc((__fstr_153_len + 1))));
        snprintf(__fstr_153_buf, (__fstr_153_len + 1), "%s.filesystems[%d]", __fstr_153_arg0, __fstr_153_arg1);
        char* entryContext = __fstr_153_buf;
        SandboxFilesystem* filesystemGrant = SandboxFilesystem_new();
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, file, entryContext, "expected object {path, mode}");
        } else {
            btrc_Vector_string* __list_154 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_154, "path");
            btrc_Vector_string_push(__list_154, "mode");
            btrc_Vector_string* filesystemKeys = __list_154;
            JsonContract_checkKeys(entry, filesystemKeys, file, entryContext, log);
            (filesystemGrant->path = JsonContract_requiredString(entry, "path", file, entryContext, log));
            if (!(strcmp(filesystemGrant->path, "host") == 0)) {
                TemplateTokens_check(filesystemGrant->path, file, __btrc_str_track(__btrc_strcat(entryContext, ".path")), log, false);
            }
            (filesystemGrant->mode = JsonContract_requiredString(entry, "mode", file, entryContext, log));
            btrc_Vector_string* __list_155 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_155, "ro");
            btrc_Vector_string_push(__list_155, "rw");
            btrc_Vector_string_push(__list_155, "create");
            btrc_Vector_string* modes = __list_155;
            JsonContract_checkEnum(filesystemGrant->mode, modes, file, __btrc_str_track(__btrc_strcat(entryContext, ".mode")), log);
            if (modes != NULL) {
                if ((--modes->__rc) <= 0) {
                    btrc_Vector_string_destroy(modes);
                }
            }
            if (filesystemKeys != NULL) {
                if ((--filesystemKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(filesystemKeys);
                }
            }
        }
        btrc_Vector_SandboxFilesystem_p1_push(sandbox->filesystems, filesystemGrant);
        if (filesystemGrant != NULL) {
            if ((--filesystemGrant->__rc) <= 0) {
                SandboxFilesystem_destroy(filesystemGrant);
            }
        }
    }
}

void SandboxPolicyParser_parseSymlinks(SandboxPolicy* sandbox, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    JsonValue* symlinks = JsonValue_get(jsonValue, "symlinks");
    if (!JsonValue_isArray(symlinks)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".symlinks")), "expected array");
        return;
    }
    for (int entryIndex = 0; (entryIndex < JsonValue_size(symlinks)); (entryIndex++)) {
        JsonValue* entry = JsonValue_at(symlinks, entryIndex);
        char* __fstr_156_arg0 = context;
        int __fstr_156_arg1 = entryIndex;
        int __fstr_156_len = snprintf(NULL, 0, "%s.symlinks[%d]", __fstr_156_arg0, __fstr_156_arg1);
        char* __fstr_156_buf = __btrc_str_track(((char*)malloc((__fstr_156_len + 1))));
        snprintf(__fstr_156_buf, (__fstr_156_len + 1), "%s.symlinks[%d]", __fstr_156_arg0, __fstr_156_arg1);
        char* entryContext = __fstr_156_buf;
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, file, entryContext, "expected object {link, target}");
        } else {
            btrc_Vector_string* __list_157 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_157, "link");
            btrc_Vector_string_push(__list_157, "target");
            btrc_Vector_string* symlinkKeys = __list_157;
            JsonContract_checkKeys(entry, symlinkKeys, file, entryContext, log);
            char* link = JsonContract_requiredString(entry, "link", file, entryContext, log);
            char* target = JsonContract_requiredString(entry, "target", file, entryContext, log);
            TemplateTokens_check(link, file, __btrc_str_track(__btrc_strcat(entryContext, ".link")), log, true);
            TemplateTokens_check(target, file, __btrc_str_track(__btrc_strcat(entryContext, ".target")), log, true);
            btrc_Vector_string_push(sandbox->symlinkLinks, link);
            btrc_Vector_string_push(sandbox->symlinkTargets, target);
            if (symlinkKeys != NULL) {
                if ((--symlinkKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(symlinkKeys);
                }
            }
        }
    }
}

SandboxPolicy* SandboxPolicyParser_parse(JsonValue* jsonValue, char* operatingSystemName, char* file, char* context, ParseLog* log) {
    SandboxPolicy* sandbox = SandboxPolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return sandbox;
    }
    (sandbox->present = true);
    btrc_Vector_string* __list_158 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_158, "sockets");
    btrc_Vector_string_push(__list_158, "devices");
    btrc_Vector_string_push(__list_158, "share");
    btrc_Vector_string_push(__list_158, "filesystems");
    btrc_Vector_string_push(__list_158, "extra_args");
    btrc_Vector_string_push(__list_158, "runtime");
    btrc_Vector_string_push(__list_158, "command");
    btrc_Vector_string_push(__list_158, "symlinks");
    btrc_Vector_string* allowedKeys = __list_158;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    if (sandbox->sockets != NULL) {
        if ((--sandbox->sockets->__rc) <= 0) {
            btrc_Vector_string_free(sandbox->sockets);
        }
    }
    (sandbox->sockets = JsonContract_stringArray(jsonValue, "sockets", false, file, context, log));
    (sandbox->sockets->__rc++);
    if (sandbox->devices != NULL) {
        if ((--sandbox->devices->__rc) <= 0) {
            btrc_Vector_string_free(sandbox->devices);
        }
    }
    (sandbox->devices = JsonContract_stringArray(jsonValue, "devices", false, file, context, log));
    (sandbox->devices->__rc++);
    if (sandbox->share != NULL) {
        if ((--sandbox->share->__rc) <= 0) {
            btrc_Vector_string_free(sandbox->share);
        }
    }
    (sandbox->share = JsonContract_stringArray(jsonValue, "share", false, file, context, log));
    (sandbox->share->__rc++);
    if (sandbox->extraArguments != NULL) {
        if ((--sandbox->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(sandbox->extraArguments);
        }
    }
    (sandbox->extraArguments = JsonContract_stringArray(jsonValue, "extra_args", false, file, context, log));
    (sandbox->extraArguments->__rc++);
    if ((strcmp(operatingSystemName, "macos") == 0) && (JsonValue_has(jsonValue, "runtime") || JsonValue_has(jsonValue, "command"))) {
        ParseLog_add(log, file, context, "runtime/command are linux-only (flatpak) fields");
    }
    (sandbox->command = JsonContract_optionalString(jsonValue, "command", "", file, context, log));
    TemplateTokens_check(sandbox->command, file, __btrc_str_track(__btrc_strcat(context, ".command")), log, false);
    (sandbox->runtime = JsonContract_optionalString(jsonValue, "runtime", "", file, context, log));
    if (JsonValue_has(jsonValue, "filesystems")) {
        SandboxPolicyParser_parseFilesystems(sandbox, jsonValue, file, context, log);
    }
    if (JsonValue_has(jsonValue, "symlinks")) {
        SandboxPolicyParser_parseSymlinks(sandbox, jsonValue, file, context, log);
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return sandbox;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (sandbox != NULL) {
        if ((--sandbox->__rc) <= 0) {
            SandboxPolicy_destroy(sandbox);
        }
    }
}

void GraphicsPolicy_init(GraphicsPolicy* self) {
    self->__rc = 1;
    (self->present = false);
    (self->method = "");
    (self->file = "");
    (self->format = "");
    (self->key = "");
    (self->valueOpenGl = "");
    (self->valueVulkan = "");
    (self->tapApi = "");
    (self->overlayApi = "");
}

GraphicsPolicy* GraphicsPolicy_new(void) {
    GraphicsPolicy* self = ((GraphicsPolicy*)malloc(sizeof(GraphicsPolicy)));
    memset(self, 0, sizeof(GraphicsPolicy));
    GraphicsPolicy_init(self);
    return self;
}

void GraphicsPolicy_destroy(GraphicsPolicy* self) {
    free(self);
}

void GraphicsPolicyParser_parseConfigFileFields(GraphicsPolicy* graphics, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    (graphics->file = JsonContract_requiredString(jsonValue, "file", file, context, log));
    TemplateTokens_check(graphics->file, file, __btrc_str_track(__btrc_strcat(context, ".file")), log, false);
    (graphics->format = JsonContract_requiredString(jsonValue, "format", file, context, log));
    btrc_Vector_string* __list_159 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_159, "ini");
    btrc_Vector_string_push(__list_159, "xml");
    btrc_Vector_string_push(__list_159, "toml");
    btrc_Vector_string_push(__list_159, "qt_ini");
    btrc_Vector_string* formats = __list_159;
    JsonContract_checkEnum(graphics->format, formats, file, __btrc_str_track(__btrc_strcat(context, ".format")), log);
    (graphics->key = JsonContract_requiredString(jsonValue, "key", file, context, log));
    if ((!JsonValue_has(jsonValue, "values")) || (!JsonValue_isObject(JsonValue_get(jsonValue, "values")))) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".values")), "expected object {opengl?, vulkan?}");
        if (formats != NULL) {
            if ((--formats->__rc) <= 0) {
                btrc_Vector_string_destroy(formats);
            }
        }
        return;
    }
    JsonValue* values = JsonValue_get(jsonValue, "values");
    btrc_Vector_string* __list_160 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_160, "opengl");
    btrc_Vector_string_push(__list_160, "vulkan");
    btrc_Vector_string* valueKeys = __list_160;
    JsonContract_checkKeys(values, valueKeys, file, __btrc_str_track(__btrc_strcat(context, ".values")), log);
    (graphics->valueOpenGl = JsonContract_optionalString(values, "opengl", "", file, __btrc_str_track(__btrc_strcat(context, ".values")), log));
    (graphics->valueVulkan = JsonContract_optionalString(values, "vulkan", "", file, __btrc_str_track(__btrc_strcat(context, ".values")), log));
    if (valueKeys != NULL) {
        if ((--valueKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(valueKeys);
        }
    }
    if (formats != NULL) {
        if ((--formats->__rc) <= 0) {
            btrc_Vector_string_destroy(formats);
        }
    }
}

GraphicsPolicy* GraphicsPolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    GraphicsPolicy* graphics = GraphicsPolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return graphics;
    }
    (graphics->present = true);
    btrc_Vector_string* __list_161 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_161, "method");
    btrc_Vector_string_push(__list_161, "file");
    btrc_Vector_string_push(__list_161, "format");
    btrc_Vector_string_push(__list_161, "key");
    btrc_Vector_string_push(__list_161, "values");
    btrc_Vector_string_push(__list_161, "tap_api");
    btrc_Vector_string_push(__list_161, "overlay_api");
    btrc_Vector_string* allowedKeys = __list_161;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (graphics->method = JsonContract_requiredString(jsonValue, "method", file, context, log));
    btrc_Vector_string* __list_162 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_162, "config_file");
    btrc_Vector_string_push(__list_162, "cli");
    btrc_Vector_string_push(__list_162, "dynamic");
    btrc_Vector_string_push(__list_162, "none");
    btrc_Vector_string* methods = __list_162;
    JsonContract_checkEnum(graphics->method, methods, file, __btrc_str_track(__btrc_strcat(context, ".method")), log);
    if (strcmp(graphics->method, "config_file") == 0) {
        GraphicsPolicyParser_parseConfigFileFields(graphics, jsonValue, file, context, log);
    } else if (((JsonValue_has(jsonValue, "file") || JsonValue_has(jsonValue, "format")) || JsonValue_has(jsonValue, "key")) || JsonValue_has(jsonValue, "values")) {
        ParseLog_add(log, file, context, "file/format/key/values only valid with method 'config_file'");
    }
    btrc_Vector_string* __list_163 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_163, "opengl");
    btrc_Vector_string_push(__list_163, "vulkan");
    btrc_Vector_string* graphicsApis = __list_163;
    (graphics->tapApi = JsonContract_optionalString(jsonValue, "tap_api", "", file, context, log));
    JsonContract_checkEnum(graphics->tapApi, graphicsApis, file, __btrc_str_track(__btrc_strcat(context, ".tap_api")), log);
    (graphics->overlayApi = JsonContract_optionalString(jsonValue, "overlay_api", "", file, context, log));
    JsonContract_checkEnum(graphics->overlayApi, graphicsApis, file, __btrc_str_track(__btrc_strcat(context, ".overlay_api")), log);
    if (graphicsApis != NULL) {
        if ((--graphicsApis->__rc) <= 0) {
            btrc_Vector_string_destroy(graphicsApis);
        }
    }
    if (methods != NULL) {
        if ((--methods->__rc) <= 0) {
            btrc_Vector_string_destroy(methods);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return graphics;
    if (graphicsApis != NULL) {
        if ((--graphicsApis->__rc) <= 0) {
            btrc_Vector_string_destroy(graphicsApis);
        }
    }
    if (methods != NULL) {
        if ((--methods->__rc) <= 0) {
            btrc_Vector_string_destroy(methods);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (graphics != NULL) {
        if ((--graphics->__rc) <= 0) {
            GraphicsPolicy_destroy(graphics);
        }
    }
}

void AspectProbe_init(AspectProbe* self) {
    self->__rc = 1;
    (self->present = false);
    if (self->files != NULL) {
        if ((--self->files->__rc) <= 0) {
            btrc_Vector_string_free(self->files);
        }
    }
    btrc_Vector_string* __list_164 = btrc_Vector_string_new();
    (self->files = __list_164);
    (self->files->__rc++);
    if (self->widescreenMarkers != NULL) {
        if ((--self->widescreenMarkers->__rc) <= 0) {
            btrc_Vector_string_free(self->widescreenMarkers);
        }
    }
    btrc_Vector_string* __list_165 = btrc_Vector_string_new();
    (self->widescreenMarkers = __list_165);
    (self->widescreenMarkers->__rc++);
    if (self->standardMarkers != NULL) {
        if ((--self->standardMarkers->__rc) <= 0) {
            btrc_Vector_string_free(self->standardMarkers);
        }
    }
    btrc_Vector_string* __list_166 = btrc_Vector_string_new();
    (self->standardMarkers = __list_166);
    (self->standardMarkers->__rc++);
    (self->fallback = "");
}

AspectProbe* AspectProbe_new(void) {
    AspectProbe* self = ((AspectProbe*)malloc(sizeof(AspectProbe)));
    memset(self, 0, sizeof(AspectProbe));
    AspectProbe_init(self);
    return self;
}

void AspectProbe_destroy(AspectProbe* self) {
    if (self->files != NULL) {
        if ((--self->files->__rc) <= 0) {
            btrc_Vector_string_free(self->files);
        }
    }
    if (self->widescreenMarkers != NULL) {
        if ((--self->widescreenMarkers->__rc) <= 0) {
            btrc_Vector_string_free(self->widescreenMarkers);
        }
    }
    if (self->standardMarkers != NULL) {
        if ((--self->standardMarkers->__rc) <= 0) {
            btrc_Vector_string_free(self->standardMarkers);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

AspectProbe* AspectProbeParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    AspectProbe* probe = AspectProbe_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return probe;
    }
    (probe->present = true);
    btrc_Vector_string* __list_167 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_167, "files");
    btrc_Vector_string_push(__list_167, "widescreen_markers");
    btrc_Vector_string_push(__list_167, "standard_markers");
    btrc_Vector_string_push(__list_167, "fallback");
    btrc_Vector_string* allowedKeys = __list_167;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    if (probe->files != NULL) {
        if ((--probe->files->__rc) <= 0) {
            btrc_Vector_string_free(probe->files);
        }
    }
    (probe->files = JsonContract_stringArray(jsonValue, "files", true, file, context, log));
    (probe->files->__rc++);
    btrc_Vector_string* __iter_168 = probe->files;
    int __n_170 = btrc_Vector_string_iterLen(__iter_168);
    for (int __i_169 = 0; (__i_169 < __n_170); (__i_169++)) {
        char* probeFile = btrc_Vector_string_iterGet(__iter_168, __i_169);
        TemplateTokens_check(probeFile, file, __btrc_str_track(__btrc_strcat(context, ".files")), log, false);
    }
    if (probe->widescreenMarkers != NULL) {
        if ((--probe->widescreenMarkers->__rc) <= 0) {
            btrc_Vector_string_free(probe->widescreenMarkers);
        }
    }
    (probe->widescreenMarkers = JsonContract_stringArray(jsonValue, "widescreen_markers", true, file, context, log));
    (probe->widescreenMarkers->__rc++);
    if (probe->standardMarkers != NULL) {
        if ((--probe->standardMarkers->__rc) <= 0) {
            btrc_Vector_string_free(probe->standardMarkers);
        }
    }
    (probe->standardMarkers = JsonContract_stringArray(jsonValue, "standard_markers", true, file, context, log));
    (probe->standardMarkers->__rc++);
    (probe->fallback = JsonContract_requiredString(jsonValue, "fallback", file, context, log));
    btrc_Vector_string* __list_171 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_171, "standard");
    btrc_Vector_string_push(__list_171, "widescreen");
    btrc_Vector_string* fallbacks = __list_171;
    JsonContract_checkEnum(probe->fallback, fallbacks, file, __btrc_str_track(__btrc_strcat(context, ".fallback")), log);
    if (fallbacks != NULL) {
        if ((--fallbacks->__rc) <= 0) {
            btrc_Vector_string_destroy(fallbacks);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return probe;
    if (fallbacks != NULL) {
        if ((--fallbacks->__rc) <= 0) {
            btrc_Vector_string_destroy(fallbacks);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (probe != NULL) {
        if ((--probe->__rc) <= 0) {
            AspectProbe_destroy(probe);
        }
    }
}

void StateSeed_init(StateSeed* self) {
    self->__rc = 1;
    (self->purpose = "");
    (self->target = "");
    if (self->sources != NULL) {
        if ((--self->sources->__rc) <= 0) {
            btrc_Vector_string_free(self->sources);
        }
    }
    btrc_Vector_string* __list_172 = btrc_Vector_string_new();
    (self->sources = __list_172);
    (self->sources->__rc++);
}

StateSeed* StateSeed_new(void) {
    StateSeed* self = ((StateSeed*)malloc(sizeof(StateSeed)));
    memset(self, 0, sizeof(StateSeed));
    StateSeed_init(self);
    return self;
}

void StateSeed_destroy(StateSeed* self) {
    if (self->sources != NULL) {
        if ((--self->sources->__rc) <= 0) {
            btrc_Vector_string_free(self->sources);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void StatePolicy_init(StatePolicy* self) {
    self->__rc = 1;
    (self->present = false);
    (self->configHome = "");
    (self->dataHome = "");
    if (self->extraArguments != NULL) {
        if ((--self->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(self->extraArguments);
        }
    }
    btrc_Vector_string* __list_173 = btrc_Vector_string_new();
    (self->extraArguments = __list_173);
    (self->extraArguments->__rc++);
    if (self->seeds != NULL) {
        if ((--self->seeds->__rc) <= 0) {
            btrc_Vector_StateSeed_p1_free(self->seeds);
        }
    }
    btrc_Vector_StateSeed_p1* __list_174 = btrc_Vector_StateSeed_p1_new();
    (self->seeds = __list_174);
    (self->seeds->__rc++);
}

StatePolicy* StatePolicy_new(void) {
    StatePolicy* self = ((StatePolicy*)malloc(sizeof(StatePolicy)));
    memset(self, 0, sizeof(StatePolicy));
    StatePolicy_init(self);
    return self;
}

void StatePolicy_destroy(StatePolicy* self) {
    if (self->extraArguments != NULL) {
        if ((--self->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(self->extraArguments);
        }
    }
    if (self->seeds != NULL) {
        if ((--self->seeds->__rc) <= 0) {
            btrc_Vector_StateSeed_p1_free(self->seeds);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void StatePolicyParser_parseSeeds(StatePolicy* state, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    JsonValue* seeds = JsonValue_get(jsonValue, "seeds");
    if (!JsonValue_isArray(seeds)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".seeds")), "expected array");
        return;
    }
    for (int seedIndex = 0; (seedIndex < JsonValue_size(seeds)); (seedIndex++)) {
        JsonValue* entry = JsonValue_at(seeds, seedIndex);
        char* __fstr_175_arg0 = context;
        int __fstr_175_arg1 = seedIndex;
        int __fstr_175_len = snprintf(NULL, 0, "%s.seeds[%d]", __fstr_175_arg0, __fstr_175_arg1);
        char* __fstr_175_buf = __btrc_str_track(((char*)malloc((__fstr_175_len + 1))));
        snprintf(__fstr_175_buf, (__fstr_175_len + 1), "%s.seeds[%d]", __fstr_175_arg0, __fstr_175_arg1);
        char* entryContext = __fstr_175_buf;
        StateSeed* seed = StateSeed_new();
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, file, entryContext, "expected object {purpose, target, sources}");
        } else {
            btrc_Vector_string* __list_176 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_176, "purpose");
            btrc_Vector_string_push(__list_176, "target");
            btrc_Vector_string_push(__list_176, "sources");
            btrc_Vector_string* seedKeys = __list_176;
            JsonContract_checkKeys(entry, seedKeys, file, entryContext, log);
            (seed->purpose = JsonContract_requiredString(entry, "purpose", file, entryContext, log));
            (seed->target = JsonContract_requiredString(entry, "target", file, entryContext, log));
            TemplateTokens_check(seed->target, file, __btrc_str_track(__btrc_strcat(entryContext, ".target")), log, false);
            if (seed->sources != NULL) {
                if ((--seed->sources->__rc) <= 0) {
                    btrc_Vector_string_free(seed->sources);
                }
            }
            (seed->sources = JsonContract_stringArray(entry, "sources", true, file, entryContext, log));
            (seed->sources->__rc++);
            btrc_Vector_string* __iter_177 = seed->sources;
            int __n_179 = btrc_Vector_string_iterLen(__iter_177);
            for (int __i_178 = 0; (__i_178 < __n_179); (__i_178++)) {
                char* source = btrc_Vector_string_iterGet(__iter_177, __i_178);
                TemplateTokens_check(source, file, __btrc_str_track(__btrc_strcat(entryContext, ".sources")), log, false);
            }
            if (seedKeys != NULL) {
                if ((--seedKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(seedKeys);
                }
            }
        }
        btrc_Vector_StateSeed_p1_push(state->seeds, seed);
        if (seed != NULL) {
            if ((--seed->__rc) <= 0) {
                StateSeed_destroy(seed);
            }
        }
    }
}

StatePolicy* StatePolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    StatePolicy* state = StatePolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return state;
    }
    (state->present = true);
    btrc_Vector_string* __list_180 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_180, "config_home");
    btrc_Vector_string_push(__list_180, "data_home");
    btrc_Vector_string_push(__list_180, "extra_args");
    btrc_Vector_string_push(__list_180, "seeds");
    btrc_Vector_string* allowedKeys = __list_180;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    btrc_Vector_string* __list_181 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_181, "xdg");
    btrc_Vector_string_push(__list_181, "flatpak_home");
    btrc_Vector_string_push(__list_181, "preserve");
    btrc_Vector_string* homeModes = __list_181;
    (state->configHome = JsonContract_optionalString(jsonValue, "config_home", "", file, context, log));
    JsonContract_checkEnum(state->configHome, homeModes, file, __btrc_str_track(__btrc_strcat(context, ".config_home")), log);
    (state->dataHome = JsonContract_optionalString(jsonValue, "data_home", "", file, context, log));
    JsonContract_checkEnum(state->dataHome, homeModes, file, __btrc_str_track(__btrc_strcat(context, ".data_home")), log);
    if (state->extraArguments != NULL) {
        if ((--state->extraArguments->__rc) <= 0) {
            btrc_Vector_string_free(state->extraArguments);
        }
    }
    (state->extraArguments = JsonContract_stringArray(jsonValue, "extra_args", false, file, context, log));
    (state->extraArguments->__rc++);
    if (JsonValue_has(jsonValue, "seeds")) {
        StatePolicyParser_parseSeeds(state, jsonValue, file, context, log);
    }
    if (homeModes != NULL) {
        if ((--homeModes->__rc) <= 0) {
            btrc_Vector_string_destroy(homeModes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return state;
    if (homeModes != NULL) {
        if ((--homeModes->__rc) <= 0) {
            btrc_Vector_string_destroy(homeModes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (state != NULL) {
        if ((--state->__rc) <= 0) {
            StatePolicy_destroy(state);
        }
    }
}

void FirmwarePolicy_init(FirmwarePolicy* self) {
    self->__rc = 1;
    (self->present = false);
    (self->directory = "");
    if (self->fallbackDirectories != NULL) {
        if ((--self->fallbackDirectories->__rc) <= 0) {
            btrc_Vector_string_free(self->fallbackDirectories);
        }
    }
    btrc_Vector_string* __list_182 = btrc_Vector_string_new();
    (self->fallbackDirectories = __list_182);
    (self->fallbackDirectories->__rc++);
}

FirmwarePolicy* FirmwarePolicy_new(void) {
    FirmwarePolicy* self = ((FirmwarePolicy*)malloc(sizeof(FirmwarePolicy)));
    memset(self, 0, sizeof(FirmwarePolicy));
    FirmwarePolicy_init(self);
    return self;
}

void FirmwarePolicy_destroy(FirmwarePolicy* self) {
    if (self->fallbackDirectories != NULL) {
        if ((--self->fallbackDirectories->__rc) <= 0) {
            btrc_Vector_string_free(self->fallbackDirectories);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

FirmwarePolicy* FirmwarePolicyParser_parse(JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    FirmwarePolicy* firmware = FirmwarePolicy_new();
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return firmware;
    }
    (firmware->present = true);
    btrc_Vector_string* __list_183 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_183, "dir");
    btrc_Vector_string_push(__list_183, "fallback_dirs");
    btrc_Vector_string* allowedKeys = __list_183;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (firmware->directory = JsonContract_requiredString(jsonValue, "dir", file, context, log));
    TemplateTokens_check(firmware->directory, file, __btrc_str_track(__btrc_strcat(context, ".dir")), log, false);
    if (firmware->fallbackDirectories != NULL) {
        if ((--firmware->fallbackDirectories->__rc) <= 0) {
            btrc_Vector_string_free(firmware->fallbackDirectories);
        }
    }
    (firmware->fallbackDirectories = JsonContract_stringArray(jsonValue, "fallback_dirs", false, file, context, log));
    (firmware->fallbackDirectories->__rc++);
    btrc_Vector_string* __iter_184 = firmware->fallbackDirectories;
    int __n_186 = btrc_Vector_string_iterLen(__iter_184);
    for (int __i_185 = 0; (__i_185 < __n_186); (__i_185++)) {
        char* fallbackDirectory = btrc_Vector_string_iterGet(__iter_184, __i_185);
        TemplateTokens_check(fallbackDirectory, file, __btrc_str_track(__btrc_strcat(context, ".fallback_dirs")), log, false);
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return firmware;
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (firmware != NULL) {
        if ((--firmware->__rc) <= 0) {
            FirmwarePolicy_destroy(firmware);
        }
    }
}

void SessionOverride_init(SessionOverride* self) {
    self->__rc = 1;
    (self->id = "");
    (self->displayServer = "");
    (self->displayFallback = "");
    (self->overlay = "");
}

SessionOverride* SessionOverride_new(void) {
    SessionOverride* self = ((SessionOverride*)malloc(sizeof(SessionOverride)));
    memset(self, 0, sizeof(SessionOverride));
    SessionOverride_init(self);
    return self;
}

void SessionOverride_destroy(SessionOverride* self) {
    free(self);
}

void EmulatorPlatform_init(EmulatorPlatform* self) {
    self->__rc = 1;
    (self->operatingSystem = "");
    (self->backend = "");
    (self->flatpakId = "");
    (self->executable = "");
    (self->displayServer = "");
    (self->displayFallback = "");
    if (self->sessions != NULL) {
        if ((--self->sessions->__rc) <= 0) {
            btrc_Vector_SessionOverride_p1_free(self->sessions);
        }
    }
    btrc_Vector_SessionOverride_p1* __list_187 = btrc_Vector_SessionOverride_p1_new();
    (self->sessions = __list_187);
    (self->sessions->__rc++);
    if (self->arguments != NULL) {
        if ((--self->arguments->__rc) <= 0) {
            btrc_Vector_string_free(self->arguments);
        }
    }
    btrc_Vector_string* __list_188 = btrc_Vector_string_new();
    (self->arguments = __list_188);
    (self->arguments->__rc++);
    (self->glWrapper = "none");
    if (self->environmentSetKeys != NULL) {
        if ((--self->environmentSetKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetKeys);
        }
    }
    btrc_Vector_string* __list_189 = btrc_Vector_string_new();
    (self->environmentSetKeys = __list_189);
    (self->environmentSetKeys->__rc++);
    if (self->environmentSetValues != NULL) {
        if ((--self->environmentSetValues->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetValues);
        }
    }
    btrc_Vector_string* __list_190 = btrc_Vector_string_new();
    (self->environmentSetValues = __list_190);
    (self->environmentSetValues->__rc++);
    if (self->environmentUnset != NULL) {
        if ((--self->environmentUnset->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentUnset);
        }
    }
    btrc_Vector_string* __list_191 = btrc_Vector_string_new();
    (self->environmentUnset = __list_191);
    (self->environmentUnset->__rc++);
    if (self->sandbox != NULL) {
        if ((--self->sandbox->__rc) <= 0) {
            SandboxPolicy_destroy(self->sandbox);
        }
    }
    (self->sandbox = SandboxPolicy_new());
    (self->sandbox->__rc++);
    if (self->graphics != NULL) {
        if ((--self->graphics->__rc) <= 0) {
            GraphicsPolicy_destroy(self->graphics);
        }
    }
    (self->graphics = GraphicsPolicy_new());
    (self->graphics->__rc++);
    (self->overlayAspect = "");
    if (self->aspectProbe != NULL) {
        if ((--self->aspectProbe->__rc) <= 0) {
            AspectProbe_destroy(self->aspectProbe);
        }
    }
    (self->aspectProbe = AspectProbe_new());
    (self->aspectProbe->__rc++);
    if (self->state != NULL) {
        if ((--self->state->__rc) <= 0) {
            StatePolicy_destroy(self->state);
        }
    }
    (self->state = StatePolicy_new());
    (self->state->__rc++);
    if (self->firmware != NULL) {
        if ((--self->firmware->__rc) <= 0) {
            FirmwarePolicy_destroy(self->firmware);
        }
    }
    (self->firmware = FirmwarePolicy_new());
    (self->firmware->__rc++);
    if (self->extensionJson != NULL) {
        if ((--self->extensionJson->__rc) <= 0) {
            JsonValue_destroy(self->extensionJson);
        }
    }
    (self->extensionJson = JsonValue_makeNull());
    (self->extensionJson->__rc++);
}

EmulatorPlatform* EmulatorPlatform_new(void) {
    EmulatorPlatform* self = ((EmulatorPlatform*)malloc(sizeof(EmulatorPlatform)));
    memset(self, 0, sizeof(EmulatorPlatform));
    EmulatorPlatform_init(self);
    return self;
}

void EmulatorPlatform_destroy(EmulatorPlatform* self) {
    if (self->sessions != NULL) {
        if ((--self->sessions->__rc) <= 0) {
            btrc_Vector_SessionOverride_p1_free(self->sessions);
        }
    }
    if (self->arguments != NULL) {
        if ((--self->arguments->__rc) <= 0) {
            btrc_Vector_string_free(self->arguments);
        }
    }
    if (self->environmentSetKeys != NULL) {
        if ((--self->environmentSetKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetKeys);
        }
    }
    if (self->environmentSetValues != NULL) {
        if ((--self->environmentSetValues->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetValues);
        }
    }
    if (self->environmentUnset != NULL) {
        if ((--self->environmentUnset->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentUnset);
        }
    }
    if (self->sandbox != NULL) {
        if ((--self->sandbox->__rc) <= 0) {
            SandboxPolicy_destroy(self->sandbox);
        }
    }
    if (self->graphics != NULL) {
        if ((--self->graphics->__rc) <= 0) {
            GraphicsPolicy_destroy(self->graphics);
        }
    }
    if (self->aspectProbe != NULL) {
        if ((--self->aspectProbe->__rc) <= 0) {
            AspectProbe_destroy(self->aspectProbe);
        }
    }
    if (self->state != NULL) {
        if ((--self->state->__rc) <= 0) {
            StatePolicy_destroy(self->state);
        }
    }
    if (self->firmware != NULL) {
        if ((--self->firmware->__rc) <= 0) {
            FirmwarePolicy_destroy(self->firmware);
        }
    }
    if (self->extensionJson != NULL) {
        if ((--self->extensionJson->__rc) <= 0) {
            JsonValue_destroy(self->extensionJson);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool EmulatorPlatform_hasExtension(EmulatorPlatform* self) {
    return (!JsonValue_isNull(self->extensionJson));
}

SessionOverride* EmulatorSessions_effective(EmulatorPlatform* platform, char* session) {
    SessionOverride* effectiveSession = SessionOverride_new();
    (effectiveSession->id = session);
    (effectiveSession->displayServer = platform->displayServer);
    (effectiveSession->displayFallback = platform->displayFallback);
    (effectiveSession->overlay = "");
    btrc_Vector_SessionOverride_p1* __iter_192 = platform->sessions;
    int __n_194 = btrc_Vector_SessionOverride_p1_iterLen(__iter_192);
    for (int __i_193 = 0; (__i_193 < __n_194); (__i_193++)) {
        SessionOverride* sessionOverride = btrc_Vector_SessionOverride_p1_iterGet(__iter_192, __i_193);
        if (strcmp(sessionOverride->id, session) == 0) {
            if (!__btrc_isEmpty(sessionOverride->displayServer)) {
                (effectiveSession->displayServer = sessionOverride->displayServer);
            }
            if (!__btrc_isEmpty(sessionOverride->displayFallback)) {
                (effectiveSession->displayFallback = sessionOverride->displayFallback);
            }
            if (!__btrc_isEmpty(sessionOverride->overlay)) {
                (effectiveSession->overlay = sessionOverride->overlay);
            }
        }
    }
    return effectiveSession;
    if (effectiveSession != NULL) {
        if ((--effectiveSession->__rc) <= 0) {
            SessionOverride_destroy(effectiveSession);
        }
    }
}

void EmulatorInput_init(EmulatorInput* self) {
    self->__rc = 1;
    (self->present = false);
    if (self->actions != NULL) {
        if ((--self->actions->__rc) <= 0) {
            btrc_Vector_string_free(self->actions);
        }
    }
    btrc_Vector_string* __list_195 = btrc_Vector_string_new();
    (self->actions = __list_195);
    (self->actions->__rc++);
}

EmulatorInput* EmulatorInput_new(void) {
    EmulatorInput* self = ((EmulatorInput*)malloc(sizeof(EmulatorInput)));
    memset(self, 0, sizeof(EmulatorInput));
    EmulatorInput_init(self);
    return self;
}

void EmulatorInput_destroy(EmulatorInput* self) {
    if (self->actions != NULL) {
        if ((--self->actions->__rc) <= 0) {
            btrc_Vector_string_free(self->actions);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void EmulatorDefinition_init(EmulatorDefinition* self) {
    self->__rc = 1;
    (self->id = "");
    (self->file = "");
    (self->name = "");
    (self->kind = "");
    (self->defaultSystem = "");
    if (self->input != NULL) {
        if ((--self->input->__rc) <= 0) {
            EmulatorInput_destroy(self->input);
        }
    }
    (self->input = EmulatorInput_new());
    (self->input->__rc++);
    if (self->platforms != NULL) {
        if ((--self->platforms->__rc) <= 0) {
            btrc_Vector_EmulatorPlatform_p1_free(self->platforms);
        }
    }
    btrc_Vector_EmulatorPlatform_p1* __list_196 = btrc_Vector_EmulatorPlatform_p1_new();
    (self->platforms = __list_196);
    (self->platforms->__rc++);
}

EmulatorDefinition* EmulatorDefinition_new(void) {
    EmulatorDefinition* self = ((EmulatorDefinition*)malloc(sizeof(EmulatorDefinition)));
    memset(self, 0, sizeof(EmulatorDefinition));
    EmulatorDefinition_init(self);
    return self;
}

void EmulatorDefinition_destroy(EmulatorDefinition* self) {
    if (self->input != NULL) {
        if ((--self->input->__rc) <= 0) {
            EmulatorInput_destroy(self->input);
        }
    }
    if (self->platforms != NULL) {
        if ((--self->platforms->__rc) <= 0) {
            btrc_Vector_EmulatorPlatform_p1_free(self->platforms);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool EmulatorDefinition_hasOperatingSystem(EmulatorDefinition* self, char* name) {
    btrc_Vector_EmulatorPlatform_p1* __iter_197 = self->platforms;
    int __n_199 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_197);
    for (int __i_198 = 0; (__i_198 < __n_199); (__i_198++)) {
        EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_197, __i_198);
        if (strcmp(platform->operatingSystem, name) == 0) {
            return true;
        }
    }
    return false;
}

EmulatorPlatform* EmulatorDefinition_platformFor(EmulatorDefinition* self, char* name) {
    btrc_Vector_EmulatorPlatform_p1* __iter_200 = self->platforms;
    int __n_202 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_200);
    for (int __i_201 = 0; (__i_201 < __n_202); (__i_201++)) {
        EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_200, __i_201);
        if (strcmp(platform->operatingSystem, name) == 0) {
            return platform;
        }
    }
    return EmulatorPlatform_new();
}

void SemuContracts_init(SemuContracts* self) {
    self->__rc = 1;
    if (self->systems != NULL) {
        if ((--self->systems->__rc) <= 0) {
            btrc_Vector_SystemDefinition_p1_free(self->systems);
        }
    }
    btrc_Vector_SystemDefinition_p1* __list_203 = btrc_Vector_SystemDefinition_p1_new();
    (self->systems = __list_203);
    (self->systems->__rc++);
    if (self->emulators != NULL) {
        if ((--self->emulators->__rc) <= 0) {
            btrc_Vector_EmulatorDefinition_p1_free(self->emulators);
        }
    }
    btrc_Vector_EmulatorDefinition_p1* __list_204 = btrc_Vector_EmulatorDefinition_p1_new();
    (self->emulators = __list_204);
    (self->emulators->__rc++);
}

SemuContracts* SemuContracts_new(void) {
    SemuContracts* self = ((SemuContracts*)malloc(sizeof(SemuContracts)));
    memset(self, 0, sizeof(SemuContracts));
    SemuContracts_init(self);
    return self;
}

void SemuContracts_destroy(SemuContracts* self) {
    if (self->systems != NULL) {
        if ((--self->systems->__rc) <= 0) {
            btrc_Vector_SystemDefinition_p1_free(self->systems);
        }
    }
    if (self->emulators != NULL) {
        if ((--self->emulators->__rc) <= 0) {
            btrc_Vector_EmulatorDefinition_p1_free(self->emulators);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool SemuContracts_hasSystem(SemuContracts* self, char* id) {
    btrc_Vector_SystemDefinition_p1* __iter_205 = self->systems;
    int __n_207 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_205);
    for (int __i_206 = 0; (__i_206 < __n_207); (__i_206++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_205, __i_206);
        if (strcmp(systemDefinition->id, id) == 0) {
            return true;
        }
    }
    return false;
}

bool SemuContracts_hasEmulator(SemuContracts* self, char* id) {
    btrc_Vector_EmulatorDefinition_p1* __iter_208 = self->emulators;
    int __n_210 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_208);
    for (int __i_209 = 0; (__i_209 < __n_210); (__i_209++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_208, __i_209);
        if (strcmp(emulatorDefinition->id, id) == 0) {
            return true;
        }
    }
    return false;
}

void ContractValidation_checkUniqueIdentity(SemuContracts* contracts, ParseLog* log) {
    btrc_Vector_string* seenIds = btrc_Vector_string_new();
    btrc_Vector_SystemDefinition_p1* __iter_211 = contracts->systems;
    int __n_213 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_211);
    for (int __i_212 = 0; (__i_212 < __n_213); (__i_212++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_211, __i_212);
        if (btrc_Vector_string_contains(seenIds, systemDefinition->id)) {
            ParseLog_add(log, systemDefinition->file, "(id)", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate system id '", systemDefinition->id)), "'")));
        }
        btrc_Vector_string_push(seenIds, systemDefinition->id);
    }
    btrc_Vector_SystemDefinition_p1* __iter_214 = contracts->systems;
    int __n_216 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_214);
    for (int __i_215 = 0; (__i_215 < __n_216); (__i_215++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_214, __i_215);
        btrc_Vector_string* __iter_217 = systemDefinition->aliases;
        int __n_219 = btrc_Vector_string_iterLen(__iter_217);
        for (int __i_218 = 0; (__i_218 < __n_219); (__i_218++)) {
            char* alias = btrc_Vector_string_iterGet(__iter_217, __i_218);
            if (btrc_Vector_string_contains(seenIds, alias)) {
                ParseLog_add(log, systemDefinition->file, "aliases", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", alias)), "' collides with another system id or alias")));
            }
            btrc_Vector_string_push(seenIds, alias);
        }
    }
    if (seenIds != NULL) {
        if ((--seenIds->__rc) <= 0) {
            btrc_Vector_string_destroy(seenIds);
        }
    }
}

EmulatorDefinition* ContractValidation_findEmulator(SemuContracts* contracts, char* emulatorId) {
    btrc_Vector_EmulatorDefinition_p1* __iter_220 = contracts->emulators;
    int __n_222 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_220);
    for (int __i_221 = 0; (__i_221 < __n_222); (__i_221++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_220, __i_221);
        if (strcmp(emulatorDefinition->id, emulatorId) == 0) {
            return emulatorDefinition;
        }
    }
    return EmulatorDefinition_new();
}

void ContractValidation_checkBindings(SemuContracts* contracts, ParseLog* log) {
    btrc_Vector_SystemDefinition_p1* __iter_223 = contracts->systems;
    int __n_225 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_223);
    for (int __i_224 = 0; (__i_224 < __n_225); (__i_224++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_223, __i_224);
        btrc_Vector_EmulatorBinding_p1* __iter_226 = systemDefinition->emulators;
        int __n_228 = btrc_Vector_EmulatorBinding_p1_iterLen(__iter_226);
        for (int __i_227 = 0; (__i_227 < __n_228); (__i_227++)) {
            EmulatorBinding* binding = btrc_Vector_EmulatorBinding_p1_iterGet(__iter_226, __i_227);
            if (__btrc_isEmpty(binding->emulator)) {
            } else if (!SemuContracts_hasEmulator(contracts, binding->emulator)) {
                ParseLog_add(log, systemDefinition->file, "emulators", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown emulator '", binding->emulator)), "' (no src/semu/emulators/")), binding->emulator)), "/emulator.json)")));
            } else {
                EmulatorDefinition* emulatorDefinition = ContractValidation_findEmulator(contracts, binding->emulator);
                btrc_Vector_string* claimedPlatforms = binding->platforms;
                if (btrc_Vector_string_size(claimedPlatforms) == 0) {
                    btrc_Vector_string* __list_229 = btrc_Vector_string_new();
                    btrc_Vector_string_push(__list_229, "linux");
                    btrc_Vector_string_push(__list_229, "macos");
                    (claimedPlatforms = __list_229);
                }
                int __n_231 = btrc_Vector_string_iterLen(claimedPlatforms);
                for (int __i_230 = 0; (__i_230 < __n_231); (__i_230++)) {
                    char* operatingSystemName = btrc_Vector_string_iterGet(claimedPlatforms, __i_230);
                    if (!EmulatorDefinition_hasOperatingSystem(emulatorDefinition, operatingSystemName)) {
                        ParseLog_add(log, systemDefinition->file, "emulators", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("binding '", binding->emulator)), "' claims platform '")), operatingSystemName)), "' but the emulator declares no such block")));
                    }
                }
                if ((!__btrc_isEmpty(binding->core)) && (!(strcmp(emulatorDefinition->kind, "libretro_frontend") == 0))) {
                    ParseLog_add(log, systemDefinition->file, "emulators", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("binding '", binding->emulator)), "' carries a core but the emulator")), " kind is '")), emulatorDefinition->kind)), "' (core requires kind 'libretro_frontend')")));
                }
            }
        }
    }
}

void ContractValidation_checkDefaultSystems(SemuContracts* contracts, ParseLog* log) {
    btrc_Vector_EmulatorDefinition_p1* __iter_232 = contracts->emulators;
    int __n_234 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_232);
    for (int __i_233 = 0; (__i_233 < __n_234); (__i_233++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_232, __i_233);
        if ((!__btrc_isEmpty(emulatorDefinition->defaultSystem)) && (!SemuContracts_hasSystem(contracts, emulatorDefinition->defaultSystem))) {
            ParseLog_add(log, emulatorDefinition->file, "default_system", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown system '", emulatorDefinition->defaultSystem)), "'")));
        }
    }
}

void ContractValidation_checkWidescreenSubstitutions(SemuContracts* contracts, ParseLog* log) {
    btrc_Vector_SystemDefinition_p1* __iter_235 = contracts->systems;
    int __n_237 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_235);
    for (int __i_236 = 0; (__i_236 < __n_237); (__i_236++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_235, __i_236);
        if (!systemDefinition->display->widescreen->set) {
            if (!__btrc_isEmpty(systemDefinition->bezels->widescreenVariant)) {
                ParseLog_add(log, systemDefinition->bezels->file, "widescreen_variant", "system declares no display.widescreen to trigger the substitution");
            }
            if (systemDefinition->shaders->hasWidescreen) {
                ParseLog_add(log, systemDefinition->shaders->file, "widescreen", "system declares no display.widescreen to trigger the substitution");
            }
        }
    }
}

bool ContractValidation_copyVariantInto(BezelCollection* bezels, char* variantId, BezelVariant* destination) {
    btrc_Vector_BezelVariant_p1* __iter_238 = bezels->variants;
    int __n_240 = btrc_Vector_BezelVariant_p1_iterLen(__iter_238);
    for (int __i_239 = 0; (__i_239 < __n_240); (__i_239++)) {
        BezelVariant* variant = btrc_Vector_BezelVariant_p1_iterGet(__iter_238, __i_239);
        if (strcmp(variant->id, variantId) == 0) {
            (destination->id = variant->id);
            (destination->art = variant->art);
            (destination->glass = variant->glass);
            if (destination->hole != NULL) {
                if ((--destination->hole->__rc) <= 0) {
                    NormalizedRect_destroy(destination->hole);
                }
            }
            (destination->hole = variant->hole);
            (destination->hole->__rc++);
            if (destination->screenHoles != NULL) {
                if ((--destination->screenHoles->__rc) <= 0) {
                    btrc_Vector_ScreenHole_p1_free(destination->screenHoles);
                }
            }
            (destination->screenHoles = variant->screenHoles);
            (destination->screenHoles->__rc++);
            (destination->hasGeometryFrom = variant->hasGeometryFrom);
            return true;
        }
    }
    return false;
}

SystemDefinition* ContractValidation_findSystem(SemuContracts* contracts, char* systemId, SystemDefinition* fallback) {
    btrc_Vector_SystemDefinition_p1* __iter_241 = contracts->systems;
    int __n_243 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_241);
    for (int __i_242 = 0; (__i_242 < __n_243); (__i_242++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_241, __i_242);
        if (strcmp(systemDefinition->id, systemId) == 0) {
            return systemDefinition;
        }
    }
    return fallback;
}

void ContractValidation_resolveVariantGeometry(SemuContracts* contracts, SystemDefinition* systemDefinition, BezelVariant* variant, ParseLog* log) {
    BezelCollection* bezels = systemDefinition->bezels;
    SystemDefinition* geometrySourceSystem = systemDefinition;
    if (!__btrc_isEmpty(variant->geometrySystem)) {
        if (!SemuContracts_hasSystem(contracts, variant->geometrySystem)) {
            ParseLog_add(log, bezels->file, "geometry_from.system", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown system '", variant->geometrySystem)), "'")));
        } else {
            (geometrySourceSystem = ContractValidation_findSystem(contracts, variant->geometrySystem, systemDefinition));
        }
    }
    BezelVariant* target = BezelVariant_new();
    if (!ContractValidation_copyVariantInto(geometrySourceSystem->bezels, variant->geometryVariant, target)) {
        ParseLog_add(log, bezels->file, "geometry_from.variant", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown variant '", variant->geometryVariant)), "' in system '")), geometrySourceSystem->id)), "'")));
        if (target != NULL) {
            if ((--target->__rc) <= 0) {
                BezelVariant_destroy(target);
            }
        }
        return;
    }
    if (target->hasGeometryFrom) {
        ParseLog_add(log, bezels->file, "geometry_from", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("target variant '", target->id)), "' uses geometry_from itself (single level only)")));
    }
    if (__btrc_isEmpty(variant->art)) {
        if (__btrc_isEmpty(target->art)) {
            ParseLog_add(log, bezels->file, "geometry_from", "target variant supplies no art");
        }
        (variant->art = target->art);
    }
    if (__btrc_isEmpty(variant->glass)) {
        (variant->glass = target->glass);
    }
    if (!variant->hole->set) {
        if (variant->hole != NULL) {
            if ((--variant->hole->__rc) <= 0) {
                NormalizedRect_destroy(variant->hole);
            }
        }
        (variant->hole = (target->hole->set ? target->hole : geometrySourceSystem->bezels->hole));
        (variant->hole->__rc++);
    }
    if (btrc_Vector_ScreenHole_p1_size(variant->screenHoles) == 0) {
        if (variant->screenHoles != NULL) {
            if ((--variant->screenHoles->__rc) <= 0) {
                btrc_Vector_ScreenHole_p1_free(variant->screenHoles);
            }
        }
        (variant->screenHoles = ((btrc_Vector_ScreenHole_p1_size(target->screenHoles) > 0) ? target->screenHoles : geometrySourceSystem->bezels->screenHoles));
        (variant->screenHoles->__rc++);
    }
    if (target != NULL) {
        if ((--target->__rc) <= 0) {
            BezelVariant_destroy(target);
        }
    }
}

void ContractValidation_resolveGeometry(SemuContracts* contracts, ParseLog* log) {
    btrc_Vector_SystemDefinition_p1* __iter_244 = contracts->systems;
    int __n_246 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_244);
    for (int __i_245 = 0; (__i_245 < __n_246); (__i_245++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_244, __i_245);
        BezelCollection* bezels = systemDefinition->bezels;
        if (bezels->enabled) {
            btrc_Vector_BezelVariant_p1* __iter_247 = bezels->variants;
            int __n_249 = btrc_Vector_BezelVariant_p1_iterLen(__iter_247);
            for (int __i_248 = 0; (__i_248 < __n_249); (__i_248++)) {
                BezelVariant* variant = btrc_Vector_BezelVariant_p1_iterGet(__iter_247, __i_248);
                if (variant->hasGeometryFrom) {
                    ContractValidation_resolveVariantGeometry(contracts, systemDefinition, variant, log);
                }
            }
            if (!bezels->hole->set) {
                btrc_Vector_BezelVariant_p1* __iter_250 = bezels->variants;
                int __n_252 = btrc_Vector_BezelVariant_p1_iterLen(__iter_250);
                for (int __i_251 = 0; (__i_251 < __n_252); (__i_251++)) {
                    BezelVariant* variant = btrc_Vector_BezelVariant_p1_iterGet(__iter_250, __i_251);
                    if ((strcmp(variant->id, bezels->defaultVariant) == 0) && variant->hole->set) {
                        if (bezels->hole != NULL) {
                            if ((--bezels->hole->__rc) <= 0) {
                                NormalizedRect_destroy(bezels->hole);
                            }
                        }
                        (bezels->hole = variant->hole);
                        (bezels->hole->__rc++);
                    }
                }
                if (!bezels->hole->set) {
                    ParseLog_add(log, bezels->file, "hole", "missing and the default variant supplies no geometry");
                }
            }
            btrc_Vector_ScreenHole_p1* __iter_253 = bezels->screenHoles;
            int __n_255 = btrc_Vector_ScreenHole_p1_iterLen(__iter_253);
            for (int __i_254 = 0; (__i_254 < __n_255); (__i_254++)) {
                ScreenHole* screenHole = btrc_Vector_ScreenHole_p1_iterGet(__iter_253, __i_254);
                JsonContract_checkContainedRect(screenHole->hole, bezels->hole, bezels->file, __btrc_str_track(__btrc_strcat("screen_holes.", screenHole->screenId)), log);
            }
        }
    }
}

void ContractValidation_crossValidate(SemuContracts* contracts, ParseLog* log) {
    ContractValidation_checkUniqueIdentity(contracts, log);
    ContractValidation_checkBindings(contracts, log);
    ContractValidation_checkDefaultSystems(contracts, log);
    ContractValidation_checkWidescreenSubstitutions(contracts, log);
    ContractValidation_resolveGeometry(contracts, log);
}

int ChordSyntax_lastPlusIndex(char* chord) {
    int result = (-1);
    for (int charIndex = 0; (charIndex < ((int)strlen(chord))); (charIndex++)) {
        if (__btrc_charAt(chord, charIndex) == '+') {
            (result = charIndex);
        }
    }
    return result;
}

char* ChordSyntax_keyPart(char* chord) {
    if (__btrc_endsWith(chord, "++")) {
        return "+";
    }
    if (__btrc_endsWith(chord, "+-")) {
        return "-";
    }
    int plusIndex = ChordSyntax_lastPlusIndex(chord);
    if (plusIndex < 0) {
        return chord;
    }
    return __btrc_str_track(__btrc_substring(chord, (plusIndex + 1), ((((int)strlen(chord)) - plusIndex) - 1)));
}

char* ChordSyntax_modifierPart(char* chord) {
    if (__btrc_endsWith(chord, "++") || __btrc_endsWith(chord, "+-")) {
        return ((((int)strlen(chord)) <= 2) ? "" : __btrc_str_track(__btrc_substring(chord, 0, (((int)strlen(chord)) - 2))));
    }
    int plusIndex = ChordSyntax_lastPlusIndex(chord);
    if (plusIndex < 0) {
        return "";
    }
    return __btrc_str_track(__btrc_substring(chord, 0, plusIndex));
}

void KeymapAction_init(KeymapAction* self) {
    self->__rc = 1;
    (self->id = "");
    (self->chord = "");
}

KeymapAction* KeymapAction_new(void) {
    KeymapAction* self = ((KeymapAction*)malloc(sizeof(KeymapAction)));
    memset(self, 0, sizeof(KeymapAction));
    KeymapAction_init(self);
    return self;
}

void KeymapAction_destroy(KeymapAction* self) {
    free(self);
}

void KeymapBinding_init(KeymapBinding* self) {
    self->__rc = 1;
    (self->button = "");
    (self->actionId = "");
}

KeymapBinding* KeymapBinding_new(void) {
    KeymapBinding* self = ((KeymapBinding*)malloc(sizeof(KeymapBinding)));
    memset(self, 0, sizeof(KeymapBinding));
    KeymapBinding_init(self);
    return self;
}

void KeymapBinding_destroy(KeymapBinding* self) {
    free(self);
}

void Keymap_init(Keymap* self) {
    self->__rc = 1;
    if (self->actions != NULL) {
        if ((--self->actions->__rc) <= 0) {
            btrc_Vector_KeymapAction_p1_free(self->actions);
        }
    }
    btrc_Vector_KeymapAction_p1* __list_256 = btrc_Vector_KeymapAction_p1_new();
    (self->actions = __list_256);
    (self->actions->__rc++);
    if (self->bindings != NULL) {
        if ((--self->bindings->__rc) <= 0) {
            btrc_Vector_KeymapBinding_p1_free(self->bindings);
        }
    }
    btrc_Vector_KeymapBinding_p1* __list_257 = btrc_Vector_KeymapBinding_p1_new();
    (self->bindings = __list_257);
    (self->bindings->__rc++);
}

Keymap* Keymap_new(void) {
    Keymap* self = ((Keymap*)malloc(sizeof(Keymap)));
    memset(self, 0, sizeof(Keymap));
    Keymap_init(self);
    return self;
}

void Keymap_destroy(Keymap* self) {
    if (self->actions != NULL) {
        if ((--self->actions->__rc) <= 0) {
            btrc_Vector_KeymapAction_p1_free(self->actions);
        }
    }
    if (self->bindings != NULL) {
        if ((--self->bindings->__rc) <= 0) {
            btrc_Vector_KeymapBinding_p1_free(self->bindings);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool Keymap_hasAction(Keymap* self, char* id) {
    btrc_Vector_KeymapAction_p1* __iter_258 = self->actions;
    int __n_260 = btrc_Vector_KeymapAction_p1_iterLen(__iter_258);
    for (int __i_259 = 0; (__i_259 < __n_260); (__i_259++)) {
        KeymapAction* action = btrc_Vector_KeymapAction_p1_iterGet(__iter_258, __i_259);
        if (strcmp(action->id, id) == 0) {
            return true;
        }
    }
    return false;
}

char* Keymap_chordFor(Keymap* self, char* id) {
    btrc_Vector_KeymapAction_p1* __iter_261 = self->actions;
    int __n_263 = btrc_Vector_KeymapAction_p1_iterLen(__iter_261);
    for (int __i_262 = 0; (__i_262 < __n_263); (__i_262++)) {
        KeymapAction* action = btrc_Vector_KeymapAction_p1_iterGet(__iter_261, __i_262);
        if (strcmp(action->id, id) == 0) {
            return action->chord;
        }
    }
    return "";
}

bool Keymap_hasBindingButton(Keymap* self, char* button) {
    btrc_Vector_KeymapBinding_p1* __iter_264 = self->bindings;
    int __n_266 = btrc_Vector_KeymapBinding_p1_iterLen(__iter_264);
    for (int __i_265 = 0; (__i_265 < __n_266); (__i_265++)) {
        KeymapBinding* binding = btrc_Vector_KeymapBinding_p1_iterGet(__iter_264, __i_265);
        if (strcmp(binding->button, button) == 0) {
            return true;
        }
    }
    return false;
}

bool KeymapParser_modifierAllowed(char* modifier) {
    btrc_Vector_string* __list_267 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_267, "Ctrl");
    btrc_Vector_string_push(__list_267, "Alt");
    btrc_Vector_string_push(__list_267, "Shift");
    btrc_Vector_string_push(__list_267, "Meta");
    btrc_Vector_string* modifiers = __list_267;
    bool __btrc_ret_268 = btrc_Vector_string_contains(modifiers, modifier);
    if (modifiers != NULL) {
        if ((--modifiers->__rc) <= 0) {
            btrc_Vector_string_destroy(modifiers);
        }
    }
    return __btrc_ret_268;
    if (modifiers != NULL) {
        if ((--modifiers->__rc) <= 0) {
            btrc_Vector_string_destroy(modifiers);
        }
    }
}

void KeymapParser_checkChord(char* actionId, char* chord, char* file, ParseLog* log) {
    if (__btrc_isEmpty(chord)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("action ", actionId)), "has no key");
    } else if (KeymapParser_modifierAllowed(chord)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("action ", actionId)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("has no key after modifier '", chord)), "'")));
    } else {
        char* modifiers = ChordSyntax_modifierPart(chord);
        if (!__btrc_isEmpty(modifiers)) {
            btrc_Vector_string* __iter_269 = Strings_split(modifiers, "+");
            int __n_271 = btrc_Vector_string_iterLen(__iter_269);
            for (int __i_270 = 0; (__i_270 < __n_271); (__i_270++)) {
                char* modifier = btrc_Vector_string_iterGet(__iter_269, __i_270);
                if (!KeymapParser_modifierAllowed(modifier)) {
                    ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("action ", actionId)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unsupported modifier '", modifier)), "'")));
                }
            }
        }
    }
}

void KeymapParser_parseActionLine(Keymap* keymap, char* rest, int lineNumber, char* file, ParseLog* log) {
    int equalsIndex = __btrc_indexOf(rest, "=");
    if (equalsIndex < 0) {
        int __fstr_272_arg0 = lineNumber;
        int __fstr_272_len = snprintf(NULL, 0, "line %d", __fstr_272_arg0);
        char* __fstr_272_buf = __btrc_str_track(((char*)malloc((__fstr_272_len + 1))));
        snprintf(__fstr_272_buf, (__fstr_272_len + 1), "line %d", __fstr_272_arg0);
        ParseLog_add(log, file, __fstr_272_buf, "action line missing '='");
        return;
    }
    KeymapAction* action = KeymapAction_new();
    (action->id = __btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(rest, 0, equalsIndex)))));
    (action->chord = __btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(rest, (equalsIndex + 1), ((((int)strlen(rest)) - equalsIndex) - 1))))));
    if (__btrc_isEmpty(action->id)) {
        int __fstr_273_arg0 = lineNumber;
        int __fstr_273_len = snprintf(NULL, 0, "line %d", __fstr_273_arg0);
        char* __fstr_273_buf = __btrc_str_track(((char*)malloc((__fstr_273_len + 1))));
        snprintf(__fstr_273_buf, (__fstr_273_len + 1), "line %d", __fstr_273_arg0);
        ParseLog_add(log, file, __fstr_273_buf, "action id is empty");
        if (action != NULL) {
            if ((--action->__rc) <= 0) {
                KeymapAction_destroy(action);
            }
        }
        return;
    }
    if (Keymap_hasAction(keymap, action->id)) {
        int __fstr_274_arg0 = lineNumber;
        int __fstr_274_len = snprintf(NULL, 0, "line %d", __fstr_274_arg0);
        char* __fstr_274_buf = __btrc_str_track(((char*)malloc((__fstr_274_len + 1))));
        snprintf(__fstr_274_buf, (__fstr_274_len + 1), "line %d", __fstr_274_arg0);
        ParseLog_add(log, file, __fstr_274_buf, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate action '", action->id)), "'")));
        if (action != NULL) {
            if ((--action->__rc) <= 0) {
                KeymapAction_destroy(action);
            }
        }
        return;
    }
    KeymapParser_checkChord(action->id, action->chord, file, log);
    btrc_Vector_KeymapAction_p1_push(keymap->actions, action);
    if (action != NULL) {
        if ((--action->__rc) <= 0) {
            KeymapAction_destroy(action);
        }
    }
}

void KeymapParser_parseBindingLine(Keymap* keymap, char* rest, int lineNumber, char* file, ParseLog* log) {
    int arrowIndex = __btrc_indexOf(rest, "->");
    if (arrowIndex < 0) {
        int __fstr_275_arg0 = lineNumber;
        int __fstr_275_len = snprintf(NULL, 0, "line %d", __fstr_275_arg0);
        char* __fstr_275_buf = __btrc_str_track(((char*)malloc((__fstr_275_len + 1))));
        snprintf(__fstr_275_buf, (__fstr_275_len + 1), "line %d", __fstr_275_arg0);
        ParseLog_add(log, file, __fstr_275_buf, "bind line missing '->'");
        return;
    }
    KeymapBinding* binding = KeymapBinding_new();
    (binding->button = __btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(rest, 0, arrowIndex)))));
    char* target = __btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(rest, (arrowIndex + 2), ((((int)strlen(rest)) - arrowIndex) - 2)))));
    if (__btrc_isEmpty(binding->button)) {
        int __fstr_276_arg0 = lineNumber;
        int __fstr_276_len = snprintf(NULL, 0, "line %d", __fstr_276_arg0);
        char* __fstr_276_buf = __btrc_str_track(((char*)malloc((__fstr_276_len + 1))));
        snprintf(__fstr_276_buf, (__fstr_276_len + 1), "line %d", __fstr_276_arg0);
        ParseLog_add(log, file, __fstr_276_buf, "bind has no button tokens");
        if (binding != NULL) {
            if ((--binding->__rc) <= 0) {
                KeymapBinding_destroy(binding);
            }
        }
        return;
    }
    if (((!__btrc_startsWith(target, "${")) || (!__btrc_endsWith(target, "}"))) || (((int)strlen(target)) <= 3)) {
        int __fstr_277_arg0 = lineNumber;
        int __fstr_277_len = snprintf(NULL, 0, "line %d", __fstr_277_arg0);
        char* __fstr_277_buf = __btrc_str_track(((char*)malloc((__fstr_277_len + 1))));
        snprintf(__fstr_277_buf, (__fstr_277_len + 1), "line %d", __fstr_277_arg0);
        ParseLog_add(log, file, __fstr_277_buf, "bind target must be a dollar-brace action reference");
        if (binding != NULL) {
            if ((--binding->__rc) <= 0) {
                KeymapBinding_destroy(binding);
            }
        }
        return;
    }
    (binding->actionId = __btrc_str_track(__btrc_substring(target, 2, (((int)strlen(target)) - 3))));
    if (Keymap_hasBindingButton(keymap, binding->button)) {
        int __fstr_278_arg0 = lineNumber;
        int __fstr_278_len = snprintf(NULL, 0, "line %d", __fstr_278_arg0);
        char* __fstr_278_buf = __btrc_str_track(((char*)malloc((__fstr_278_len + 1))));
        snprintf(__fstr_278_buf, (__fstr_278_len + 1), "line %d", __fstr_278_arg0);
        ParseLog_add(log, file, __fstr_278_buf, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate controller combo '", binding->button)), "'")));
        if (binding != NULL) {
            if ((--binding->__rc) <= 0) {
                KeymapBinding_destroy(binding);
            }
        }
        return;
    }
    btrc_Vector_KeymapBinding_p1_push(keymap->bindings, binding);
    if (binding != NULL) {
        if ((--binding->__rc) <= 0) {
            KeymapBinding_destroy(binding);
        }
    }
}

Keymap* KeymapParser_parse(char* text, ParseLog* log) {
    char* file = "(keymap)";
    Keymap* keymap = Keymap_new();
    int lineNumber = 0;
    btrc_Vector_string* __iter_279 = Strings_split(text, "\n");
    int __n_281 = btrc_Vector_string_iterLen(__iter_279);
    for (int __i_280 = 0; (__i_280 < __n_281); (__i_280++)) {
        char* rawLine = btrc_Vector_string_iterGet(__iter_279, __i_280);
        (lineNumber++);
        char* line = __btrc_str_track(__btrc_trim(rawLine));
        if (__btrc_isEmpty(line) || __btrc_startsWith(line, "#")) {
        } else if (__btrc_startsWith(line, "action ")) {
            KeymapParser_parseActionLine(keymap, __btrc_str_track(__btrc_substring(line, 7, (((int)strlen(line)) - 7))), lineNumber, file, log);
        } else if (__btrc_startsWith(line, "bind ")) {
            KeymapParser_parseBindingLine(keymap, __btrc_str_track(__btrc_substring(line, 5, (((int)strlen(line)) - 5))), lineNumber, file, log);
        } else {
            int __fstr_282_arg0 = lineNumber;
            int __fstr_282_len = snprintf(NULL, 0, "line %d", __fstr_282_arg0);
            char* __fstr_282_buf = __btrc_str_track(((char*)malloc((__fstr_282_len + 1))));
            snprintf(__fstr_282_buf, (__fstr_282_len + 1), "line %d", __fstr_282_arg0);
            ParseLog_add(log, file, __fstr_282_buf, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unrecognized line '", line)), "'")));
        }
    }
    if (btrc_Vector_KeymapAction_p1_size(keymap->actions) == 0) {
        ParseLog_add(log, file, "(keymap)", "keymap has no actions");
    }
    if (btrc_Vector_KeymapBinding_p1_size(keymap->bindings) == 0) {
        ParseLog_add(log, file, "(keymap)", "keymap has no controller bindings");
    }
    btrc_Vector_KeymapBinding_p1* __iter_283 = keymap->bindings;
    int __n_285 = btrc_Vector_KeymapBinding_p1_iterLen(__iter_283);
    for (int __i_284 = 0; (__i_284 < __n_285); (__i_284++)) {
        KeymapBinding* binding = btrc_Vector_KeymapBinding_p1_iterGet(__iter_283, __i_284);
        if (!Keymap_hasAction(keymap, binding->actionId)) {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("bind ", binding->button)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("references unknown action '", binding->actionId)), "'")));
        }
    }
    return keymap;
    if (keymap != NULL) {
        if ((--keymap->__rc) <= 0) {
            Keymap_destroy(keymap);
        }
    }
}

void SteamInputTarget_init(SteamInputTarget* self) {
    self->__rc = 1;
    (self->file = "");
    (self->keymap = "");
    if (self->modifiers != NULL) {
        if ((--self->modifiers->__rc) <= 0) {
            btrc_Vector_string_free(self->modifiers);
        }
    }
    btrc_Vector_string* __list_286 = btrc_Vector_string_new();
    (self->modifiers = __list_286);
    (self->modifiers->__rc++);
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    btrc_Vector_string* __list_287 = btrc_Vector_string_new();
    (self->actionIds = __list_287);
    (self->actionIds->__rc++);
    if (self->actionAliases != NULL) {
        if ((--self->actionAliases->__rc) <= 0) {
            btrc_Vector_string_free(self->actionAliases);
        }
    }
    btrc_Vector_string* __list_288 = btrc_Vector_string_new();
    (self->actionAliases = __list_288);
    (self->actionAliases->__rc++);
    if (self->actionLabels != NULL) {
        if ((--self->actionLabels->__rc) <= 0) {
            btrc_Vector_string_free(self->actionLabels);
        }
    }
    btrc_Vector_string* __list_289 = btrc_Vector_string_new();
    (self->actionLabels = __list_289);
    (self->actionLabels->__rc++);
    (self->controllerModel = "");
    (self->gyro = false);
    (self->trackpadLeft = "");
    (self->trackpadRight = "");
    (self->hotkeyButton = "");
    if (self->radialSlots != NULL) {
        if ((--self->radialSlots->__rc) <= 0) {
            btrc_Vector_string_free(self->radialSlots);
        }
    }
    btrc_Vector_string* __list_290 = btrc_Vector_string_new();
    (self->radialSlots = __list_290);
    (self->radialSlots->__rc++);
    if (self->quitExtraBindings != NULL) {
        if ((--self->quitExtraBindings->__rc) <= 0) {
            btrc_Vector_string_free(self->quitExtraBindings);
        }
    }
    btrc_Vector_string* __list_291 = btrc_Vector_string_new();
    (self->quitExtraBindings = __list_291);
    (self->quitExtraBindings->__rc++);
    if (self->templateIds != NULL) {
        if ((--self->templateIds->__rc) <= 0) {
            btrc_Vector_string_free(self->templateIds);
        }
    }
    btrc_Vector_string* __list_292 = btrc_Vector_string_new();
    (self->templateIds = __list_292);
    (self->templateIds->__rc++);
    if (self->templateTitles != NULL) {
        if ((--self->templateTitles->__rc) <= 0) {
            btrc_Vector_string_free(self->templateTitles);
        }
    }
    btrc_Vector_string* __list_293 = btrc_Vector_string_new();
    (self->templateTitles = __list_293);
    (self->templateTitles->__rc++);
    if (self->templateRadialNames != NULL) {
        if ((--self->templateRadialNames->__rc) <= 0) {
            btrc_Vector_string_free(self->templateRadialNames);
        }
    }
    btrc_Vector_string* __list_294 = btrc_Vector_string_new();
    (self->templateRadialNames = __list_294);
    (self->templateRadialNames->__rc++);
    if (self->templateOutputs != NULL) {
        if ((--self->templateOutputs->__rc) <= 0) {
            btrc_Vector_string_free(self->templateOutputs);
        }
    }
    btrc_Vector_string* __list_295 = btrc_Vector_string_new();
    (self->templateOutputs = __list_295);
    (self->templateOutputs->__rc++);
    (self->defaultTemplateId = "");
    if (self->keyNameKeys != NULL) {
        if ((--self->keyNameKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->keyNameKeys);
        }
    }
    btrc_Vector_string* __list_296 = btrc_Vector_string_new();
    (self->keyNameKeys = __list_296);
    (self->keyNameKeys->__rc++);
    if (self->keyNameValues != NULL) {
        if ((--self->keyNameValues->__rc) <= 0) {
            btrc_Vector_string_free(self->keyNameValues);
        }
    }
    btrc_Vector_string* __list_297 = btrc_Vector_string_new();
    (self->keyNameValues = __list_297);
    (self->keyNameValues->__rc++);
    if (self->deviceIdentityKeys != NULL) {
        if ((--self->deviceIdentityKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->deviceIdentityKeys);
        }
    }
    btrc_Vector_string* __list_298 = btrc_Vector_string_new();
    (self->deviceIdentityKeys = __list_298);
    (self->deviceIdentityKeys->__rc++);
    if (self->deviceIdentityValues != NULL) {
        if ((--self->deviceIdentityValues->__rc) <= 0) {
            btrc_Vector_string_free(self->deviceIdentityValues);
        }
    }
    btrc_Vector_string* __list_299 = btrc_Vector_string_new();
    (self->deviceIdentityValues = __list_299);
    (self->deviceIdentityValues->__rc++);
    (self->templatesDirectory = "");
}

SteamInputTarget* SteamInputTarget_new(void) {
    SteamInputTarget* self = ((SteamInputTarget*)malloc(sizeof(SteamInputTarget)));
    memset(self, 0, sizeof(SteamInputTarget));
    SteamInputTarget_init(self);
    return self;
}

void SteamInputTarget_destroy(SteamInputTarget* self) {
    if (self->modifiers != NULL) {
        if ((--self->modifiers->__rc) <= 0) {
            btrc_Vector_string_free(self->modifiers);
        }
    }
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    if (self->actionAliases != NULL) {
        if ((--self->actionAliases->__rc) <= 0) {
            btrc_Vector_string_free(self->actionAliases);
        }
    }
    if (self->actionLabels != NULL) {
        if ((--self->actionLabels->__rc) <= 0) {
            btrc_Vector_string_free(self->actionLabels);
        }
    }
    if (self->radialSlots != NULL) {
        if ((--self->radialSlots->__rc) <= 0) {
            btrc_Vector_string_free(self->radialSlots);
        }
    }
    if (self->quitExtraBindings != NULL) {
        if ((--self->quitExtraBindings->__rc) <= 0) {
            btrc_Vector_string_free(self->quitExtraBindings);
        }
    }
    if (self->templateIds != NULL) {
        if ((--self->templateIds->__rc) <= 0) {
            btrc_Vector_string_free(self->templateIds);
        }
    }
    if (self->templateTitles != NULL) {
        if ((--self->templateTitles->__rc) <= 0) {
            btrc_Vector_string_free(self->templateTitles);
        }
    }
    if (self->templateRadialNames != NULL) {
        if ((--self->templateRadialNames->__rc) <= 0) {
            btrc_Vector_string_free(self->templateRadialNames);
        }
    }
    if (self->templateOutputs != NULL) {
        if ((--self->templateOutputs->__rc) <= 0) {
            btrc_Vector_string_free(self->templateOutputs);
        }
    }
    if (self->keyNameKeys != NULL) {
        if ((--self->keyNameKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->keyNameKeys);
        }
    }
    if (self->keyNameValues != NULL) {
        if ((--self->keyNameValues->__rc) <= 0) {
            btrc_Vector_string_free(self->keyNameValues);
        }
    }
    if (self->deviceIdentityKeys != NULL) {
        if ((--self->deviceIdentityKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->deviceIdentityKeys);
        }
    }
    if (self->deviceIdentityValues != NULL) {
        if ((--self->deviceIdentityValues->__rc) <= 0) {
            btrc_Vector_string_free(self->deviceIdentityValues);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool SteamInputTarget_hasAction(SteamInputTarget* self, char* id) {
    return btrc_Vector_string_contains(self->actionIds, id);
}

char* SteamInputTarget_actionLabel(SteamInputTarget* self, char* id) {
    for (int actionIndex = 0; (actionIndex < btrc_Vector_string_size(self->actionIds)); (actionIndex++)) {
        if (strcmp(btrc_Vector_string_get(self->actionIds, actionIndex), id) == 0) {
            return btrc_Vector_string_get(self->actionLabels, actionIndex);
        }
    }
    return id;
}

char* SteamInputTarget_keyName(SteamInputTarget* self, char* token) {
    char* lowered = __btrc_str_track(__btrc_toLower(token));
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(self->keyNameKeys)); (keyIndex++)) {
        if (strcmp(btrc_Vector_string_get(self->keyNameKeys, keyIndex), lowered) == 0) {
            return btrc_Vector_string_get(self->keyNameValues, keyIndex);
        }
    }
    return __btrc_str_track(__btrc_toUpper(token));
}

char* SteamInputTarget_deviceIdentity(SteamInputTarget* self, char* key) {
    for (int identityIndex = 0; (identityIndex < btrc_Vector_string_size(self->deviceIdentityKeys)); (identityIndex++)) {
        if (strcmp(btrc_Vector_string_get(self->deviceIdentityKeys, identityIndex), key) == 0) {
            return btrc_Vector_string_get(self->deviceIdentityValues, identityIndex);
        }
    }
    return "";
}

void SteamInputParser_parseActions(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log) {
    if ((!JsonValue_has(root, "actions")) || (!JsonValue_isObject(JsonValue_get(root, "actions")))) {
        ParseLog_add(log, path, "actions", "expected object keyed by action id");
        return;
    }
    JsonValue* actions = JsonValue_get(root, "actions");
    btrc_Vector_string* __iter_300 = JsonValue_keys(actions);
    int __n_302 = btrc_Vector_string_iterLen(__iter_300);
    for (int __i_301 = 0; (__i_301 < __n_302); (__i_301++)) {
        char* actionId = btrc_Vector_string_iterGet(__iter_300, __i_301);
        if (!(strcmp(actionId, "doc") == 0)) {
            JsonValue* action = JsonValue_get(actions, actionId);
            char* context = __btrc_str_track(__btrc_strcat("actions.", actionId));
            if (!JsonValue_isObject(action)) {
                ParseLog_add(log, path, context, "expected object {alias, label}");
            } else {
                btrc_Vector_string* __list_303 = btrc_Vector_string_new();
                btrc_Vector_string_push(__list_303, "alias");
                btrc_Vector_string_push(__list_303, "label");
                btrc_Vector_string* allowedKeys = __list_303;
                JsonContract_checkKeys(action, allowedKeys, path, context, log);
                char* alias = JsonContract_requiredString(action, "alias", path, context, log);
                char* label = JsonContract_requiredString(action, "label", path, context, log);
                if (btrc_Vector_string_contains(target->actionAliases, alias)) {
                    ParseLog_add(log, path, __btrc_str_track(__btrc_strcat(context, ".alias")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate alias '", alias)), "'")));
                }
                btrc_Vector_string_push(target->actionIds, actionId);
                btrc_Vector_string_push(target->actionAliases, alias);
                btrc_Vector_string_push(target->actionLabels, label);
                if (allowedKeys != NULL) {
                    if ((--allowedKeys->__rc) <= 0) {
                        btrc_Vector_string_destroy(allowedKeys);
                    }
                }
            }
        }
    }
    if (btrc_Vector_string_size(target->actionIds) == 0) {
        ParseLog_add(log, path, "actions", "must declare at least one action");
    }
}

void SteamInputParser_parseController(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log) {
    if ((!JsonValue_has(root, "controller")) || (!JsonValue_isObject(JsonValue_get(root, "controller")))) {
        ParseLog_add(log, path, "controller", "expected object");
        return;
    }
    JsonValue* controller = JsonValue_get(root, "controller");
    btrc_Vector_string* __list_304 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_304, "model");
    btrc_Vector_string_push(__list_304, "gyro");
    btrc_Vector_string_push(__list_304, "trackpads");
    btrc_Vector_string_push(__list_304, "hotkey_button");
    btrc_Vector_string* allowedKeys = __list_304;
    JsonContract_checkKeys(controller, allowedKeys, path, "controller", log);
    (target->controllerModel = JsonContract_requiredString(controller, "model", path, "controller", log));
    btrc_Vector_string* __list_305 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_305, "steam_deck");
    btrc_Vector_string* models = __list_305;
    JsonContract_checkEnum(target->controllerModel, models, path, "controller.model", log);
    (target->gyro = JsonContract_requiredBool(controller, "gyro", path, "controller", log));
    if ((!JsonValue_has(controller, "trackpads")) || (!JsonValue_isObject(JsonValue_get(controller, "trackpads")))) {
        ParseLog_add(log, path, "controller.trackpads", "expected object {left, right}");
    } else {
        JsonValue* trackpads = JsonValue_get(controller, "trackpads");
        btrc_Vector_string* __list_306 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_306, "left");
        btrc_Vector_string_push(__list_306, "right");
        btrc_Vector_string* trackpadKeys = __list_306;
        JsonContract_checkKeys(trackpads, trackpadKeys, path, "controller.trackpads", log);
        (target->trackpadLeft = JsonContract_requiredString(trackpads, "left", path, "controller.trackpads", log));
        (target->trackpadRight = JsonContract_requiredString(trackpads, "right", path, "controller.trackpads", log));
        btrc_Vector_string* __list_307 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_307, "radial_hotkeys");
        btrc_Vector_string_push(__list_307, "mouse");
        btrc_Vector_string* trackpadModes = __list_307;
        JsonContract_checkEnum(target->trackpadLeft, trackpadModes, path, "controller.trackpads.left", log);
        JsonContract_checkEnum(target->trackpadRight, trackpadModes, path, "controller.trackpads.right", log);
        if (trackpadModes != NULL) {
            if ((--trackpadModes->__rc) <= 0) {
                btrc_Vector_string_destroy(trackpadModes);
            }
        }
        if (trackpadKeys != NULL) {
            if ((--trackpadKeys->__rc) <= 0) {
                btrc_Vector_string_destroy(trackpadKeys);
            }
        }
    }
    (target->hotkeyButton = JsonContract_requiredString(controller, "hotkey_button", path, "controller", log));
    btrc_Vector_string* __list_308 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_308, "view_long_press");
    btrc_Vector_string* hotkeyButtons = __list_308;
    JsonContract_checkEnum(target->hotkeyButton, hotkeyButtons, path, "controller.hotkey_button", log);
    if (hotkeyButtons != NULL) {
        if ((--hotkeyButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(hotkeyButtons);
        }
    }
    if (models != NULL) {
        if ((--models->__rc) <= 0) {
            btrc_Vector_string_destroy(models);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
}

void SteamInputParser_parseRadial(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log) {
    if ((!JsonValue_has(root, "radial")) || (!JsonValue_isObject(JsonValue_get(root, "radial")))) {
        ParseLog_add(log, path, "radial", "expected object {slots}");
    } else {
        JsonValue* radial = JsonValue_get(root, "radial");
        btrc_Vector_string* __list_309 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_309, "slots");
        btrc_Vector_string* radialKeys = __list_309;
        JsonContract_checkKeys(radial, radialKeys, path, "radial", log);
        if (target->radialSlots != NULL) {
            if ((--target->radialSlots->__rc) <= 0) {
                btrc_Vector_string_free(target->radialSlots);
            }
        }
        (target->radialSlots = JsonContract_stringArray(radial, "slots", true, path, "radial", log));
        (target->radialSlots->__rc++);
        if ((btrc_Vector_string_size(target->radialSlots) < 1) || (btrc_Vector_string_size(target->radialSlots) > 6)) {
            ParseLog_add(log, path, "radial.slots", "expected 1..6 ordered slots (touch_menu_button_0..5)");
        }
        if (radialKeys != NULL) {
            if ((--radialKeys->__rc) <= 0) {
                btrc_Vector_string_destroy(radialKeys);
            }
        }
    }
    btrc_Vector_string* __iter_310 = target->radialSlots;
    int __n_312 = btrc_Vector_string_iterLen(__iter_310);
    for (int __i_311 = 0; (__i_311 < __n_312); (__i_311++)) {
        char* slot = btrc_Vector_string_iterGet(__iter_310, __i_311);
        if (!SteamInputTarget_hasAction(target, slot)) {
            ParseLog_add(log, path, "radial.slots", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown action '", slot)), "' (must exist in the actions vocabulary)")));
        }
    }
}

void SteamInputParser_parseTemplates(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log) {
    if (((!JsonValue_has(root, "templates")) || (!JsonValue_isArray(JsonValue_get(root, "templates")))) || (JsonValue_size(JsonValue_get(root, "templates")) < 1)) {
        ParseLog_add(log, path, "templates", "expected non-empty array");
        return;
    }
    JsonValue* templates = JsonValue_get(root, "templates");
    int defaultCount = 0;
    for (int templateIndex = 0; (templateIndex < JsonValue_size(templates)); (templateIndex++)) {
        JsonValue* entry = JsonValue_at(templates, templateIndex);
        int __fstr_313_arg0 = templateIndex;
        int __fstr_313_len = snprintf(NULL, 0, "templates[%d]", __fstr_313_arg0);
        char* __fstr_313_buf = __btrc_str_track(((char*)malloc((__fstr_313_len + 1))));
        snprintf(__fstr_313_buf, (__fstr_313_len + 1), "templates[%d]", __fstr_313_arg0);
        char* context = __fstr_313_buf;
        if (!JsonValue_isObject(entry)) {
            ParseLog_add(log, path, context, "expected object {id, title, radial_name, output, default?}");
        } else {
            btrc_Vector_string* __list_314 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_314, "id");
            btrc_Vector_string_push(__list_314, "title");
            btrc_Vector_string_push(__list_314, "radial_name");
            btrc_Vector_string_push(__list_314, "output");
            btrc_Vector_string_push(__list_314, "default");
            btrc_Vector_string* allowedKeys = __list_314;
            JsonContract_checkKeys(entry, allowedKeys, path, context, log);
            char* templateId = JsonContract_requiredString(entry, "id", path, context, log);
            char* title = JsonContract_requiredString(entry, "title", path, context, log);
            char* radialName = JsonContract_requiredString(entry, "radial_name", path, context, log);
            char* output = JsonContract_requiredString(entry, "output", path, context, log);
            bool isDefault = JsonContract_optionalBool(entry, "default", false, path, context, log);
            if (btrc_Vector_string_contains(target->templateIds, templateId)) {
                ParseLog_add(log, path, __btrc_str_track(__btrc_strcat(context, ".id")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate template id '", templateId)), "'")));
            }
            if (btrc_Vector_string_contains(target->templateOutputs, output)) {
                ParseLog_add(log, path, __btrc_str_track(__btrc_strcat(context, ".output")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("duplicate template output '", output)), "'")));
            }
            if (!__btrc_endsWith(output, ".vdf")) {
                ParseLog_add(log, path, __btrc_str_track(__btrc_strcat(context, ".output")), "must end with .vdf");
            }
            btrc_Vector_string_push(target->templateIds, templateId);
            btrc_Vector_string_push(target->templateTitles, title);
            btrc_Vector_string_push(target->templateRadialNames, radialName);
            btrc_Vector_string_push(target->templateOutputs, output);
            if (isDefault) {
                (defaultCount++);
                (target->defaultTemplateId = templateId);
            }
            if (allowedKeys != NULL) {
                if ((--allowedKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(allowedKeys);
                }
            }
        }
    }
    if (defaultCount != 1) {
        int __fstr_315_arg0 = defaultCount;
        int __fstr_315_len = snprintf(NULL, 0, "exactly one template must set default:true (found %d)", __fstr_315_arg0);
        char* __fstr_315_buf = __btrc_str_track(((char*)malloc((__fstr_315_len + 1))));
        snprintf(__fstr_315_buf, (__fstr_315_len + 1), "exactly one template must set default:true (found %d)", __fstr_315_arg0);
        ParseLog_add(log, path, "templates", __fstr_315_buf);
    }
}

void SteamInputParser_parseStringTable(JsonValue* root, char* key, btrc_Vector_string* outputKeys, btrc_Vector_string* outputValues, char* path, ParseLog* log) {
    if ((!JsonValue_has(root, key)) || (!JsonValue_isObject(JsonValue_get(root, key)))) {
        ParseLog_add(log, path, key, "expected object of string values");
        return;
    }
    JsonValue* table = JsonValue_get(root, key);
    btrc_Vector_string* __iter_316 = JsonValue_keys(table);
    int __n_318 = btrc_Vector_string_iterLen(__iter_316);
    for (int __i_317 = 0; (__i_317 < __n_318); (__i_317++)) {
        char* tableKey = btrc_Vector_string_iterGet(__iter_316, __i_317);
        if (!(strcmp(tableKey, "doc") == 0)) {
            JsonValue* value = JsonValue_get(table, tableKey);
            if ((!JsonValue_isString(value)) || __btrc_isEmpty(JsonValue_asString(value))) {
                ParseLog_add(log, path, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(key, ".")), tableKey)), "expected non-empty string");
            } else {
                btrc_Vector_string_push(outputKeys, tableKey);
                btrc_Vector_string_push(outputValues, JsonValue_asString(value));
            }
        }
    }
}

void SteamInputParser_parsePlatforms(SteamInputTarget* target, JsonValue* root, char* path, ParseLog* log) {
    if (!JsonValue_has(root, "platforms")) {
        return;
    }
    JsonValue* platforms = JsonValue_get(root, "platforms");
    if (!JsonValue_isObject(platforms)) {
        ParseLog_add(log, path, "platforms", "expected object keyed by os");
        return;
    }
    btrc_Vector_string* __list_319 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_319, "linux");
    btrc_Vector_string_push(__list_319, "macos");
    btrc_Vector_string* operatingSystems = __list_319;
    btrc_Vector_string* __iter_320 = JsonValue_keys(platforms);
    int __n_322 = btrc_Vector_string_iterLen(__iter_320);
    for (int __i_321 = 0; (__i_321 < __n_322); (__i_321++)) {
        char* operatingSystemName = btrc_Vector_string_iterGet(__iter_320, __i_321);
        if (!(strcmp(operatingSystemName, "doc") == 0)) {
            JsonContract_checkEnum(operatingSystemName, operatingSystems, path, "platforms", log);
            JsonValue* block = JsonValue_get(platforms, operatingSystemName);
            char* context = __btrc_str_track(__btrc_strcat("platforms.", operatingSystemName));
            if (!JsonValue_isObject(block)) {
                ParseLog_add(log, path, context, "expected object {templates_dir}");
            } else {
                btrc_Vector_string* __list_323 = btrc_Vector_string_new();
                btrc_Vector_string_push(__list_323, "templates_dir");
                btrc_Vector_string* allowedKeys = __list_323;
                JsonContract_checkKeys(block, allowedKeys, path, context, log);
                char* directory = JsonContract_requiredString(block, "templates_dir", path, context, log);
                TemplateTokens_check(directory, path, __btrc_str_track(__btrc_strcat(context, ".templates_dir")), log, false);
                if ((strcmp(operatingSystemName, "linux") == 0) || __btrc_isEmpty(target->templatesDirectory)) {
                    (target->templatesDirectory = directory);
                }
                if (allowedKeys != NULL) {
                    if ((--allowedKeys->__rc) <= 0) {
                        btrc_Vector_string_destroy(allowedKeys);
                    }
                }
            }
        }
    }
    if (operatingSystems != NULL) {
        if ((--operatingSystems->__rc) <= 0) {
            btrc_Vector_string_destroy(operatingSystems);
        }
    }
}

void SteamInputParser_checkKeymapConsistency(SteamInputTarget* target, char* path, ParseLog* log) {
    if (__btrc_isEmpty(target->keymap)) {
        return;
    }
    char* keymapPath = PathTools_join(PathTools_dirname(path), target->keymap);
    if (!FileSystem_isFile(keymapPath)) {
        ParseLog_add(log, path, "keymap", __btrc_str_track(__btrc_strcat("file not found: ", target->keymap)));
        return;
    }
    Keymap* keymap = KeymapParser_parse(Path_readAll(keymapPath), log);
    btrc_Vector_KeymapAction_p1* __iter_324 = keymap->actions;
    int __n_326 = btrc_Vector_KeymapAction_p1_iterLen(__iter_324);
    for (int __i_325 = 0; (__i_325 < __n_326); (__i_325++)) {
        KeymapAction* action = btrc_Vector_KeymapAction_p1_iterGet(__iter_324, __i_325);
        if (!SteamInputTarget_hasAction(target, action->id)) {
            ParseLog_add(log, path, "actions", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("keymap action '", action->id)), "' missing from vocabulary")));
        }
    }
    btrc_Vector_string* __iter_327 = target->actionIds;
    int __n_329 = btrc_Vector_string_iterLen(__iter_327);
    for (int __i_328 = 0; (__i_328 < __n_329); (__i_328++)) {
        char* actionId = btrc_Vector_string_iterGet(__iter_327, __i_328);
        if (!Keymap_hasAction(keymap, actionId)) {
            ParseLog_add(log, keymapPath, "actions", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("vocabulary action '", actionId)), "' not declared in keymap")));
        }
    }
}

SteamInputTarget* SteamInputParser_parse(char* filePath, ParseLog* log) {
    SteamInputTarget* target = SteamInputTarget_new();
    (target->file = filePath);
    JsonValue* root = JsonValue_readFile(filePath);
    if (JsonValue_isError(root)) {
        ParseLog_add(log, filePath, "(root)", "invalid JSON");
        return target;
    }
    JsonContract_checkSchemaVersion(root, 1, filePath, log);
    btrc_Vector_string* __list_330 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_330, "schema_version");
    btrc_Vector_string_push(__list_330, "keymap");
    btrc_Vector_string_push(__list_330, "modifiers");
    btrc_Vector_string_push(__list_330, "actions");
    btrc_Vector_string_push(__list_330, "controller");
    btrc_Vector_string_push(__list_330, "radial");
    btrc_Vector_string_push(__list_330, "quit_extra_bindings");
    btrc_Vector_string_push(__list_330, "templates");
    btrc_Vector_string_push(__list_330, "key_names");
    btrc_Vector_string_push(__list_330, "device_identities");
    btrc_Vector_string_push(__list_330, "platforms");
    btrc_Vector_string* allowedKeys = __list_330;
    JsonContract_checkKeys(root, allowedKeys, filePath, "", log);
    (target->keymap = JsonContract_requiredString(root, "keymap", filePath, "", log));
    if (target->modifiers != NULL) {
        if ((--target->modifiers->__rc) <= 0) {
            btrc_Vector_string_free(target->modifiers);
        }
    }
    (target->modifiers = JsonContract_stringArray(root, "modifiers", true, filePath, "", log));
    (target->modifiers->__rc++);
    btrc_Vector_string* __list_331 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_331, "ctrl");
    btrc_Vector_string_push(__list_331, "alt");
    btrc_Vector_string_push(__list_331, "shift");
    btrc_Vector_string_push(__list_331, "meta");
    btrc_Vector_string* allowedModifiers = __list_331;
    btrc_Vector_string* __iter_332 = target->modifiers;
    int __n_334 = btrc_Vector_string_iterLen(__iter_332);
    for (int __i_333 = 0; (__i_333 < __n_334); (__i_333++)) {
        char* modifier = btrc_Vector_string_iterGet(__iter_332, __i_333);
        JsonContract_checkEnum(modifier, allowedModifiers, filePath, "modifiers", log);
    }
    SteamInputParser_parseActions(target, root, filePath, log);
    SteamInputParser_parseController(target, root, filePath, log);
    SteamInputParser_parseRadial(target, root, filePath, log);
    if (target->quitExtraBindings != NULL) {
        if ((--target->quitExtraBindings->__rc) <= 0) {
            btrc_Vector_string_free(target->quitExtraBindings);
        }
    }
    (target->quitExtraBindings = JsonContract_stringArray(root, "quit_extra_bindings", true, filePath, "", log));
    (target->quitExtraBindings->__rc++);
    btrc_Vector_string* __list_335 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_335, "xinput_select");
    btrc_Vector_string_push(__list_335, "xinput_start");
    btrc_Vector_string_push(__list_335, "escape");
    btrc_Vector_string_push(__list_335, "alt_f4");
    btrc_Vector_string* quitKinds = __list_335;
    btrc_Vector_string* __iter_336 = target->quitExtraBindings;
    int __n_338 = btrc_Vector_string_iterLen(__iter_336);
    for (int __i_337 = 0; (__i_337 < __n_338); (__i_337++)) {
        char* extraBinding = btrc_Vector_string_iterGet(__iter_336, __i_337);
        JsonContract_checkEnum(extraBinding, quitKinds, filePath, "quit_extra_bindings", log);
    }
    SteamInputParser_parseTemplates(target, root, filePath, log);
    SteamInputParser_parseStringTable(root, "key_names", target->keyNameKeys, target->keyNameValues, filePath, log);
    SteamInputParser_parseStringTable(root, "device_identities", target->deviceIdentityKeys, target->deviceIdentityValues, filePath, log);
    SteamInputParser_parsePlatforms(target, root, filePath, log);
    SteamInputParser_checkKeymapConsistency(target, filePath, log);
    if (quitKinds != NULL) {
        if ((--quitKinds->__rc) <= 0) {
            btrc_Vector_string_destroy(quitKinds);
        }
    }
    if (allowedModifiers != NULL) {
        if ((--allowedModifiers->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedModifiers);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return target;
    if (quitKinds != NULL) {
        if ((--quitKinds->__rc) <= 0) {
            btrc_Vector_string_destroy(quitKinds);
        }
    }
    if (allowedModifiers != NULL) {
        if ((--allowedModifiers->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedModifiers);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (target != NULL) {
        if ((--target->__rc) <= 0) {
            SteamInputTarget_destroy(target);
        }
    }
}

char* ChordTranslator_keyPart(char* chord) {
    return ChordSyntax_keyPart(chord);
}

btrc_Vector_string* ChordTranslator_modifierList(char* chord) {
    btrc_Vector_string* modifiers = btrc_Vector_string_new();
    char* joinedModifiers = ChordSyntax_modifierPart(chord);
    if (!__btrc_isEmpty(joinedModifiers)) {
        btrc_Vector_string* __iter_339 = Strings_split(joinedModifiers, "+");
        int __n_341 = btrc_Vector_string_iterLen(__iter_339);
        for (int __i_340 = 0; (__i_340 < __n_341); (__i_340++)) {
            char* modifier = btrc_Vector_string_iterGet(__iter_339, __i_340);
            btrc_Vector_string_push(modifiers, modifier);
        }
    }
    return modifiers;
    if (modifiers != NULL) {
        if ((--modifiers->__rc) <= 0) {
            btrc_Vector_string_destroy(modifiers);
        }
    }
}

char* ChordTranslator_vdfTokens(char* chord, SteamInputTarget* target) {
    btrc_Vector_string* keyNames = btrc_Vector_string_new();
    btrc_Vector_string* __iter_342 = ChordTranslator_modifierList(chord);
    int __n_344 = btrc_Vector_string_iterLen(__iter_342);
    for (int __i_343 = 0; (__i_343 < __n_344); (__i_343++)) {
        char* modifier = btrc_Vector_string_iterGet(__iter_342, __i_343);
        btrc_Vector_string_push(keyNames, SteamInputTarget_keyName(target, modifier));
    }
    btrc_Vector_string_push(keyNames, SteamInputTarget_keyName(target, ChordTranslator_keyPart(chord)));
    char* __btrc_ret_345 = btrc_Vector_string_join(keyNames, " ");
    if (keyNames != NULL) {
        if ((--keyNames->__rc) <= 0) {
            btrc_Vector_string_destroy(keyNames);
        }
    }
    return __btrc_ret_345;
    if (keyNames != NULL) {
        if ((--keyNames->__rc) <= 0) {
            btrc_Vector_string_destroy(keyNames);
        }
    }
}

char* RetroArchEmitter_hotkeyToken(char* chord) {
    char* keyName = ChordTranslator_keyPart(chord);
    if (strcmp(keyName, "+") == 0) {
        return "add";
    }
    if (strcmp(keyName, "-") == 0) {
        return "subtract";
    }
    char* loweredKeyName = __btrc_str_track(__btrc_toLower(keyName));
    if (strcmp(loweredKeyName, "esc") == 0) {
        return "escape";
    }
    return loweredKeyName;
}

char* RetroArchEmitter_configurationLine(char* key, char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(key, " = \"")), value)), "\""));
}

char* RetroArchEmitter_runtimeContentRoot(char* outputRoot) {
    char* generatedRoot = PathTools_dirname(PathTools_dirname(PathTools_dirname(outputRoot)));
    return PathTools_join(generatedRoot, "runtime/content");
}

btrc_Vector_string* RetroArchEmitter_hotkeyLines(Keymap* keymap) {
    btrc_Vector_string* lines = btrc_Vector_string_new();
    if (Keymap_hasAction(keymap, "ui.escape")) {
        btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("input_exit_emulator", RetroArchEmitter_hotkeyToken(Keymap_chordFor(keymap, "ui.escape"))));
        btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("input_quit_gamepad_combo", "4"));
    }
    btrc_Vector_string* __list_346 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_346, "input_load_state");
    btrc_Vector_string_push(__list_346, "input_menu_toggle");
    btrc_Vector_string_push(__list_346, "input_save_state");
    btrc_Vector_string_push(__list_346, "input_screenshot");
    btrc_Vector_string_push(__list_346, "input_state_slot_decrease");
    btrc_Vector_string_push(__list_346, "input_state_slot_increase");
    btrc_Vector_string_push(__list_346, "input_toggle_fast_forward");
    btrc_Vector_string_push(__list_346, "input_toggle_fullscreen");
    btrc_Vector_string* hotkeyKeys = __list_346;
    btrc_Vector_string* __list_347 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_347, "state.load");
    btrc_Vector_string_push(__list_347, "ui.menu");
    btrc_Vector_string_push(__list_347, "state.save");
    btrc_Vector_string_push(__list_347, "ui.screenshot");
    btrc_Vector_string_push(__list_347, "state.prev");
    btrc_Vector_string_push(__list_347, "state.next");
    btrc_Vector_string_push(__list_347, "speed.fast");
    btrc_Vector_string_push(__list_347, "ui.fullscreen");
    btrc_Vector_string* hotkeyActionIds = __list_347;
    for (int hotkeyIndex = 0; (hotkeyIndex < btrc_Vector_string_size(hotkeyKeys)); (hotkeyIndex++)) {
        char* actionId = btrc_Vector_string_get(hotkeyActionIds, hotkeyIndex);
        if (Keymap_hasAction(keymap, actionId)) {
            btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine(btrc_Vector_string_get(hotkeyKeys, hotkeyIndex), RetroArchEmitter_hotkeyToken(Keymap_chordFor(keymap, actionId))));
        }
    }
    if (hotkeyActionIds != NULL) {
        if ((--hotkeyActionIds->__rc) <= 0) {
            btrc_Vector_string_destroy(hotkeyActionIds);
        }
    }
    if (hotkeyKeys != NULL) {
        if ((--hotkeyKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(hotkeyKeys);
        }
    }
    return lines;
    if (hotkeyActionIds != NULL) {
        if ((--hotkeyActionIds->__rc) <= 0) {
            btrc_Vector_string_destroy(hotkeyActionIds);
        }
    }
    if (hotkeyKeys != NULL) {
        if ((--hotkeyKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(hotkeyKeys);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* RetroArchEmitter_profileText(char* inputDriver, char* audioDriver, char* contentRoot, Keymap* keymap) {
    btrc_Vector_string* lines = btrc_Vector_string_new();
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("config_save_on_exit", "true"));
    if (!__btrc_isEmpty(audioDriver)) {
        btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("audio_driver", audioDriver));
    }
    if (!__btrc_isEmpty(inputDriver)) {
        btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("input_driver", inputDriver));
    }
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("input_autodetect_enable", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("input_remap_binds_enable", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("savestate_directory", PathTools_join(contentRoot, "states")));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("savefile_directory", PathTools_join(contentRoot, "saves")));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("screenshot_directory", PathTools_join(contentRoot, "screenshots")));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("video_fullscreen", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("video_scale_integer", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("video_force_aspect", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("video_vsync", "true"));
    btrc_Vector_string_push(lines, RetroArchEmitter_configurationLine("video_shader_enable", "false"));
    btrc_Vector_string* hotkeys = RetroArchEmitter_hotkeyLines(keymap);
    int __n_349 = btrc_Vector_string_iterLen(hotkeys);
    for (int __i_348 = 0; (__i_348 < __n_349); (__i_348++)) {
        char* hotkeyLine = btrc_Vector_string_iterGet(hotkeys, __i_348);
        btrc_Vector_string_push(lines, hotkeyLine);
    }
    char* __btrc_ret_350 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_350;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void RetroArchEmitter_emitProfile(char* profileName, char* inputDriver, char* audioDriver, Keymap* keymap, char* outputRoot) {
    char* profileDirectory = PathTools_join(outputRoot, profileName);
    UnixFileSystem_mkdirp(profileDirectory);
    char* contentRoot = RetroArchEmitter_runtimeContentRoot(outputRoot);
    Path_writeAll(PathTools_join(profileDirectory, "retroarch.cfg"), RetroArchEmitter_profileText(inputDriver, audioDriver, contentRoot, keymap));
}

void RetroArchConfiguration_init(RetroArchConfiguration* self) {
    self->__rc = 1;
    (self->present = false);
    (self->videoDriver = "");
    (self->inputDriver = "");
    (self->coreSuffix = "");
    if (self->coreDirectories != NULL) {
        if ((--self->coreDirectories->__rc) <= 0) {
            btrc_Vector_string_free(self->coreDirectories);
        }
    }
    btrc_Vector_string* __list_351 = btrc_Vector_string_new();
    (self->coreDirectories = __list_351);
    (self->coreDirectories->__rc++);
    if (self->settingsOverrideKeys != NULL) {
        if ((--self->settingsOverrideKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->settingsOverrideKeys);
        }
    }
    btrc_Vector_string* __list_352 = btrc_Vector_string_new();
    (self->settingsOverrideKeys = __list_352);
    (self->settingsOverrideKeys->__rc++);
    if (self->settingsOverrideValues != NULL) {
        if ((--self->settingsOverrideValues->__rc) <= 0) {
            btrc_Vector_string_free(self->settingsOverrideValues);
        }
    }
    btrc_Vector_string* __list_353 = btrc_Vector_string_new();
    (self->settingsOverrideValues = __list_353);
    (self->settingsOverrideValues->__rc++);
    if (self->coreOptionEntries != NULL) {
        if ((--self->coreOptionEntries->__rc) <= 0) {
            btrc_Vector_string_free(self->coreOptionEntries);
        }
    }
    btrc_Vector_string* __list_354 = btrc_Vector_string_new();
    (self->coreOptionEntries = __list_354);
    (self->coreOptionEntries->__rc++);
    if (self->tapBinaries != NULL) {
        if ((--self->tapBinaries->__rc) <= 0) {
            btrc_Vector_string_free(self->tapBinaries);
        }
    }
    btrc_Vector_string* __list_355 = btrc_Vector_string_new();
    (self->tapBinaries = __list_355);
    (self->tapBinaries->__rc++);
}

RetroArchConfiguration* RetroArchConfiguration_new(void) {
    RetroArchConfiguration* self = ((RetroArchConfiguration*)malloc(sizeof(RetroArchConfiguration)));
    memset(self, 0, sizeof(RetroArchConfiguration));
    RetroArchConfiguration_init(self);
    return self;
}

void RetroArchConfiguration_destroy(RetroArchConfiguration* self) {
    if (self->coreDirectories != NULL) {
        if ((--self->coreDirectories->__rc) <= 0) {
            btrc_Vector_string_free(self->coreDirectories);
        }
    }
    if (self->settingsOverrideKeys != NULL) {
        if ((--self->settingsOverrideKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->settingsOverrideKeys);
        }
    }
    if (self->settingsOverrideValues != NULL) {
        if ((--self->settingsOverrideValues->__rc) <= 0) {
            btrc_Vector_string_free(self->settingsOverrideValues);
        }
    }
    if (self->coreOptionEntries != NULL) {
        if ((--self->coreOptionEntries->__rc) <= 0) {
            btrc_Vector_string_free(self->coreOptionEntries);
        }
    }
    if (self->tapBinaries != NULL) {
        if ((--self->tapBinaries->__rc) <= 0) {
            btrc_Vector_string_free(self->tapBinaries);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void RetroArchEmulator_init(RetroArchEmulator* self) {
    self->__rc = 1;
}

RetroArchEmulator* RetroArchEmulator_new(void) {
    RetroArchEmulator* self = ((RetroArchEmulator*)malloc(sizeof(RetroArchEmulator)));
    memset(self, 0, sizeof(RetroArchEmulator));
    RetroArchEmulator_init(self);
    return self;
}

char* RetroArchEmulator_id(RetroArchEmulator* self) {
    return "retroarch";
}

char* RetroArchEmulator_contractFile(RetroArchEmulator* self) {
    return "emulators/retroarch/emulator.json";
}

RetroArchConfiguration* RetroArchEmulator_parseConfiguration(RetroArchEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    RetroArchConfiguration* configuration = RetroArchConfiguration_new();
    char* file = RetroArchEmulator_contractFile(self);
    char* context = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".retroarch"));
    if (!EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, file, context, "missing required retroarch extension block");
        return configuration;
    }
    JsonValue* jsonValue = platform->extensionJson;
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return configuration;
    }
    (configuration->present = true);
    btrc_Vector_string* __list_356 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_356, "video_driver");
    btrc_Vector_string_push(__list_356, "input_driver");
    btrc_Vector_string_push(__list_356, "core_suffix");
    btrc_Vector_string_push(__list_356, "core_dirs");
    btrc_Vector_string_push(__list_356, "settings_overrides");
    btrc_Vector_string_push(__list_356, "core_options");
    btrc_Vector_string_push(__list_356, "tap_binary");
    btrc_Vector_string* allowedKeys = __list_356;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (configuration->videoDriver = JsonContract_requiredString(jsonValue, "video_driver", file, context, log));
    (configuration->inputDriver = JsonContract_optionalString(jsonValue, "input_driver", "", file, context, log));
    (configuration->coreSuffix = JsonContract_requiredString(jsonValue, "core_suffix", file, context, log));
    btrc_Vector_string* __list_357 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_357, ".so");
    btrc_Vector_string_push(__list_357, ".dylib");
    btrc_Vector_string* coreSuffixes = __list_357;
    JsonContract_checkEnum(configuration->coreSuffix, coreSuffixes, file, __btrc_str_track(__btrc_strcat(context, ".core_suffix")), log);
    if (configuration->coreDirectories != NULL) {
        if ((--configuration->coreDirectories->__rc) <= 0) {
            btrc_Vector_string_free(configuration->coreDirectories);
        }
    }
    (configuration->coreDirectories = JsonContract_stringArray(jsonValue, "core_dirs", true, file, context, log));
    (configuration->coreDirectories->__rc++);
    btrc_Vector_string* __iter_358 = configuration->coreDirectories;
    int __n_360 = btrc_Vector_string_iterLen(__iter_358);
    for (int __i_359 = 0; (__i_359 < __n_360); (__i_359++)) {
        char* coreDirectory = btrc_Vector_string_iterGet(__iter_358, __i_359);
        if (!__btrc_startsWith(coreDirectory, "/")) {
            TemplateTokens_check(coreDirectory, file, __btrc_str_track(__btrc_strcat(context, ".core_dirs")), log, false);
        }
    }
    RetroArchEmulator_parseSettingsOverrides(self, configuration, jsonValue, file, context, log);
    RetroArchEmulator_parseCoreOptions(self, configuration, jsonValue, file, context, log);
    if (configuration->tapBinaries != NULL) {
        if ((--configuration->tapBinaries->__rc) <= 0) {
            btrc_Vector_string_free(configuration->tapBinaries);
        }
    }
    (configuration->tapBinaries = JsonContract_stringArray(jsonValue, "tap_binary", false, file, context, log));
    (configuration->tapBinaries->__rc++);
    btrc_Vector_string* __iter_361 = configuration->tapBinaries;
    int __n_363 = btrc_Vector_string_iterLen(__iter_361);
    for (int __i_362 = 0; (__i_362 < __n_363); (__i_362++)) {
        char* tapBinary = btrc_Vector_string_iterGet(__iter_361, __i_362);
        TemplateTokens_check(tapBinary, file, __btrc_str_track(__btrc_strcat(context, ".tap_binary")), log, false);
    }
    if (coreSuffixes != NULL) {
        if ((--coreSuffixes->__rc) <= 0) {
            btrc_Vector_string_destroy(coreSuffixes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return configuration;
    if (coreSuffixes != NULL) {
        if ((--coreSuffixes->__rc) <= 0) {
            btrc_Vector_string_destroy(coreSuffixes);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (configuration != NULL) {
        if ((--configuration->__rc) <= 0) {
            RetroArchConfiguration_destroy(configuration);
        }
    }
}

void RetroArchEmulator_parseSettingsOverrides(RetroArchEmulator* self, RetroArchConfiguration* configuration, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    if (!JsonValue_has(jsonValue, "settings_overrides")) {
        return;
    }
    JsonValue* settingsOverrides = JsonValue_get(jsonValue, "settings_overrides");
    if (!JsonValue_isObject(settingsOverrides)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".settings_overrides")), "expected object");
        return;
    }
    btrc_Vector_string* __iter_364 = JsonValue_keys(settingsOverrides);
    int __n_366 = btrc_Vector_string_iterLen(__iter_364);
    for (int __i_365 = 0; (__i_365 < __n_366); (__i_365++)) {
        char* key = btrc_Vector_string_iterGet(__iter_364, __i_365);
        char* value = JsonContract_requiredString(settingsOverrides, key, file, __btrc_str_track(__btrc_strcat(context, ".settings_overrides")), log);
        btrc_Vector_string_push(configuration->settingsOverrideKeys, key);
        btrc_Vector_string_push(configuration->settingsOverrideValues, value);
    }
}

void RetroArchEmulator_parseCoreOptions(RetroArchEmulator* self, RetroArchConfiguration* configuration, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    if (!JsonValue_has(jsonValue, "core_options")) {
        return;
    }
    JsonValue* coreOptions = JsonValue_get(jsonValue, "core_options");
    if (!JsonValue_isObject(coreOptions)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".core_options")), "expected object keyed by core name");
        return;
    }
    btrc_Vector_string* __iter_367 = JsonValue_keys(coreOptions);
    int __n_369 = btrc_Vector_string_iterLen(__iter_367);
    for (int __i_368 = 0; (__i_368 < __n_369); (__i_368++)) {
        char* coreName = btrc_Vector_string_iterGet(__iter_367, __i_368);
        JsonValue* roles = JsonValue_get(coreOptions, coreName);
        if (!JsonValue_isObject(roles)) {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".core_options.")), coreName)), "expected object of role -> option key");
        } else {
            btrc_Vector_string* __iter_370 = JsonValue_keys(roles);
            int __n_372 = btrc_Vector_string_iterLen(__iter_370);
            for (int __i_371 = 0; (__i_371 < __n_372); (__i_371++)) {
                char* role = btrc_Vector_string_iterGet(__iter_370, __i_371);
                char* optionKey = JsonContract_requiredString(roles, role, file, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".core_options.")), coreName)), log);
                btrc_Vector_string_push(configuration->coreOptionEntries, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(coreName, "|")), role)), "|")), optionKey)));
            }
        }
    }
}

void RetroArchEmulator_parseExtension(RetroArchEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    RetroArchEmulator_parseConfiguration(self, platform, log);
}

btrc_Vector_string* RetroArchEmulator_commandFragments(RetroArchEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    if (__btrc_isEmpty(binding->core)) {
        return fragments;
    }
    ParseLog* scratchLog = ParseLog_new();
    RetroArchConfiguration* configuration = RetroArchEmulator_parseConfiguration(self, platform, scratchLog);
    if (!configuration->present) {
        if (scratchLog != NULL) {
            if ((--scratchLog->__rc) <= 0) {
                ParseLog_destroy(scratchLog);
            }
        }
        return fragments;
    }
    btrc_Vector_string_push(fragments, "-L");
    btrc_Vector_string_push(fragments, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("%CORE_RETROARCH%/", binding->core)), "_libretro")), configuration->coreSuffix)));
    if (scratchLog != NULL) {
        if ((--scratchLog->__rc) <= 0) {
            ParseLog_destroy(scratchLog);
        }
    }
    return fragments;
    if (scratchLog != NULL) {
        if ((--scratchLog->__rc) <= 0) {
            ParseLog_destroy(scratchLog);
        }
    }
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

char* RetroArchEmulator_bundledCorePathEntry(RetroArchEmulator* self) {
    return "%ESPATH%/../lib/retroarch/cores";
}

btrc_Vector_string* RetroArchEmulator_esDeCorePathDirectories(RetroArchEmulator* self, EmulatorPlatform* platform) {
    btrc_Vector_string* directories = btrc_Vector_string_new();
    ParseLog* scratchLog = ParseLog_new();
    RetroArchConfiguration* configuration = RetroArchEmulator_parseConfiguration(self, platform, scratchLog);
    if (!configuration->present) {
        if (scratchLog != NULL) {
            if ((--scratchLog->__rc) <= 0) {
                ParseLog_destroy(scratchLog);
            }
        }
        return directories;
    }
    btrc_Vector_string_push(directories, RetroArchEmulator_bundledCorePathEntry(self));
    btrc_Vector_string* __iter_373 = configuration->coreDirectories;
    int __n_375 = btrc_Vector_string_iterLen(__iter_373);
    for (int __i_374 = 0; (__i_374 < __n_375); (__i_374++)) {
        char* coreDirectory = btrc_Vector_string_iterGet(__iter_373, __i_374);
        if (__btrc_startsWith(coreDirectory, "/")) {
            btrc_Vector_string_push(directories, coreDirectory);
        }
    }
    if (scratchLog != NULL) {
        if ((--scratchLog->__rc) <= 0) {
            ParseLog_destroy(scratchLog);
        }
    }
    return directories;
    if (scratchLog != NULL) {
        if ((--scratchLog->__rc) <= 0) {
            ParseLog_destroy(scratchLog);
        }
    }
    if (directories != NULL) {
        if ((--directories->__rc) <= 0) {
            btrc_Vector_string_destroy(directories);
        }
    }
}

EmulatorPlatform* RetroArchEmulator_profilePlatform(RetroArchEmulator* self, EmulatorDefinition* definition) {
    if (EmulatorDefinition_hasOperatingSystem(definition, "linux")) {
        return EmulatorDefinition_platformFor(definition, "linux");
    }
    if (btrc_Vector_EmulatorPlatform_p1_size(definition->platforms) > 0) {
        return btrc_Vector_EmulatorPlatform_p1_get(definition->platforms, 0);
    }
    return EmulatorPlatform_new();
}

char* RetroArchEmulator_audioDriver(RetroArchEmulator* self, EmulatorPlatform* platform) {
    if (platform->sandbox->present && btrc_Vector_string_contains(platform->sandbox->sockets, "pulseaudio")) {
        return "pulse";
    }
    return "";
}

void RetroArchEmulator_emitProfiles(RetroArchEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    EmulatorPlatform* platform = RetroArchEmulator_profilePlatform(self, definition);
    ParseLog* scratchLog = ParseLog_new();
    RetroArchConfiguration* configuration = RetroArchEmulator_parseConfiguration(self, platform, scratchLog);
    char* inputDriver = SteamInputTarget_deviceIdentity(steamInput, __btrc_str_track(__btrc_strcat(RetroArchEmulator_id(self), "_input_driver")));
    if (__btrc_isEmpty(inputDriver)) {
        (inputDriver = configuration->inputDriver);
    }
    RetroArchEmitter_emitProfile(definition->name, inputDriver, RetroArchEmulator_audioDriver(self, platform), keymap, outputRoot);
    if (scratchLog != NULL) {
        if ((--scratchLog->__rc) <= 0) {
            ParseLog_destroy(scratchLog);
        }
    }
}

void DolphinWiimoteProfiles_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions) {
    int __n_377 = btrc_Vector_string_iterLen(additions);
    for (int __i_376 = 0; (__i_376 < __n_377); (__i_376++)) {
        char* addition = btrc_Vector_string_iterGet(additions, __i_376);
        btrc_Vector_string_push(lines, addition);
    }
}

char* DolphinWiimoteProfiles_qualifiedControl(char* deviceIdentity, char* control) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("`", deviceIdentity)), ":")), control)), "`"));
}

btrc_Vector_string* DolphinWiimoteProfiles_wiimoteProfileCommon(char* wiimoteIdentity, char* pointerIdentity) {
    btrc_Vector_string* __list_378 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("Device = ", wiimoteIdentity)));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Buttons/A = `A`|", DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Click 1"))), "|")), DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Return"))));
    btrc_Vector_string_push(__list_378, "Buttons/B = B|`R4`");
    btrc_Vector_string_push(__list_378, "Buttons/1 = X");
    btrc_Vector_string_push(__list_378, "Buttons/2 = Y");
    btrc_Vector_string_push(__list_378, "Buttons/- = View");
    btrc_Vector_string_push(__list_378, "Buttons/+ = Menu");
    btrc_Vector_string_push(__list_378, "D-Pad/Up = `D-Pad Up`");
    btrc_Vector_string_push(__list_378, "D-Pad/Down = `D-Pad Down`");
    btrc_Vector_string_push(__list_378, "D-Pad/Left = `D-Pad Left`");
    btrc_Vector_string_push(__list_378, "D-Pad/Right = `D-Pad Right`");
    btrc_Vector_string_push(__list_378, "IR/Vertical Offset = 12.");
    btrc_Vector_string_push(__list_378, "IR/Total Yaw = 19.");
    btrc_Vector_string_push(__list_378, "IR/Total Pitch = 22.");
    btrc_Vector_string_push(__list_378, "IR/Auto-Hide = True");
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("IR/Up = ", DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Cursor Y-"))));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("IR/Down = ", DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Cursor Y+"))));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("IR/Left = ", DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Cursor X-"))));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("IR/Right = ", DolphinWiimoteProfiles_qualifiedControl(pointerIdentity, "Cursor X+"))));
    btrc_Vector_string_push(__list_378, "IR/Hide = `Thumb L`");
    btrc_Vector_string_push(__list_378, "IMUIR/Enabled = False");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Up = `Accel Up`");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Down = `Accel Down`");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Left = `Accel Left`");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Right = `Accel Right`");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Forward = `Accel Forward`");
    btrc_Vector_string_push(__list_378, "IMUAccelerometer/Backward = `Accel Backward`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Pitch Up = `Gyro Pitch Up`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Pitch Down = `Gyro Pitch Down`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Roll Left = `Gyro Roll Left`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Roll Right = `Gyro Roll Right`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Yaw Left = `Gyro Yaw Left`");
    btrc_Vector_string_push(__list_378, "IMUGyroscope/Yaw Right = `Gyro Yaw Right`");
    btrc_Vector_string* lines = __list_378;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* DolphinWiimoteProfiles_wiimoteNunchukProfile(char* wiimoteIdentity, char* pointerIdentity) {
    btrc_Vector_string* __list_379 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_379, "[Profile]");
    btrc_Vector_string* lines = __list_379;
    DolphinWiimoteProfiles_pushAll(lines, DolphinWiimoteProfiles_wiimoteProfileCommon(wiimoteIdentity, pointerIdentity));
    btrc_Vector_string* __list_380 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_380, "Extension = Nunchuk");
    btrc_Vector_string_push(__list_380, "Nunchuk/Buttons/C = `Shoulder R`");
    btrc_Vector_string_push(__list_380, "Nunchuk/Buttons/Z = `Trigger R`");
    btrc_Vector_string_push(__list_380, "Nunchuk/Stick/Up = `Left Stick Y+`");
    btrc_Vector_string_push(__list_380, "Nunchuk/Stick/Down = `Left Stick Y-`");
    btrc_Vector_string_push(__list_380, "Nunchuk/Stick/Left = `Left Stick X-`");
    btrc_Vector_string_push(__list_380, "Nunchuk/Stick/Right = `Left Stick X+`");
    btrc_Vector_string_push(__list_380, "Rumble/Motor = Strong");
    btrc_Vector_string* nunchukBindings = __list_380;
    DolphinWiimoteProfiles_pushAll(lines, nunchukBindings);
    char* __btrc_ret_381 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (nunchukBindings != NULL) {
        if ((--nunchukBindings->__rc) <= 0) {
            btrc_Vector_string_destroy(nunchukBindings);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_381;
    if (nunchukBindings != NULL) {
        if ((--nunchukBindings->__rc) <= 0) {
            btrc_Vector_string_destroy(nunchukBindings);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* DolphinWiimoteProfiles_wiimoteClassicProfile(char* wiimoteIdentity, char* pointerIdentity) {
    btrc_Vector_string* __list_382 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_382, "[Profile]");
    btrc_Vector_string* lines = __list_382;
    DolphinWiimoteProfiles_pushAll(lines, DolphinWiimoteProfiles_wiimoteProfileCommon(wiimoteIdentity, pointerIdentity));
    btrc_Vector_string* __list_383 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_383, "Extension = Classic");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/A = B");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/B = A");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/X = Y");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/Y = X");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/ZL = `L1`");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/ZR = `R1`");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/- = View");
    btrc_Vector_string_push(__list_383, "Classic/Buttons/+ = Menu");
    btrc_Vector_string_push(__list_383, "Classic/Left Stick/Up = `Left Stick Y+`");
    btrc_Vector_string_push(__list_383, "Classic/Left Stick/Down = `Left Stick Y-`");
    btrc_Vector_string_push(__list_383, "Classic/Left Stick/Left = `Left Stick X-`");
    btrc_Vector_string_push(__list_383, "Classic/Left Stick/Right = `Left Stick X+`");
    btrc_Vector_string_push(__list_383, "Classic/Right Stick/Up = `Right Stick Y+`");
    btrc_Vector_string_push(__list_383, "Classic/Right Stick/Down = `Right Stick Y-`");
    btrc_Vector_string_push(__list_383, "Classic/Right Stick/Left = `Right Stick X-`");
    btrc_Vector_string_push(__list_383, "Classic/Right Stick/Right = `Right Stick X+`");
    btrc_Vector_string_push(__list_383, "Classic/Triggers/L = `L2`");
    btrc_Vector_string_push(__list_383, "Classic/Triggers/R = `R2`");
    btrc_Vector_string_push(__list_383, "Classic/D-Pad/Up = `D-Pad Up`");
    btrc_Vector_string_push(__list_383, "Classic/D-Pad/Down = `D-Pad Down`");
    btrc_Vector_string_push(__list_383, "Classic/D-Pad/Left = `D-Pad Left`");
    btrc_Vector_string_push(__list_383, "Classic/D-Pad/Right = `D-Pad Right`");
    btrc_Vector_string_push(__list_383, "Rumble/Motor = Strong");
    btrc_Vector_string* classicBindings = __list_383;
    DolphinWiimoteProfiles_pushAll(lines, classicBindings);
    char* __btrc_ret_384 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (classicBindings != NULL) {
        if ((--classicBindings->__rc) <= 0) {
            btrc_Vector_string_destroy(classicBindings);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_384;
    if (classicBindings != NULL) {
        if ((--classicBindings->__rc) <= 0) {
            btrc_Vector_string_destroy(classicBindings);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* DolphinWiimoteProfiles_wiimoteNewFile(char* wiimoteIdentity, char* pointerIdentity) {
    btrc_Vector_string* __list_385 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_385, "[Wiimote1]");
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Device = ", pointerIdentity)));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Buttons/A = Return|`Click 1`|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "A"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Buttons/B = B|`Click 3`|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "B"))), "|")), DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "R4"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Buttons/1 = 1|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "X"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Buttons/2 = 2|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Y"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Buttons/- = Q|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "View"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Buttons/+ = E|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Menu"))));
    btrc_Vector_string_push(__list_385, "Buttons/Home = Return");
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("D-Pad/Up = Up|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "D-Pad Up"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("D-Pad/Down = Down|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "D-Pad Down"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("D-Pad/Left = Left|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "D-Pad Left"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("D-Pad/Right = Right|", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "D-Pad Right"))));
    btrc_Vector_string_push(__list_385, "IR/Vertical Offset = 12.");
    btrc_Vector_string_push(__list_385, "IR/Total Yaw = 19.");
    btrc_Vector_string_push(__list_385, "IR/Total Pitch = 22.");
    btrc_Vector_string_push(__list_385, "IR/Auto-Hide = True");
    btrc_Vector_string_push(__list_385, "IR/Up = `Cursor Y-`");
    btrc_Vector_string_push(__list_385, "IR/Down = `Cursor Y+`");
    btrc_Vector_string_push(__list_385, "IR/Left = `Cursor X-`");
    btrc_Vector_string_push(__list_385, "IR/Right = `Cursor X+`");
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("IR/Hide = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Thumb L"))));
    btrc_Vector_string_push(__list_385, "IMUIR/Enabled = False");
    btrc_Vector_string_push(__list_385, "Extension = Nunchuk");
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Buttons/C = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Shoulder R"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Buttons/Z = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Trigger R"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Stick/Up = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Left Stick Y+"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Stick/Down = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Left Stick Y-"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Stick/Left = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Left Stick X-"))));
    btrc_Vector_string_push(__list_385, __btrc_str_track(__btrc_strcat("Nunchuk/Stick/Right = ", DolphinWiimoteProfiles_qualifiedControl(wiimoteIdentity, "Left Stick X+"))));
    btrc_Vector_string_push(__list_385, "Rumble/Motor = Strong");
    btrc_Vector_string_push(__list_385, "Source = 1");
    btrc_Vector_string* lines = __list_385;
    btrc_Vector_string* __list_386 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_386, "[Wiimote2]");
    btrc_Vector_string_push(__list_386, "[Wiimote3]");
    btrc_Vector_string_push(__list_386, "[Wiimote4]");
    btrc_Vector_string_push(__list_386, "[BalanceBoard]");
    btrc_Vector_string* parkedSections = __list_386;
    int __n_388 = btrc_Vector_string_iterLen(parkedSections);
    for (int __i_387 = 0; (__i_387 < __n_388); (__i_387++)) {
        char* parkedSection = btrc_Vector_string_iterGet(parkedSections, __i_387);
        btrc_Vector_string_push(lines, parkedSection);
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("Device = ", wiimoteIdentity)));
    }
    char* __btrc_ret_389 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (parkedSections != NULL) {
        if ((--parkedSections->__rc) <= 0) {
            btrc_Vector_string_destroy(parkedSections);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_389;
    if (parkedSections != NULL) {
        if ((--parkedSections->__rc) <= 0) {
            btrc_Vector_string_destroy(parkedSections);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void DolphinEmitter_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions) {
    int __n_391 = btrc_Vector_string_iterLen(additions);
    for (int __i_390 = 0; (__i_390 < __n_391); (__i_390++)) {
        char* addition = btrc_Vector_string_iterGet(additions, __i_390);
        btrc_Vector_string_push(lines, addition);
    }
}

char* DolphinEmitter_nativeChord(char* chord) {
    if (strcmp(chord, "Esc") == 0) {
        return "Escape";
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("@(", chord)), ")"));
}

char* DolphinEmitter_dolphinIni(void) {
    btrc_Vector_string* __list_392 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_392, "[Analytics]");
    btrc_Vector_string_push(__list_392, "Enabled = False");
    btrc_Vector_string_push(__list_392, "PermissionAsked = True");
    btrc_Vector_string_push(__list_392, "");
    btrc_Vector_string_push(__list_392, "[Core]");
    btrc_Vector_string_push(__list_392, "SIDevice0 = 6");
    btrc_Vector_string_push(__list_392, "SIDevice1 = 0");
    btrc_Vector_string_push(__list_392, "SIDevice2 = 0");
    btrc_Vector_string_push(__list_392, "SIDevice3 = 0");
    btrc_Vector_string_push(__list_392, "WiimoteSource0 = 1");
    btrc_Vector_string_push(__list_392, "WiimoteSource1 = 0");
    btrc_Vector_string_push(__list_392, "WiimoteSource2 = 0");
    btrc_Vector_string_push(__list_392, "WiimoteSource3 = 0");
    btrc_Vector_string_push(__list_392, "");
    btrc_Vector_string_push(__list_392, "[Interface]");
    btrc_Vector_string_push(__list_392, "ConfirmStop = False");
    btrc_Vector_string_push(__list_392, "UsePanicHandlers = False");
    btrc_Vector_string* lines = __list_392;
    char* __btrc_ret_393 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_393;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

btrc_Vector_string* DolphinEmitter_gamecubePadBody(char* padIdentity) {
    btrc_Vector_string* __list_394 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_394, __btrc_str_track(__btrc_strcat("Device = ", padIdentity)));
    btrc_Vector_string_push(__list_394, "Buttons/A = `Button S`");
    btrc_Vector_string_push(__list_394, "Buttons/B = `Button W`");
    btrc_Vector_string_push(__list_394, "Buttons/X = `Button E`");
    btrc_Vector_string_push(__list_394, "Buttons/Y = `Button N`");
    btrc_Vector_string_push(__list_394, "Buttons/Z = `Shoulder R`|Back");
    btrc_Vector_string_push(__list_394, "Buttons/Start = Start");
    btrc_Vector_string_push(__list_394, "Main Stick/Up = `Axis 1-`");
    btrc_Vector_string_push(__list_394, "Main Stick/Down = `Axis 1+`");
    btrc_Vector_string_push(__list_394, "Main Stick/Left = `Axis 0-`");
    btrc_Vector_string_push(__list_394, "Main Stick/Right = `Axis 0+`");
    btrc_Vector_string_push(__list_394, "C-Stick/Up = `Axis 4-`");
    btrc_Vector_string_push(__list_394, "C-Stick/Down = `Axis 4+`");
    btrc_Vector_string_push(__list_394, "C-Stick/Left = `Axis 3-`");
    btrc_Vector_string_push(__list_394, "C-Stick/Right = `Axis 3+`");
    btrc_Vector_string_push(__list_394, "Triggers/L = `Trigger L`");
    btrc_Vector_string_push(__list_394, "Triggers/R = `Trigger R`");
    btrc_Vector_string_push(__list_394, "Triggers/L-Analog = `Trigger L`");
    btrc_Vector_string_push(__list_394, "Triggers/R-Analog = `Trigger R`");
    btrc_Vector_string_push(__list_394, "D-Pad/Up = `Pad N`");
    btrc_Vector_string_push(__list_394, "D-Pad/Down = `Pad S`");
    btrc_Vector_string_push(__list_394, "D-Pad/Left = `Pad W`");
    btrc_Vector_string_push(__list_394, "D-Pad/Right = `Pad E`");
    btrc_Vector_string_push(__list_394, "Rumble/Motor = Strong");
    btrc_Vector_string_push(__list_394, "Options/Always Connected = True");
    btrc_Vector_string* lines = __list_394;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* DolphinEmitter_gamecubePadFile(char* padIdentity) {
    btrc_Vector_string* __list_395 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_395, "[GCPad1]");
    btrc_Vector_string* lines = __list_395;
    DolphinEmitter_pushAll(lines, DolphinEmitter_gamecubePadBody(padIdentity));
    btrc_Vector_string* __list_396 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_396, "[GCPad2]");
    btrc_Vector_string_push(__list_396, "[GCPad3]");
    btrc_Vector_string_push(__list_396, "[GCPad4]");
    btrc_Vector_string* extraPortSections = __list_396;
    int __n_398 = btrc_Vector_string_iterLen(extraPortSections);
    for (int __i_397 = 0; (__i_397 < __n_398); (__i_397++)) {
        char* portSection = btrc_Vector_string_iterGet(extraPortSections, __i_397);
        btrc_Vector_string_push(lines, portSection);
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("Device = ", padIdentity)));
    }
    char* __btrc_ret_399 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (extraPortSections != NULL) {
        if ((--extraPortSections->__rc) <= 0) {
            btrc_Vector_string_destroy(extraPortSections);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_399;
    if (extraPortSections != NULL) {
        if ((--extraPortSections->__rc) <= 0) {
            btrc_Vector_string_destroy(extraPortSections);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* DolphinEmitter_gamecubePadProfile(char* padIdentity) {
    btrc_Vector_string* __list_400 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_400, "[Profile]");
    btrc_Vector_string* lines = __list_400;
    DolphinEmitter_pushAll(lines, DolphinEmitter_gamecubePadBody(padIdentity));
    char* __btrc_ret_401 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_401;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void DolphinEmitter_pushHotkeyLine(btrc_Vector_string* lines, EmulatorDefinition* definition, Keymap* keymap, char* settingName, char* actionId) {
    if (btrc_Vector_string_contains(definition->input->actions, actionId)) {
        char* chord = Keymap_chordFor(keymap, actionId);
        if (!__btrc_isEmpty(chord)) {
            btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(settingName, " = ")), DolphinEmitter_nativeChord(chord))));
        }
    }
}

char* DolphinEmitter_speedLimitExpression(Keymap* keymap) {
    char* expression = "Tab";
    btrc_Vector_KeymapAction_p1* __iter_402 = keymap->actions;
    int __n_404 = btrc_Vector_KeymapAction_p1_iterLen(__iter_402);
    for (int __i_403 = 0; (__i_403 < __n_404); (__i_403++)) {
        KeymapAction* action = btrc_Vector_KeymapAction_p1_iterGet(__iter_402, __i_403);
        if ((strcmp(ChordSyntax_keyPart(action->chord), "Tab") == 0) && (!__btrc_isEmpty(ChordSyntax_modifierPart(action->chord)))) {
            (expression = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(expression, " & !@(")), action->chord)), ")")));
        }
    }
    return expression;
}

char* DolphinEmitter_hotkeysProfile(EmulatorDefinition* definition, Keymap* keymap, char* pointerIdentity) {
    btrc_Vector_string* __list_405 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_405, "[Profile]");
    btrc_Vector_string* lines = __list_405;
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("Device = ", pointerIdentity)));
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "General/Open", "ui.open");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "General/Toggle Pause", "ui.pause");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "General/Stop", "app.quit");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "General/Toggle Fullscreen", "ui.fullscreen");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "General/Take Screenshot", "ui.screenshot");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "Load State/Load State Slot 1", "state.load");
    DolphinEmitter_pushHotkeyLine(lines, definition, keymap, "Save State/Save State Slot 1", "state.save");
    btrc_Vector_string* __list_406 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_406, __btrc_str_track(__btrc_strcat("Emulation Speed/Disable Emulation Speed Limit = ", DolphinEmitter_speedLimitExpression(keymap))));
    btrc_Vector_string_push(__list_406, "Controller Profile 1/Next Profile = @(Alt+F5)");
    btrc_Vector_string_push(__list_406, "Other State Hotkeys/Undo Load State = F12");
    btrc_Vector_string_push(__list_406, "Other State Hotkeys/Undo Save State = @(Shift+F12)");
    btrc_Vector_string* fixedHotkeys = __list_406;
    DolphinEmitter_pushAll(lines, fixedHotkeys);
    char* __btrc_ret_407 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (fixedHotkeys != NULL) {
        if ((--fixedHotkeys->__rc) <= 0) {
            btrc_Vector_string_destroy(fixedHotkeys);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_407;
    if (fixedHotkeys != NULL) {
        if ((--fixedHotkeys->__rc) <= 0) {
            btrc_Vector_string_destroy(fixedHotkeys);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void DolphinEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    char* padIdentity = SteamInputTarget_deviceIdentity(steamInput, "dolphin_pad");
    char* wiimoteIdentity = SteamInputTarget_deviceIdentity(steamInput, "dolphin_wiimote");
    char* pointerIdentity = SteamInputTarget_deviceIdentity(steamInput, "dolphin_pointer");
    char* configRoot = PathTools_join(PathTools_join(outputRoot, definition->name), "config");
    char* gamecubeProfileDirectory = PathTools_join(configRoot, "Profiles/GCPad");
    char* hotkeysProfileDirectory = PathTools_join(configRoot, "Profiles/Hotkeys");
    char* wiimoteProfileDirectory = PathTools_join(configRoot, "Profiles/Wiimote");
    UnixFileSystem_mkdirp(gamecubeProfileDirectory);
    UnixFileSystem_mkdirp(hotkeysProfileDirectory);
    UnixFileSystem_mkdirp(wiimoteProfileDirectory);
    Path_writeAll(PathTools_join(configRoot, "Dolphin.ini"), DolphinEmitter_dolphinIni());
    Path_writeAll(PathTools_join(configRoot, "GCPadNew.ini"), DolphinEmitter_gamecubePadFile(padIdentity));
    Path_writeAll(PathTools_join(configRoot, "WiimoteNew.ini"), DolphinWiimoteProfiles_wiimoteNewFile(wiimoteIdentity, pointerIdentity));
    Path_writeAll(PathTools_join(gamecubeProfileDirectory, "Steam Deck.ini"), DolphinEmitter_gamecubePadProfile(padIdentity));
    Path_writeAll(PathTools_join(hotkeysProfileDirectory, "Steam Deck.ini"), DolphinEmitter_hotkeysProfile(definition, keymap, pointerIdentity));
    Path_writeAll(PathTools_join(wiimoteProfileDirectory, "Wiimote (SD).ini"), DolphinWiimoteProfiles_wiimoteNunchukProfile(wiimoteIdentity, pointerIdentity));
    Path_writeAll(PathTools_join(wiimoteProfileDirectory, "Wiimote + Classic Controller (SD).ini"), DolphinWiimoteProfiles_wiimoteClassicProfile(wiimoteIdentity, pointerIdentity));
}

void DolphinEmulator_init(DolphinEmulator* self) {
    self->__rc = 1;
}

DolphinEmulator* DolphinEmulator_new(void) {
    DolphinEmulator* self = ((DolphinEmulator*)malloc(sizeof(DolphinEmulator)));
    memset(self, 0, sizeof(DolphinEmulator));
    DolphinEmulator_init(self);
    return self;
}

char* DolphinEmulator_id(DolphinEmulator* self) {
    return "dolphin";
}

void DolphinEmulator_parseExtension(DolphinEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, "emulators/dolphin/emulator.json", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".dolphin")), "dolphin declares no platform extension block");
    }
}

btrc_Vector_string* DolphinEmulator_commandFragments(DolphinEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void DolphinEmulator_emitProfiles(DolphinEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    DolphinEmitter_emitProfiles(definition, steamInput, keymap, outputRoot);
}

void TemplateContext_init(TemplateContext* self) {
    self->__rc = 1;
    (self->project = "");
    (self->emulatorId = "");
    (self->backend = "");
    (self->flatpakId = "");
}

TemplateContext* TemplateContext_new(void) {
    TemplateContext* self = ((TemplateContext*)malloc(sizeof(TemplateContext)));
    memset(self, 0, sizeof(TemplateContext));
    TemplateContext_init(self);
    return self;
}

void TemplateContext_destroy(TemplateContext* self) {
    free(self);
}

char* TemplatePaths_home(void) {
    return Environment_get("SEMU_HOME", Environment_get("HOME", "."));
}

char* TemplatePaths_generatedRoot(char* project) {
    return PathTools_join(project, "src/generated");
}

char* TemplatePaths_runtimeRoot(char* project) {
    return PathTools_join(TemplatePaths_generatedRoot(project), "runtime");
}

char* TemplatePaths_contentRoot(char* project) {
    return PathTools_join(TemplatePaths_runtimeRoot(project), "content");
}

char* TemplatePaths_biosRoot(char* project) {
    return PathTools_join(TemplatePaths_contentRoot(project), "bios");
}

char* TemplatePaths_normalizedRomsRoot(char* romsDirectory) {
    btrc_Vector_string* __list_408 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_408, PathTools_join(romsDirectory, "Emulation/ES-DE/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_408, PathTools_join(romsDirectory, "ES-DE/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_408, PathTools_join(romsDirectory, "Emulation/ROMs"));
    btrc_Vector_string_push(__list_408, PathTools_join(romsDirectory, "ROMs"));
    btrc_Vector_string* candidates = __list_408;
    int __n_410 = btrc_Vector_string_iterLen(candidates);
    for (int __i_409 = 0; (__i_409 < __n_410); (__i_409++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_409);
        if (FileSystem_isDir(candidate)) {
            if (candidates != NULL) {
                if ((--candidates->__rc) <= 0) {
                    btrc_Vector_string_destroy(candidates);
                }
            }
            return candidate;
        }
    }
    if (candidates != NULL) {
        if ((--candidates->__rc) <= 0) {
            btrc_Vector_string_destroy(candidates);
        }
    }
    return romsDirectory;
    if (candidates != NULL) {
        if ((--candidates->__rc) <= 0) {
            btrc_Vector_string_destroy(candidates);
        }
    }
}

char* TemplatePaths_romsRoot(char* project) {
    char* configured = Environment_get("SEMU_ROMS_DIR", "");
    if (!__btrc_isEmpty(configured)) {
        return TemplatePaths_normalizedRomsRoot(configured);
    }
    return PathTools_join(TemplatePaths_contentRoot(project), "ROMs");
}

char* TemplatePaths_stateRoot(TemplateContext* context) {
    char* stateKind = ((strcmp(context->backend, "flatpak") == 0) ? "flatpak-state" : "appimage-state");
    return PathTools_join(PathTools_join(TemplatePaths_runtimeRoot(context->project), stateKind), __btrc_str_track(__btrc_toLower(context->emulatorId)));
}

char* TemplatePaths_emulationRoot(char* project) {
    char* configured = Environment_get("SEMU_EMULATION_ROOT", "");
    if (!__btrc_isEmpty(configured)) {
        return configured;
    }
    char* roms = TemplatePaths_romsRoot(project);
    if (__btrc_endsWith(roms, "/ES-DE/ES-DE/ROMs")) {
        return PathTools_dirname(PathTools_dirname(PathTools_dirname(roms)));
    }
    btrc_Vector_string* __list_411 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_411, "/run/media/deck/SD/Emulation");
    btrc_Vector_string_push(__list_411, "/mnt/SD/Emulation");
    btrc_Vector_string* deckCandidates = __list_411;
    int __n_413 = btrc_Vector_string_iterLen(deckCandidates);
    for (int __i_412 = 0; (__i_412 < __n_413); (__i_412++)) {
        char* candidate = btrc_Vector_string_iterGet(deckCandidates, __i_412);
        if (FileSystem_isDir(candidate)) {
            if (deckCandidates != NULL) {
                if ((--deckCandidates->__rc) <= 0) {
                    btrc_Vector_string_destroy(deckCandidates);
                }
            }
            return candidate;
        }
    }
    char* __btrc_ret_414 = "";
    if (deckCandidates != NULL) {
        if ((--deckCandidates->__rc) <= 0) {
            btrc_Vector_string_destroy(deckCandidates);
        }
    }
    return __btrc_ret_414;
    if (deckCandidates != NULL) {
        if ((--deckCandidates->__rc) <= 0) {
            btrc_Vector_string_destroy(deckCandidates);
        }
    }
}

char* TemplatePaths_flatpakHome(char* flatpakId) {
    if (__btrc_isEmpty(flatpakId)) {
        return "";
    }
    return PathTools_join(PathTools_join(TemplatePaths_home(), ".var/app"), flatpakId);
}

char* TemplatePaths_nixResult(char* project) {
    return PathTools_join(TemplatePaths_generatedRoot(project), "nix/result");
}

char* TemplatePaths_assetRoot(char* project) {
    return Environment_get("SEMU_ASSET_ROOT", project);
}

char* TemplatePaths_resolveEnvironmentTokens(char* value) {
    char* result = value;
    while (__btrc_strContains(result, "${env:")) {
        int openIndex = __btrc_indexOf(result, "${env:");
        char* after = __btrc_str_track(__btrc_substring(result, (openIndex + 6), ((int)strlen(result))));
        int closeIndex = __btrc_indexOf(after, "}");
        if (closeIndex < 0) {
            return result;
        }
        char* name = __btrc_str_track(__btrc_substring(after, 0, closeIndex));
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_substring(result, 0, openIndex)), Environment_get(name, ""))), __btrc_str_track(__btrc_substring(after, (closeIndex + 1), ((int)strlen(after)))))));
    }
    return result;
}

char* TemplatePaths_resolve(char* value, TemplateContext* context) {
    char* result = TemplatePaths_resolveEnvironmentTokens(value);
    (result = __btrc_str_track(__btrc_replace(result, "${home}", TemplatePaths_home())));
    (result = __btrc_str_track(__btrc_replace(result, "${project}", context->project)));
    (result = __btrc_str_track(__btrc_replace(result, "${roms}", TemplatePaths_romsRoot(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${state_root}", TemplatePaths_stateRoot(context))));
    (result = __btrc_str_track(__btrc_replace(result, "${emulation_root}", TemplatePaths_emulationRoot(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${flatpak_home}", TemplatePaths_flatpakHome(context->flatpakId))));
    (result = __btrc_str_track(__btrc_replace(result, "${nix_result}", TemplatePaths_nixResult(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${asset_root}", TemplatePaths_assetRoot(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_roms}", TemplatePaths_romsRoot(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_bios}", TemplatePaths_biosRoot(context->project))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_saves}", PathTools_join(TemplatePaths_contentRoot(context->project), "saves"))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_states}", PathTools_join(TemplatePaths_contentRoot(context->project), "states"))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_screenshots}", PathTools_join(TemplatePaths_contentRoot(context->project), "screenshots"))));
    (result = __btrc_str_track(__btrc_replace(result, "${paths.project_gamelists}", PathTools_join(TemplatePaths_contentRoot(context->project), "gamelists"))));
    return result;
}

btrc_Vector_string* TemplatePaths_resolveAll(btrc_Vector_string* values, TemplateContext* context) {
    btrc_Vector_string* resolved = btrc_Vector_string_new();
    int __n_416 = btrc_Vector_string_iterLen(values);
    for (int __i_415 = 0; (__i_415 < __n_416); (__i_415++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_415);
        btrc_Vector_string_push(resolved, TemplatePaths_resolve(value, context));
    }
    return resolved;
    if (resolved != NULL) {
        if ((--resolved->__rc) <= 0) {
            btrc_Vector_string_destroy(resolved);
        }
    }
}

char* Pcsx2Rendering_automaticRendererValue(void) {
    return "-1";
}

char* Pcsx2Rendering_rendererKey(GraphicsPolicy* graphics) {
    if (graphics->present && (!__btrc_isEmpty(graphics->key))) {
        return graphics->key;
    }
    return "Renderer";
}

btrc_Vector_string* Pcsx2Rendering_graphicsSectionLines(GraphicsPolicy* graphics) {
    btrc_Vector_string* __list_417 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_417, "[EmuCore/GS]");
    btrc_Vector_string_push(__list_417, "AspectRatio = Auto 4:3/3:2");
    btrc_Vector_string_push(__list_417, "IntegerScaling = true");
    btrc_Vector_string_push(__list_417, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(Pcsx2Rendering_rendererKey(graphics), " = ")), Pcsx2Rendering_automaticRendererValue())));
    btrc_Vector_string_push(__list_417, "upscale_multiplier = 1");
    btrc_Vector_string_push(__list_417, "filter = 0");
    btrc_Vector_string_push(__list_417, "VsyncEnable = true");
    btrc_Vector_string* lines = __list_417;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void Pcsx2Emitter_pushAll(btrc_Vector_string* lines, btrc_Vector_string* additions) {
    int __n_419 = btrc_Vector_string_iterLen(additions);
    for (int __i_418 = 0; (__i_418 < __n_419); (__i_418++)) {
        char* addition = btrc_Vector_string_iterGet(additions, __i_418);
        btrc_Vector_string_push(lines, addition);
    }
}

char* Pcsx2Emitter_hotkeyChord(char* chord) {
    btrc_Vector_string* parts = btrc_Vector_string_new();
    btrc_Vector_string* __iter_420 = ChordTranslator_modifierList(chord);
    int __n_422 = btrc_Vector_string_iterLen(__iter_420);
    for (int __i_421 = 0; (__i_421 < __n_422); (__i_421++)) {
        char* modifier = btrc_Vector_string_iterGet(__iter_420, __i_421);
        if (strcmp(modifier, "Ctrl") == 0) {
            btrc_Vector_string_push(parts, "Keyboard/Control");
        } else {
            btrc_Vector_string_push(parts, __btrc_str_track(__btrc_strcat("Keyboard/", modifier)));
        }
    }
    char* keyName = ChordTranslator_keyPart(chord);
    if (strcmp(keyName, "+") == 0) {
        (keyName = "Plus");
    }
    if (strcmp(keyName, "-") == 0) {
        (keyName = "Minus");
    }
    btrc_Vector_string_push(parts, __btrc_str_track(__btrc_strcat("Keyboard/", keyName)));
    char* __btrc_ret_423 = btrc_Vector_string_join(parts, " & ");
    if (parts != NULL) {
        if ((--parts->__rc) <= 0) {
            btrc_Vector_string_destroy(parts);
        }
    }
    return __btrc_ret_423;
    if (parts != NULL) {
        if ((--parts->__rc) <= 0) {
            btrc_Vector_string_destroy(parts);
        }
    }
}

btrc_Vector_string* Pcsx2Emitter_userInterfaceSectionLines(void) {
    btrc_Vector_string* __list_424 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_424, "[UI]");
    btrc_Vector_string_push(__list_424, "SettingsVersion = 1");
    btrc_Vector_string_push(__list_424, "InhibitScreensaver = true");
    btrc_Vector_string_push(__list_424, "ConfirmShutdown = false");
    btrc_Vector_string_push(__list_424, "StartPaused = false");
    btrc_Vector_string_push(__list_424, "PauseOnFocusLoss = false");
    btrc_Vector_string_push(__list_424, "StartFullscreen = true");
    btrc_Vector_string_push(__list_424, "DoubleClickTogglesFullscreen = true");
    btrc_Vector_string_push(__list_424, "HideMouseCursor = true");
    btrc_Vector_string_push(__list_424, "RenderToSeparateWindow = false");
    btrc_Vector_string_push(__list_424, "HideMainWindowWhenRunning = true");
    btrc_Vector_string_push(__list_424, "DisableWindowResize = false");
    btrc_Vector_string_push(__list_424, "PreferEnglishGameList = false");
    btrc_Vector_string_push(__list_424, "Theme = darkfusionblue");
    btrc_Vector_string_push(__list_424, "SetupWizardIncomplete = false");
    btrc_Vector_string* lines = __list_424;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

btrc_Vector_string* Pcsx2Emitter_foldersSectionLines(char* biosDirectory) {
    btrc_Vector_string* __list_425 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_425, "[Folders]");
    btrc_Vector_string_push(__list_425, __btrc_str_track(__btrc_strcat("Bios = ", biosDirectory)));
    btrc_Vector_string_push(__list_425, "Snapshots = snaps");
    btrc_Vector_string_push(__list_425, "Savestates = sstates");
    btrc_Vector_string_push(__list_425, "MemoryCards = memcards");
    btrc_Vector_string_push(__list_425, "Logs = logs");
    btrc_Vector_string_push(__list_425, "Cheats = cheats");
    btrc_Vector_string_push(__list_425, "Patches = patches");
    btrc_Vector_string_push(__list_425, "UserResources = resources");
    btrc_Vector_string_push(__list_425, "Cache = cache");
    btrc_Vector_string_push(__list_425, "Textures = textures");
    btrc_Vector_string_push(__list_425, "InputProfiles = inputprofiles");
    btrc_Vector_string* lines = __list_425;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

btrc_Vector_string* Pcsx2Emitter_coreSectionLines(void) {
    btrc_Vector_string* __list_426 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_426, "[EmuCore]");
    btrc_Vector_string_push(__list_426, "EnablePatches = true");
    btrc_Vector_string_push(__list_426, "EnableWideScreenPatches = false");
    btrc_Vector_string_push(__list_426, "EnableFastBoot = true");
    btrc_Vector_string_push(__list_426, "UseSavestateSelector = true");
    btrc_Vector_string_push(__list_426, "McdFolderAutoManage = true");
    btrc_Vector_string* lines = __list_426;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

btrc_Vector_string* Pcsx2Emitter_padSectionsLines(char* padIdentity) {
    btrc_Vector_string* __list_427 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_427, "[Pad]");
    btrc_Vector_string_push(__list_427, "UseProfileHotkeyBindings = true");
    btrc_Vector_string_push(__list_427, "MultitapPort1 = false");
    btrc_Vector_string_push(__list_427, "MultitapPort2 = false");
    btrc_Vector_string_push(__list_427, "");
    btrc_Vector_string_push(__list_427, "[InputSources]");
    btrc_Vector_string_push(__list_427, "Keyboard = true");
    btrc_Vector_string_push(__list_427, "Mouse = true");
    btrc_Vector_string_push(__list_427, "SDL = true");
    btrc_Vector_string_push(__list_427, "DInput = false");
    btrc_Vector_string_push(__list_427, "XInput = false");
    btrc_Vector_string_push(__list_427, "");
    btrc_Vector_string_push(__list_427, "[Pad1]");
    btrc_Vector_string_push(__list_427, "Type = DualShock2");
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Up = ", padIdentity)), "/DPadUp | Keyboard/Up")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Right = ", padIdentity)), "/DPadRight | Keyboard/Right")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Down = ", padIdentity)), "/DPadDown | Keyboard/Down")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Left = ", padIdentity)), "/DPadLeft | Keyboard/Left")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Triangle = ", padIdentity)), "/Y | Keyboard/I")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Circle = ", padIdentity)), "/B | Keyboard/X")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Cross = ", padIdentity)), "/A | Keyboard/Z")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Square = ", padIdentity)), "/X | Keyboard/A")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Select = ", padIdentity)), "/Back | Keyboard/Backspace")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("Start = ", padIdentity)), "/Start | Keyboard/Enter")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("L1 = ", padIdentity)), "/LeftShoulder")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("L2 = ", padIdentity)), "/+LeftTrigger")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("R1 = ", padIdentity)), "/RightShoulder")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("R2 = ", padIdentity)), "/+RightTrigger")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("L3 = ", padIdentity)), "/LeftStick")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("R3 = ", padIdentity)), "/RightStick")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("LUp = ", padIdentity)), "/-LeftY")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("LRight = ", padIdentity)), "/+LeftX")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("LDown = ", padIdentity)), "/+LeftY")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("LLeft = ", padIdentity)), "/-LeftX")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("RUp = ", padIdentity)), "/-RightY")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("RRight = ", padIdentity)), "/+RightX")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("RDown = ", padIdentity)), "/+RightY")));
    btrc_Vector_string_push(__list_427, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("RLeft = ", padIdentity)), "/-RightX")));
    btrc_Vector_string_push(__list_427, "");
    btrc_Vector_string_push(__list_427, "[Pad2]");
    btrc_Vector_string_push(__list_427, "Type = None");
    btrc_Vector_string* lines = __list_427;
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void Pcsx2Emitter_pushHotkeyLine(btrc_Vector_string* lines, EmulatorDefinition* definition, Keymap* keymap, char* settingName, char* actionId) {
    if (btrc_Vector_string_contains(definition->input->actions, actionId)) {
        char* chord = Keymap_chordFor(keymap, actionId);
        if (!__btrc_isEmpty(chord)) {
            btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(settingName, " = ")), Pcsx2Emitter_hotkeyChord(chord))));
        }
    }
}

btrc_Vector_string* Pcsx2Emitter_hotkeysSectionLines(EmulatorDefinition* definition, Keymap* keymap) {
    btrc_Vector_string* __list_428 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_428, "[Hotkeys]");
    btrc_Vector_string* lines = __list_428;
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "OpenPauseMenu", "ui.menu");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "TogglePause", "ui.pause");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "ToggleFullscreen", "ui.fullscreen");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "PreviousSaveStateSlot", "state.prev");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "NextSaveStateSlot", "state.next");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "SaveStateToSlot", "state.save");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "LoadStateFromSlot", "state.load");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "Screenshot", "ui.screenshot");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "ToggleTurbo", "speed.fast");
    Pcsx2Emitter_pushHotkeyLine(lines, definition, keymap, "ToggleSlowMotion", "speed.rewind");
    return lines;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* Pcsx2Emitter_projectRootFor(char* outputRoot) {
    char* packagingRoot = PathTools_dirname(PathTools_dirname(outputRoot));
    char* generatedRoot = PathTools_dirname(packagingRoot);
    return PathTools_dirname(PathTools_dirname(generatedRoot));
}

char* Pcsx2Emitter_biosDirectory(EmulatorPlatform* platform, char* outputRoot) {
    TemplateContext* context = TemplateContext_new();
    (context->project = Pcsx2Emitter_projectRootFor(outputRoot));
    btrc_Vector_string* __iter_429 = platform->firmware->fallbackDirectories;
    int __n_431 = btrc_Vector_string_iterLen(__iter_429);
    for (int __i_430 = 0; (__i_430 < __n_431); (__i_430++)) {
        char* fallbackDirectory = btrc_Vector_string_iterGet(__iter_429, __i_430);
        char* resolved = TemplatePaths_resolve(fallbackDirectory, context);
        if (!__btrc_strContains(resolved, "${")) {
            if (context != NULL) {
                if ((--context->__rc) <= 0) {
                    TemplateContext_destroy(context);
                }
            }
            return resolved;
        }
    }
    char* __btrc_ret_432 = TemplatePaths_biosRoot(context->project);
    if (context != NULL) {
        if ((--context->__rc) <= 0) {
            TemplateContext_destroy(context);
        }
    }
    return __btrc_ret_432;
    if (context != NULL) {
        if ((--context->__rc) <= 0) {
            TemplateContext_destroy(context);
        }
    }
}

EmulatorPlatform* Pcsx2Emitter_profilePlatform(EmulatorDefinition* definition) {
    if (EmulatorDefinition_hasOperatingSystem(definition, "linux")) {
        return EmulatorDefinition_platformFor(definition, "linux");
    }
    if (btrc_Vector_EmulatorPlatform_p1_size(definition->platforms) > 0) {
        return btrc_Vector_EmulatorPlatform_p1_get(definition->platforms, 0);
    }
    return EmulatorPlatform_new();
}

char* Pcsx2Emitter_baseConfigurationText(EmulatorPlatform* platform, char* padIdentity, char* biosDirectory) {
    btrc_Vector_string* lines = btrc_Vector_string_new();
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_userInterfaceSectionLines());
    btrc_Vector_string_push(lines, "");
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_foldersSectionLines(biosDirectory));
    btrc_Vector_string_push(lines, "");
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_coreSectionLines());
    btrc_Vector_string_push(lines, "");
    Pcsx2Emitter_pushAll(lines, Pcsx2Rendering_graphicsSectionLines(platform->graphics));
    btrc_Vector_string_push(lines, "");
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_padSectionsLines(padIdentity));
    char* __btrc_ret_433 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_433;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

char* Pcsx2Emitter_inputProfileText(EmulatorDefinition* definition, Keymap* keymap, char* padIdentity) {
    btrc_Vector_string* lines = btrc_Vector_string_new();
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_padSectionsLines(padIdentity));
    Pcsx2Emitter_pushAll(lines, Pcsx2Emitter_hotkeysSectionLines(definition, keymap));
    char* __btrc_ret_434 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_434;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void Pcsx2Emitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    EmulatorPlatform* platform = Pcsx2Emitter_profilePlatform(definition);
    char* padIdentity = SteamInputTarget_deviceIdentity(steamInput, "pcsx2_pad");
    char* configRoot = PathTools_join(PathTools_join(outputRoot, definition->name), "config");
    char* configurationDirectory = PathTools_join(configRoot, "inis");
    char* inputProfileDirectory = PathTools_join(configRoot, "inputprofiles");
    UnixFileSystem_mkdirp(configurationDirectory);
    UnixFileSystem_mkdirp(inputProfileDirectory);
    Path_writeAll(PathTools_join(configurationDirectory, "PCSX2.ini"), Pcsx2Emitter_baseConfigurationText(platform, padIdentity, Pcsx2Emitter_biosDirectory(platform, outputRoot)));
    Path_writeAll(PathTools_join(inputProfileDirectory, "Steam Deck.ini"), Pcsx2Emitter_inputProfileText(definition, keymap, padIdentity));
}

void Pcsx2Emulator_init(Pcsx2Emulator* self) {
    self->__rc = 1;
}

Pcsx2Emulator* Pcsx2Emulator_new(void) {
    Pcsx2Emulator* self = ((Pcsx2Emulator*)malloc(sizeof(Pcsx2Emulator)));
    memset(self, 0, sizeof(Pcsx2Emulator));
    Pcsx2Emulator_init(self);
    return self;
}

char* Pcsx2Emulator_id(Pcsx2Emulator* self) {
    return "pcsx2";
}

void Pcsx2Emulator_parseExtension(Pcsx2Emulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, "emulators/pcsx2/emulator.json", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".pcsx2")), "pcsx2 declares no platform extension block");
    }
}

btrc_Vector_string* Pcsx2Emulator_commandFragments(Pcsx2Emulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void Pcsx2Emulator_emitProfiles(Pcsx2Emulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    Pcsx2Emitter_emitProfiles(definition, steamInput, keymap, outputRoot);
}

char* CemuEmitter_stagedRootMarker(void) {
    return "generated/packaging/emulators/profiles/";
}

char* CemuEmitter_stagedRelativePath(char* declaredPath) {
    char* marker = CemuEmitter_stagedRootMarker();
    int markerIndex = __btrc_indexOf(declaredPath, marker);
    if (markerIndex < 0) {
        return "";
    }
    return __btrc_str_track(__btrc_substring(declaredPath, (markerIndex + ((int)strlen(marker))), ((int)strlen(declaredPath))));
}

btrc_Vector_string* CemuEmitter_stagedSeedDirectories(EmulatorDefinition* definition) {
    btrc_Vector_string* directories = btrc_Vector_string_new();
    btrc_Vector_EmulatorPlatform_p1* __iter_435 = definition->platforms;
    int __n_437 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_435);
    for (int __i_436 = 0; (__i_436 < __n_437); (__i_436++)) {
        EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_435, __i_436);
        StatePolicy* state = platform->state;
        btrc_Vector_StateSeed_p1* __iter_438 = state->seeds;
        int __n_440 = btrc_Vector_StateSeed_p1_iterLen(__iter_438);
        for (int __i_439 = 0; (__i_439 < __n_440); (__i_439++)) {
            StateSeed* seed = btrc_Vector_StateSeed_p1_iterGet(__iter_438, __i_439);
            btrc_Vector_string* sources = seed->sources;
            int __n_442 = btrc_Vector_string_iterLen(sources);
            for (int __i_441 = 0; (__i_441 < __n_442); (__i_441++)) {
                char* source = btrc_Vector_string_iterGet(sources, __i_441);
                char* relativeFile = CemuEmitter_stagedRelativePath(source);
                if (!__btrc_isEmpty(relativeFile)) {
                    char* relativeDirectory = PathTools_dirname(relativeFile);
                    if (!btrc_Vector_string_contains(directories, relativeDirectory)) {
                        btrc_Vector_string_push(directories, relativeDirectory);
                    }
                }
            }
        }
        FirmwarePolicy* firmware = platform->firmware;
        btrc_Vector_string* __iter_443 = firmware->fallbackDirectories;
        int __n_445 = btrc_Vector_string_iterLen(__iter_443);
        for (int __i_444 = 0; (__i_444 < __n_445); (__i_444++)) {
            char* fallbackDirectory = btrc_Vector_string_iterGet(__iter_443, __i_444);
            char* relativeDirectory = CemuEmitter_stagedRelativePath(fallbackDirectory);
            if (!__btrc_isEmpty(relativeDirectory)) {
                if (!btrc_Vector_string_contains(directories, relativeDirectory)) {
                    btrc_Vector_string_push(directories, relativeDirectory);
                }
            }
        }
    }
    return directories;
    if (directories != NULL) {
        if ((--directories->__rc) <= 0) {
            btrc_Vector_string_destroy(directories);
        }
    }
}

char* CemuEmitter_controllerProfileXml(SteamInputTarget* steamInput) {
    char* controllerApi = SteamInputTarget_deviceIdentity(steamInput, "cemu_api");
    char* controllerDisplayName = SteamInputTarget_deviceIdentity(steamInput, "cemu_display_name");
    char* profileName = SteamInputTarget_deviceIdentity(steamInput, "cemu_profile");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", "<emulated_controller>\n")), "\t<type>Wii U GamePad</type>\n")), "\t<profile>")), profileName)), "</profile>\n")), "\t<controller>\n")), "\t\t<api>")), controllerApi)), "</api>\n")), "\t\t<display_name>")), controllerDisplayName)), "</display_name>\n")), "\t\t<rumble>0</rumble>\n")), "\t</controller>\n")), "</emulated_controller>\n"));
}

char* CemuEmitter_controllerProfilePath(EmulatorDefinition* definition, SteamInputTarget* steamInput, char* outputRoot) {
    char* profileName = SteamInputTarget_deviceIdentity(steamInput, "cemu_profile");
    return PathTools_join(outputRoot, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(definition->name, "/config/controllerProfiles/")), profileName)), ".xml")));
}

void CemuEmitter_emit(EmulatorDefinition* definition, SteamInputTarget* steamInput, char* outputRoot) {
    char* profilePath = CemuEmitter_controllerProfilePath(definition, steamInput, outputRoot);
    UnixFileSystem_mkdirp(PathTools_dirname(profilePath));
    Path_writeAll(profilePath, CemuEmitter_controllerProfileXml(steamInput));
    btrc_Vector_string* seedDirectories = CemuEmitter_stagedSeedDirectories(definition);
    int __n_447 = btrc_Vector_string_iterLen(seedDirectories);
    for (int __i_446 = 0; (__i_446 < __n_447); (__i_446++)) {
        char* seedDirectory = btrc_Vector_string_iterGet(seedDirectories, __i_446);
        UnixFileSystem_mkdirp(PathTools_join(outputRoot, seedDirectory));
    }
}

void CemuEmulator_init(CemuEmulator* self) {
    self->__rc = 1;
}

CemuEmulator* CemuEmulator_new(void) {
    CemuEmulator* self = ((CemuEmulator*)malloc(sizeof(CemuEmulator)));
    memset(self, 0, sizeof(CemuEmulator));
    CemuEmulator_init(self);
    return self;
}

char* CemuEmulator_id(CemuEmulator* self) {
    return "cemu";
}

char* CemuEmulator_contractFile(CemuEmulator* self) {
    return "emulators/cemu/emulator.json";
}

void CemuEmulator_parseExtension(CemuEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, CemuEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), CemuEmulator_id(self))), "unexpected extension block: this emulator declares none");
    }
}

btrc_Vector_string* CemuEmulator_commandFragments(CemuEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void CemuEmulator_emitProfiles(CemuEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    CemuEmitter_emit(definition, steamInput, outputRoot);
}

char* RyujinxEmitter_textField(char* fieldKey, char* fieldValue) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", fieldKey)), "\": \"")), fieldValue)), "\""));
}

char* RyujinxEmitter_literalField(char* fieldKey, char* literalValue) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", fieldKey)), "\": ")), literalValue));
}

char* RyujinxEmitter_booleanField(char* fieldKey, bool fieldValue) {
    if (fieldValue) {
        return RyujinxEmitter_literalField(fieldKey, "true");
    }
    return RyujinxEmitter_literalField(fieldKey, "false");
}

char* RyujinxEmitter_objectText(btrc_Vector_string* fields) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", btrc_Vector_string_join(fields, ", "))), "}"));
}

char* RyujinxEmitter_objectField(char* fieldKey, btrc_Vector_string* fields) {
    return RyujinxEmitter_literalField(fieldKey, RyujinxEmitter_objectText(fields));
}

btrc_Vector_string* RyujinxEmitter_leftJoyconStickFields(void) {
    btrc_Vector_string* __list_448 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_448, RyujinxEmitter_textField("joystick", "Left"));
    btrc_Vector_string_push(__list_448, RyujinxEmitter_booleanField("invert_stick_x", false));
    btrc_Vector_string_push(__list_448, RyujinxEmitter_booleanField("invert_stick_y", false));
    btrc_Vector_string_push(__list_448, RyujinxEmitter_textField("stick_button", "LeftStick"));
    btrc_Vector_string* fields = __list_448;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

btrc_Vector_string* RyujinxEmitter_rightJoyconStickFields(void) {
    btrc_Vector_string* __list_449 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_449, RyujinxEmitter_textField("joystick", "Right"));
    btrc_Vector_string_push(__list_449, RyujinxEmitter_booleanField("invert_stick_x", false));
    btrc_Vector_string_push(__list_449, RyujinxEmitter_booleanField("invert_stick_y", false));
    btrc_Vector_string_push(__list_449, RyujinxEmitter_textField("stick_button", "RightStick"));
    btrc_Vector_string* fields = __list_449;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

btrc_Vector_string* RyujinxEmitter_motionFields(void) {
    btrc_Vector_string* __list_450 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_450, RyujinxEmitter_literalField("slot", "0"));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_literalField("alt_slot", "0"));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_booleanField("mirror_input", false));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_textField("motion_backend", "CemuHook"));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_literalField("sensitivity", "100"));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_literalField("gyro_deadzone", "1"));
    btrc_Vector_string_push(__list_450, RyujinxEmitter_booleanField("enable_motion", false));
    btrc_Vector_string* fields = __list_450;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

btrc_Vector_string* RyujinxEmitter_rumbleFields(void) {
    btrc_Vector_string* __list_451 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_451, RyujinxEmitter_literalField("strong_rumble", "1"));
    btrc_Vector_string_push(__list_451, RyujinxEmitter_literalField("weak_rumble", "1"));
    btrc_Vector_string_push(__list_451, RyujinxEmitter_booleanField("enable_rumble", true));
    btrc_Vector_string* fields = __list_451;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

btrc_Vector_string* RyujinxEmitter_leftJoyconFields(void) {
    btrc_Vector_string* __list_452 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("button_minus", "Back"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("button_l", "LeftShoulder"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("button_zl", "LeftTrigger"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("dpad_up", "DpadUp"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("dpad_down", "DpadDown"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("dpad_left", "DpadLeft"));
    btrc_Vector_string_push(__list_452, RyujinxEmitter_textField("dpad_right", "DpadRight"));
    btrc_Vector_string* fields = __list_452;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

btrc_Vector_string* RyujinxEmitter_rightJoyconFields(void) {
    btrc_Vector_string* __list_453 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_plus", "Start"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_r", "RightShoulder"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_zr", "RightTrigger"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_x", "Y"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_b", "A"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_y", "X"));
    btrc_Vector_string_push(__list_453, RyujinxEmitter_textField("button_a", "B"));
    btrc_Vector_string* fields = __list_453;
    return fields;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

char* RyujinxEmitter_controllerProfileText(char* controllerIdentity) {
    btrc_Vector_string* fields = btrc_Vector_string_new();
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("left_joycon_stick", RyujinxEmitter_leftJoyconStickFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("right_joycon_stick", RyujinxEmitter_rightJoyconStickFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("motion", RyujinxEmitter_motionFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("rumble", RyujinxEmitter_rumbleFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("left_joycon", RyujinxEmitter_leftJoyconFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_objectField("right_joycon", RyujinxEmitter_rightJoyconFields()));
    btrc_Vector_string_push(fields, RyujinxEmitter_literalField("version", "1"));
    btrc_Vector_string_push(fields, RyujinxEmitter_textField("backend", "GamepadSDL2"));
    btrc_Vector_string_push(fields, RyujinxEmitter_textField("id", controllerIdentity));
    btrc_Vector_string_push(fields, RyujinxEmitter_textField("controller_type", "ProController"));
    btrc_Vector_string_push(fields, RyujinxEmitter_textField("player_index", "Player1"));
    char* __btrc_ret_454 = __btrc_str_track(__btrc_strcat(RyujinxEmitter_objectText(fields), "\n"));
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
    return __btrc_ret_454;
    if (fields != NULL) {
        if ((--fields->__rc) <= 0) {
            btrc_Vector_string_destroy(fields);
        }
    }
}

char* RyujinxEmitter_controllerIdentityKey(char* emulatorId) {
    return __btrc_str_track(__btrc_strcat(emulatorId, "_controller_id"));
}

char* RyujinxEmitter_controllerProfileRelativePath(char* emulatorName) {
    return __btrc_str_track(__btrc_strcat(emulatorName, "/config/profiles/controller/Steam Virtual Controller.json"));
}

void RyujinxEmulator_init(RyujinxEmulator* self) {
    self->__rc = 1;
}

RyujinxEmulator* RyujinxEmulator_new(void) {
    RyujinxEmulator* self = ((RyujinxEmulator*)malloc(sizeof(RyujinxEmulator)));
    memset(self, 0, sizeof(RyujinxEmulator));
    RyujinxEmulator_init(self);
    return self;
}

char* RyujinxEmulator_id(RyujinxEmulator* self) {
    return "ryujinx";
}

char* RyujinxEmulator_contractFile(RyujinxEmulator* self) {
    return "emulators/ryujinx/emulator.json";
}

void RyujinxEmulator_parseExtension(RyujinxEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, RyujinxEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), RyujinxEmulator_id(self))), "this emulator defines no extension block");
    }
}

btrc_Vector_string* RyujinxEmulator_commandFragments(RyujinxEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void RyujinxEmulator_emitProfiles(RyujinxEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    char* controllerIdentity = SteamInputTarget_deviceIdentity(steamInput, RyujinxEmitter_controllerIdentityKey(RyujinxEmulator_id(self)));
    char* profilePath = PathTools_join(outputRoot, RyujinxEmitter_controllerProfileRelativePath(definition->name));
    UnixFileSystem_mkdirp(PathTools_dirname(profilePath));
    Path_writeAll(profilePath, RyujinxEmitter_controllerProfileText(controllerIdentity));
}

void PpssppEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
}

void PpssppEmulator_init(PpssppEmulator* self) {
    self->__rc = 1;
}

PpssppEmulator* PpssppEmulator_new(void) {
    PpssppEmulator* self = ((PpssppEmulator*)malloc(sizeof(PpssppEmulator)));
    memset(self, 0, sizeof(PpssppEmulator));
    PpssppEmulator_init(self);
    return self;
}

char* PpssppEmulator_id(PpssppEmulator* self) {
    return "ppsspp";
}

char* PpssppEmulator_contractFile(PpssppEmulator* self) {
    return "emulators/ppsspp/emulator.json";
}

void PpssppEmulator_parseExtension(PpssppEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, PpssppEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), PpssppEmulator_id(self))), "ppsspp declares no extension block grammar");
    }
}

btrc_Vector_string* PpssppEmulator_commandFragments(PpssppEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void PpssppEmulator_emitProfiles(PpssppEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    PpssppEmitter_emitProfiles(definition, steamInput, keymap, outputRoot);
}

void FlycastEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
}

void FlycastEmulator_init(FlycastEmulator* self) {
    self->__rc = 1;
}

FlycastEmulator* FlycastEmulator_new(void) {
    FlycastEmulator* self = ((FlycastEmulator*)malloc(sizeof(FlycastEmulator)));
    memset(self, 0, sizeof(FlycastEmulator));
    FlycastEmulator_init(self);
    return self;
}

char* FlycastEmulator_id(FlycastEmulator* self) {
    return "flycast";
}

char* FlycastEmulator_contractFile(FlycastEmulator* self) {
    return "emulators/flycast/emulator.json";
}

void FlycastEmulator_parseExtension(FlycastEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, FlycastEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), FlycastEmulator_id(self))), __btrc_str_track(__btrc_strcat("flycast declares no extension block grammar; remove it or ", "teach FlycastEmulator.parseExtension its schema")));
    }
}

btrc_Vector_string* FlycastEmulator_commandFragments(FlycastEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void FlycastEmulator_emitProfiles(FlycastEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    FlycastEmitter_emitProfiles(definition, steamInput, keymap, outputRoot);
}

char* AzaharRendering_graphicsApiValue(GraphicsPolicy* graphics, char* graphicsApi) {
    if (strcmp(graphicsApi, "opengl") == 0) {
        return graphics->valueOpenGl;
    }
    if (strcmp(graphicsApi, "vulkan") == 0) {
        return graphics->valueVulkan;
    }
    return "";
}

char* AzaharRendering_overlayGraphicsValue(GraphicsPolicy* graphics) {
    return AzaharRendering_graphicsApiValue(graphics, graphics->overlayApi);
}

char* AzaharEmitter_configurationFileName(GraphicsPolicy* graphics) {
    return PathTools_basename(graphics->file);
}

char* AzaharEmitter_configurationDirectoryName(GraphicsPolicy* graphics) {
    return PathTools_basename(PathTools_dirname(graphics->file));
}

char* AzaharEmitter_profileRelativePath(EmulatorDefinition* definition, EmulatorPlatform* platform) {
    char* configDirectory = PathTools_join(definition->name, "config");
    return PathTools_join(configDirectory, AzaharEmitter_configurationFileName(platform->graphics));
}

char* AzaharEmitter_runtimeDataRoot(char* outputRoot, char* emulatorId) {
    char* packagingRoot = PathTools_dirname(PathTools_dirname(outputRoot));
    char* generatedRoot = PathTools_dirname(packagingRoot);
    char* stateRoot = PathTools_join(generatedRoot, "runtime/appimage-state");
    return PathTools_join(PathTools_join(stateRoot, emulatorId), "data");
}

void AzaharEmitter_pushSetting(btrc_Vector_string* lines, char* key, char* value, bool valueIsQtDefault) {
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(key, "=")), value)));
    if (valueIsQtDefault) {
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(key, "\\default=true")));
    } else {
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(key, "\\default=false")));
    }
}

char* AzaharEmitter_qtConfigurationText(char* dataRoot, GraphicsPolicy* graphics) {
    char* azaharRoot = PathTools_join(dataRoot, AzaharEmitter_configurationDirectoryName(graphics));
    btrc_Vector_string* lines = btrc_Vector_string_new();
    btrc_Vector_string_push(lines, "[Data%20Storage]");
    AzaharEmitter_pushSetting(lines, "nand_directory", PathTools_join(azaharRoot, "nand/"), false);
    AzaharEmitter_pushSetting(lines, "sdmc_directory", PathTools_join(azaharRoot, "sdmc/"), false);
    AzaharEmitter_pushSetting(lines, "use_custom_storage", "false", true);
    AzaharEmitter_pushSetting(lines, "use_virtual_sd", "true", true);
    btrc_Vector_string_push(lines, "");
    btrc_Vector_string_push(lines, "[Debugging]");
    AzaharEmitter_pushSetting(lines, "renderer_debug", "false", false);
    btrc_Vector_string_push(lines, "");
    btrc_Vector_string_push(lines, "[Renderer]");
    AzaharEmitter_pushSetting(lines, graphics->key, AzaharRendering_overlayGraphicsValue(graphics), false);
    btrc_Vector_string_push(lines, "");
    btrc_Vector_string_push(lines, "[UI]");
    AzaharEmitter_pushSetting(lines, "confirmClose", "false", false);
    AzaharEmitter_pushSetting(lines, "firstStart", "false", false);
    AzaharEmitter_pushSetting(lines, "fullscreen", "true", false);
    AzaharEmitter_pushSetting(lines, "saveStateWarning", "false", false);
    char* __btrc_ret_458 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_458;
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

bool AzaharEmitter_emitPlatformProfile(EmulatorDefinition* definition, EmulatorPlatform* platform, char* outputRoot) {
    char* profilePath = PathTools_join(outputRoot, AzaharEmitter_profileRelativePath(definition, platform));
    char* dataRoot = AzaharEmitter_runtimeDataRoot(outputRoot, definition->id);
    UnixFileSystem_mkdirp(PathTools_dirname(profilePath));
    return Path_writeAll(profilePath, AzaharEmitter_qtConfigurationText(dataRoot, platform->graphics));
}

void AzaharEmulator_init(AzaharEmulator* self) {
    self->__rc = 1;
}

AzaharEmulator* AzaharEmulator_new(void) {
    AzaharEmulator* self = ((AzaharEmulator*)malloc(sizeof(AzaharEmulator)));
    memset(self, 0, sizeof(AzaharEmulator));
    AzaharEmulator_init(self);
    return self;
}

char* AzaharEmulator_id(AzaharEmulator* self) {
    return "azahar";
}

char* AzaharEmulator_contractFile(AzaharEmulator* self) {
    return "emulators/azahar/emulator.json";
}

void AzaharEmulator_parseExtension(AzaharEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, AzaharEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), AzaharEmulator_id(self))), "azahar declares no extension block");
    }
}

btrc_Vector_string* AzaharEmulator_commandFragments(AzaharEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void AzaharEmulator_emitProfiles(AzaharEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    btrc_Vector_EmulatorPlatform_p1* __iter_459 = definition->platforms;
    int __n_461 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_459);
    for (int __i_460 = 0; (__i_460 < __n_461); (__i_460++)) {
        EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_459, __i_460);
        if (platform->graphics->present && (strcmp(platform->graphics->method, "config_file") == 0)) {
            AzaharEmitter_emitPlatformProfile(definition, platform, outputRoot);
        }
    }
}

void MelonDsEmitter_emitProfiles(EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
}

void MelonDsEmulator_init(MelonDsEmulator* self) {
    self->__rc = 1;
}

MelonDsEmulator* MelonDsEmulator_new(void) {
    MelonDsEmulator* self = ((MelonDsEmulator*)malloc(sizeof(MelonDsEmulator)));
    memset(self, 0, sizeof(MelonDsEmulator));
    MelonDsEmulator_init(self);
    return self;
}

char* MelonDsEmulator_id(MelonDsEmulator* self) {
    return "melonds";
}

char* MelonDsEmulator_contractFile(MelonDsEmulator* self) {
    return "emulators/melonds/emulator.json";
}

void MelonDsEmulator_parseExtension(MelonDsEmulator* self, EmulatorPlatform* platform, ParseLog* log) {
    if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, MelonDsEmulator_contractFile(self), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), MelonDsEmulator_id(self))), __btrc_str_track(__btrc_strcat("melonds defines no extension block; remove it or teach ", "MelonDsEmulator.parseExtension its schema")));
    }
}

btrc_Vector_string* MelonDsEmulator_commandFragments(MelonDsEmulator* self, EmulatorBinding* binding, EmulatorPlatform* platform) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void MelonDsEmulator_emitProfiles(MelonDsEmulator* self, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    MelonDsEmitter_emitProfiles(definition, steamInput, keymap, outputRoot);
}

void EmulatorRegistry_parseExtension(char* emulatorId, EmulatorPlatform* platform, ParseLog* log) {
    if (strcmp(emulatorId, RetroArchEmulator_id(RetroArchEmulator_new())) == 0) {
        RetroArchEmulator_parseExtension(RetroArchEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, DolphinEmulator_id(DolphinEmulator_new())) == 0) {
        DolphinEmulator_parseExtension(DolphinEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, Pcsx2Emulator_id(Pcsx2Emulator_new())) == 0) {
        Pcsx2Emulator_parseExtension(Pcsx2Emulator_new(), platform, log);
    } else if (strcmp(emulatorId, CemuEmulator_id(CemuEmulator_new())) == 0) {
        CemuEmulator_parseExtension(CemuEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, RyujinxEmulator_id(RyujinxEmulator_new())) == 0) {
        RyujinxEmulator_parseExtension(RyujinxEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, PpssppEmulator_id(PpssppEmulator_new())) == 0) {
        PpssppEmulator_parseExtension(PpssppEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, FlycastEmulator_id(FlycastEmulator_new())) == 0) {
        FlycastEmulator_parseExtension(FlycastEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, AzaharEmulator_id(AzaharEmulator_new())) == 0) {
        AzaharEmulator_parseExtension(AzaharEmulator_new(), platform, log);
    } else if (strcmp(emulatorId, MelonDsEmulator_id(MelonDsEmulator_new())) == 0) {
        MelonDsEmulator_parseExtension(MelonDsEmulator_new(), platform, log);
    } else if (EmulatorPlatform_hasExtension(platform)) {
        ParseLog_add(log, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("emulators/", emulatorId)), "/emulator.json")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("platforms.", platform->operatingSystem)), ".")), emulatorId)), "no emulator class is registered to validate this extension block");
    }
}

btrc_Vector_string* EmulatorRegistry_commandFragments(char* emulatorId, EmulatorBinding* binding, EmulatorPlatform* platform) {
    if (strcmp(emulatorId, RetroArchEmulator_id(RetroArchEmulator_new())) == 0) {
        return RetroArchEmulator_commandFragments(RetroArchEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, DolphinEmulator_id(DolphinEmulator_new())) == 0) {
        return DolphinEmulator_commandFragments(DolphinEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, Pcsx2Emulator_id(Pcsx2Emulator_new())) == 0) {
        return Pcsx2Emulator_commandFragments(Pcsx2Emulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, CemuEmulator_id(CemuEmulator_new())) == 0) {
        return CemuEmulator_commandFragments(CemuEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, RyujinxEmulator_id(RyujinxEmulator_new())) == 0) {
        return RyujinxEmulator_commandFragments(RyujinxEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, PpssppEmulator_id(PpssppEmulator_new())) == 0) {
        return PpssppEmulator_commandFragments(PpssppEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, FlycastEmulator_id(FlycastEmulator_new())) == 0) {
        return FlycastEmulator_commandFragments(FlycastEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, AzaharEmulator_id(AzaharEmulator_new())) == 0) {
        return AzaharEmulator_commandFragments(AzaharEmulator_new(), binding, platform);
    }
    if (strcmp(emulatorId, MelonDsEmulator_id(MelonDsEmulator_new())) == 0) {
        return MelonDsEmulator_commandFragments(MelonDsEmulator_new(), binding, platform);
    }
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    return fragments;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

void EmulatorRegistry_emitProfiles(char* emulatorId, EmulatorDefinition* definition, SteamInputTarget* steamInput, Keymap* keymap, char* outputRoot) {
    if (strcmp(emulatorId, RetroArchEmulator_id(RetroArchEmulator_new())) == 0) {
        RetroArchEmulator_emitProfiles(RetroArchEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, DolphinEmulator_id(DolphinEmulator_new())) == 0) {
        DolphinEmulator_emitProfiles(DolphinEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, Pcsx2Emulator_id(Pcsx2Emulator_new())) == 0) {
        Pcsx2Emulator_emitProfiles(Pcsx2Emulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, CemuEmulator_id(CemuEmulator_new())) == 0) {
        CemuEmulator_emitProfiles(CemuEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, RyujinxEmulator_id(RyujinxEmulator_new())) == 0) {
        RyujinxEmulator_emitProfiles(RyujinxEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, PpssppEmulator_id(PpssppEmulator_new())) == 0) {
        PpssppEmulator_emitProfiles(PpssppEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, FlycastEmulator_id(FlycastEmulator_new())) == 0) {
        FlycastEmulator_emitProfiles(FlycastEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, AzaharEmulator_id(AzaharEmulator_new())) == 0) {
        AzaharEmulator_emitProfiles(AzaharEmulator_new(), definition, steamInput, keymap, outputRoot);
    } else if (strcmp(emulatorId, MelonDsEmulator_id(MelonDsEmulator_new())) == 0) {
        MelonDsEmulator_emitProfiles(MelonDsEmulator_new(), definition, steamInput, keymap, outputRoot);
    }
}

btrc_Vector_string* EmulatorRegistry_esDeCorePathDirectories(char* emulatorId, EmulatorPlatform* platform) {
    if (strcmp(emulatorId, RetroArchEmulator_id(RetroArchEmulator_new())) == 0) {
        return RetroArchEmulator_esDeCorePathDirectories(RetroArchEmulator_new(), platform);
    }
    btrc_Vector_string* directories = btrc_Vector_string_new();
    return directories;
    if (directories != NULL) {
        if ((--directories->__rc) <= 0) {
            btrc_Vector_string_destroy(directories);
        }
    }
}

void EmulatorParser_parseSessions(EmulatorPlatform* platform, JsonValue* jsonValue, bool isLinux, char* file, char* context, ParseLog* log) {
    if (!isLinux) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".sessions")), "linux-only field");
    }
    JsonValue* sessions = JsonValue_get(jsonValue, "sessions");
    if (!JsonValue_isObject(sessions)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".sessions")), "expected object");
        return;
    }
    btrc_Vector_string* __list_463 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_463, "gamescope");
    btrc_Vector_string_push(__list_463, "desktop");
    btrc_Vector_string* sessionNames = __list_463;
    btrc_Vector_string* __list_464 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_464, "x11");
    btrc_Vector_string_push(__list_464, "wayland");
    btrc_Vector_string* displayServers = __list_464;
    btrc_Vector_string* __iter_465 = JsonValue_keys(sessions);
    int __n_467 = btrc_Vector_string_iterLen(__iter_465);
    for (int __i_466 = 0; (__i_466 < __n_467); (__i_466++)) {
        char* sessionName = btrc_Vector_string_iterGet(__iter_465, __i_466);
        char* sessionContext = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(context, ".sessions.")), sessionName));
        if (!btrc_Vector_string_contains(sessionNames, sessionName)) {
            ParseLog_add(log, file, sessionContext, "unknown session (allowed: gamescope, desktop)");
        }
        JsonValue* session = JsonValue_get(sessions, sessionName);
        if (!JsonValue_isObject(session)) {
            ParseLog_add(log, file, sessionContext, "expected object");
        } else {
            btrc_Vector_string* __list_468 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_468, "display_server");
            btrc_Vector_string_push(__list_468, "display_default");
            btrc_Vector_string_push(__list_468, "overlay");
            btrc_Vector_string* sessionKeys = __list_468;
            JsonContract_checkKeys(session, sessionKeys, file, sessionContext, log);
            SessionOverride* sessionOverride = SessionOverride_new();
            (sessionOverride->id = sessionName);
            (sessionOverride->displayServer = JsonContract_optionalString(session, "display_server", "", file, sessionContext, log));
            JsonContract_checkEnum(sessionOverride->displayServer, displayServers, file, __btrc_str_track(__btrc_strcat(sessionContext, ".display_server")), log);
            (sessionOverride->displayFallback = JsonContract_optionalString(session, "display_default", "", file, sessionContext, log));
            (sessionOverride->overlay = JsonContract_optionalString(session, "overlay", "", file, sessionContext, log));
            btrc_Vector_string* __list_469 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_469, "gamescope_external");
            btrc_Vector_string_push(__list_469, "none");
            btrc_Vector_string* overlays = __list_469;
            JsonContract_checkEnum(sessionOverride->overlay, overlays, file, __btrc_str_track(__btrc_strcat(sessionContext, ".overlay")), log);
            btrc_Vector_SessionOverride_p1_push(platform->sessions, sessionOverride);
            if (overlays != NULL) {
                if ((--overlays->__rc) <= 0) {
                    btrc_Vector_string_destroy(overlays);
                }
            }
            if (sessionOverride != NULL) {
                if ((--sessionOverride->__rc) <= 0) {
                    SessionOverride_destroy(sessionOverride);
                }
            }
            if (sessionKeys != NULL) {
                if ((--sessionKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(sessionKeys);
                }
            }
        }
    }
    if (displayServers != NULL) {
        if ((--displayServers->__rc) <= 0) {
            btrc_Vector_string_destroy(displayServers);
        }
    }
    if (sessionNames != NULL) {
        if ((--sessionNames->__rc) <= 0) {
            btrc_Vector_string_destroy(sessionNames);
        }
    }
}

void EmulatorParser_parseEnvironment(EmulatorPlatform* platform, JsonValue* jsonValue, char* file, char* context, ParseLog* log) {
    JsonValue* environment = JsonValue_get(jsonValue, "env");
    if (!JsonValue_isObject(environment)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".env")), "expected object {set?, unset?}");
        return;
    }
    btrc_Vector_string* __list_470 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_470, "set");
    btrc_Vector_string_push(__list_470, "unset");
    btrc_Vector_string* environmentKeys = __list_470;
    JsonContract_checkKeys(environment, environmentKeys, file, __btrc_str_track(__btrc_strcat(context, ".env")), log);
    if (JsonValue_has(environment, "set")) {
        JsonValue* setEntries = JsonValue_get(environment, "set");
        if (!JsonValue_isObject(setEntries)) {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".env.set")), "expected object");
        } else {
            btrc_Vector_string* __iter_471 = JsonValue_keys(setEntries);
            int __n_473 = btrc_Vector_string_iterLen(__iter_471);
            for (int __i_472 = 0; (__i_472 < __n_473); (__i_472++)) {
                char* key = btrc_Vector_string_iterGet(__iter_471, __i_472);
                char* value = JsonContract_requiredString(setEntries, key, file, __btrc_str_track(__btrc_strcat(context, ".env.set")), log);
                btrc_Vector_string_push(platform->environmentSetKeys, key);
                btrc_Vector_string_push(platform->environmentSetValues, value);
            }
        }
    }
    if (platform->environmentUnset != NULL) {
        if ((--platform->environmentUnset->__rc) <= 0) {
            btrc_Vector_string_free(platform->environmentUnset);
        }
    }
    (platform->environmentUnset = JsonContract_stringArray(environment, "unset", false, file, __btrc_str_track(__btrc_strcat(context, ".env")), log));
    (platform->environmentUnset->__rc++);
    if (environmentKeys != NULL) {
        if ((--environmentKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(environmentKeys);
        }
    }
}

void EmulatorParser_checkPlatformAxes(EmulatorPlatform* platform, JsonValue* jsonValue, bool isLinux, char* file, char* context, ParseLog* log) {
    if ((!isLinux) && ((strcmp(platform->backend, "flatpak") == 0) || (strcmp(platform->backend, "bwrap") == 0))) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".backend")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", platform->backend)), "' is a linux-only backend")));
    }
    if ((!isLinux) && JsonValue_has(jsonValue, "flatpak_id")) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".flatpak_id")), "linux-only field");
    }
    if ((isLinux && (strcmp(platform->backend, "flatpak") == 0)) && __btrc_isEmpty(platform->flatpakId)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".flatpak_id")), "required when backend is 'flatpak'");
    }
    if (((strcmp(platform->backend, "nix") == 0) || (strcmp(platform->backend, "native") == 0)) && __btrc_isEmpty(platform->executable)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".executable")), "required for nix/native backends");
    }
    if (isLinux && __btrc_isEmpty(platform->displayServer)) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".display_server")), "required on linux ('x11' or 'wayland')");
    }
    if ((!isLinux) && JsonValue_has(jsonValue, "display_server")) {
        ParseLog_add(log, file, __btrc_str_track(__btrc_strcat(context, ".display_server")), "linux-only field (display server is not a macos concept)");
    }
}

EmulatorPlatform* EmulatorParser_parsePlatform(JsonValue* jsonValue, char* operatingSystemName, char* emulatorId, char* file, ParseLog* log) {
    EmulatorPlatform* platform = EmulatorPlatform_new();
    (platform->operatingSystem = operatingSystemName);
    char* context = __btrc_str_track(__btrc_strcat("platforms.", operatingSystemName));
    if (!JsonValue_isObject(jsonValue)) {
        ParseLog_add(log, file, context, "expected object");
        return platform;
    }
    btrc_Vector_string* __list_474 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_474, "backend");
    btrc_Vector_string_push(__list_474, "flatpak_id");
    btrc_Vector_string_push(__list_474, "executable");
    btrc_Vector_string_push(__list_474, "display_server");
    btrc_Vector_string_push(__list_474, "display_default");
    btrc_Vector_string_push(__list_474, "sessions");
    btrc_Vector_string_push(__list_474, "args");
    btrc_Vector_string_push(__list_474, "gl_wrapper");
    btrc_Vector_string_push(__list_474, "env");
    btrc_Vector_string_push(__list_474, "sandbox");
    btrc_Vector_string_push(__list_474, "graphics");
    btrc_Vector_string_push(__list_474, "overlay_aspect");
    btrc_Vector_string_push(__list_474, "aspect_probe");
    btrc_Vector_string_push(__list_474, "state");
    btrc_Vector_string_push(__list_474, "firmware");
    btrc_Vector_string_push(__list_474, emulatorId);
    btrc_Vector_string* allowedKeys = __list_474;
    JsonContract_checkKeys(jsonValue, allowedKeys, file, context, log);
    (platform->backend = JsonContract_requiredString(jsonValue, "backend", file, context, log));
    btrc_Vector_string* __list_475 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_475, "flatpak");
    btrc_Vector_string_push(__list_475, "nix");
    btrc_Vector_string_push(__list_475, "bwrap");
    btrc_Vector_string_push(__list_475, "native");
    btrc_Vector_string* backends = __list_475;
    JsonContract_checkEnum(platform->backend, backends, file, __btrc_str_track(__btrc_strcat(context, ".backend")), log);
    (platform->flatpakId = JsonContract_optionalString(jsonValue, "flatpak_id", "", file, context, log));
    (platform->executable = JsonContract_optionalString(jsonValue, "executable", "", file, context, log));
    TemplateTokens_check(platform->executable, file, __btrc_str_track(__btrc_strcat(context, ".executable")), log, false);
    (platform->displayServer = JsonContract_optionalString(jsonValue, "display_server", "", file, context, log));
    btrc_Vector_string* __list_476 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_476, "x11");
    btrc_Vector_string_push(__list_476, "wayland");
    btrc_Vector_string* displayServers = __list_476;
    JsonContract_checkEnum(platform->displayServer, displayServers, file, __btrc_str_track(__btrc_strcat(context, ".display_server")), log);
    (platform->displayFallback = JsonContract_optionalString(jsonValue, "display_default", "", file, context, log));
    bool isLinux = (strcmp(operatingSystemName, "linux") == 0);
    EmulatorParser_checkPlatformAxes(platform, jsonValue, isLinux, file, context, log);
    if (JsonValue_has(jsonValue, "sessions")) {
        EmulatorParser_parseSessions(platform, jsonValue, isLinux, file, context, log);
    }
    if (platform->arguments != NULL) {
        if ((--platform->arguments->__rc) <= 0) {
            btrc_Vector_string_free(platform->arguments);
        }
    }
    (platform->arguments = JsonContract_stringArray(jsonValue, "args", false, file, context, log));
    (platform->arguments->__rc++);
    (platform->glWrapper = JsonContract_optionalString(jsonValue, "gl_wrapper", "none", file, context, log));
    btrc_Vector_string* __list_477 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_477, "nixgl");
    btrc_Vector_string_push(__list_477, "none");
    btrc_Vector_string* glWrappers = __list_477;
    JsonContract_checkEnum(platform->glWrapper, glWrappers, file, __btrc_str_track(__btrc_strcat(context, ".gl_wrapper")), log);
    if (JsonValue_has(jsonValue, "env")) {
        EmulatorParser_parseEnvironment(platform, jsonValue, file, context, log);
    }
    if (JsonValue_has(jsonValue, "sandbox")) {
        if (platform->sandbox != NULL) {
            if ((--platform->sandbox->__rc) <= 0) {
                SandboxPolicy_destroy(platform->sandbox);
            }
        }
        (platform->sandbox = SandboxPolicyParser_parse(JsonValue_get(jsonValue, "sandbox"), operatingSystemName, file, __btrc_str_track(__btrc_strcat(context, ".sandbox")), log));
        (platform->sandbox->__rc++);
    }
    if (JsonValue_has(jsonValue, "graphics")) {
        if (platform->graphics != NULL) {
            if ((--platform->graphics->__rc) <= 0) {
                GraphicsPolicy_destroy(platform->graphics);
            }
        }
        (platform->graphics = GraphicsPolicyParser_parse(JsonValue_get(jsonValue, "graphics"), file, __btrc_str_track(__btrc_strcat(context, ".graphics")), log));
        (platform->graphics->__rc++);
    }
    (platform->overlayAspect = JsonContract_optionalString(jsonValue, "overlay_aspect", "", file, context, log));
    btrc_Vector_string* __list_478 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_478, "4x3");
    btrc_Vector_string_push(__list_478, "16x9");
    btrc_Vector_string_push(__list_478, "3ds");
    btrc_Vector_string* overlayAspects = __list_478;
    JsonContract_checkEnum(platform->overlayAspect, overlayAspects, file, __btrc_str_track(__btrc_strcat(context, ".overlay_aspect")), log);
    if (JsonValue_has(jsonValue, "aspect_probe")) {
        if (platform->aspectProbe != NULL) {
            if ((--platform->aspectProbe->__rc) <= 0) {
                AspectProbe_destroy(platform->aspectProbe);
            }
        }
        (platform->aspectProbe = AspectProbeParser_parse(JsonValue_get(jsonValue, "aspect_probe"), file, __btrc_str_track(__btrc_strcat(context, ".aspect_probe")), log));
        (platform->aspectProbe->__rc++);
    }
    if (JsonValue_has(jsonValue, "state")) {
        if (platform->state != NULL) {
            if ((--platform->state->__rc) <= 0) {
                StatePolicy_destroy(platform->state);
            }
        }
        (platform->state = StatePolicyParser_parse(JsonValue_get(jsonValue, "state"), file, __btrc_str_track(__btrc_strcat(context, ".state")), log));
        (platform->state->__rc++);
    }
    if (JsonValue_has(jsonValue, "firmware")) {
        if (platform->firmware != NULL) {
            if ((--platform->firmware->__rc) <= 0) {
                FirmwarePolicy_destroy(platform->firmware);
            }
        }
        (platform->firmware = FirmwarePolicyParser_parse(JsonValue_get(jsonValue, "firmware"), file, __btrc_str_track(__btrc_strcat(context, ".firmware")), log));
        (platform->firmware->__rc++);
    }
    if (JsonValue_has(jsonValue, emulatorId)) {
        if (platform->extensionJson != NULL) {
            if ((--platform->extensionJson->__rc) <= 0) {
                JsonValue_destroy(platform->extensionJson);
            }
        }
        (platform->extensionJson = JsonValue_get(jsonValue, emulatorId));
        (platform->extensionJson->__rc++);
    }
    EmulatorRegistry_parseExtension(emulatorId, platform, log);
    if (overlayAspects != NULL) {
        if ((--overlayAspects->__rc) <= 0) {
            btrc_Vector_string_destroy(overlayAspects);
        }
    }
    if (glWrappers != NULL) {
        if ((--glWrappers->__rc) <= 0) {
            btrc_Vector_string_destroy(glWrappers);
        }
    }
    if (displayServers != NULL) {
        if ((--displayServers->__rc) <= 0) {
            btrc_Vector_string_destroy(displayServers);
        }
    }
    if (backends != NULL) {
        if ((--backends->__rc) <= 0) {
            btrc_Vector_string_destroy(backends);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return platform;
    if (overlayAspects != NULL) {
        if ((--overlayAspects->__rc) <= 0) {
            btrc_Vector_string_destroy(overlayAspects);
        }
    }
    if (glWrappers != NULL) {
        if ((--glWrappers->__rc) <= 0) {
            btrc_Vector_string_destroy(glWrappers);
        }
    }
    if (displayServers != NULL) {
        if ((--displayServers->__rc) <= 0) {
            btrc_Vector_string_destroy(displayServers);
        }
    }
    if (backends != NULL) {
        if ((--backends->__rc) <= 0) {
            btrc_Vector_string_destroy(backends);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (platform != NULL) {
        if ((--platform->__rc) <= 0) {
            EmulatorPlatform_destroy(platform);
        }
    }
}

void EmulatorParser_parseInput(EmulatorDefinition* emulatorDefinition, JsonValue* root, char* filePath, ParseLog* log) {
    bool hasQuitAction = false;
    if (JsonValue_has(root, "input")) {
        JsonValue* input = JsonValue_get(root, "input");
        if (!JsonValue_isObject(input)) {
            ParseLog_add(log, filePath, "input", "expected object {actions}");
        } else {
            btrc_Vector_string* __list_479 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_479, "actions");
            btrc_Vector_string* inputKeys = __list_479;
            JsonContract_checkKeys(input, inputKeys, filePath, "input", log);
            (emulatorDefinition->input->present = true);
            if (emulatorDefinition->input->actions != NULL) {
                if ((--emulatorDefinition->input->actions->__rc) <= 0) {
                    btrc_Vector_string_free(emulatorDefinition->input->actions);
                }
            }
            (emulatorDefinition->input->actions = JsonContract_stringArray(input, "actions", true, filePath, "input", log));
            (emulatorDefinition->input->actions->__rc++);
            btrc_Vector_string* __iter_480 = emulatorDefinition->input->actions;
            int __n_482 = btrc_Vector_string_iterLen(__iter_480);
            for (int __i_481 = 0; (__i_481 < __n_482); (__i_481++)) {
                char* action = btrc_Vector_string_iterGet(__iter_480, __i_481);
                if (!__btrc_strContains(action, ".")) {
                    ParseLog_add(log, filePath, "input.actions", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", action)), "' is not a dotted action id")));
                }
                if (strcmp(action, "app.quit") == 0) {
                    (hasQuitAction = true);
                }
            }
            if (inputKeys != NULL) {
                if ((--inputKeys->__rc) <= 0) {
                    btrc_Vector_string_destroy(inputKeys);
                }
            }
        }
    }
    if (!hasQuitAction) {
        ParseLog_add(log, filePath, "input.actions", "must include 'app.quit' — every emulator must be quittable");
    }
}

EmulatorDefinition* EmulatorParser_parseFile(char* emulatorId, char* filePath, ParseLog* log) {
    EmulatorDefinition* emulatorDefinition = EmulatorDefinition_new();
    (emulatorDefinition->id = emulatorId);
    (emulatorDefinition->file = filePath);
    JsonValue* root = JsonValue_readFile(filePath);
    if (JsonValue_isError(root)) {
        ParseLog_add(log, filePath, "(root)", "invalid JSON");
        return emulatorDefinition;
    }
    JsonContract_checkSchemaVersion(root, 1, filePath, log);
    btrc_Vector_string* __list_483 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_483, "schema_version");
    btrc_Vector_string_push(__list_483, "name");
    btrc_Vector_string_push(__list_483, "kind");
    btrc_Vector_string_push(__list_483, "default_system");
    btrc_Vector_string_push(__list_483, "input");
    btrc_Vector_string_push(__list_483, "platforms");
    btrc_Vector_string* allowedKeys = __list_483;
    JsonContract_checkKeys(root, allowedKeys, filePath, "", log);
    EmulatorParser_parseInput(emulatorDefinition, root, filePath, log);
    (emulatorDefinition->name = JsonContract_requiredString(root, "name", filePath, "", log));
    (emulatorDefinition->kind = JsonContract_requiredString(root, "kind", filePath, "", log));
    btrc_Vector_string* __list_484 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_484, "libretro_frontend");
    btrc_Vector_string_push(__list_484, "standalone");
    btrc_Vector_string* kinds = __list_484;
    JsonContract_checkEnum(emulatorDefinition->kind, kinds, filePath, "kind", log);
    (emulatorDefinition->defaultSystem = JsonContract_optionalString(root, "default_system", "", filePath, "", log));
    if ((strcmp(emulatorDefinition->kind, "standalone") == 0) && __btrc_isEmpty(emulatorDefinition->defaultSystem)) {
        ParseLog_add(log, filePath, "default_system", "required for standalone emulators");
    }
    if ((strcmp(emulatorDefinition->kind, "libretro_frontend") == 0) && (!__btrc_isEmpty(emulatorDefinition->defaultSystem))) {
        ParseLog_add(log, filePath, "default_system", "only valid for standalone emulators");
    }
    if ((!JsonValue_has(root, "platforms")) || (!JsonValue_isObject(JsonValue_get(root, "platforms")))) {
        ParseLog_add(log, filePath, "platforms", "expected object with linux and/or macos blocks");
    } else {
        JsonValue* platforms = JsonValue_get(root, "platforms");
        btrc_Vector_string* __list_485 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_485, "linux");
        btrc_Vector_string_push(__list_485, "macos");
        btrc_Vector_string* operatingSystems = __list_485;
        btrc_Vector_string* __iter_486 = JsonValue_keys(platforms);
        int __n_488 = btrc_Vector_string_iterLen(__iter_486);
        for (int __i_487 = 0; (__i_487 < __n_488); (__i_487++)) {
            char* operatingSystemName = btrc_Vector_string_iterGet(__iter_486, __i_487);
            if (!btrc_Vector_string_contains(operatingSystems, operatingSystemName)) {
                ParseLog_add(log, filePath, __btrc_str_track(__btrc_strcat("platforms.", operatingSystemName)), "unknown platform (allowed: linux, macos)");
            } else {
                btrc_Vector_EmulatorPlatform_p1_push(emulatorDefinition->platforms, EmulatorParser_parsePlatform(JsonValue_get(platforms, operatingSystemName), operatingSystemName, emulatorId, filePath, log));
            }
        }
        if (btrc_Vector_EmulatorPlatform_p1_size(emulatorDefinition->platforms) == 0) {
            ParseLog_add(log, filePath, "platforms", "at least one platform block is required");
        }
        if (operatingSystems != NULL) {
            if ((--operatingSystems->__rc) <= 0) {
                btrc_Vector_string_destroy(operatingSystems);
            }
        }
    }
    if (kinds != NULL) {
        if ((--kinds->__rc) <= 0) {
            btrc_Vector_string_destroy(kinds);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return emulatorDefinition;
    if (kinds != NULL) {
        if ((--kinds->__rc) <= 0) {
            btrc_Vector_string_destroy(kinds);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (emulatorDefinition != NULL) {
        if ((--emulatorDefinition->__rc) <= 0) {
            EmulatorDefinition_destroy(emulatorDefinition);
        }
    }
}

btrc_Vector_EmulatorDefinition_p1* EmulatorParser_parseAll(char* sourceRoot, ParseLog* log) {
    btrc_Vector_EmulatorDefinition_p1* emulators = btrc_Vector_EmulatorDefinition_p1_new();
    char* emulatorsRoot = PathTools_join(sourceRoot, "emulators");
    btrc_Vector_string* __iter_489 = SourceDirectories_sortedChildDirectories(emulatorsRoot);
    int __n_491 = btrc_Vector_string_iterLen(__iter_489);
    for (int __i_490 = 0; (__i_490 < __n_491); (__i_490++)) {
        char* emulatorId = btrc_Vector_string_iterGet(__iter_489, __i_490);
        char* contractFile = PathTools_join(PathTools_join(emulatorsRoot, emulatorId), "emulator.json");
        if (FileSystem_isFile(contractFile)) {
            btrc_Vector_EmulatorDefinition_p1_push(emulators, EmulatorParser_parseFile(emulatorId, contractFile, log));
        }
    }
    return emulators;
    if (emulators != NULL) {
        if ((--emulators->__rc) <= 0) {
            btrc_Vector_EmulatorDefinition_p1_destroy(emulators);
        }
    }
}

void SystemParser_parseEsDe(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log) {
    if (!JsonValue_has(root, "es_de")) {
        ParseLog_add(log, path, "es_de", "missing required field");
        return;
    }
    JsonValue* esDe = JsonValue_get(root, "es_de");
    if (!JsonValue_isObject(esDe)) {
        ParseLog_add(log, path, "es_de", "expected object");
        return;
    }
    btrc_Vector_string* __list_492 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_492, "platforms");
    btrc_Vector_string_push(__list_492, "theme");
    btrc_Vector_string* esDeKeys = __list_492;
    JsonContract_checkKeys(esDe, esDeKeys, path, "es_de", log);
    if (systemDefinition->esDePlatforms != NULL) {
        if ((--systemDefinition->esDePlatforms->__rc) <= 0) {
            btrc_Vector_string_free(systemDefinition->esDePlatforms);
        }
    }
    (systemDefinition->esDePlatforms = JsonContract_stringArray(esDe, "platforms", true, path, "es_de", log));
    (systemDefinition->esDePlatforms->__rc++);
    if (btrc_Vector_string_size(systemDefinition->esDePlatforms) == 0) {
        ParseLog_add(log, path, "es_de.platforms", "must be non-empty");
    }
    (systemDefinition->esDeTheme = JsonContract_requiredString(esDe, "theme", path, "es_de", log));
    if (esDeKeys != NULL) {
        if ((--esDeKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(esDeKeys);
        }
    }
}

void SystemParser_parseRom(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log) {
    if (!JsonValue_has(root, "rom")) {
        ParseLog_add(log, path, "rom", "missing required field");
        return;
    }
    JsonValue* rom = JsonValue_get(root, "rom");
    if (!JsonValue_isObject(rom)) {
        ParseLog_add(log, path, "rom", "expected object");
        return;
    }
    btrc_Vector_string* __list_493 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_493, "dir");
    btrc_Vector_string_push(__list_493, "extensions");
    btrc_Vector_string* romKeys = __list_493;
    JsonContract_checkKeys(rom, romKeys, path, "rom", log);
    (systemDefinition->romDirectory = JsonContract_requiredString(rom, "dir", path, "rom", log));
    if (systemDefinition->extensions != NULL) {
        if ((--systemDefinition->extensions->__rc) <= 0) {
            btrc_Vector_string_free(systemDefinition->extensions);
        }
    }
    (systemDefinition->extensions = JsonContract_stringArray(rom, "extensions", true, path, "rom", log));
    (systemDefinition->extensions->__rc++);
    btrc_Vector_string* __iter_494 = systemDefinition->extensions;
    int __n_496 = btrc_Vector_string_iterLen(__iter_494);
    for (int __i_495 = 0; (__i_495 < __n_496); (__i_495++)) {
        char* extension = btrc_Vector_string_iterGet(__iter_494, __i_495);
        if (!__btrc_startsWith(extension, ".")) {
            ParseLog_add(log, path, "rom.extensions", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", extension)), "' must start with '.'")));
        }
        if (!(strcmp(__btrc_str_track(__btrc_toLower(extension)), extension) == 0)) {
            ParseLog_add(log, path, "rom.extensions", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", extension)), "' must be lowercase (uppercase variants are derived)")));
        }
    }
    if (romKeys != NULL) {
        if ((--romKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(romKeys);
        }
    }
}

void SystemParser_parseBindings(SystemDefinition* systemDefinition, JsonValue* root, char* path, ParseLog* log) {
    if (((!JsonValue_has(root, "emulators")) || (!JsonValue_isArray(JsonValue_get(root, "emulators")))) || (JsonValue_size(JsonValue_get(root, "emulators")) < 1)) {
        ParseLog_add(log, path, "emulators", "expected non-empty array");
        return;
    }
    JsonValue* bindings = JsonValue_get(root, "emulators");
    for (int bindingIndex = 0; (bindingIndex < JsonValue_size(bindings)); (bindingIndex++)) {
        int __fstr_497_arg0 = bindingIndex;
        int __fstr_497_len = snprintf(NULL, 0, "emulators[%d]", __fstr_497_arg0);
        char* __fstr_497_buf = __btrc_str_track(((char*)malloc((__fstr_497_len + 1))));
        snprintf(__fstr_497_buf, (__fstr_497_len + 1), "emulators[%d]", __fstr_497_arg0);
        btrc_Vector_EmulatorBinding_p1_push(systemDefinition->emulators, EmulatorBindingParser_parse(JsonValue_at(bindings, bindingIndex), path, __fstr_497_buf, log));
    }
}

SystemDefinition* SystemParser_parseSystemFile(char* systemId, char* path, ParseLog* log) {
    SystemDefinition* systemDefinition = SystemDefinition_new();
    (systemDefinition->id = systemId);
    (systemDefinition->file = path);
    JsonValue* root = JsonValue_readFile(path);
    if (JsonValue_isError(root)) {
        ParseLog_add(log, path, "(root)", "invalid JSON");
        return systemDefinition;
    }
    JsonContract_checkSchemaVersion(root, 1, path, log);
    btrc_Vector_string* __list_498 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_498, "schema_version");
    btrc_Vector_string_push(__list_498, "name");
    btrc_Vector_string_push(__list_498, "aliases");
    btrc_Vector_string_push(__list_498, "es_de");
    btrc_Vector_string_push(__list_498, "rom");
    btrc_Vector_string_push(__list_498, "emulators");
    btrc_Vector_string_push(__list_498, "bios");
    btrc_Vector_string_push(__list_498, "controller_profile");
    btrc_Vector_string_push(__list_498, "display");
    btrc_Vector_string_push(__list_498, "render");
    btrc_Vector_string* allowedKeys = __list_498;
    JsonContract_checkKeys(root, allowedKeys, path, "", log);
    (systemDefinition->name = JsonContract_requiredString(root, "name", path, "", log));
    if (systemDefinition->aliases != NULL) {
        if ((--systemDefinition->aliases->__rc) <= 0) {
            btrc_Vector_string_free(systemDefinition->aliases);
        }
    }
    (systemDefinition->aliases = JsonContract_stringArray(root, "aliases", false, path, "", log));
    (systemDefinition->aliases->__rc++);
    SystemParser_parseEsDe(systemDefinition, root, path, log);
    SystemParser_parseRom(systemDefinition, root, path, log);
    SystemParser_parseBindings(systemDefinition, root, path, log);
    if (JsonValue_has(root, "bios")) {
        if (systemDefinition->bios != NULL) {
            if ((--systemDefinition->bios->__rc) <= 0) {
                BiosRequirement_destroy(systemDefinition->bios);
            }
        }
        (systemDefinition->bios = BiosRequirementParser_parse(JsonValue_get(root, "bios"), path, log));
        (systemDefinition->bios->__rc++);
    }
    (systemDefinition->controllerProfile = JsonContract_optionalString(root, "controller_profile", "", path, "", log));
    btrc_Vector_string* __list_499 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_499, "steam_deck");
    btrc_Vector_string* controllerProfiles = __list_499;
    JsonContract_checkEnum(systemDefinition->controllerProfile, controllerProfiles, path, "controller_profile", log);
    if (JsonValue_has(root, "display")) {
        if (systemDefinition->display != NULL) {
            if ((--systemDefinition->display->__rc) <= 0) {
                DisplayPolicy_destroy(systemDefinition->display);
            }
        }
        (systemDefinition->display = DisplayPolicyParser_parse(JsonValue_get(root, "display"), path, log));
        (systemDefinition->display->__rc++);
    } else {
        ParseLog_add(log, path, "display", "missing required field");
    }
    if (JsonValue_has(root, "render")) {
        if (systemDefinition->render != NULL) {
            if ((--systemDefinition->render->__rc) <= 0) {
                RenderPolicy_destroy(systemDefinition->render);
            }
        }
        (systemDefinition->render = RenderPolicyParser_parse(JsonValue_get(root, "render"), path, btrc_Vector_SystemScreen_p1_size(systemDefinition->display->screens), log));
        (systemDefinition->render->__rc++);
    } else {
        ParseLog_add(log, path, "render", "missing required field");
    }
    if (controllerProfiles != NULL) {
        if ((--controllerProfiles->__rc) <= 0) {
            btrc_Vector_string_destroy(controllerProfiles);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    return systemDefinition;
    if (controllerProfiles != NULL) {
        if ((--controllerProfiles->__rc) <= 0) {
            btrc_Vector_string_destroy(controllerProfiles);
        }
    }
    if (allowedKeys != NULL) {
        if ((--allowedKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(allowedKeys);
        }
    }
    if (systemDefinition != NULL) {
        if ((--systemDefinition->__rc) <= 0) {
            SystemDefinition_destroy(systemDefinition);
        }
    }
}

BezelCollection* SystemParser_parseBezelsFile(char* path, btrc_Vector_string* screenIds, ParseLog* log) {
    return BezelCollectionParser_parseFile(path, screenIds, log);
}

ShaderCollection* SystemParser_parseShadersFile(char* path, ParseLog* log) {
    return ShaderCollectionParser_parseFile(path, log);
}

SemuContracts* SystemParser_loadAll(char* sourceRoot, ParseLog* log) {
    SemuContracts* contracts = SemuContracts_new();
    if (contracts->emulators != NULL) {
        if ((--contracts->emulators->__rc) <= 0) {
            btrc_Vector_EmulatorDefinition_p1_free(contracts->emulators);
        }
    }
    (contracts->emulators = EmulatorParser_parseAll(sourceRoot, log));
    (contracts->emulators->__rc++);
    char* systemsRoot = PathTools_join(sourceRoot, "systems");
    btrc_Vector_string* __iter_500 = SourceDirectories_sortedChildDirectories(systemsRoot);
    int __n_502 = btrc_Vector_string_iterLen(__iter_500);
    for (int __i_501 = 0; (__i_501 < __n_502); (__i_501++)) {
        char* systemId = btrc_Vector_string_iterGet(__iter_500, __i_501);
        char* systemDirectory = PathTools_join(systemsRoot, systemId);
        char* systemFile = PathTools_join(systemDirectory, "system.json");
        if (FileSystem_isFile(systemFile)) {
            SystemDefinition* systemDefinition = SystemParser_parseSystemFile(systemId, systemFile, log);
            if (systemDefinition->bezels != NULL) {
                if ((--systemDefinition->bezels->__rc) <= 0) {
                    BezelCollection_destroy(systemDefinition->bezels);
                }
            }
            (systemDefinition->bezels = SystemParser_parseBezelsFile(PathTools_join(systemDirectory, "bezels.json"), SystemDefinition_screenIds(systemDefinition), log));
            (systemDefinition->bezels->__rc++);
            if (systemDefinition->shaders != NULL) {
                if ((--systemDefinition->shaders->__rc) <= 0) {
                    ShaderCollection_destroy(systemDefinition->shaders);
                }
            }
            (systemDefinition->shaders = SystemParser_parseShadersFile(PathTools_join(systemDirectory, "shaders.json"), log));
            (systemDefinition->shaders->__rc++);
            btrc_Vector_SystemDefinition_p1_push(contracts->systems, systemDefinition);
        }
    }
    ContractValidation_crossValidate(contracts, log);
    return contracts;
    if (contracts != NULL) {
        if ((--contracts->__rc) <= 0) {
            SemuContracts_destroy(contracts);
        }
    }
}

char* EmulatorEmitter_quoteArgument(char* argument) {
    if (__btrc_strContains(argument, " ")) {
        return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", argument)), "\""));
    }
    return argument;
}

char* EmulatorEmitter_esDeCommand(EmulatorBinding* binding, EmulatorDefinition* emulator, char* operatingSystem) {
    EmulatorPlatform* platform = EmulatorDefinition_platformFor(emulator, operatingSystem);
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("%EMULATOR_", __btrc_str_track(__btrc_toUpper(emulator->id)))), "%"));
    btrc_Vector_string* __iter_503 = platform->arguments;
    int __n_505 = btrc_Vector_string_iterLen(__iter_503);
    for (int __i_504 = 0; (__i_504 < __n_505); (__i_504++)) {
        char* argument = btrc_Vector_string_iterGet(__iter_503, __i_504);
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, " ")), EmulatorEmitter_quoteArgument(argument))));
    }
    btrc_Vector_string* __iter_506 = binding->arguments;
    int __n_508 = btrc_Vector_string_iterLen(__iter_506);
    for (int __i_507 = 0; (__i_507 < __n_508); (__i_507++)) {
        char* argument = btrc_Vector_string_iterGet(__iter_506, __i_507);
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, " ")), EmulatorEmitter_quoteArgument(argument))));
    }
    btrc_Vector_string* __iter_509 = EmulatorRegistry_commandFragments(emulator->id, binding, platform);
    int __n_511 = btrc_Vector_string_iterLen(__iter_509);
    for (int __i_510 = 0; (__i_510 < __n_511); (__i_510++)) {
        char* fragment = btrc_Vector_string_iterGet(__iter_509, __i_510);
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, " ")), EmulatorEmitter_quoteArgument(fragment))));
    }
    return __btrc_str_track(__btrc_strcat(command, " %ROM%"));
}

char* EmulatorEmitter_shimScript(EmulatorDefinition* emulator) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "exec \"$(dirname \"$0\")/semu\" launcher ")), emulator->id)), " \"$@\"\n"));
}

char* EmulatorEmitter_shimName(EmulatorDefinition* emulator) {
    return __btrc_str_track(__btrc_strcat("semu-", emulator->id));
}

void EmulatorEmitter_emitShims(btrc_Vector_EmulatorDefinition_p1* emulators, char* outputDirectory) {
    UnixFileSystem_mkdirp(outputDirectory);
    int __n_513 = btrc_Vector_EmulatorDefinition_p1_iterLen(emulators);
    for (int __i_512 = 0; (__i_512 < __n_513); (__i_512++)) {
        EmulatorDefinition* emulator = btrc_Vector_EmulatorDefinition_p1_iterGet(emulators, __i_512);
        char* shimPath = PathTools_join(outputDirectory, EmulatorEmitter_shimName(emulator));
        Path_writeAll(shimPath, EmulatorEmitter_shimScript(emulator));
        FileSystem_chmod(shimPath, 493);
    }
}

char* SystemEmitter_xmlEscape(char* value) {
    char* escaped = __btrc_str_track(__btrc_replace(value, "&", "&amp;"));
    (escaped = __btrc_str_track(__btrc_replace(escaped, "\"", "&quot;")));
    (escaped = __btrc_str_track(__btrc_replace(escaped, "<", "&lt;")));
    (escaped = __btrc_str_track(__btrc_replace(escaped, ">", "&gt;")));
    return escaped;
}

char* SystemEmitter_jsonEscape(char* value) {
    char* escaped = __btrc_str_track(__btrc_replace(value, "\\", "\\\\"));
    (escaped = __btrc_str_track(__btrc_replace(escaped, "\"", "\\\"")));
    return escaped;
}

char* SystemEmitter_jsonString(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", SystemEmitter_jsonEscape(value))), "\""));
}

char* SystemEmitter_jsonStringArray(btrc_Vector_string* values) {
    btrc_Vector_string* quotedValues = btrc_Vector_string_new();
    int __n_515 = btrc_Vector_string_iterLen(values);
    for (int __i_514 = 0; (__i_514 < __n_515); (__i_514++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_514);
        btrc_Vector_string_push(quotedValues, SystemEmitter_jsonString(value));
    }
    char* __btrc_ret_516 = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[", btrc_Vector_string_join(quotedValues, ", "))), "]"));
    if (quotedValues != NULL) {
        if ((--quotedValues->__rc) <= 0) {
            btrc_Vector_string_destroy(quotedValues);
        }
    }
    return __btrc_ret_516;
    if (quotedValues != NULL) {
        if ((--quotedValues->__rc) <= 0) {
            btrc_Vector_string_destroy(quotedValues);
        }
    }
}

EmulatorDefinition* SystemEmitter_emulatorForId(btrc_Vector_EmulatorDefinition_p1* emulators, char* emulatorId) {
    int __n_518 = btrc_Vector_EmulatorDefinition_p1_iterLen(emulators);
    for (int __i_517 = 0; (__i_517 < __n_518); (__i_517++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(emulators, __i_517);
        if (strcmp(emulatorDefinition->id, emulatorId) == 0) {
            return emulatorDefinition;
        }
    }
    return EmulatorDefinition_new();
}

bool SystemEmitter_bindingApplies(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem) {
    if ((btrc_Vector_string_size(binding->platforms) > 0) && (!btrc_Vector_string_contains(binding->platforms, operatingSystem))) {
        return false;
    }
    EmulatorDefinition* emulatorDefinition = SystemEmitter_emulatorForId(emulators, binding->emulator);
    if (__btrc_isEmpty(emulatorDefinition->id)) {
        return false;
    }
    return EmulatorDefinition_hasOperatingSystem(emulatorDefinition, operatingSystem);
}

char* SystemEmitter_extensionList(btrc_Vector_string* extensions) {
    btrc_Vector_string* doubledCases = btrc_Vector_string_new();
    int __n_520 = btrc_Vector_string_iterLen(extensions);
    for (int __i_519 = 0; (__i_519 < __n_520); (__i_519++)) {
        char* extension = btrc_Vector_string_iterGet(extensions, __i_519);
        btrc_Vector_string_push(doubledCases, extension);
        btrc_Vector_string_push(doubledCases, __btrc_str_track(__btrc_toUpper(extension)));
    }
    char* __btrc_ret_521 = btrc_Vector_string_join(doubledCases, " ");
    if (doubledCases != NULL) {
        if ((--doubledCases->__rc) <= 0) {
            btrc_Vector_string_destroy(doubledCases);
        }
    }
    return __btrc_ret_521;
    if (doubledCases != NULL) {
        if ((--doubledCases->__rc) <= 0) {
            btrc_Vector_string_destroy(doubledCases);
        }
    }
}

char* SystemEmitter_commandLabel(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators) {
    if (!__btrc_isEmpty(binding->label)) {
        return binding->label;
    }
    return SystemEmitter_emulatorForId(emulators, binding->emulator)->name;
}

char* SystemEmitter_systemEntryXml(SystemDefinition* systemDefinition, btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem) {
    char* xml = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <system>\n", "    <name>")), SystemEmitter_xmlEscape(systemDefinition->id))), "</name>\n")), "    <fullname>")), SystemEmitter_xmlEscape(systemDefinition->name))), "</fullname>\n")), "    <path>%ROMPATH%/")), SystemEmitter_xmlEscape(systemDefinition->romDirectory))), "</path>\n")), "    <extension>")), SystemEmitter_xmlEscape(SystemEmitter_extensionList(systemDefinition->extensions)))), "</extension>\n"));
    btrc_Vector_EmulatorBinding_p1* __iter_522 = systemDefinition->emulators;
    int __n_524 = btrc_Vector_EmulatorBinding_p1_iterLen(__iter_522);
    for (int __i_523 = 0; (__i_523 < __n_524); (__i_523++)) {
        EmulatorBinding* binding = btrc_Vector_EmulatorBinding_p1_iterGet(__iter_522, __i_523);
        if (SystemEmitter_bindingApplies(binding, emulators, operatingSystem)) {
            EmulatorDefinition* emulatorDefinition = SystemEmitter_emulatorForId(emulators, binding->emulator);
            (xml = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(xml, "    <command label=\"")), SystemEmitter_xmlEscape(SystemEmitter_commandLabel(binding, emulators)))), "\">")), SystemEmitter_xmlEscape(EmulatorEmitter_esDeCommand(binding, emulatorDefinition, operatingSystem)))), "</command>\n")));
        }
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(xml, "    <platform>")), SystemEmitter_xmlEscape(btrc_Vector_string_join(systemDefinition->esDePlatforms, ",")))), "</platform>\n")), "    <theme>")), SystemEmitter_xmlEscape(systemDefinition->esDeTheme))), "</theme>\n")), "  </system>\n"));
}

char* SystemEmitter_esSystemsXml(SemuContracts* contracts, char* operatingSystem) {
    char* xml = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by semu from the system contracts -->\n")), "<systemList>\n"));
    btrc_Vector_SystemDefinition_p1* __iter_525 = contracts->systems;
    int __n_527 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_525);
    for (int __i_526 = 0; (__i_526 < __n_527); (__i_526++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_525, __i_526);
        (xml = __btrc_str_track(__btrc_strcat(xml, SystemEmitter_systemEntryXml(systemDefinition, contracts->emulators, operatingSystem))));
    }
    return __btrc_str_track(__btrc_strcat(xml, "</systemList>\n"));
}

char* SystemEmitter_launcherRuleXml(EmulatorDefinition* emulatorDefinition) {
    char* shimName = EmulatorEmitter_shimName(emulatorDefinition);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <emulator name=\"", SystemEmitter_xmlEscape(__btrc_str_track(__btrc_toUpper(emulatorDefinition->id))))), "\">\n")), "    <rule type=\"systempath\">\n")), "      <entry>")), SystemEmitter_xmlEscape(shimName))), "</entry>\n")), "    </rule>\n")), "    <rule type=\"staticpath\">\n")), "      <entry>%ESPATH%/")), SystemEmitter_xmlEscape(shimName))), "</entry>\n")), "    </rule>\n")), "  </emulator>\n"));
}

char* SystemEmitter_corePathRuleXml(EmulatorDefinition* emulatorDefinition, char* operatingSystem) {
    EmulatorPlatform* platform = EmulatorDefinition_platformFor(emulatorDefinition, operatingSystem);
    btrc_Vector_string* coreDirectories = EmulatorRegistry_esDeCorePathDirectories(emulatorDefinition->id, platform);
    if (btrc_Vector_string_isEmpty(coreDirectories)) {
        return "";
    }
    char* entries = "";
    int __n_529 = btrc_Vector_string_iterLen(coreDirectories);
    for (int __i_528 = 0; (__i_528 < __n_529); (__i_528++)) {
        char* coreDirectory = btrc_Vector_string_iterGet(coreDirectories, __i_528);
        (entries = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(entries, "      <entry>")), SystemEmitter_xmlEscape(coreDirectory))), "</entry>\n")));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <core name=\"", SystemEmitter_xmlEscape(__btrc_str_track(__btrc_toUpper(emulatorDefinition->id))))), "\">\n")), "    <rule type=\"corepath\">\n")), entries)), "    </rule>\n")), "  </core>\n"));
}

char* SystemEmitter_esFindRulesXml(btrc_Vector_EmulatorDefinition_p1* emulators, char* operatingSystem) {
    char* xml = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by semu from the emulator contracts -->\n")), "<ruleList>\n"));
    int __n_531 = btrc_Vector_EmulatorDefinition_p1_iterLen(emulators);
    for (int __i_530 = 0; (__i_530 < __n_531); (__i_530++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(emulators, __i_530);
        if (EmulatorDefinition_hasOperatingSystem(emulatorDefinition, operatingSystem)) {
            (xml = __btrc_str_track(__btrc_strcat(xml, SystemEmitter_launcherRuleXml(emulatorDefinition))));
        }
    }
    int __n_533 = btrc_Vector_EmulatorDefinition_p1_iterLen(emulators);
    for (int __i_532 = 0; (__i_532 < __n_533); (__i_532++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(emulators, __i_532);
        if (EmulatorDefinition_hasOperatingSystem(emulatorDefinition, operatingSystem)) {
            (xml = __btrc_str_track(__btrc_strcat(xml, SystemEmitter_corePathRuleXml(emulatorDefinition, operatingSystem))));
        }
    }
    return __btrc_str_track(__btrc_strcat(xml, "</ruleList>\n"));
}

char* SystemEmitter_manifestBindingJson(EmulatorBinding* binding, btrc_Vector_EmulatorDefinition_p1* emulators) {
    char* json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{ \"emulator\": ", SystemEmitter_jsonString(binding->emulator))), ", \"label\": ")), SystemEmitter_jsonString(SystemEmitter_commandLabel(binding, emulators))));
    if (!__btrc_isEmpty(binding->core)) {
        (json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, ", \"core\": ")), SystemEmitter_jsonString(binding->core))));
    }
    if (btrc_Vector_string_size(binding->platforms) > 0) {
        (json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, ", \"platforms\": ")), SystemEmitter_jsonStringArray(binding->platforms))));
    }
    return __btrc_str_track(__btrc_strcat(json, " }"));
}

char* SystemEmitter_manifestSystemJson(SystemDefinition* systemDefinition, btrc_Vector_EmulatorDefinition_p1* emulators) {
    char* json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    {\n", "      \"id\": ")), SystemEmitter_jsonString(systemDefinition->id))), ",\n")), "      \"name\": ")), SystemEmitter_jsonString(systemDefinition->name))), ",\n")), "      \"rom_dir\": ")), SystemEmitter_jsonString(systemDefinition->romDirectory))), ",\n")), "      \"extensions\": ")), SystemEmitter_jsonStringArray(systemDefinition->extensions))), ",\n")), "      \"es_de\": { \"platforms\": ")), SystemEmitter_jsonStringArray(systemDefinition->esDePlatforms))), ", \"theme\": ")), SystemEmitter_jsonString(systemDefinition->esDeTheme))), " },\n"));
    btrc_Vector_string* bindingEntries = btrc_Vector_string_new();
    btrc_Vector_EmulatorBinding_p1* __iter_534 = systemDefinition->emulators;
    int __n_536 = btrc_Vector_EmulatorBinding_p1_iterLen(__iter_534);
    for (int __i_535 = 0; (__i_535 < __n_536); (__i_535++)) {
        EmulatorBinding* binding = btrc_Vector_EmulatorBinding_p1_iterGet(__iter_534, __i_535);
        btrc_Vector_string_push(bindingEntries, SystemEmitter_manifestBindingJson(binding, emulators));
    }
    (json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, "      \"emulators\": [ ")), btrc_Vector_string_join(bindingEntries, ", "))), " ]")));
    if (systemDefinition->bios->present) {
        btrc_Vector_string* biosFileNames = btrc_Vector_string_new();
        btrc_Vector_BiosFile_p1* __iter_537 = systemDefinition->bios->files;
        int __n_539 = btrc_Vector_BiosFile_p1_iterLen(__iter_537);
        for (int __i_538 = 0; (__i_538 < __n_539); (__i_538++)) {
            BiosFile* biosFile = btrc_Vector_BiosFile_p1_iterGet(__iter_537, __i_538);
            btrc_Vector_string_push(biosFileNames, biosFile->name);
        }
        (json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, ",\n      \"bios\": { \"required\": ")), (systemDefinition->bios->required ? "true" : "false"))), ", \"dir\": ")), SystemEmitter_jsonString(systemDefinition->bios->directory))), ", \"files\": ")), SystemEmitter_jsonStringArray(biosFileNames))), " }")));
        if (biosFileNames != NULL) {
            if ((--biosFileNames->__rc) <= 0) {
                btrc_Vector_string_destroy(biosFileNames);
            }
        }
    }
    char* __btrc_ret_540 = __btrc_str_track(__btrc_strcat(json, "\n    }"));
    if (bindingEntries != NULL) {
        if ((--bindingEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(bindingEntries);
        }
    }
    return __btrc_ret_540;
    if (bindingEntries != NULL) {
        if ((--bindingEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(bindingEntries);
        }
    }
}

char* SystemEmitter_manifestEmulatorJson(EmulatorDefinition* emulatorDefinition) {
    btrc_Vector_string* operatingSystems = btrc_Vector_string_new();
    btrc_Vector_EmulatorPlatform_p1* __iter_541 = emulatorDefinition->platforms;
    int __n_543 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_541);
    for (int __i_542 = 0; (__i_542 < __n_543); (__i_542++)) {
        EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_541, __i_542);
        btrc_Vector_string_push(operatingSystems, platform->operatingSystem);
    }
    char* json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    {\n", "      \"id\": ")), SystemEmitter_jsonString(emulatorDefinition->id))), ",\n")), "      \"name\": ")), SystemEmitter_jsonString(emulatorDefinition->name))), ",\n")), "      \"kind\": ")), SystemEmitter_jsonString(emulatorDefinition->kind))), ",\n"));
    if (!__btrc_isEmpty(emulatorDefinition->defaultSystem)) {
        (json = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, "      \"default_system\": ")), SystemEmitter_jsonString(emulatorDefinition->defaultSystem))), ",\n")));
    }
    char* __btrc_ret_544 = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(json, "      \"platforms\": ")), SystemEmitter_jsonStringArray(operatingSystems))), "\n    }"));
    if (operatingSystems != NULL) {
        if ((--operatingSystems->__rc) <= 0) {
            btrc_Vector_string_destroy(operatingSystems);
        }
    }
    return __btrc_ret_544;
    if (operatingSystems != NULL) {
        if ((--operatingSystems->__rc) <= 0) {
            btrc_Vector_string_destroy(operatingSystems);
        }
    }
}

char* SystemEmitter_manifestJson(SemuContracts* contracts) {
    btrc_Vector_string* systemEntries = btrc_Vector_string_new();
    btrc_Vector_SystemDefinition_p1* __iter_545 = contracts->systems;
    int __n_547 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_545);
    for (int __i_546 = 0; (__i_546 < __n_547); (__i_546++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_545, __i_546);
        btrc_Vector_string_push(systemEntries, SystemEmitter_manifestSystemJson(systemDefinition, contracts->emulators));
    }
    btrc_Vector_string* emulatorEntries = btrc_Vector_string_new();
    btrc_Vector_EmulatorDefinition_p1* __iter_548 = contracts->emulators;
    int __n_550 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_548);
    for (int __i_549 = 0; (__i_549 < __n_550); (__i_549++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_548, __i_549);
        btrc_Vector_string_push(emulatorEntries, SystemEmitter_manifestEmulatorJson(emulatorDefinition));
    }
    char* __btrc_ret_551 = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{\n", "  \"schema_version\": 1,\n")), "  \"systems\": [\n")), btrc_Vector_string_join(systemEntries, ",\n"))), "\n  ],\n")), "  \"emulators\": [\n")), btrc_Vector_string_join(emulatorEntries, ",\n"))), "\n  ]\n")), "}\n"));
    if (emulatorEntries != NULL) {
        if ((--emulatorEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(emulatorEntries);
        }
    }
    if (systemEntries != NULL) {
        if ((--systemEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(systemEntries);
        }
    }
    return __btrc_ret_551;
    if (emulatorEntries != NULL) {
        if ((--emulatorEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(emulatorEntries);
        }
    }
    if (systemEntries != NULL) {
        if ((--systemEntries->__rc) <= 0) {
            btrc_Vector_string_destroy(systemEntries);
        }
    }
}

void SystemEmitter_emitAll(SemuContracts* contracts, char* generatedRoot) {
    UnixFileSystem_mkdirp(generatedRoot);
    Path_writeAll(PathTools_join(generatedRoot, "semu.json"), SystemEmitter_manifestJson(contracts));
    char* customSystemsDirectory = PathTools_join(generatedRoot, "packaging/es-de/custom_systems");
    UnixFileSystem_mkdirp(customSystemsDirectory);
    Path_writeAll(PathTools_join(customSystemsDirectory, "es_systems.xml"), SystemEmitter_esSystemsXml(contracts, "linux"));
    Path_writeAll(PathTools_join(customSystemsDirectory, "es_find_rules.xml"), SystemEmitter_esFindRulesXml(contracts->emulators, "linux"));
}

void LaunchPlan_init(LaunchPlan* self) {
    self->__rc = 1;
    if (self->argumentVector != NULL) {
        if ((--self->argumentVector->__rc) <= 0) {
            btrc_Vector_string_free(self->argumentVector);
        }
    }
    btrc_Vector_string* __list_552 = btrc_Vector_string_new();
    (self->argumentVector = __list_552);
    (self->argumentVector->__rc++);
    if (self->environmentSetKeys != NULL) {
        if ((--self->environmentSetKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetKeys);
        }
    }
    btrc_Vector_string* __list_553 = btrc_Vector_string_new();
    (self->environmentSetKeys = __list_553);
    (self->environmentSetKeys->__rc++);
    if (self->environmentSetValues != NULL) {
        if ((--self->environmentSetValues->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetValues);
        }
    }
    btrc_Vector_string* __list_554 = btrc_Vector_string_new();
    (self->environmentSetValues = __list_554);
    (self->environmentSetValues->__rc++);
    if (self->environmentUnset != NULL) {
        if ((--self->environmentUnset->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentUnset);
        }
    }
    btrc_Vector_string* __list_555 = btrc_Vector_string_new();
    (self->environmentUnset = __list_555);
    (self->environmentUnset->__rc++);
    (self->backend = "");
    (self->note = "");
}

LaunchPlan* LaunchPlan_new(void) {
    LaunchPlan* self = ((LaunchPlan*)malloc(sizeof(LaunchPlan)));
    memset(self, 0, sizeof(LaunchPlan));
    LaunchPlan_init(self);
    return self;
}

void LaunchPlan_destroy(LaunchPlan* self) {
    if (self->argumentVector != NULL) {
        if ((--self->argumentVector->__rc) <= 0) {
            btrc_Vector_string_free(self->argumentVector);
        }
    }
    if (self->environmentSetKeys != NULL) {
        if ((--self->environmentSetKeys->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetKeys);
        }
    }
    if (self->environmentSetValues != NULL) {
        if ((--self->environmentSetValues->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentSetValues);
        }
    }
    if (self->environmentUnset != NULL) {
        if ((--self->environmentUnset->__rc) <= 0) {
            btrc_Vector_string_free(self->environmentUnset);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void EmulatorLauncher_setEnvironment(LaunchPlan* launchPlan, char* key, char* value) {
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        if (strcmp(btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex), key) == 0) {
            btrc_Vector_string_set(launchPlan->environmentSetValues, keyIndex, value);
            return;
        }
    }
    btrc_Vector_string_push(launchPlan->environmentSetKeys, key);
    btrc_Vector_string_push(launchPlan->environmentSetValues, value);
}

void EmulatorLauncher_unsetEnvironment(LaunchPlan* launchPlan, char* key) {
    if (!btrc_Vector_string_contains(launchPlan->environmentUnset, key)) {
        btrc_Vector_string_push(launchPlan->environmentUnset, key);
    }
}

void EmulatorLauncher_bundleEnvironment(LaunchPlan* launchPlan, char* key, char* value) {
    EmulatorLauncher_setEnvironment(launchPlan, key, value);
    if (strcmp(launchPlan->backend, "flatpak") == 0) {
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("--env=", key)), "=")), value)));
    }
}

void EmulatorLauncher_bundleUnsetEnvironment(LaunchPlan* launchPlan, char* key) {
    EmulatorLauncher_unsetEnvironment(launchPlan, key);
    if (strcmp(launchPlan->backend, "flatpak") == 0) {
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat("--unset-env=", key)));
    }
}

void EmulatorLauncher_applyDisplayServer(LaunchPlan* launchPlan, char* displayServer, char* displayFallback) {
    if (strcmp(displayServer, "x11") == 0) {
        if (strcmp(launchPlan->backend, "flatpak") == 0) {
            btrc_Vector_string_push(launchPlan->argumentVector, "--socket=x11");
        }
        EmulatorLauncher_bundleUnsetEnvironment(launchPlan, "WAYLAND_DISPLAY");
        EmulatorLauncher_bundleEnvironment(launchPlan, "SDL_VIDEODRIVER", "x11");
        EmulatorLauncher_bundleEnvironment(launchPlan, "QT_QPA_PLATFORM", "xcb");
        EmulatorLauncher_setEnvironment(launchPlan, "DISPLAY", (__btrc_isEmpty(displayFallback) ? ":0" : displayFallback));
        EmulatorLauncher_unsetEnvironment(launchPlan, "MESA_LOADER_DRIVER_OVERRIDE");
        EmulatorLauncher_unsetEnvironment(launchPlan, "MESA_GL_VERSION_OVERRIDE");
        EmulatorLauncher_unsetEnvironment(launchPlan, "MESA_GLSL_VERSION_OVERRIDE");
    }
    if (strcmp(displayServer, "wayland") == 0) {
        if (strcmp(launchPlan->backend, "flatpak") == 0) {
            btrc_Vector_string_push(launchPlan->argumentVector, "--socket=wayland");
            btrc_Vector_string_push(launchPlan->argumentVector, "--env=DISPLAY=");
            btrc_Vector_string_push(launchPlan->argumentVector, "--env=XAUTHORITY=");
        }
        EmulatorLauncher_bundleEnvironment(launchPlan, "SDL_VIDEODRIVER", "wayland");
        EmulatorLauncher_bundleEnvironment(launchPlan, "QT_QPA_PLATFORM", "wayland");
        EmulatorLauncher_bundleEnvironment(launchPlan, "GDK_BACKEND", "wayland");
        EmulatorLauncher_bundleEnvironment(launchPlan, "XDG_SESSION_TYPE", "wayland");
        EmulatorLauncher_bundleEnvironment(launchPlan, "WAYLAND_DISPLAY", (__btrc_isEmpty(displayFallback) ? "wayland-0" : displayFallback));
    }
}

void EmulatorLauncher_applyStateEnvironment(LaunchPlan* launchPlan, StatePolicy* state) {
    if (!state->present) {
        return;
    }
    if (__btrc_isEmpty(state->configHome) || (strcmp(state->configHome, "xdg") == 0)) {
        EmulatorLauncher_bundleEnvironment(launchPlan, "XDG_CONFIG_HOME", "${state_root}/config");
    }
    if (__btrc_isEmpty(state->dataHome) || (strcmp(state->dataHome, "xdg") == 0)) {
        EmulatorLauncher_bundleEnvironment(launchPlan, "XDG_DATA_HOME", "${state_root}/data");
        EmulatorLauncher_bundleEnvironment(launchPlan, "XDG_CACHE_HOME", "${state_root}/cache");
    }
}

void EmulatorLauncher_applyContractEnvironment(LaunchPlan* launchPlan, EmulatorPlatform* platform) {
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(platform->environmentSetKeys)); (keyIndex++)) {
        EmulatorLauncher_bundleEnvironment(launchPlan, btrc_Vector_string_get(platform->environmentSetKeys, keyIndex), btrc_Vector_string_get(platform->environmentSetValues, keyIndex));
    }
    btrc_Vector_string* __iter_556 = platform->environmentUnset;
    int __n_558 = btrc_Vector_string_iterLen(__iter_556);
    for (int __i_557 = 0; (__i_557 < __n_558); (__i_557++)) {
        char* key = btrc_Vector_string_iterGet(__iter_556, __i_557);
        EmulatorLauncher_bundleUnsetEnvironment(launchPlan, key);
    }
}

void EmulatorLauncher_pushAllArguments(LaunchPlan* launchPlan, btrc_Vector_string* values) {
    int __n_560 = btrc_Vector_string_iterLen(values);
    for (int __i_559 = 0; (__i_559 < __n_560); (__i_559++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_559);
        btrc_Vector_string_push(launchPlan->argumentVector, value);
    }
}

void EmulatorLauncher_pushMissingPlatformArguments(LaunchPlan* launchPlan, EmulatorPlatform* platform, btrc_Vector_string* systemArguments) {
    int argumentIndex = 0;
    while (argumentIndex < btrc_Vector_string_size(platform->arguments)) {
        char* flag = btrc_Vector_string_get(platform->arguments, argumentIndex);
        btrc_Vector_string* __list_561 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_561, flag);
        btrc_Vector_string* group = __list_561;
        int nextIndex = (argumentIndex + 1);
        while ((nextIndex < btrc_Vector_string_size(platform->arguments)) && (!__btrc_startsWith(btrc_Vector_string_get(platform->arguments, nextIndex), "-"))) {
            btrc_Vector_string_push(group, btrc_Vector_string_get(platform->arguments, nextIndex));
            (nextIndex++);
        }
        if (!btrc_Vector_string_contains(systemArguments, flag)) {
            EmulatorLauncher_pushAllArguments(launchPlan, group);
        }
        (argumentIndex = nextIndex);
        if (group != NULL) {
            if ((--group->__rc) <= 0) {
                btrc_Vector_string_destroy(group);
            }
        }
    }
}

void EmulatorLauncher_pushTailArguments(LaunchPlan* launchPlan, EmulatorPlatform* platform, btrc_Vector_string* systemArguments, char* romPath) {
    EmulatorLauncher_pushAllArguments(launchPlan, platform->state->extraArguments);
    EmulatorLauncher_pushMissingPlatformArguments(launchPlan, platform, systemArguments);
    EmulatorLauncher_pushAllArguments(launchPlan, systemArguments);
    if (!__btrc_isEmpty(romPath)) {
        btrc_Vector_string_push(launchPlan->argumentVector, romPath);
    }
}

void EmulatorLauncher_assembleFlatpak(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath) {
    btrc_Vector_string_push(launchPlan->argumentVector, "flatpak");
    btrc_Vector_string_push(launchPlan->argumentVector, "run");
    btrc_Vector_string* __iter_562 = platform->sandbox->devices;
    int __n_564 = btrc_Vector_string_iterLen(__iter_562);
    for (int __i_563 = 0; (__i_563 < __n_564); (__i_563++)) {
        char* device = btrc_Vector_string_iterGet(__iter_562, __i_563);
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat("--device=", device)));
    }
    btrc_Vector_SandboxFilesystem_p1* __iter_565 = platform->sandbox->filesystems;
    int __n_567 = btrc_Vector_SandboxFilesystem_p1_iterLen(__iter_565);
    for (int __i_566 = 0; (__i_566 < __n_567); (__i_566++)) {
        SandboxFilesystem* filesystemGrant = btrc_Vector_SandboxFilesystem_p1_iterGet(__iter_565, __i_566);
        char* grant = __btrc_str_track(__btrc_strcat("--filesystem=", filesystemGrant->path));
        if ((strcmp(filesystemGrant->mode, "ro") == 0) || (strcmp(filesystemGrant->mode, "create") == 0)) {
            (grant = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(grant, ":")), filesystemGrant->mode)));
        }
        btrc_Vector_string_push(launchPlan->argumentVector, grant);
    }
    btrc_Vector_string* __iter_568 = platform->sandbox->sockets;
    int __n_570 = btrc_Vector_string_iterLen(__iter_568);
    for (int __i_569 = 0; (__i_569 < __n_570); (__i_569++)) {
        char* socket = btrc_Vector_string_iterGet(__iter_568, __i_569);
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat("--socket=", socket)));
    }
    btrc_Vector_string* __iter_571 = platform->sandbox->share;
    int __n_573 = btrc_Vector_string_iterLen(__iter_571);
    for (int __i_572 = 0; (__i_572 < __n_573); (__i_572++)) {
        char* shareGrant = btrc_Vector_string_iterGet(__iter_571, __i_572);
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat("--share=", shareGrant)));
    }
    EmulatorLauncher_applyStateEnvironment(launchPlan, platform->state);
    EmulatorLauncher_applyContractEnvironment(launchPlan, platform);
    EmulatorLauncher_applyDisplayServer(launchPlan, effectiveSession->displayServer, effectiveSession->displayFallback);
    EmulatorLauncher_pushAllArguments(launchPlan, platform->sandbox->extraArguments);
    EmulatorLauncher_unsetEnvironment(launchPlan, "VK_ICD_FILENAMES");
    EmulatorLauncher_unsetEnvironment(launchPlan, "LIBGL_ALWAYS_SOFTWARE");
    char* applicationId = platform->flatpakId;
    if (!__btrc_isEmpty(platform->sandbox->command)) {
        btrc_Vector_string_push(launchPlan->argumentVector, __btrc_str_track(__btrc_strcat("--command=", platform->sandbox->command)));
    }
    if (!__btrc_isEmpty(platform->sandbox->runtime)) {
        (applicationId = platform->sandbox->runtime);
    }
    btrc_Vector_string_push(launchPlan->argumentVector, applicationId);
    EmulatorLauncher_pushTailArguments(launchPlan, platform, systemArguments, romPath);
}

void EmulatorLauncher_assembleExecutable(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath) {
    if (strcmp(platform->glWrapper, "nixgl") == 0) {
        btrc_Vector_string_push(launchPlan->argumentVector, "nixGL");
    }
    btrc_Vector_string_push(launchPlan->argumentVector, platform->executable);
    EmulatorLauncher_applyStateEnvironment(launchPlan, platform->state);
    EmulatorLauncher_applyContractEnvironment(launchPlan, platform);
    EmulatorLauncher_applyDisplayServer(launchPlan, effectiveSession->displayServer, effectiveSession->displayFallback);
    EmulatorLauncher_pushTailArguments(launchPlan, platform, systemArguments, romPath);
}

void EmulatorLauncher_assembleBwrap(LaunchPlan* launchPlan, EmulatorPlatform* platform, SessionOverride* effectiveSession, btrc_Vector_string* systemArguments, char* romPath) {
    btrc_Vector_string* __list_574 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_574, "bwrap");
    btrc_Vector_string_push(__list_574, "--ro-bind");
    btrc_Vector_string_push(__list_574, "/usr");
    btrc_Vector_string_push(__list_574, "/usr");
    btrc_Vector_string_push(__list_574, "--ro-bind");
    btrc_Vector_string_push(__list_574, "/etc");
    btrc_Vector_string_push(__list_574, "/etc");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/opt");
    btrc_Vector_string_push(__list_574, "/opt");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/var");
    btrc_Vector_string_push(__list_574, "/var");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/lib");
    btrc_Vector_string_push(__list_574, "/lib");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/lib64");
    btrc_Vector_string_push(__list_574, "/lib64");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/bin");
    btrc_Vector_string_push(__list_574, "/bin");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/sbin");
    btrc_Vector_string_push(__list_574, "/sbin");
    btrc_Vector_string_push(__list_574, "--proc");
    btrc_Vector_string_push(__list_574, "/proc");
    btrc_Vector_string_push(__list_574, "--dev");
    btrc_Vector_string_push(__list_574, "/dev");
    btrc_Vector_string_push(__list_574, "--dev-bind");
    btrc_Vector_string_push(__list_574, "/dev/dri");
    btrc_Vector_string_push(__list_574, "/dev/dri");
    btrc_Vector_string_push(__list_574, "--dev-bind-try");
    btrc_Vector_string_push(__list_574, "/dev/snd");
    btrc_Vector_string_push(__list_574, "/dev/snd");
    btrc_Vector_string_push(__list_574, "--dev-bind-try");
    btrc_Vector_string_push(__list_574, "/dev/input");
    btrc_Vector_string_push(__list_574, "/dev/input");
    btrc_Vector_string_push(__list_574, "--dev-bind-try");
    btrc_Vector_string_push(__list_574, "/dev/uinput");
    btrc_Vector_string_push(__list_574, "/dev/uinput");
    btrc_Vector_string_push(__list_574, "--ro-bind");
    btrc_Vector_string_push(__list_574, "/sys");
    btrc_Vector_string_push(__list_574, "/sys");
    btrc_Vector_string_push(__list_574, "--bind");
    btrc_Vector_string_push(__list_574, "${state_root}/home");
    btrc_Vector_string_push(__list_574, "${home}");
    btrc_Vector_string_push(__list_574, "--tmpfs");
    btrc_Vector_string_push(__list_574, "/tmp");
    btrc_Vector_string_push(__list_574, "--ro-bind-try");
    btrc_Vector_string_push(__list_574, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_574, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_574, "--bind-try");
    btrc_Vector_string_push(__list_574, "${env:XDG_RUNTIME_DIR}");
    btrc_Vector_string_push(__list_574, "${env:XDG_RUNTIME_DIR}");
    btrc_Vector_string_push(__list_574, "--unshare-pid");
    btrc_Vector_string_push(__list_574, "--unshare-uts");
    btrc_Vector_string_push(__list_574, "--unshare-ipc");
    btrc_Vector_string_push(__list_574, "--share-net");
    btrc_Vector_string_push(__list_574, "--die-with-parent");
    btrc_Vector_string_push(__list_574, "--new-session");
    btrc_Vector_string_push(__list_574, "--setenv");
    btrc_Vector_string_push(__list_574, "HOME");
    btrc_Vector_string_push(__list_574, "${home}");
    btrc_Vector_string_push(__list_574, "--setenv");
    btrc_Vector_string_push(__list_574, "XDG_RUNTIME_DIR");
    btrc_Vector_string_push(__list_574, "${env:XDG_RUNTIME_DIR}");
    btrc_Vector_string* baseArguments = __list_574;
    EmulatorLauncher_pushAllArguments(launchPlan, baseArguments);
    btrc_Vector_SandboxFilesystem_p1* __iter_575 = platform->sandbox->filesystems;
    int __n_577 = btrc_Vector_SandboxFilesystem_p1_iterLen(__iter_575);
    for (int __i_576 = 0; (__i_576 < __n_577); (__i_576++)) {
        SandboxFilesystem* filesystemGrant = btrc_Vector_SandboxFilesystem_p1_iterGet(__iter_575, __i_576);
        if (strcmp(filesystemGrant->mode, "ro") == 0) {
            btrc_Vector_string_push(launchPlan->argumentVector, "--ro-bind-try");
        } else {
            btrc_Vector_string_push(launchPlan->argumentVector, "--bind-try");
        }
        btrc_Vector_string_push(launchPlan->argumentVector, filesystemGrant->path);
        btrc_Vector_string_push(launchPlan->argumentVector, filesystemGrant->path);
    }
    btrc_Vector_string_push(launchPlan->argumentVector, "--");
    btrc_Vector_string_push(launchPlan->argumentVector, platform->executable);
    EmulatorLauncher_applyStateEnvironment(launchPlan, platform->state);
    EmulatorLauncher_applyContractEnvironment(launchPlan, platform);
    EmulatorLauncher_applyDisplayServer(launchPlan, effectiveSession->displayServer, effectiveSession->displayFallback);
    EmulatorLauncher_pushTailArguments(launchPlan, platform, systemArguments, romPath);
    if (btrc_Vector_string_size(platform->sandbox->symlinkLinks) > 0) {
        (launchPlan->note = __btrc_str_track(__btrc_strcat("sandbox symlinks are materialized at the exec edge", " (emulator.json sandbox.symlinks via EmulatorExecution)")));
    }
    if (baseArguments != NULL) {
        if ((--baseArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(baseArguments);
        }
    }
}

LaunchPlan* EmulatorLauncher_plan(EmulatorDefinition* emulator, char* operatingSystem, char* session, char* romPath, btrc_Vector_string* systemArguments) {
    LaunchPlan* launchPlan = LaunchPlan_new();
    EmulatorPlatform* platform = EmulatorDefinition_platformFor(emulator, operatingSystem);
    if (__btrc_isEmpty(platform->operatingSystem)) {
        (launchPlan->note = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("emulator '", emulator->id)), "' declares no '")), operatingSystem)), "' platform")));
        return launchPlan;
    }
    (launchPlan->backend = platform->backend);
    SessionOverride* effectiveSession = EmulatorSessions_effective(platform, session);
    if (strcmp(platform->backend, "flatpak") == 0) {
        EmulatorLauncher_assembleFlatpak(launchPlan, platform, effectiveSession, systemArguments, romPath);
    } else if (strcmp(platform->backend, "bwrap") == 0) {
        EmulatorLauncher_assembleBwrap(launchPlan, platform, effectiveSession, systemArguments, romPath);
    } else {
        EmulatorLauncher_assembleExecutable(launchPlan, platform, effectiveSession, systemArguments, romPath);
    }
    return launchPlan;
    if (launchPlan != NULL) {
        if ((--launchPlan->__rc) <= 0) {
            LaunchPlan_destroy(launchPlan);
        }
    }
}

char* EmulatorLauncher_shellCommand(LaunchPlan* launchPlan) {
    char* command = "";
    btrc_Vector_string* __iter_578 = launchPlan->environmentUnset;
    int __n_580 = btrc_Vector_string_iterLen(__iter_578);
    for (int __i_579 = 0; (__i_579 < __n_580); (__i_579++)) {
        char* key = btrc_Vector_string_iterGet(__iter_578, __i_579);
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, "unset ")), key)), "; ")));
    }
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex))), "=")), ShellWords_quote(btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex)))), " ")));
    }
    btrc_Vector_string* quotedArguments = btrc_Vector_string_new();
    btrc_Vector_string* __iter_581 = launchPlan->argumentVector;
    int __n_583 = btrc_Vector_string_iterLen(__iter_581);
    for (int __i_582 = 0; (__i_582 < __n_583); (__i_582++)) {
        char* argument = btrc_Vector_string_iterGet(__iter_581, __i_582);
        btrc_Vector_string_push(quotedArguments, ShellWords_quote(argument));
    }
    char* __btrc_ret_584 = __btrc_str_track(__btrc_strcat(command, btrc_Vector_string_join(quotedArguments, " ")));
    if (quotedArguments != NULL) {
        if ((--quotedArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(quotedArguments);
        }
    }
    return __btrc_ret_584;
    if (quotedArguments != NULL) {
        if ((--quotedArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(quotedArguments);
        }
    }
}

TemplateContext* EmulatorExecution_contextFor(EmulatorDefinition* emulator, EmulatorPlatform* platform, char* project) {
    TemplateContext* context = TemplateContext_new();
    (context->project = project);
    (context->emulatorId = emulator->id);
    (context->backend = platform->backend);
    (context->flatpakId = platform->flatpakId);
    return context;
    if (context != NULL) {
        if ((--context->__rc) <= 0) {
            TemplateContext_destroy(context);
        }
    }
}

void EmulatorExecution_prepareStateRoot(EmulatorPlatform* platform, TemplateContext* context) {
    char* stateRoot = TemplatePaths_stateRoot(context);
    UnixFileSystem_mkdirp(PathTools_join(stateRoot, "config"));
    UnixFileSystem_mkdirp(PathTools_join(stateRoot, "data"));
    UnixFileSystem_mkdirp(PathTools_join(stateRoot, "cache"));
    if (strcmp(platform->backend, "bwrap") == 0) {
        UnixFileSystem_mkdirp(PathTools_join(stateRoot, "home"));
    }
}

void EmulatorExecution_copyFile(char* sourcePath, char* targetPath) {
    UnixFileSystem_mkdirp(PathTools_dirname(targetPath));
    UnixShell* shell = UnixShell_new();
    UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cp -a ", ShellWords_quote(sourcePath))), " ")), ShellWords_quote(targetPath))), CommandOutput_stream(), CommandOutput_stream(), false, true, false, "", "", "", CommandEnvironment_empty(), false, "");
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void EmulatorExecution_applySeeds(EmulatorPlatform* platform, TemplateContext* context) {
    btrc_Vector_StateSeed_p1* __iter_586 = platform->state->seeds;
    int __n_588 = btrc_Vector_StateSeed_p1_iterLen(__iter_586);
    for (int __i_587 = 0; (__i_587 < __n_588); (__i_587++)) {
        StateSeed* seed = btrc_Vector_StateSeed_p1_iterGet(__iter_586, __i_587);
        char* targetPath = TemplatePaths_resolve(seed->target, context);
        if ((!__btrc_strContains(targetPath, "${")) && (!FileSystem_isFile(targetPath))) {
            bool seeded = false;
            btrc_Vector_string* __iter_589 = seed->sources;
            int __n_591 = btrc_Vector_string_iterLen(__iter_589);
            for (int __i_590 = 0; (__i_590 < __n_591); (__i_590++)) {
                char* source = btrc_Vector_string_iterGet(__iter_589, __i_590);
                char* sourcePath = TemplatePaths_resolve(source, context);
                if (((!seeded) && (!__btrc_strContains(sourcePath, "${"))) && FileSystem_isFile(sourcePath)) {
                    EmulatorExecution_copyFile(sourcePath, targetPath);
                    (seeded = true);
                }
            }
        }
    }
}

void EmulatorExecution_materializeSymlinks(EmulatorPlatform* platform, TemplateContext* context) {
    for (int linkIndex = 0; (linkIndex < btrc_Vector_string_size(platform->sandbox->symlinkLinks)); (linkIndex++)) {
        char* linkPath = TemplatePaths_resolve(btrc_Vector_string_get(platform->sandbox->symlinkLinks, linkIndex), context);
        char* targetPath = TemplatePaths_resolve(btrc_Vector_string_get(platform->sandbox->symlinkTargets, linkIndex), context);
        if ((((!__btrc_isEmpty(linkPath)) && (!__btrc_isEmpty(targetPath))) && (!__btrc_strContains(linkPath, "${"))) && (!__btrc_strContains(targetPath, "${"))) {
            UnixFileSystem_mkdirp(PathTools_dirname(linkPath));
            UnixShell* shell = UnixShell_new();
            UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("ln -sfn ", ShellWords_quote(targetPath))), " ")), ShellWords_quote(linkPath))), CommandOutput_stream(), CommandOutput_stream(), false, true, false, "", "", "", CommandEnvironment_empty(), false, "");
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
        }
    }
}

char* EmulatorExecution_environmentValueFor(LaunchPlan* launchPlan, char* key) {
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        if (strcmp(btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex), key) == 0) {
            return btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex);
        }
    }
    return "";
}

LaunchPlan* EmulatorExecution_resolvePlan(LaunchPlan* launchPlan, TemplateContext* context) {
    LaunchPlan* resolvedPlan = LaunchPlan_new();
    (resolvedPlan->backend = launchPlan->backend);
    (resolvedPlan->note = launchPlan->note);
    btrc_Vector_string* resolvedArguments = TemplatePaths_resolveAll(launchPlan->argumentVector, context);
    if (resolvedPlan->argumentVector != NULL) {
        if ((--resolvedPlan->argumentVector->__rc) <= 0) {
            btrc_Vector_string_free(resolvedPlan->argumentVector);
        }
    }
    (resolvedPlan->argumentVector = resolvedArguments);
    (resolvedPlan->argumentVector->__rc++);
    btrc_Vector_string* __iter_592 = launchPlan->environmentUnset;
    int __n_594 = btrc_Vector_string_iterLen(__iter_592);
    for (int __i_593 = 0; (__i_593 < __n_594); (__i_593++)) {
        char* key = btrc_Vector_string_iterGet(__iter_592, __i_593);
        btrc_Vector_string_push(resolvedPlan->environmentUnset, key);
    }
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        btrc_Vector_string_push(resolvedPlan->environmentSetKeys, btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex));
        btrc_Vector_string_push(resolvedPlan->environmentSetValues, TemplatePaths_resolve(btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex), context));
    }
    return resolvedPlan;
    if (resolvedPlan != NULL) {
        if ((--resolvedPlan->__rc) <= 0) {
            LaunchPlan_destroy(resolvedPlan);
        }
    }
}

void EmulatorExecution_removeEnvironmentKey(LaunchPlan* launchPlan, char* key) {
    btrc_Vector_string* keptKeys = btrc_Vector_string_new();
    btrc_Vector_string* keptValues = btrc_Vector_string_new();
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        if (!(strcmp(btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex), key) == 0)) {
            btrc_Vector_string_push(keptKeys, btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex));
            btrc_Vector_string_push(keptValues, btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex));
        }
    }
    if (launchPlan->environmentSetKeys != NULL) {
        if ((--launchPlan->environmentSetKeys->__rc) <= 0) {
            btrc_Vector_string_free(launchPlan->environmentSetKeys);
        }
    }
    (launchPlan->environmentSetKeys = keptKeys);
    (launchPlan->environmentSetKeys->__rc++);
    if (launchPlan->environmentSetValues != NULL) {
        if ((--launchPlan->environmentSetValues->__rc) <= 0) {
            btrc_Vector_string_free(launchPlan->environmentSetValues);
        }
    }
    (launchPlan->environmentSetValues = keptValues);
    (launchPlan->environmentSetValues->__rc++);
    if (keptValues != NULL) {
        if ((--keptValues->__rc) <= 0) {
            btrc_Vector_string_destroy(keptValues);
        }
    }
    if (keptKeys != NULL) {
        if ((--keptKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(keptKeys);
        }
    }
}

char* EmulatorExecution_x11DisplayPreamble(char* displayFallback) {
    char* fallback = (__btrc_isEmpty(displayFallback) ? ":0" : displayFallback);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if [ -z \"${DISPLAY:-}\" ] || [ -z \"${XAUTHORITY:-}\" ]; then ", "p=$(pgrep -x plasmashell | head -1 || pgrep -x kwin_wayland | head -1 || true); ")), "if [ -n \"$p\" ] && [ -r \"/proc/$p/environ\" ]; then ")), "eval \"$(tr '\\0' '\\n' < /proc/$p/environ")), " | grep -E '^(DISPLAY|XAUTHORITY)=' | sed 's/^/export /')\"; ")), "fi; fi; ")), "if [ -z \"${XAUTHORITY:-}\" ]; then ")), "for xauthFile in /run/user/$(id -u)/xauth_*; do ")), "[ -r \"$xauthFile\" ] && export XAUTHORITY=\"$xauthFile\" && break; done; fi; ")), "export DISPLAY=\"${DISPLAY:-")), fallback)), "}\"; "));
}

char* EmulatorExecution_applyHostDisplayPrecedence(LaunchPlan* resolvedPlan) {
    char* preamble = "";
    char* displayFallback = EmulatorExecution_environmentValueFor(resolvedPlan, "DISPLAY");
    if (!__btrc_isEmpty(displayFallback)) {
        (preamble = EmulatorExecution_x11DisplayPreamble(displayFallback));
        EmulatorExecution_removeEnvironmentKey(resolvedPlan, "DISPLAY");
    }
    char* waylandFallback = EmulatorExecution_environmentValueFor(resolvedPlan, "WAYLAND_DISPLAY");
    if (!__btrc_isEmpty(waylandFallback)) {
        char* liveDisplay = Environment_get("WAYLAND_DISPLAY", waylandFallback);
        for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(resolvedPlan->environmentSetKeys)); (keyIndex++)) {
            if (strcmp(btrc_Vector_string_get(resolvedPlan->environmentSetKeys, keyIndex), "WAYLAND_DISPLAY") == 0) {
                btrc_Vector_string_set(resolvedPlan->environmentSetValues, keyIndex, liveDisplay);
            }
        }
        char* bundledPrefix = "--env=WAYLAND_DISPLAY=";
        for (int argumentIndex = 0; (argumentIndex < btrc_Vector_string_size(resolvedPlan->argumentVector)); (argumentIndex++)) {
            if (__btrc_startsWith(btrc_Vector_string_get(resolvedPlan->argumentVector, argumentIndex), bundledPrefix)) {
                btrc_Vector_string_set(resolvedPlan->argumentVector, argumentIndex, __btrc_str_track(__btrc_strcat(bundledPrefix, liveDisplay)));
            }
        }
    }
    return preamble;
}

int EmulatorExecution_run(EmulatorDefinition* emulator, char* operatingSystem, LaunchPlan* launchPlan, char* project) {
    if (btrc_Vector_string_size(launchPlan->argumentVector) == 0) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("launcher: nothing to exec (", launchPlan->note)), ")")));
        return 1;
    }
    EmulatorPlatform* platform = EmulatorDefinition_platformFor(emulator, operatingSystem);
    TemplateContext* context = EmulatorExecution_contextFor(emulator, platform, project);
    EmulatorExecution_prepareStateRoot(platform, context);
    EmulatorExecution_applySeeds(platform, context);
    EmulatorExecution_materializeSymlinks(platform, context);
    LaunchPlan* resolvedPlan = EmulatorExecution_resolvePlan(launchPlan, context);
    char* preamble = EmulatorExecution_applyHostDisplayPrecedence(resolvedPlan);
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_run(shell, __btrc_str_track(__btrc_strcat(preamble, EmulatorLauncher_shellCommand(resolvedPlan))), CommandOutput_stream(), CommandOutput_stream(), false, true, false, "", "", "", CommandEnvironment_empty(), false, "");
    int __btrc_ret_595 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_595;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void SteamdeckHotkeyAssignments_init(SteamdeckHotkeyAssignments* self) {
    self->__rc = 1;
    if (self->buttons != NULL) {
        if ((--self->buttons->__rc) <= 0) {
            btrc_Vector_string_free(self->buttons);
        }
    }
    btrc_Vector_string* __list_596 = btrc_Vector_string_new();
    (self->buttons = __list_596);
    (self->buttons->__rc++);
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    btrc_Vector_string* __list_597 = btrc_Vector_string_new();
    (self->actionIds = __list_597);
    (self->actionIds->__rc++);
}

SteamdeckHotkeyAssignments* SteamdeckHotkeyAssignments_new(void) {
    SteamdeckHotkeyAssignments* self = ((SteamdeckHotkeyAssignments*)malloc(sizeof(SteamdeckHotkeyAssignments)));
    memset(self, 0, sizeof(SteamdeckHotkeyAssignments));
    SteamdeckHotkeyAssignments_init(self);
    return self;
}

void SteamdeckHotkeyAssignments_destroy(SteamdeckHotkeyAssignments* self) {
    if (self->buttons != NULL) {
        if ((--self->buttons->__rc) <= 0) {
            btrc_Vector_string_free(self->buttons);
        }
    }
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* SteamdeckHotkeyAssignments_hotkeyButtonToken(char* combo) {
    int plusIndex = __btrc_indexOf(combo, "+");
    if (plusIndex < 0) {
        return "";
    }
    if (!(strcmp(__btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(combo, 0, plusIndex)))), "HKB") == 0)) {
        return "";
    }
    return __btrc_str_track(__btrc_trim(__btrc_str_track(__btrc_substring(combo, (plusIndex + 1), ((((int)strlen(combo)) - plusIndex) - 1)))));
}

SteamdeckHotkeyAssignments* SteamdeckHotkeyAssignments_collect(Keymap* keymap) {
    SteamdeckHotkeyAssignments* assignments = SteamdeckHotkeyAssignments_new();
    btrc_Vector_KeymapBinding_p1* __iter_598 = keymap->bindings;
    int __n_600 = btrc_Vector_KeymapBinding_p1_iterLen(__iter_598);
    for (int __i_599 = 0; (__i_599 < __n_600); (__i_599++)) {
        KeymapBinding* binding = btrc_Vector_KeymapBinding_p1_iterGet(__iter_598, __i_599);
        char* button = SteamdeckHotkeyAssignments_hotkeyButtonToken(binding->button);
        if (!__btrc_isEmpty(button)) {
            btrc_Vector_string_push(assignments->buttons, button);
            btrc_Vector_string_push(assignments->actionIds, binding->actionId);
        }
    }
    return assignments;
    if (assignments != NULL) {
        if ((--assignments->__rc) <= 0) {
            SteamdeckHotkeyAssignments_destroy(assignments);
        }
    }
}

char* SteamdeckHotkeyAssignments_actionFor(SteamdeckHotkeyAssignments* self, char* button) {
    for (int buttonIndex = 0; (buttonIndex < btrc_Vector_string_size(self->buttons)); (buttonIndex++)) {
        if (strcmp(btrc_Vector_string_get(self->buttons, buttonIndex), button) == 0) {
            return btrc_Vector_string_get(self->actionIds, buttonIndex);
        }
    }
    return "";
}

bool SteamdeckHotkeyAssignments_anyBound(SteamdeckHotkeyAssignments* self, btrc_Vector_string* candidateButtons) {
    int __n_602 = btrc_Vector_string_iterLen(candidateButtons);
    for (int __i_601 = 0; (__i_601 < __n_602); (__i_601++)) {
        char* candidate = btrc_Vector_string_iterGet(candidateButtons, __i_601);
        if (!__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(self, candidate))) {
            return true;
        }
    }
    return false;
}

char* SteamdeckInputEmitter_keyPressBindings(SteamInputTarget* target, char* chord, char* label) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    char* tokens = ChordTranslator_vdfTokens(chord, target);
    btrc_Vector_string* __iter_603 = Strings_split(tokens, " ");
    int __n_605 = btrc_Vector_string_iterLen(__iter_603);
    for (int __i_604 = 0; (__i_604 < __n_605); (__i_604++)) {
        char* keyName = btrc_Vector_string_iterGet(__iter_603, __i_604);
        btrc_Vector_string_push(fragments, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"key_press ", keyName)), ", ")), label)), ", , \"")));
    }
    char* __btrc_ret_606 = btrc_Vector_string_join(fragments, " ");
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
    return __btrc_ret_606;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

char* SteamdeckInputEmitter_xinputBinding(char* button, char* label) {
    if (__btrc_isEmpty(label)) {
        return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"xinput_button ", button)), ", , \""));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"xinput_button ", button)), ", ")), label)), ", , \""));
}

char* SteamdeckInputEmitter_quitExtraBindings(SteamInputTarget* target, char* label) {
    btrc_Vector_string* fragments = btrc_Vector_string_new();
    btrc_Vector_string* __iter_607 = target->quitExtraBindings;
    int __n_609 = btrc_Vector_string_iterLen(__iter_607);
    for (int __i_608 = 0; (__i_608 < __n_609); (__i_608++)) {
        char* extraBinding = btrc_Vector_string_iterGet(__iter_607, __i_608);
        if (__btrc_startsWith(extraBinding, "xinput_")) {
            char* button = __btrc_str_track(__btrc_toUpper(__btrc_str_track(__btrc_substring(extraBinding, 7, (((int)strlen(extraBinding)) - 7)))));
            btrc_Vector_string_push(fragments, SteamdeckInputEmitter_xinputBinding(button, label));
        } else if (strcmp(extraBinding, "escape") == 0) {
            btrc_Vector_string_push(fragments, SteamdeckInputEmitter_keyPressBindings(target, "Esc", label));
        } else if (strcmp(extraBinding, "alt_f4") == 0) {
            btrc_Vector_string_push(fragments, SteamdeckInputEmitter_keyPressBindings(target, "Alt+F4", label));
        }
    }
    char* __btrc_ret_610 = btrc_Vector_string_join(fragments, " ");
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
    return __btrc_ret_610;
    if (fragments != NULL) {
        if ((--fragments->__rc) <= 0) {
            btrc_Vector_string_destroy(fragments);
        }
    }
}

char* SteamdeckInputEmitter_actionBindings(SteamInputTarget* target, Keymap* keymap, char* actionId) {
    char* label = SteamInputTarget_actionLabel(target, actionId);
    char* chord = Keymap_chordFor(keymap, actionId);
    char* bindings = (__btrc_isEmpty(chord) ? "" : SteamdeckInputEmitter_keyPressBindings(target, chord, label));
    if (strcmp(actionId, "app.quit") == 0) {
        char* extras = SteamdeckInputEmitter_quitExtraBindings(target, label);
        if (!__btrc_isEmpty(extras)) {
            (bindings = (__btrc_isEmpty(bindings) ? extras : __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(bindings, " ")), extras))));
        }
    }
    return bindings;
}

char* SteamdeckInputEmitter_presetHotkeyBinding(void) {
    return "\"binding\" \"controller_action CHANGE_PRESET 2 0 0, Hotkey, , \"";
}

char* SteamdeckInputEmitter_presetDefaultBinding(void) {
    return "\"binding\" \"controller_action CHANGE_PRESET 1 0 0, , \"";
}

char* SteamdeckInputEmitter_inputEntry(char* inputName, char* activator, char* bindings) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\t\t\"", inputName)), "\" { \"activators\" { \"")), activator)), "\" { \"bindings\" { ")), bindings)), " } } } }"));
}

char* SteamdeckInputEmitter_singleInputGroup(char* groupId, char* mode, char* inputName, char* bindings) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\"group\" { \"id\" \"", groupId)), "\" \"mode\" \"")), mode)), "\" \"inputs\" { \"")), inputName)), "\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), bindings)), " } } } } } }"));
}

void SteamdeckInputEmitter_pushGroupHead(btrc_Vector_string* lines, char* groupId, char* mode) {
    btrc_Vector_string_push(lines, "\t\"group\"");
    btrc_Vector_string_push(lines, "\t{");
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\t\"id\" \"", groupId)), "\"")));
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\t\"mode\" \"", mode)), "\"")));
    btrc_Vector_string_push(lines, "\t\t\"inputs\"");
    btrc_Vector_string_push(lines, "\t\t{");
}

void SteamdeckInputEmitter_pushGroupTail(btrc_Vector_string* lines) {
    btrc_Vector_string_push(lines, "\t\t}");
    btrc_Vector_string_push(lines, "\t}");
}

void SteamdeckInputEmitter_pushBoundEntry(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, SteamdeckHotkeyAssignments* assignments, char* button, char* inputName) {
    char* actionId = SteamdeckHotkeyAssignments_actionFor(assignments, button);
    if (!__btrc_isEmpty(actionId)) {
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry(inputName, "Full_Press", SteamdeckInputEmitter_actionBindings(target, keymap, actionId)));
    }
}

void SteamdeckInputEmitter_pushGamepadGroups(btrc_Vector_string* lines, SteamInputTarget* target) {
    SteamdeckInputEmitter_pushGroupHead(lines, "0", "four_buttons");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_a", "Full_Press", SteamdeckInputEmitter_xinputBinding("A", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_b", "Full_Press", SteamdeckInputEmitter_xinputBinding("B", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_x", "Full_Press", SteamdeckInputEmitter_xinputBinding("X", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_y", "Full_Press", SteamdeckInputEmitter_xinputBinding("Y", "")));
    SteamdeckInputEmitter_pushGroupTail(lines);
    SteamdeckInputEmitter_pushGroupHead(lines, "1", "dpad");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("dpad_north", "Full_Press", SteamdeckInputEmitter_xinputBinding("DPAD_UP", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("dpad_south", "Full_Press", SteamdeckInputEmitter_xinputBinding("DPAD_DOWN", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("dpad_east", "Full_Press", SteamdeckInputEmitter_xinputBinding("DPAD_RIGHT", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("dpad_west", "Full_Press", SteamdeckInputEmitter_xinputBinding("DPAD_LEFT", "")));
    SteamdeckInputEmitter_pushGroupTail(lines);
    btrc_Vector_string_push(lines, "\t\"group\" { \"id\" \"2\" \"mode\" \"joystick_move\" }");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("3", "joystick_move", "click", SteamdeckInputEmitter_xinputBinding("JOYSTICK_RIGHT", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("4", "trigger", "click", SteamdeckInputEmitter_xinputBinding("TRIGGER_LEFT", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("5", "trigger", "click", SteamdeckInputEmitter_xinputBinding("TRIGGER_RIGHT", "")));
    if (strcmp(target->trackpadRight, "mouse") == 0) {
        btrc_Vector_string_push(lines, "\t\"group\" { \"id\" \"6\" \"mode\" \"absolute_mouse\" \"inputs\" { \"click\" { \"activators\" { \"Soft_Press\" { \"bindings\" { \"binding\" \"mouse_button LEFT, , \" } } } } } }");
    }
    SteamdeckInputEmitter_pushGroupHead(lines, "7", "switches");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_escape", "Full_Press", SteamdeckInputEmitter_xinputBinding("START", "")));
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\t\t\"button_menu\" { \"activators\" { \"Full_Press\" { \"bindings\" { ", SteamdeckInputEmitter_xinputBinding("SELECT", ""))), " } } \"Long_Press\" { \"bindings\" { ")), SteamdeckInputEmitter_presetHotkeyBinding())), " } } } }")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("left_bumper", "Full_Press", SteamdeckInputEmitter_xinputBinding("SHOULDER_LEFT", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("right_bumper", "Full_Press", SteamdeckInputEmitter_xinputBinding("SHOULDER_RIGHT", "")));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_back_left_upper", "Full_Press", SteamdeckInputEmitter_presetHotkeyBinding()));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_back_right_upper", "Full_Press", SteamdeckInputEmitter_presetHotkeyBinding()));
    SteamdeckInputEmitter_pushGroupTail(lines);
}

void SteamdeckInputEmitter_pushHotkeyGroups(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, SteamdeckHotkeyAssignments* assignments) {
    btrc_Vector_string* __list_611 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_611, "A");
    btrc_Vector_string_push(__list_611, "B");
    btrc_Vector_string_push(__list_611, "X");
    btrc_Vector_string_push(__list_611, "Y");
    btrc_Vector_string* diamondButtons = __list_611;
    if (SteamdeckHotkeyAssignments_anyBound(assignments, diamondButtons)) {
        SteamdeckInputEmitter_pushGroupHead(lines, "10", "four_buttons");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "A", "button_a");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "B", "button_b");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "X", "button_x");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "Y", "button_y");
        SteamdeckInputEmitter_pushGroupTail(lines);
    }
    if (!__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "L2"))) {
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("11", "trigger", "click", SteamdeckInputEmitter_actionBindings(target, keymap, SteamdeckHotkeyAssignments_actionFor(assignments, "L2"))));
    }
    if (!__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "R2"))) {
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("12", "trigger", "click", SteamdeckInputEmitter_actionBindings(target, keymap, SteamdeckHotkeyAssignments_actionFor(assignments, "R2"))));
    }
    btrc_Vector_string* __list_612 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_612, "D-Pad Up");
    btrc_Vector_string_push(__list_612, "D-Pad Down");
    btrc_Vector_string_push(__list_612, "D-Pad Left");
    btrc_Vector_string_push(__list_612, "D-Pad Right");
    btrc_Vector_string* directionalPadButtons = __list_612;
    if (SteamdeckHotkeyAssignments_anyBound(assignments, directionalPadButtons)) {
        SteamdeckInputEmitter_pushGroupHead(lines, "13", "dpad");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "D-Pad Up", "dpad_north");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "D-Pad Down", "dpad_south");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "D-Pad Right", "dpad_east");
        SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "D-Pad Left", "dpad_west");
        SteamdeckInputEmitter_pushGroupTail(lines);
    }
    SteamdeckInputEmitter_pushGroupHead(lines, "14", "switches");
    SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "Start", "button_escape");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_menu", "release", SteamdeckInputEmitter_presetDefaultBinding()));
    SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "L1", "left_bumper");
    SteamdeckInputEmitter_pushBoundEntry(lines, target, keymap, assignments, "R1", "right_bumper");
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_back_left_upper", "release", SteamdeckInputEmitter_presetDefaultBinding()));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry("button_back_right_upper", "release", SteamdeckInputEmitter_presetDefaultBinding()));
    SteamdeckInputEmitter_pushGroupTail(lines);
    if (!__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "L3"))) {
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("15", "joystick_move", "click", SteamdeckInputEmitter_actionBindings(target, keymap, SteamdeckHotkeyAssignments_actionFor(assignments, "L3"))));
    }
    if (!__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "R3"))) {
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_singleInputGroup("16", "joystick_move", "click", SteamdeckInputEmitter_actionBindings(target, keymap, SteamdeckHotkeyAssignments_actionFor(assignments, "R3"))));
    }
    if (directionalPadButtons != NULL) {
        if ((--directionalPadButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(directionalPadButtons);
        }
    }
    if (diamondButtons != NULL) {
        if ((--diamondButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(diamondButtons);
        }
    }
}

void SteamdeckInputEmitter_pushRadialGroup(btrc_Vector_string* lines, SteamInputTarget* target, Keymap* keymap, char* radialName) {
    btrc_Vector_string_push(lines, "\t\"group\"");
    btrc_Vector_string_push(lines, "\t{");
    btrc_Vector_string_push(lines, "\t\t\"id\" \"20\"");
    btrc_Vector_string_push(lines, "\t\t\"mode\" \"radial_menu\"");
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\t\"name\" \"", radialName)), "\"")));
    btrc_Vector_string_push(lines, "\t\t\"inputs\"");
    btrc_Vector_string_push(lines, "\t\t{");
    for (int slotIndex = 0; (slotIndex < btrc_Vector_string_size(target->radialSlots)); (slotIndex++)) {
        int __fstr_613_arg0 = slotIndex;
        int __fstr_613_len = snprintf(NULL, 0, "touch_menu_button_%d", __fstr_613_arg0);
        char* __fstr_613_buf = __btrc_str_track(((char*)malloc((__fstr_613_len + 1))));
        snprintf(__fstr_613_buf, (__fstr_613_len + 1), "touch_menu_button_%d", __fstr_613_arg0);
        btrc_Vector_string_push(lines, SteamdeckInputEmitter_inputEntry(__fstr_613_buf, "Full_Press", SteamdeckInputEmitter_actionBindings(target, keymap, btrc_Vector_string_get(target->radialSlots, slotIndex))));
    }
    SteamdeckInputEmitter_pushGroupTail(lines);
}

char* SteamdeckInputEmitter_presetLine(char* presetId, char* presetName, btrc_Vector_string* groupIds, SteamInputTarget* target) {
    char* bindings = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", btrc_Vector_string_get(groupIds, 0))), "\" \"button_diamond active\"")), " \"")), btrc_Vector_string_get(groupIds, 1))), "\" \"dpad active\"")), " \"")), btrc_Vector_string_get(groupIds, 2))), "\" \"joystick active\"")), " \"")), btrc_Vector_string_get(groupIds, 3))), "\" \"right_joystick active\"")), " \"")), btrc_Vector_string_get(groupIds, 4))), "\" \"left_trigger active\"")), " \"")), btrc_Vector_string_get(groupIds, 5))), "\" \"right_trigger active\""));
    if (strcmp(target->trackpadRight, "mouse") == 0) {
        (bindings = __btrc_str_track(__btrc_strcat(bindings, " \"6\" \"right_trackpad active\"")));
    }
    (bindings = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(bindings, " \"")), btrc_Vector_string_get(groupIds, 6))), "\" \"switch active\"")));
    if (strcmp(target->trackpadLeft, "radial_hotkeys") == 0) {
        (bindings = __btrc_str_track(__btrc_strcat(bindings, " \"20\" \"left_trackpad active\"")));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\"preset\" { \"id\" \"", presetId)), "\" \"name\" \"")), presetName)), "\" \"group_source_bindings\" { ")), bindings)), " } }"));
}

btrc_Vector_string* SteamdeckInputEmitter_hotkeyPresetGroupIds(SteamdeckHotkeyAssignments* assignments) {
    btrc_Vector_string* __list_614 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_614, "A");
    btrc_Vector_string_push(__list_614, "B");
    btrc_Vector_string_push(__list_614, "X");
    btrc_Vector_string_push(__list_614, "Y");
    btrc_Vector_string* diamondButtons = __list_614;
    btrc_Vector_string* __list_615 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_615, "D-Pad Up");
    btrc_Vector_string_push(__list_615, "D-Pad Down");
    btrc_Vector_string_push(__list_615, "D-Pad Left");
    btrc_Vector_string_push(__list_615, "D-Pad Right");
    btrc_Vector_string* directionalPadButtons = __list_615;
    btrc_Vector_string* groupIds = btrc_Vector_string_new();
    btrc_Vector_string_push(groupIds, (SteamdeckHotkeyAssignments_anyBound(assignments, diamondButtons) ? "10" : "0"));
    btrc_Vector_string_push(groupIds, (SteamdeckHotkeyAssignments_anyBound(assignments, directionalPadButtons) ? "13" : "1"));
    btrc_Vector_string_push(groupIds, (__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "L3")) ? "2" : "15"));
    btrc_Vector_string_push(groupIds, (__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "R3")) ? "3" : "16"));
    btrc_Vector_string_push(groupIds, (__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "L2")) ? "4" : "11"));
    btrc_Vector_string_push(groupIds, (__btrc_isEmpty(SteamdeckHotkeyAssignments_actionFor(assignments, "R2")) ? "5" : "12"));
    btrc_Vector_string_push(groupIds, "14");
    if (directionalPadButtons != NULL) {
        if ((--directionalPadButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(directionalPadButtons);
        }
    }
    if (diamondButtons != NULL) {
        if ((--diamondButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(diamondButtons);
        }
    }
    return groupIds;
    if (groupIds != NULL) {
        if ((--groupIds->__rc) <= 0) {
            btrc_Vector_string_destroy(groupIds);
        }
    }
    if (directionalPadButtons != NULL) {
        if ((--directionalPadButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(directionalPadButtons);
        }
    }
    if (diamondButtons != NULL) {
        if ((--diamondButtons->__rc) <= 0) {
            btrc_Vector_string_destroy(diamondButtons);
        }
    }
}

char* SteamdeckInputEmitter_templateVdf(SteamInputTarget* target, Keymap* keymap, char* title, char* radialName) {
    SteamdeckHotkeyAssignments* assignments = SteamdeckHotkeyAssignments_collect(keymap);
    btrc_Vector_string* lines = btrc_Vector_string_new();
    btrc_Vector_string_push(lines, "\"controller_mappings\"");
    btrc_Vector_string_push(lines, "{");
    btrc_Vector_string_push(lines, "\t\"version\"\t\t\"3\"");
    btrc_Vector_string_push(lines, "\t\"revision\"\t\t\"1\"");
    btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\t\"title\"\t\t\"", title)), "\"")));
    btrc_Vector_string_push(lines, "\t\"description\"\t\t\"Steam Deck controls for Semu: gyro opt-in, right pad mouse, left pad hotkeys.\"");
    btrc_Vector_string_push(lines, "\t\"creator\"\t\t\"Semu\"");
    btrc_Vector_string_push(lines, "\t\"controller_type\"\t\t\"controller_neptune\"");
    btrc_Vector_string_push(lines, "\t\"actions\"");
    btrc_Vector_string_push(lines, "\t{");
    btrc_Vector_string_push(lines, "\t\t\"Default\" { \"title\" \"Gamepad\" \"legacy_set\" \"1\" }");
    btrc_Vector_string_push(lines, "\t\t\"Preset_1000001\" { \"title\" \"Hotkeys\" \"legacy_set\" \"1\" }");
    btrc_Vector_string_push(lines, "\t}");
    SteamdeckInputEmitter_pushGamepadGroups(lines, target);
    SteamdeckInputEmitter_pushHotkeyGroups(lines, target, keymap, assignments);
    if (strcmp(target->trackpadLeft, "radial_hotkeys") == 0) {
        SteamdeckInputEmitter_pushRadialGroup(lines, target, keymap, radialName);
    }
    btrc_Vector_string* __list_616 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_616, "0");
    btrc_Vector_string_push(__list_616, "1");
    btrc_Vector_string_push(__list_616, "2");
    btrc_Vector_string_push(__list_616, "3");
    btrc_Vector_string_push(__list_616, "4");
    btrc_Vector_string_push(__list_616, "5");
    btrc_Vector_string_push(__list_616, "7");
    btrc_Vector_string* gamepadGroupIds = __list_616;
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_presetLine("0", "Default", gamepadGroupIds, target));
    btrc_Vector_string_push(lines, SteamdeckInputEmitter_presetLine("1", "Preset_1000001", SteamdeckInputEmitter_hotkeyPresetGroupIds(assignments), target));
    btrc_Vector_string_push(lines, "\t\"settings\" { \"left_trackpad_mode\" \"0\" \"right_trackpad_mode\" \"0\" }");
    btrc_Vector_string_push(lines, "}");
    char* __btrc_ret_617 = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
    if (gamepadGroupIds != NULL) {
        if ((--gamepadGroupIds->__rc) <= 0) {
            btrc_Vector_string_destroy(gamepadGroupIds);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
    return __btrc_ret_617;
    if (gamepadGroupIds != NULL) {
        if ((--gamepadGroupIds->__rc) <= 0) {
            btrc_Vector_string_destroy(gamepadGroupIds);
        }
    }
    if (lines != NULL) {
        if ((--lines->__rc) <= 0) {
            btrc_Vector_string_destroy(lines);
        }
    }
}

void SteamdeckInputEmitter_emitTemplates(SteamInputTarget* target, Keymap* keymap, char* outputDirectory) {
    UnixFileSystem_mkdirp(outputDirectory);
    for (int templateIndex = 0; (templateIndex < btrc_Vector_string_size(target->templateIds)); (templateIndex++)) {
        char* content = SteamdeckInputEmitter_templateVdf(target, keymap, btrc_Vector_string_get(target->templateTitles, templateIndex), btrc_Vector_string_get(target->templateRadialNames, templateIndex));
        Path_writeAll(PathTools_join(outputDirectory, btrc_Vector_string_get(target->templateOutputs, templateIndex)), content);
    }
}

char* AppImagePackage_templateDirectory(char* repoRoot) {
    return PathTools_join(repoRoot, "src/semu/packaging/appimage");
}

char* AppImagePackage_renderTemplate(char* repoRoot, char* templateName) {
    char* templatePath = PathTools_join(AppImagePackage_templateDirectory(repoRoot), __btrc_str_track(__btrc_strcat(templateName, ".template")));
    if (!FileSystem_isFile(templatePath)) {
        return "";
    }
    return Path_readAll(templatePath);
}

bool AppImagePackage_emitTemplate(char* repoRoot, char* templateName, char* outputPath, bool executable) {
    char* rendered = AppImagePackage_renderTemplate(repoRoot, templateName);
    if (__btrc_isEmpty(rendered)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("appimage: missing template ", templateName)), ".template under ")), AppImagePackage_templateDirectory(repoRoot))));
        return false;
    }
    Path_writeAll(outputPath, rendered);
    if (executable) {
        FileSystem_chmod(outputPath, 493);
    }
    return true;
}

void AppImagePackage_stageCliBinary(char* repoRoot, char* binDirectory) {
    char* builtCli = PathTools_join(repoRoot, "src/generated/build/semu");
    if (!FileSystem_isFile(builtCli)) {
        return;
    }
    char* stagedCli = PathTools_join(binDirectory, "semu");
    UnixShell* shell = UnixShell_new();
    UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cp -f ", ShellWords_quote(builtCli))), " ")), ShellWords_quote(stagedCli))), CommandOutput_stream(), CommandOutput_stream(), false, true, false, "", "", "", CommandEnvironment_empty(), false, "");
    FileSystem_chmod(stagedCli, 493);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void AppImagePackage_emitForEmulators(btrc_Vector_EmulatorDefinition_p1* emulators, char* repoRoot, char* outputRoot) {
    char* linuxRoot = PathTools_join(outputRoot, "packaging/linux");
    UnixFileSystem_mkdirp(linuxRoot);
    AppImagePackage_emitTemplate(repoRoot, "AppRun", PathTools_join(linuxRoot, "AppRun"), true);
    AppImagePackage_emitTemplate(repoRoot, "semu.desktop", PathTools_join(linuxRoot, "semu.desktop"), false);
    char* binDirectory = PathTools_join(linuxRoot, "bin");
    EmulatorEmitter_emitShims(emulators, binDirectory);
    AppImagePackage_stageCliBinary(repoRoot, binDirectory);
}

char* SystemContractTest_aspectLabel(SystemDefinition* systemDefinition) {
    if (btrc_Vector_SystemScreen_p1_size(systemDefinition->display->screens) > 1) {
        char* __fstr_621_arg0 = systemDefinition->display->arrangement;
        int __fstr_621_len = snprintf(NULL, 0, "dual/%s", __fstr_621_arg0);
        char* __fstr_621_buf = __btrc_str_track(((char*)malloc((__fstr_621_len + 1))));
        snprintf(__fstr_621_buf, (__fstr_621_len + 1), "dual/%s", __fstr_621_arg0);
        return __fstr_621_buf;
    }
    if (systemDefinition->display->aspect->set) {
        int __fstr_622_arg0 = systemDefinition->display->aspect->w;
        int __fstr_622_arg1 = systemDefinition->display->aspect->h;
        int __fstr_622_len = snprintf(NULL, 0, "%d:%d", __fstr_622_arg0, __fstr_622_arg1);
        char* __fstr_622_buf = __btrc_str_track(((char*)malloc((__fstr_622_len + 1))));
        snprintf(__fstr_622_buf, (__fstr_622_len + 1), "%d:%d", __fstr_622_arg0, __fstr_622_arg1);
        char* base = __fstr_622_buf;
        if (systemDefinition->display->widescreen->set) {
            int __fstr_623_arg0 = systemDefinition->display->widescreen->aspect->w;
            int __fstr_623_arg1 = systemDefinition->display->widescreen->aspect->h;
            int __fstr_623_len = snprintf(NULL, 0, "+wide %d:%d", __fstr_623_arg0, __fstr_623_arg1);
            char* __fstr_623_buf = __btrc_str_track(((char*)malloc((__fstr_623_len + 1))));
            snprintf(__fstr_623_buf, (__fstr_623_len + 1), "+wide %d:%d", __fstr_623_arg0, __fstr_623_arg1);
            return __btrc_str_track(__btrc_strcat(base, __fstr_623_buf));
        }
        return base;
    }
    return "?";
}

char* SystemContractTest_defaultEmulatorLabel(SystemDefinition* systemDefinition) {
    if (btrc_Vector_EmulatorBinding_p1_size(systemDefinition->emulators) == 0) {
        return "?";
    }
    EmulatorBinding* firstBinding = btrc_Vector_EmulatorBinding_p1_get(systemDefinition->emulators, 0);
    if (__btrc_isEmpty(firstBinding->core)) {
        return firstBinding->emulator;
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(firstBinding->emulator, "/")), firstBinding->core));
}

int SystemContractTest_run(char* repoRoot) {
    char* sourceRoot = Environment_get("SEMU_SRC_ROOT", PathTools_join(repoRoot, "src/semu"));
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(sourceRoot, log);
    int __fstr_625_arg0 = btrc_Vector_EmulatorDefinition_p1_size(contracts->emulators);
    int __fstr_625_len = snprintf(NULL, 0, "emulators: %d", __fstr_625_arg0);
    char* __fstr_625_buf = __btrc_str_track(((char*)malloc((__fstr_625_len + 1))));
    snprintf(__fstr_625_buf, (__fstr_625_len + 1), "emulators: %d", __fstr_625_arg0);
    printf("%s\n", __fstr_625_buf);
    btrc_Vector_EmulatorDefinition_p1* __iter_626 = contracts->emulators;
    int __n_628 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_626);
    for (int __i_627 = 0; (__i_627 < __n_628); (__i_627++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_626, __i_627);
        char* platformList = "";
        btrc_Vector_EmulatorPlatform_p1* __iter_629 = emulatorDefinition->platforms;
        int __n_631 = btrc_Vector_EmulatorPlatform_p1_iterLen(__iter_629);
        for (int __i_630 = 0; (__i_630 < __n_631); (__i_630++)) {
            EmulatorPlatform* platform = btrc_Vector_EmulatorPlatform_p1_iterGet(__iter_629, __i_630);
            (platformList = (__btrc_isEmpty(platformList) ? platform->operatingSystem : __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(platformList, ",")), platform->operatingSystem))));
        }
        char* defaultLabel = (__btrc_isEmpty(emulatorDefinition->defaultSystem) ? "" : __btrc_str_track(__btrc_strcat(" default=", emulatorDefinition->defaultSystem)));
        char* __fstr_633_arg0 = emulatorDefinition->id;
        char* __fstr_633_arg1 = emulatorDefinition->kind;
        char* __fstr_633_arg2 = platformList;
        char* __fstr_633_arg3 = defaultLabel;
        int __fstr_633_len = snprintf(NULL, 0, "  %s: %s [%s]%s", __fstr_633_arg0, __fstr_633_arg1, __fstr_633_arg2, __fstr_633_arg3);
        char* __fstr_633_buf = __btrc_str_track(((char*)malloc((__fstr_633_len + 1))));
        snprintf(__fstr_633_buf, (__fstr_633_len + 1), "  %s: %s [%s]%s", __fstr_633_arg0, __fstr_633_arg1, __fstr_633_arg2, __fstr_633_arg3);
        printf("%s\n", __fstr_633_buf);
    }
    int __fstr_635_arg0 = btrc_Vector_SystemDefinition_p1_size(contracts->systems);
    int __fstr_635_len = snprintf(NULL, 0, "systems: %d", __fstr_635_arg0);
    char* __fstr_635_buf = __btrc_str_track(((char*)malloc((__fstr_635_len + 1))));
    snprintf(__fstr_635_buf, (__fstr_635_len + 1), "systems: %d", __fstr_635_arg0);
    printf("%s\n", __fstr_635_buf);
    btrc_Vector_SystemDefinition_p1* __iter_636 = contracts->systems;
    int __n_638 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_636);
    for (int __i_637 = 0; (__i_637 < __n_638); (__i_637++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_636, __i_637);
        char* aspect = SystemContractTest_aspectLabel(systemDefinition);
        int __fstr_639_arg0 = btrc_Vector_BezelVariant_p1_size(systemDefinition->bezels->variants);
        int __fstr_639_len = snprintf(NULL, 0, "%d bezel variants", __fstr_639_arg0);
        char* __fstr_639_buf = __btrc_str_track(((char*)malloc((__fstr_639_len + 1))));
        snprintf(__fstr_639_buf, (__fstr_639_len + 1), "%d bezel variants", __fstr_639_arg0);
        char* bezelLabel = (systemDefinition->bezels->enabled ? __fstr_639_buf : "bezels off");
        char* shaderLabel = (systemDefinition->shaders->enabled ? (__btrc_isEmpty(systemDefinition->shaders->composite) ? "screen shader" : "screen+composite shaders") : "shaders off");
        char* __fstr_641_arg0 = systemDefinition->id;
        char* __fstr_641_arg1 = SystemContractTest_defaultEmulatorLabel(systemDefinition);
        char* __fstr_641_arg2 = aspect;
        char* __fstr_641_arg3 = bezelLabel;
        char* __fstr_641_arg4 = shaderLabel;
        int __fstr_641_len = snprintf(NULL, 0, "  %s: %s %s | %s | %s", __fstr_641_arg0, __fstr_641_arg1, __fstr_641_arg2, __fstr_641_arg3, __fstr_641_arg4);
        char* __fstr_641_buf = __btrc_str_track(((char*)malloc((__fstr_641_len + 1))));
        snprintf(__fstr_641_buf, (__fstr_641_len + 1), "  %s: %s %s | %s | %s", __fstr_641_arg0, __fstr_641_arg1, __fstr_641_arg2, __fstr_641_arg3, __fstr_641_arg4);
        printf("%s\n", __fstr_641_buf);
    }
    if (!ParseLog_ok(log)) {
        int __fstr_643_arg0 = btrc_Vector_string_size(log->errors);
        int __fstr_643_len = snprintf(NULL, 0, "FAIL: %d contract violations", __fstr_643_arg0);
        char* __fstr_643_buf = __btrc_str_track(((char*)malloc((__fstr_643_len + 1))));
        snprintf(__fstr_643_buf, (__fstr_643_len + 1), "FAIL: %d contract violations", __fstr_643_arg0);
        printf("%s\n", __fstr_643_buf);
        btrc_Vector_string* __iter_644 = log->errors;
        int __n_646 = btrc_Vector_string_iterLen(__iter_644);
        for (int __i_645 = 0; (__i_645 < __n_646); (__i_645++)) {
            char* error = btrc_Vector_string_iterGet(__iter_644, __i_645);
            printf("%s\n", __btrc_str_track(__btrc_strcat("  ", error)));
        }
        int __btrc_ret_647 = btrc_Vector_string_size(log->errors);
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_647;
    }
    printf("%s\n", "system_parser: all contracts valid");
    int __btrc_ret_648 = 0;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_648;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

int EmulatorContractTest_fail(char* message) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("  FAIL ", message)));
    return 1;
}

bool EmulatorContractTest_argumentVectorHas(LaunchPlan* launchPlan, char* expected) {
    return btrc_Vector_string_contains(launchPlan->argumentVector, expected);
}

char* EmulatorContractTest_environmentValue(LaunchPlan* launchPlan, char* key) {
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        if (strcmp(btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex), key) == 0) {
            return btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex);
        }
    }
    return "";
}

EmulatorDefinition* EmulatorContractTest_findEmulator(SemuContracts* contracts, char* emulatorId) {
    btrc_Vector_EmulatorDefinition_p1* __iter_649 = contracts->emulators;
    int __n_651 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_649);
    for (int __i_650 = 0; (__i_650 < __n_651); (__i_650++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_649, __i_650);
        if (strcmp(emulatorDefinition->id, emulatorId) == 0) {
            return emulatorDefinition;
        }
    }
    return EmulatorDefinition_new();
}

SystemDefinition* EmulatorContractTest_findSystem(SemuContracts* contracts, char* systemId) {
    btrc_Vector_SystemDefinition_p1* __iter_652 = contracts->systems;
    int __n_654 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_652);
    for (int __i_653 = 0; (__i_653 < __n_654); (__i_653++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_652, __i_653);
        if (strcmp(systemDefinition->id, systemId) == 0) {
            return systemDefinition;
        }
    }
    return SystemDefinition_new();
}

EmulatorBinding* EmulatorContractTest_findBinding(SystemDefinition* systemDefinition, char* emulatorId) {
    btrc_Vector_EmulatorBinding_p1* __iter_655 = systemDefinition->emulators;
    int __n_657 = btrc_Vector_EmulatorBinding_p1_iterLen(__iter_655);
    for (int __i_656 = 0; (__i_656 < __n_657); (__i_656++)) {
        EmulatorBinding* binding = btrc_Vector_EmulatorBinding_p1_iterGet(__iter_655, __i_656);
        if (strcmp(binding->emulator, emulatorId) == 0) {
            return binding;
        }
    }
    return EmulatorBinding_new();
}

int EmulatorContractTest_checkDolphinPlan(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* dolphin = EmulatorContractTest_findEmulator(contracts, "dolphin");
    btrc_Vector_string* noArguments = btrc_Vector_string_new();
    LaunchPlan* dolphinPlan = EmulatorLauncher_plan(dolphin, "linux", "gamescope", "/roms/gc/game.iso", noArguments);
    if (!(strcmp(dolphinPlan->backend, "flatpak") == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("dolphin linux backend: got '", dolphinPlan->backend)), "'")))));
    }
    if (!EmulatorContractTest_argumentVectorHas(dolphinPlan, "--socket=x11")) {
        (failures = (failures + EmulatorContractTest_fail("dolphin gamescope plan lacks --socket=x11")));
    }
    if (!btrc_Vector_string_contains(dolphinPlan->environmentUnset, "WAYLAND_DISPLAY")) {
        (failures = (failures + EmulatorContractTest_fail("dolphin gamescope plan does not unset WAYLAND_DISPLAY")));
    }
    if (!EmulatorContractTest_argumentVectorHas(dolphinPlan, "--unset-env=WAYLAND_DISPLAY")) {
        (failures = (failures + EmulatorContractTest_fail("dolphin gamescope plan lacks --unset-env=WAYLAND_DISPLAY")));
    }
    if (!(strcmp(EmulatorContractTest_environmentValue(dolphinPlan, "DISPLAY"), ":1") == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("dolphin gamescope DISPLAY: got '", EmulatorContractTest_environmentValue(dolphinPlan, "DISPLAY"))), "' (want ':1')")))));
    }
    if (!EmulatorContractTest_argumentVectorHas(dolphinPlan, "org.DolphinEmu.dolphin-emu")) {
        (failures = (failures + EmulatorContractTest_fail("dolphin plan lacks the flatpak app id")));
    }
    if (!EmulatorContractTest_argumentVectorHas(dolphinPlan, "/roms/gc/game.iso")) {
        (failures = (failures + EmulatorContractTest_fail("dolphin plan lacks the rom path")));
    }
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
    return failures;
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
}

int EmulatorContractTest_countInArgumentVector(LaunchPlan* launchPlan, char* expected) {
    int count = 0;
    btrc_Vector_string* __iter_658 = launchPlan->argumentVector;
    int __n_660 = btrc_Vector_string_iterLen(__iter_658);
    for (int __i_659 = 0; (__i_659 < __n_660); (__i_659++)) {
        char* argument = btrc_Vector_string_iterGet(__iter_658, __i_659);
        if (strcmp(argument, expected) == 0) {
            (count++);
        }
    }
    return count;
}

int EmulatorContractTest_checkDolphinEsDeTailPlan(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* dolphin = EmulatorContractTest_findEmulator(contracts, "dolphin");
    btrc_Vector_string* __list_661 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_661, "--config");
    btrc_Vector_string_push(__list_661, "Dolphin.Display.Fullscreen=True");
    btrc_Vector_string_push(__list_661, "-b");
    btrc_Vector_string_push(__list_661, "-e");
    btrc_Vector_string_push(__list_661, "/roms/wii/game.rvz");
    btrc_Vector_string* esDeTail = __list_661;
    LaunchPlan* tailPlan = EmulatorLauncher_plan(dolphin, "linux", "gamescope", "", esDeTail);
    if (((EmulatorContractTest_countInArgumentVector(tailPlan, "--config") != 1) || (EmulatorContractTest_countInArgumentVector(tailPlan, "-b") != 1)) || (EmulatorContractTest_countInArgumentVector(tailPlan, "-e") != 1)) {
        (failures = (failures + EmulatorContractTest_fail("dolphin es-de tail plan duplicated platform arguments")));
    }
    int lastIndex = (btrc_Vector_string_size(tailPlan->argumentVector) - 1);
    if ((lastIndex < 0) || (!(strcmp(btrc_Vector_string_get(tailPlan->argumentVector, lastIndex), "/roms/wii/game.rvz") == 0))) {
        (failures = (failures + EmulatorContractTest_fail("dolphin es-de tail plan must keep the rom as the final argument")));
    }
    if (EmulatorContractTest_countInArgumentVector(tailPlan, "Dolphin.Display.Fullscreen=True") != 1) {
        (failures = (failures + EmulatorContractTest_fail("dolphin es-de tail plan duplicated the --config value")));
    }
    if (esDeTail != NULL) {
        if ((--esDeTail->__rc) <= 0) {
            btrc_Vector_string_destroy(esDeTail);
        }
    }
    return failures;
    if (esDeTail != NULL) {
        if ((--esDeTail->__rc) <= 0) {
            btrc_Vector_string_destroy(esDeTail);
        }
    }
}

int EmulatorContractTest_checkRetroArchLinuxPlan(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* retroArch = EmulatorContractTest_findEmulator(contracts, "retroarch");
    btrc_Vector_string* noArguments = btrc_Vector_string_new();
    LaunchPlan* retroArchPlan = EmulatorLauncher_plan(retroArch, "linux", "gamescope", "", noArguments);
    if (!EmulatorContractTest_argumentVectorHas(retroArchPlan, "--command=${asset_root}/bin/retroarch-semu")) {
        (failures = (failures + EmulatorContractTest_fail("retroarch linux plan lacks the tap --command override")));
    }
    if (!EmulatorContractTest_argumentVectorHas(retroArchPlan, "org.kde.Sdk//6.10")) {
        (failures = (failures + EmulatorContractTest_fail("retroarch linux plan does not run in the SDK runtime")));
    }
    if (EmulatorContractTest_argumentVectorHas(retroArchPlan, "org.libretro.RetroArch")) {
        (failures = (failures + EmulatorContractTest_fail("retroarch linux plan must replace the stock app id with the runtime")));
    }
    if ((((!EmulatorContractTest_argumentVectorHas(retroArchPlan, "--devel")) || (!EmulatorContractTest_argumentVectorHas(retroArchPlan, "--socket=pulseaudio"))) || (!EmulatorContractTest_argumentVectorHas(retroArchPlan, "--share=ipc"))) || (!EmulatorContractTest_argumentVectorHas(retroArchPlan, "--filesystem=host"))) {
        (failures = (failures + EmulatorContractTest_fail("retroarch linux plan lacks the sandbox grants (devel/pulseaudio/ipc/host)")));
    }
    if (!(strcmp(EmulatorContractTest_environmentValue(retroArchPlan, "DISPLAY"), ":1") == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("retroarch gamescope DISPLAY: got '", EmulatorContractTest_environmentValue(retroArchPlan, "DISPLAY"))), "' (want ':1')")))));
    }
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
    return failures;
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
}

int EmulatorContractTest_checkRetroArchMacosPlan(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* retroArch = EmulatorContractTest_findEmulator(contracts, "retroarch");
    btrc_Vector_string* noArguments = btrc_Vector_string_new();
    LaunchPlan* retroArchMacPlan = EmulatorLauncher_plan(retroArch, "macos", "", "/roms/gb/game.gb", noArguments);
    if (!(strcmp(retroArchMacPlan->backend, "nix") == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("retroarch macos backend: got '", retroArchMacPlan->backend)), "'")))));
    }
    if ((btrc_Vector_string_size(retroArchMacPlan->argumentVector) == 0) || (!(strcmp(btrc_Vector_string_get(retroArchMacPlan->argumentVector, 0), "${nix_result}/bin/retroarch") == 0))) {
        (failures = (failures + EmulatorContractTest_fail("retroarch macos argv[0] must be the nix executable")));
    }
    if (!EmulatorContractTest_argumentVectorHas(retroArchMacPlan, "-f")) {
        (failures = (failures + EmulatorContractTest_fail("retroarch macos plan lacks the -f platform arg")));
    }
    if ((btrc_Vector_string_size(retroArchMacPlan->argumentVector) == 0) || (!(strcmp(btrc_Vector_string_get(retroArchMacPlan->argumentVector, (btrc_Vector_string_size(retroArchMacPlan->argumentVector) - 1)), "/roms/gb/game.gb") == 0))) {
        (failures = (failures + EmulatorContractTest_fail("retroarch macos plan must end with the rom path")));
    }
    if (EmulatorContractTest_argumentVectorHas(retroArchMacPlan, "--socket=x11")) {
        (failures = (failures + EmulatorContractTest_fail("retroarch macos plan must not carry linux display sockets")));
    }
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
    return failures;
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
}

int EmulatorContractTest_checkAresPlans(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* ares = EmulatorContractTest_findEmulator(contracts, "ares");
    btrc_Vector_string* noArguments = btrc_Vector_string_new();
    LaunchPlan* aresPlan = EmulatorLauncher_plan(ares, "macos", "", "/roms/n64/game.z64", noArguments);
    if (!(strcmp(aresPlan->backend, "native") == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("ares macos backend: got '", aresPlan->backend)), "'")))));
    }
    if ((btrc_Vector_string_size(aresPlan->argumentVector) == 0) || (!(strcmp(btrc_Vector_string_get(aresPlan->argumentVector, 0), "${nix_result}/Applications/ares.app/Contents/MacOS/ares") == 0))) {
        (failures = (failures + EmulatorContractTest_fail("ares macos argv[0] must be the app executable")));
    }
    if (!EmulatorContractTest_argumentVectorHas(aresPlan, "--fullscreen")) {
        (failures = (failures + EmulatorContractTest_fail("ares macos plan lacks --fullscreen")));
    }
    LaunchPlan* aresLinuxPlan = EmulatorLauncher_plan(ares, "linux", "desktop", "", noArguments);
    if (btrc_Vector_string_size(aresLinuxPlan->argumentVector) != 0) {
        (failures = (failures + EmulatorContractTest_fail("ares must produce an empty plan on linux (no platform block)")));
    }
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
    return failures;
    if (noArguments != NULL) {
        if ((--noArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(noArguments);
        }
    }
}

int EmulatorContractTest_checkEsDeCommands(SemuContracts* contracts) {
    int failures = 0;
    EmulatorDefinition* retroArch = EmulatorContractTest_findEmulator(contracts, "retroarch");
    SystemDefinition* gameBoy = EmulatorContractTest_findSystem(contracts, "gb");
    EmulatorBinding* gameBoyBinding = EmulatorContractTest_findBinding(gameBoy, "retroarch");
    char* gameBoyCommand = EmulatorEmitter_esDeCommand(gameBoyBinding, retroArch, "linux");
    char* gameBoyExpected = "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%";
    if (!(strcmp(gameBoyCommand, gameBoyExpected) == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb esde command: got '", gameBoyCommand)), "' want '")), gameBoyExpected)), "'")))));
    }
    EmulatorDefinition* ares = EmulatorContractTest_findEmulator(contracts, "ares");
    SystemDefinition* nintendo64 = EmulatorContractTest_findSystem(contracts, "n64");
    EmulatorBinding* nintendo64Binding = EmulatorContractTest_findBinding(nintendo64, "ares");
    char* nintendo64Command = EmulatorEmitter_esDeCommand(nintendo64Binding, ares, "macos");
    char* nintendo64Expected = "%EMULATOR_ARES% --fullscreen --system \"Nintendo 64\" %ROM%";
    if (!(strcmp(nintendo64Command, nintendo64Expected) == 0)) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("n64 esde command: got '", nintendo64Command)), "' want '")), nintendo64Expected)), "'")))));
    }
    return failures;
}

int EmulatorContractTest_comparePcsx2GoldenFile(char* emittedPath, char* goldenPath, EmulatorDefinition* pcsx2, char* name) {
    if (!FileSystem_isFile(emittedPath)) {
        return EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, " was not emitted: ")), emittedPath)));
    }
    if (!FileSystem_isFile(goldenPath)) {
        return EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, " golden missing: ")), goldenPath)));
    }
    int failures = 0;
    btrc_Vector_string* emittedLines = Strings_split(Path_readAll(emittedPath), "\n");
    int emittedIndex = 0;
    btrc_Vector_string* __iter_662 = Strings_split(Path_readAll(goldenPath), "\n");
    int __n_664 = btrc_Vector_string_iterLen(__iter_662);
    for (int __i_663 = 0; (__i_663 < __n_664); (__i_663++)) {
        char* goldenLine = btrc_Vector_string_iterGet(__iter_662, __i_663);
        bool slowMotionGated = (__btrc_startsWith(goldenLine, "ToggleSlowMotion") && (!btrc_Vector_string_contains(pcsx2->input->actions, "speed.rewind")));
        if (!slowMotionGated) {
            char* emittedLine = ((emittedIndex < btrc_Vector_string_size(emittedLines)) ? btrc_Vector_string_get(emittedLines, emittedIndex) : "(missing)");
            (emittedIndex++);
            if (__btrc_startsWith(goldenLine, "Bios = ")) {
                if ((!__btrc_startsWith(emittedLine, "Bios = ")) || (!__btrc_endsWith(emittedLine, "src/generated/runtime/content/bios/ps2"))) {
                    (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, " bios folder line: got '")), emittedLine)), "'")))));
                }
            } else if (!(strcmp(emittedLine, goldenLine) == 0)) {
                int __fstr_665_arg0 = emittedIndex;
                int __fstr_665_len = snprintf(NULL, 0, "%d", __fstr_665_arg0);
                char* __fstr_665_buf = __btrc_str_track(((char*)malloc((__fstr_665_len + 1))));
                snprintf(__fstr_665_buf, (__fstr_665_len + 1), "%d", __fstr_665_arg0);
                (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, " line ")), __fstr_665_buf)), ": got '")), emittedLine)), "' want '")), goldenLine)), "'")))));
            }
        }
    }
    if (emittedIndex != btrc_Vector_string_size(emittedLines)) {
        int __fstr_666_arg0 = (btrc_Vector_string_size(emittedLines) - emittedIndex);
        int __fstr_666_len = snprintf(NULL, 0, " carries %d extra lines", __fstr_666_arg0);
        char* __fstr_666_buf = __btrc_str_track(((char*)malloc((__fstr_666_len + 1))));
        snprintf(__fstr_666_buf, (__fstr_666_len + 1), " carries %d extra lines", __fstr_666_arg0);
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(name, __fstr_666_buf)))));
    }
    return failures;
}

int EmulatorContractTest_checkPcsx2Profiles(SemuContracts* contracts, char* repoRoot) {
    ParseLog* log = ParseLog_new();
    char* targetDirectory = PathTools_join(repoRoot, "src/semu/input/targets/steamdeck");
    Keymap* keymap = KeymapParser_parse(Path_readAll(PathTools_join(targetDirectory, "steamdeck.skm")), log);
    SteamInputTarget* steamInput = SteamInputParser_parse(PathTools_join(targetDirectory, "steam_input.json"), log);
    EmulatorDefinition* pcsx2 = EmulatorContractTest_findEmulator(contracts, "pcsx2");
    char* outputRoot = PathTools_join(Environment_get("SEMU_TEST_TMP", "/tmp/semu-core-tests"), "pcsx2-profiles");
    EmulatorRegistry_emitProfiles(pcsx2->id, pcsx2, steamInput, keymap, outputRoot);
    char* goldenRoot = PathTools_join(repoRoot, "garbage/generated/packaging/emulators/profiles/PCSX2/config");
    int failures = EmulatorContractTest_comparePcsx2GoldenFile(PathTools_join(outputRoot, "PCSX2/config/inis/PCSX2.ini"), PathTools_join(goldenRoot, "inis/PCSX2.ini"), pcsx2, "pcsx2 base ini");
    (failures = (failures + EmulatorContractTest_comparePcsx2GoldenFile(PathTools_join(outputRoot, "PCSX2/config/inputprofiles/Steam Deck.ini"), PathTools_join(goldenRoot, "inputprofiles/Steam Deck.ini"), pcsx2, "pcsx2 input profile")));
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return failures;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

int EmulatorContractTest_run(char* repoRoot) {
    char* sourceRoot = PathTools_join(repoRoot, "src/semu");
    int failures = 0;
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(sourceRoot, log);
    if (!ParseLog_ok(log)) {
        btrc_Vector_string* __iter_667 = log->errors;
        int __n_669 = btrc_Vector_string_iterLen(__iter_667);
        for (int __i_668 = 0; (__i_668 < __n_669); (__i_668++)) {
            char* error = btrc_Vector_string_iterGet(__iter_667, __i_668);
            (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat("parse: ", error)))));
        }
    }
    if (btrc_Vector_EmulatorDefinition_p1_size(contracts->emulators) != 10) {
        int __fstr_670_arg0 = btrc_Vector_EmulatorDefinition_p1_size(contracts->emulators);
        int __fstr_670_len = snprintf(NULL, 0, "expected 10 emulator contracts, got %d", __fstr_670_arg0);
        char* __fstr_670_buf = __btrc_str_track(((char*)malloc((__fstr_670_len + 1))));
        snprintf(__fstr_670_buf, (__fstr_670_len + 1), "expected 10 emulator contracts, got %d", __fstr_670_arg0);
        (failures = (failures + EmulatorContractTest_fail(__fstr_670_buf)));
    }
    btrc_Vector_string* __list_671 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_671, "ares");
    btrc_Vector_string_push(__list_671, "azahar");
    btrc_Vector_string_push(__list_671, "cemu");
    btrc_Vector_string_push(__list_671, "dolphin");
    btrc_Vector_string_push(__list_671, "flycast");
    btrc_Vector_string_push(__list_671, "melonds");
    btrc_Vector_string_push(__list_671, "pcsx2");
    btrc_Vector_string_push(__list_671, "ppsspp");
    btrc_Vector_string_push(__list_671, "retroarch");
    btrc_Vector_string_push(__list_671, "ryujinx");
    btrc_Vector_string* expectedIds = __list_671;
    int __n_673 = btrc_Vector_string_iterLen(expectedIds);
    for (int __i_672 = 0; (__i_672 < __n_673); (__i_672++)) {
        char* emulatorId = btrc_Vector_string_iterGet(expectedIds, __i_672);
        if (!SemuContracts_hasEmulator(contracts, emulatorId)) {
            (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("missing emulator contract '", emulatorId)), "'")))));
        }
    }
    (failures = (failures + EmulatorContractTest_checkDolphinPlan(contracts)));
    (failures = (failures + EmulatorContractTest_checkDolphinEsDeTailPlan(contracts)));
    (failures = (failures + EmulatorContractTest_checkRetroArchLinuxPlan(contracts)));
    (failures = (failures + EmulatorContractTest_checkRetroArchMacosPlan(contracts)));
    (failures = (failures + EmulatorContractTest_checkAresPlans(contracts)));
    (failures = (failures + EmulatorContractTest_checkEsDeCommands(contracts)));
    (failures = (failures + EmulatorContractTest_checkPcsx2Profiles(contracts, repoRoot)));
    EmulatorDefinition* dolphin = EmulatorContractTest_findEmulator(contracts, "dolphin");
    char* shim = EmulatorEmitter_shimScript(dolphin);
    if ((!__btrc_startsWith(shim, "#!/usr/bin/env sh\n")) || (!__btrc_strContains(shim, "launcher dolphin"))) {
        (failures = (failures + EmulatorContractTest_fail(__btrc_str_track(__btrc_strcat("dolphin shim script malformed: ", shim)))));
    }
    if (failures > 0) {
        int __fstr_675_arg0 = failures;
        int __fstr_675_len = snprintf(NULL, 0, "emulator contract: FAIL (%d violations)", __fstr_675_arg0);
        char* __fstr_675_buf = __btrc_str_track(((char*)malloc((__fstr_675_len + 1))));
        snprintf(__fstr_675_buf, (__fstr_675_len + 1), "emulator contract: FAIL (%d violations)", __fstr_675_arg0);
        printf("%s\n", __fstr_675_buf);
        if (expectedIds != NULL) {
            if ((--expectedIds->__rc) <= 0) {
                btrc_Vector_string_destroy(expectedIds);
            }
        }
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return failures;
    }
    printf("%s\n", "emulator contract: parse + launch plans + esde commands valid");
    int __btrc_ret_676 = 0;
    if (expectedIds != NULL) {
        if ((--expectedIds->__rc) <= 0) {
            btrc_Vector_string_destroy(expectedIds);
        }
    }
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_676;
    if (expectedIds != NULL) {
        if ((--expectedIds->__rc) <= 0) {
            btrc_Vector_string_destroy(expectedIds);
        }
    }
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

void ResolvedBezel_init(ResolvedBezel* self) {
    self->__rc = 1;
    (self->art = "");
    (self->glass = "");
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    (self->hole = NormalizedRect_new());
    (self->hole->__rc++);
    if (self->screenHoleIds != NULL) {
        if ((--self->screenHoleIds->__rc) <= 0) {
            btrc_Vector_string_free(self->screenHoleIds);
        }
    }
    btrc_Vector_string* __list_677 = btrc_Vector_string_new();
    (self->screenHoleIds = __list_677);
    (self->screenHoleIds->__rc++);
    if (self->screenHoleRects != NULL) {
        if ((--self->screenHoleRects->__rc) <= 0) {
            btrc_Vector_NormalizedRect_p1_free(self->screenHoleRects);
        }
    }
    btrc_Vector_NormalizedRect_p1* __list_678 = btrc_Vector_NormalizedRect_p1_new();
    (self->screenHoleRects = __list_678);
    (self->screenHoleRects->__rc++);
    (self->reflect = 0.0);
    (self->curvature = 0.0);
    (self->cornerRadius = 0.0);
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    (self->shellTint = RgbColor_new());
    (self->shellTint->__rc++);
    (self->fill = false);
}

ResolvedBezel* ResolvedBezel_new(void) {
    ResolvedBezel* self = ((ResolvedBezel*)malloc(sizeof(ResolvedBezel)));
    memset(self, 0, sizeof(ResolvedBezel));
    ResolvedBezel_init(self);
    return self;
}

void ResolvedBezel_destroy(ResolvedBezel* self) {
    if (self->hole != NULL) {
        if ((--self->hole->__rc) <= 0) {
            NormalizedRect_destroy(self->hole);
        }
    }
    if (self->screenHoleIds != NULL) {
        if ((--self->screenHoleIds->__rc) <= 0) {
            btrc_Vector_string_free(self->screenHoleIds);
        }
    }
    if (self->screenHoleRects != NULL) {
        if ((--self->screenHoleRects->__rc) <= 0) {
            btrc_Vector_NormalizedRect_p1_free(self->screenHoleRects);
        }
    }
    if (self->shellTint != NULL) {
        if ((--self->shellTint->__rc) <= 0) {
            RgbColor_destroy(self->shellTint);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* BezelResolver_assetRoot(char* repoRoot) {
    char* environmentRoot = Environment_get("SEMU_ASSET_ROOT", "");
    if (!__btrc_isEmpty(environmentRoot)) {
        char* stagedRoot = PathTools_join(environmentRoot, "share/semu");
        if (FileSystem_isDir(PathTools_join(stagedRoot, "assets"))) {
            return stagedRoot;
        }
    }
    return PathTools_join(repoRoot, "src/semu/emulators/rendering");
}

char* BezelResolver_resolveAssetPath(char* canonicalPath, char* assetRoot) {
    if (__btrc_isEmpty(canonicalPath) || __btrc_isEmpty(assetRoot)) {
        return canonicalPath;
    }
    return PathTools_join(assetRoot, canonicalPath);
}

char* BezelResolver_fallbackProfile(SystemDefinition* system) {
    if (!__btrc_isEmpty(system->render->assetProfile)) {
        return system->render->assetProfile;
    }
    if ((strcmp(system->render->kind, "dual") == 0) || (btrc_Vector_SystemScreen_p1_size(system->display->screens) > 1)) {
        return "dual";
    }
    if (system->display->aspect->set && (system->display->aspect->h > 0)) {
        double aspectRatio = __btrc_div_double((0.0 + system->display->aspect->w), (0.0 + system->display->aspect->h));
        if (aspectRatio >= 1.6) {
            return "wide16x9";
        }
    }
    if (strcmp(system->render->style, "handheld") == 0) {
        return "handheld";
    }
    return "crt4x3";
}

ResolvedBezel* BezelResolver_genericFallback(char* assetProfile) {
    ResolvedBezel* resolved = ResolvedBezel_new();
    (resolved->hole->set = true);
    if (strcmp(assetProfile, "wide16x9") == 0) {
        (resolved->art = "assets/bezels/generic/16x9.png");
        (resolved->hole->x = 0.0300);
        (resolved->hole->y = 0.0800);
        (resolved->hole->w = 0.9400);
        (resolved->hole->h = 0.8400);
        return resolved;
    }
    if (strcmp(assetProfile, "dual") == 0) {
        (resolved->art = "assets/bezels/generic/dual.png");
        (resolved->hole->x = 0.3300);
        (resolved->hole->y = 0.0400);
        (resolved->hole->w = 0.3400);
        (resolved->hole->h = 0.9200);
        return resolved;
    }
    (resolved->art = "assets/bezels/generic/4x3.png");
    (resolved->hole->x = 0.1250);
    (resolved->hole->y = 0.0500);
    (resolved->hole->w = 0.7500);
    (resolved->hole->h = 0.9000);
    return resolved;
    if (resolved != NULL) {
        if ((--resolved->__rc) <= 0) {
            ResolvedBezel_destroy(resolved);
        }
    }
}

int BezelResolver_variantIndex(BezelCollection* bezels, char* variantId) {
    for (int candidateIndex = 0; (candidateIndex < btrc_Vector_BezelVariant_p1_size(bezels->variants)); (candidateIndex++)) {
        if (strcmp(btrc_Vector_BezelVariant_p1_get(bezels->variants, candidateIndex)->id, variantId) == 0) {
            return candidateIndex;
        }
    }
    return (-1);
}

void BezelResolver_applyRenderDefaults(ResolvedBezel* resolved, SystemDefinition* system) {
    (resolved->fill = system->render->fill);
    (resolved->reflect = system->render->reflect);
    (resolved->curvature = system->render->curvature);
    (resolved->cornerRadius = system->render->cornerRadius);
    if (system->render->shellTint->set) {
        if (resolved->shellTint != NULL) {
            if ((--resolved->shellTint->__rc) <= 0) {
                RgbColor_destroy(resolved->shellTint);
            }
        }
        (resolved->shellTint = system->render->shellTint);
        (resolved->shellTint->__rc++);
    }
}

ResolvedBezel* BezelResolver_resolveFallback(SystemDefinition* system, char* assetRoot) {
    ResolvedBezel* resolved = BezelResolver_genericFallback(BezelResolver_fallbackProfile(system));
    (resolved->art = BezelResolver_resolveAssetPath(resolved->art, assetRoot));
    BezelResolver_applyRenderDefaults(resolved, system);
    return resolved;
}

void BezelResolver_applyVariantOverrides(ResolvedBezel* resolved, BezelVariant* variant) {
    if (variant->hasReflect) {
        (resolved->reflect = variant->reflect);
    }
    if (variant->hasCurvature) {
        (resolved->curvature = variant->curvature);
    }
    if (variant->hasCornerRadius) {
        (resolved->cornerRadius = variant->cornerRadius);
    }
    if (variant->shellTint->set) {
        if (resolved->shellTint != NULL) {
            if ((--resolved->shellTint->__rc) <= 0) {
                RgbColor_destroy(resolved->shellTint);
            }
        }
        (resolved->shellTint = variant->shellTint);
        (resolved->shellTint->__rc++);
    }
}

ResolvedBezel* BezelResolver_resolve(SystemDefinition* system, char* variantId, char* assetRoot) {
    BezelCollection* bezels = system->bezels;
    if ((!bezels->present) || (!bezels->enabled)) {
        return BezelResolver_resolveFallback(system, assetRoot);
    }
    char* requestedVariantId = (__btrc_isEmpty(variantId) ? bezels->defaultVariant : variantId);
    int selectedIndex = BezelResolver_variantIndex(bezels, requestedVariantId);
    if (selectedIndex < 0) {
        (selectedIndex = BezelResolver_variantIndex(bezels, bezels->defaultVariant));
    }
    if ((selectedIndex < 0) && (btrc_Vector_BezelVariant_p1_size(bezels->variants) > 0)) {
        (selectedIndex = 0);
    }
    if (selectedIndex < 0) {
        return BezelResolver_resolveFallback(system, assetRoot);
    }
    ResolvedBezel* resolved = ResolvedBezel_new();
    BezelResolver_applyRenderDefaults(resolved, system);
    BezelVariant* variant = btrc_Vector_BezelVariant_p1_get(bezels->variants, selectedIndex);
    (resolved->art = BezelResolver_resolveAssetPath(variant->art, assetRoot));
    (resolved->glass = BezelResolver_resolveAssetPath(variant->glass, assetRoot));
    if (resolved->hole != NULL) {
        if ((--resolved->hole->__rc) <= 0) {
            NormalizedRect_destroy(resolved->hole);
        }
    }
    (resolved->hole = (variant->hole->set ? variant->hole : bezels->hole));
    (resolved->hole->__rc++);
    btrc_Vector_ScreenHole_p1* screenHoles = ((btrc_Vector_ScreenHole_p1_size(variant->screenHoles) > 0) ? variant->screenHoles : bezels->screenHoles);
    int __n_680 = btrc_Vector_ScreenHole_p1_iterLen(screenHoles);
    for (int __i_679 = 0; (__i_679 < __n_680); (__i_679++)) {
        ScreenHole* screenHole = btrc_Vector_ScreenHole_p1_iterGet(screenHoles, __i_679);
        btrc_Vector_string_push(resolved->screenHoleIds, screenHole->screenId);
        btrc_Vector_NormalizedRect_p1_push(resolved->screenHoleRects, screenHole->hole);
    }
    BezelResolver_applyVariantOverrides(resolved, variant);
    return resolved;
    if (resolved != NULL) {
        if ((--resolved->__rc) <= 0) {
            ResolvedBezel_destroy(resolved);
        }
    }
}

char* ShaderSelector_select(SystemDefinition* system, bool visualsEnabled, bool tapActive, bool bezelsEnabled, bool crtShadersEnabled, bool widescreenActive) {
    if (!visualsEnabled) {
        return "";
    }
    if ((!system->shaders->present) || (!system->shaders->enabled)) {
        return "";
    }
    char* screenPreset = system->shaders->screen;
    char* compositePreset = system->shaders->composite;
    if (widescreenActive && system->shaders->hasWidescreen) {
        (screenPreset = system->shaders->widescreenScreen);
        (compositePreset = system->shaders->widescreenComposite);
    }
    if (tapActive) {
        return screenPreset;
    }
    if (bezelsEnabled) {
        return (__btrc_isEmpty(compositePreset) ? screenPreset : compositePreset);
    }
    if (crtShadersEnabled) {
        return screenPreset;
    }
    return "";
}

char* ShaderSelector_resolvePath(char* canonicalPath, char* shaderRoot) {
    if (__btrc_isEmpty(canonicalPath)) {
        return "";
    }
    char* canonicalPrefix = "assets/shaders/";
    if ((!__btrc_startsWith(canonicalPath, canonicalPrefix)) || __btrc_isEmpty(shaderRoot)) {
        return canonicalPath;
    }
    char* relativePath = __btrc_str_track(__btrc_substring(canonicalPath, ((int)strlen(canonicalPrefix)), (((int)strlen(canonicalPath)) - ((int)strlen(canonicalPrefix)))));
    return PathTools_join(shaderRoot, PathTools_join("semu", relativePath));
}

void TapEnvironment_init(TapEnvironment* self) {
    self->__rc = 1;
    if (self->keys != NULL) {
        if ((--self->keys->__rc) <= 0) {
            btrc_Vector_string_free(self->keys);
        }
    }
    btrc_Vector_string* __list_681 = btrc_Vector_string_new();
    (self->keys = __list_681);
    (self->keys->__rc++);
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    btrc_Vector_string* __list_682 = btrc_Vector_string_new();
    (self->values = __list_682);
    (self->values->__rc++);
}

TapEnvironment* TapEnvironment_new(void) {
    TapEnvironment* self = ((TapEnvironment*)malloc(sizeof(TapEnvironment)));
    memset(self, 0, sizeof(TapEnvironment));
    TapEnvironment_init(self);
    return self;
}

void TapEnvironment_destroy(TapEnvironment* self) {
    if (self->keys != NULL) {
        if ((--self->keys->__rc) <= 0) {
            btrc_Vector_string_free(self->keys);
        }
    }
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void TapEnvironment_put(TapEnvironment* self, char* key, char* value) {
    btrc_Vector_string_push(self->keys, key);
    btrc_Vector_string_push(self->values, value);
}

char* TapEnvironment_valueOf(TapEnvironment* self, char* key) {
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(self->keys)); (keyIndex++)) {
        if (strcmp(btrc_Vector_string_get(self->keys, keyIndex), key) == 0) {
            return btrc_Vector_string_get(self->values, keyIndex);
        }
    }
    return "";
}

int RenderPlanner_systemKindWireValue(char* kind) {
    if (strcmp(kind, "dual") == 0) {
        return 1;
    }
    if (strcmp(kind, "pointer") == 0) {
        return 2;
    }
    return 0;
}

char* RenderPlanner_wireFloat(double value) {
    double __fstr_683_arg0 = value;
    int __fstr_683_len = snprintf(NULL, 0, "%f", __fstr_683_arg0);
    char* __fstr_683_buf = __btrc_str_track(((char*)malloc((__fstr_683_len + 1))));
    snprintf(__fstr_683_buf, (__fstr_683_len + 1), "%f", __fstr_683_arg0);
    char* formatted = __fstr_683_buf;
    if (!__btrc_strContains(formatted, ".")) {
        return formatted;
    }
    int keptLength = ((int)strlen(formatted));
    while ((keptLength > 1) && (strcmp(__btrc_str_track(__btrc_substring(formatted, (keptLength - 1), 1)), "0") == 0)) {
        (keptLength = (keptLength - 1));
    }
    if (strcmp(__btrc_str_track(__btrc_substring(formatted, (keptLength - 1), 1)), ".") == 0) {
        (keptLength = (keptLength + 1));
    }
    return __btrc_str_track(__btrc_substring(formatted, 0, keptLength));
}

PixelSize* RenderPlanner_combinedNativeSize(SystemDefinition* system) {
    PixelSize* combined = PixelSize_new();
    bool horizontal = (strcmp(system->display->arrangement, "horizontal") == 0);
    btrc_Vector_SystemScreen_p1* __iter_684 = system->display->screens;
    int __n_686 = btrc_Vector_SystemScreen_p1_iterLen(__iter_684);
    for (int __i_685 = 0; (__i_685 < __n_686); (__i_685++)) {
        SystemScreen* screen = btrc_Vector_SystemScreen_p1_iterGet(__iter_684, __i_685);
        if (horizontal) {
            (combined->w = (combined->w + screen->native->w));
            (combined->h = ((screen->native->h > combined->h) ? screen->native->h : combined->h));
        } else {
            (combined->w = ((screen->native->w > combined->w) ? screen->native->w : combined->w));
            (combined->h = (combined->h + screen->native->h));
        }
    }
    return combined;
    if (combined != NULL) {
        if ((--combined->__rc) <= 0) {
            PixelSize_destroy(combined);
        }
    }
}

char* RenderPlanner_aspectWireValue(SystemDefinition* system, PixelSize* nativeSize) {
    if (btrc_Vector_SystemScreen_p1_size(system->display->screens) > 1) {
        int __fstr_687_arg0 = nativeSize->w;
        int __fstr_687_arg1 = nativeSize->h;
        int __fstr_687_len = snprintf(NULL, 0, "%d:%d", __fstr_687_arg0, __fstr_687_arg1);
        char* __fstr_687_buf = __btrc_str_track(((char*)malloc((__fstr_687_len + 1))));
        snprintf(__fstr_687_buf, (__fstr_687_len + 1), "%d:%d", __fstr_687_arg0, __fstr_687_arg1);
        return __fstr_687_buf;
    }
    double aspectRatio = 0.0;
    if (system->display->aspect->set && (system->display->aspect->h > 0)) {
        (aspectRatio = __btrc_div_double((0.0 + system->display->aspect->w), (0.0 + system->display->aspect->h)));
    } else if (nativeSize->h > 0) {
        (aspectRatio = __btrc_div_double((0.0 + nativeSize->w), (0.0 + nativeSize->h)));
    }
    return RenderPlanner_wireFloat(aspectRatio);
}

TapEnvironment* RenderPlanner_tapEnvironment(SystemDefinition* system, ResolvedBezel* bezel, char* renderMode) {
    TapEnvironment* environment = TapEnvironment_new();
    PixelSize* nativeSize = RenderPlanner_combinedNativeSize(system);
    bool dual = (strcmp(system->render->kind, "dual") == 0);
    int __fstr_688_arg0 = nativeSize->w;
    int __fstr_688_len = snprintf(NULL, 0, "%d", __fstr_688_arg0);
    char* __fstr_688_buf = __btrc_str_track(((char*)malloc((__fstr_688_len + 1))));
    snprintf(__fstr_688_buf, (__fstr_688_len + 1), "%d", __fstr_688_arg0);
    TapEnvironment_put(environment, "SEMU_TAP_NATIVE_W", __fstr_688_buf);
    int __fstr_689_arg0 = nativeSize->h;
    int __fstr_689_len = snprintf(NULL, 0, "%d", __fstr_689_arg0);
    char* __fstr_689_buf = __btrc_str_track(((char*)malloc((__fstr_689_len + 1))));
    snprintf(__fstr_689_buf, (__fstr_689_len + 1), "%d", __fstr_689_arg0);
    TapEnvironment_put(environment, "SEMU_TAP_NATIVE_H", __fstr_689_buf);
    TapEnvironment_put(environment, "SEMU_TAP_ASPECT", RenderPlanner_aspectWireValue(system, nativeSize));
    TapEnvironment_put(environment, "SEMU_TAP_STYLE", system->render->style);
    TapEnvironment_put(environment, "SEMU_TAP_PRIORITY", system->render->priority);
    TapEnvironment_put(environment, "SEMU_TAP_DUAL", (dual ? "1" : "0"));
    int __fstr_690_arg0 = RenderPlanner_systemKindWireValue(system->render->kind);
    int __fstr_690_len = snprintf(NULL, 0, "%d", __fstr_690_arg0);
    char* __fstr_690_buf = __btrc_str_track(((char*)malloc((__fstr_690_len + 1))));
    snprintf(__fstr_690_buf, (__fstr_690_len + 1), "%d", __fstr_690_arg0);
    TapEnvironment_put(environment, "SEMU_TAP_SYSKIND", __fstr_690_buf);
    TapEnvironment_put(environment, "SEMU_TAP_FILL", (bezel->fill ? "1" : "0"));
    TapEnvironment_put(environment, "SEMU_TAP_REFLECT", RenderPlanner_wireFloat(bezel->reflect));
    TapEnvironment_put(environment, "SEMU_TAP_CURVE", RenderPlanner_wireFloat(bezel->curvature));
    TapEnvironment_put(environment, "SEMU_TAP_CORNER", RenderPlanner_wireFloat(bezel->cornerRadius));
    TapEnvironment_put(environment, "SEMU_TAP_SHELL", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(RenderPlanner_wireFloat(bezel->shellTint->r), ",")), RenderPlanner_wireFloat(bezel->shellTint->g))), ",")), RenderPlanner_wireFloat(bezel->shellTint->b))));
    if (!__btrc_isEmpty(bezel->art)) {
        TapEnvironment_put(environment, "SEMU_TAP_ART", bezel->art);
    }
    if (!__btrc_isEmpty(bezel->glass)) {
        TapEnvironment_put(environment, "SEMU_TAP_GLASS", bezel->glass);
    }
    if (bezel->hole->set) {
        TapEnvironment_put(environment, "SEMU_TAP_SCREEN", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(RenderPlanner_wireFloat(bezel->hole->x), ",")), RenderPlanner_wireFloat(bezel->hole->y))), ",")), RenderPlanner_wireFloat(bezel->hole->w))), ",")), RenderPlanner_wireFloat(bezel->hole->h))));
    }
    return environment;
    if (environment != NULL) {
        if ((--environment->__rc) <= 0) {
            TapEnvironment_destroy(environment);
        }
    }
}

char* RenderPlanner_targetResolution(SystemDefinition* system, char* renderMode, int screenWidth, int screenHeight) {
    PixelSize* nativeSize = RenderPlanner_combinedNativeSize(system);
    if ((nativeSize->w < 1) || (nativeSize->h < 1)) {
        return "";
    }
    char* mode = __btrc_str_track(__btrc_toLower(renderMode));
    if (strcmp(mode, "native") == 0) {
        return "";
    }
    if (strcmp(mode, "integer") == 0) {
        int widthScale = __btrc_div_int(screenWidth, nativeSize->w);
        int heightScale = __btrc_div_int(screenHeight, nativeSize->h);
        int scale = ((heightScale < widthScale) ? heightScale : widthScale);
        if (scale < 1) {
            (scale = 1);
        }
        int __fstr_691_arg0 = (scale * nativeSize->w);
        int __fstr_691_arg1 = (scale * nativeSize->h);
        int __fstr_691_len = snprintf(NULL, 0, "%dx%d", __fstr_691_arg0, __fstr_691_arg1);
        char* __fstr_691_buf = __btrc_str_track(((char*)malloc((__fstr_691_len + 1))));
        snprintf(__fstr_691_buf, (__fstr_691_len + 1), "%dx%d", __fstr_691_arg0, __fstr_691_arg1);
        return __fstr_691_buf;
    }
    int __fstr_692_arg0 = nativeSize->w;
    int __fstr_692_arg1 = nativeSize->h;
    int __fstr_692_len = snprintf(NULL, 0, "%dx%d", __fstr_692_arg0, __fstr_692_arg1);
    char* __fstr_692_buf = __btrc_str_track(((char*)malloc((__fstr_692_len + 1))));
    snprintf(__fstr_692_buf, (__fstr_692_len + 1), "%dx%d", __fstr_692_arg0, __fstr_692_arg1);
    return __fstr_692_buf;
}

int RenderingContractTest_fail(char* message) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("  FAIL ", message)));
    return 1;
}

bool RenderingContractTest_nearlyEqual(double actualValue, double expectedValue) {
    double difference = (actualValue - expectedValue);
    if (difference < 0.0) {
        (difference = (0.0 - difference));
    }
    return (difference < 0.0001);
}

SystemDefinition* RenderingContractTest_findSystem(SemuContracts* contracts, char* systemId) {
    btrc_Vector_SystemDefinition_p1* __iter_693 = contracts->systems;
    int __n_695 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_693);
    for (int __i_694 = 0; (__i_694 < __n_695); (__i_694++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_693, __i_694);
        if (strcmp(systemDefinition->id, systemId) == 0) {
            return systemDefinition;
        }
    }
    return SystemDefinition_new();
}

int RenderingContractTest_checkHandheldBezel(SystemDefinition* gameBoy, char* assetRoot) {
    int failures = 0;
    ResolvedBezel* bezel = BezelResolver_resolve(gameBoy, "", assetRoot);
    TapEnvironment* environment = RenderPlanner_tapEnvironment(gameBoy, bezel, "1x");
    if (!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_FILL"), "1") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb SEMU_TAP_FILL: want 1, got '", TapEnvironment_valueOf(environment, "SEMU_TAP_FILL"))), "'")))));
    }
    if (!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_REFLECT"), "0.55") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb SEMU_TAP_REFLECT: want 0.55, got '", TapEnvironment_valueOf(environment, "SEMU_TAP_REFLECT"))), "'")))));
    }
    if (!__btrc_endsWith(bezel->art, "assets/bezels/gb/classic.png")) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb art: want ...assets/bezels/gb/classic.png, got '", bezel->art)), "'")))));
    }
    if (!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_ART"), bezel->art) == 0)) {
        (failures = (failures + RenderingContractTest_fail("gb SEMU_TAP_ART does not match the resolved art")));
    }
    if (!RenderingContractTest_nearlyEqual(bezel->hole->x, 0.1843)) {
        double __fstr_696_arg0 = bezel->hole->x;
        int __fstr_696_len = snprintf(NULL, 0, "gb hole.x: want 0.1843, got %f", __fstr_696_arg0);
        char* __fstr_696_buf = __btrc_str_track(((char*)malloc((__fstr_696_len + 1))));
        snprintf(__fstr_696_buf, (__fstr_696_len + 1), "gb hole.x: want 0.1843, got %f", __fstr_696_arg0);
        (failures = (failures + RenderingContractTest_fail(__fstr_696_buf)));
    }
    if (!__btrc_startsWith(TapEnvironment_valueOf(environment, "SEMU_TAP_SCREEN"), "0.1843,")) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb SEMU_TAP_SCREEN: want 0.1843,... got '", TapEnvironment_valueOf(environment, "SEMU_TAP_SCREEN"))), "'")))));
    }
    ResolvedBezel* colorwayBezel = BezelResolver_resolve(gameBoy, "play_it_loud_red", assetRoot);
    if (!__btrc_endsWith(colorwayBezel->art, "assets/bezels/gb/red.png")) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb red variant art: got '", colorwayBezel->art)), "'")))));
    }
    if (!RenderingContractTest_nearlyEqual(colorwayBezel->hole->x, 0.1843)) {
        (failures = (failures + RenderingContractTest_fail("gb red variant must inherit the classic hole")));
    }
    return failures;
}

int RenderingContractTest_checkGeometryIndirection(SystemDefinition* gameCube, char* assetRoot) {
    int failures = 0;
    ResolvedBezel* bezel = BezelResolver_resolve(gameCube, "", assetRoot);
    if (!__btrc_endsWith(bezel->art, "assets/bezels/n64/tv.png")) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gc art via geometry_from: want ...assets/bezels/n64/tv.png, got '", bezel->art)), "'")))));
    }
    if ((((!RenderingContractTest_nearlyEqual(bezel->hole->x, 0.2542)) || (!RenderingContractTest_nearlyEqual(bezel->hole->y, 0.1155))) || (!RenderingContractTest_nearlyEqual(bezel->hole->w, 0.4917))) || (!RenderingContractTest_nearlyEqual(bezel->hole->h, 0.6556))) {
        double __fstr_697_arg0 = bezel->hole->x;
        double __fstr_697_arg1 = bezel->hole->y;
        double __fstr_697_arg2 = bezel->hole->w;
        double __fstr_697_arg3 = bezel->hole->h;
        int __fstr_697_len = snprintf(NULL, 0, "gc hole: want 0.2542,0.1155,0.4917,0.6556 got %f,%f,%f,%f", __fstr_697_arg0, __fstr_697_arg1, __fstr_697_arg2, __fstr_697_arg3);
        char* __fstr_697_buf = __btrc_str_track(((char*)malloc((__fstr_697_len + 1))));
        snprintf(__fstr_697_buf, (__fstr_697_len + 1), "gc hole: want 0.2542,0.1155,0.4917,0.6556 got %f,%f,%f,%f", __fstr_697_arg0, __fstr_697_arg1, __fstr_697_arg2, __fstr_697_arg3);
        (failures = (failures + RenderingContractTest_fail(__fstr_697_buf)));
    }
    return failures;
}

int RenderingContractTest_checkGenericFallback(SystemDefinition* switchSystem, char* assetRoot) {
    int failures = 0;
    ResolvedBezel* bezel = BezelResolver_resolve(switchSystem, "", assetRoot);
    if (!__btrc_endsWith(bezel->art, "assets/bezels/generic/16x9.png")) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("switch fallback art: want ...assets/bezels/generic/16x9.png, got '", bezel->art)), "'")))));
    }
    if ((((!RenderingContractTest_nearlyEqual(bezel->hole->x, 0.0300)) || (!RenderingContractTest_nearlyEqual(bezel->hole->y, 0.0800))) || (!RenderingContractTest_nearlyEqual(bezel->hole->w, 0.9400))) || (!RenderingContractTest_nearlyEqual(bezel->hole->h, 0.8400))) {
        (failures = (failures + RenderingContractTest_fail("switch fallback hole: want the wide16x9 table row")));
    }
    return failures;
}

int RenderingContractTest_checkSystemKindWire(SystemDefinition* dualScreenSystem) {
    int failures = 0;
    if (RenderPlanner_systemKindWireValue(dualScreenSystem->render->kind) != 1) {
        int __fstr_698_arg0 = RenderPlanner_systemKindWireValue(dualScreenSystem->render->kind);
        int __fstr_698_len = snprintf(NULL, 0, "n3ds system-kind wire: want 1, got %d", __fstr_698_arg0);
        char* __fstr_698_buf = __btrc_str_track(((char*)malloc((__fstr_698_len + 1))));
        snprintf(__fstr_698_buf, (__fstr_698_len + 1), "n3ds system-kind wire: want 1, got %d", __fstr_698_arg0);
        (failures = (failures + RenderingContractTest_fail(__fstr_698_buf)));
    }
    if ((RenderPlanner_systemKindWireValue("standard") != 0) || (RenderPlanner_systemKindWireValue("pointer") != 2)) {
        (failures = (failures + RenderingContractTest_fail("system-kind wire table: standard=0 pointer=2")));
    }
    return failures;
}

int RenderingContractTest_checkShaderMatrix(SystemDefinition* gameBoy, SystemDefinition* gameCube) {
    int failures = 0;
    char* tapPreset = ShaderSelector_select(gameBoy, true, true, true, true, false);
    if (!(strcmp(tapPreset, "assets/shaders/gb/screen.slangp") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb tap shader: want the screen preset, got '", tapPreset)), "'")))));
    }
    char* compositePreset = ShaderSelector_select(gameBoy, true, false, true, true, false);
    if (!(strcmp(compositePreset, "assets/shaders/gb/composite.slangp") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb no-tap+bezels shader: want the composite preset, got '", compositePreset)), "'")))));
    }
    char* crtPreset = ShaderSelector_select(gameBoy, true, false, false, true, false);
    if (!(strcmp(crtPreset, "assets/shaders/gb/screen.slangp") == 0)) {
        (failures = (failures + RenderingContractTest_fail("gb no-tap no-bezel crt shader: want the screen preset")));
    }
    if (!__btrc_isEmpty(ShaderSelector_select(gameBoy, false, true, true, true, false))) {
        (failures = (failures + RenderingContractTest_fail("visuals off must select no shader")));
    }
    if (!__btrc_isEmpty(ShaderSelector_select(gameBoy, true, false, false, false, false))) {
        (failures = (failures + RenderingContractTest_fail("no tap/bezels/crt must select no shader")));
    }
    if (((!__btrc_isEmpty(ShaderSelector_select(gameCube, true, true, true, true, false))) || (!__btrc_isEmpty(ShaderSelector_select(gameCube, true, false, true, true, false)))) || (!__btrc_isEmpty(ShaderSelector_select(gameCube, true, false, false, true, true)))) {
        (failures = (failures + RenderingContractTest_fail("gc (shaders disabled) must always select ''")));
    }
    char* resolvedPath = ShaderSelector_resolvePath("assets/shaders/gb/screen.slangp", "/tmp/shaders");
    if (!(strcmp(resolvedPath, "/tmp/shaders/semu/gb/screen.slangp") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("ShaderSelector.resolvePath: got '", resolvedPath)), "'")))));
    }
    return failures;
}

int RenderingContractTest_checkTargetResolution(SystemDefinition* gameBoy, SystemDefinition* dualScreenSystem, char* assetRoot) {
    int failures = 0;
    char* oneTimesResolution = RenderPlanner_targetResolution(gameBoy, "1x", 1280, 800);
    if (!(strcmp(oneTimesResolution, "160x144") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb 1x resolution: want 160x144, got '", oneTimesResolution)), "'")))));
    }
    char* integerResolution = RenderPlanner_targetResolution(gameBoy, "integer", 1280, 800);
    if (!(strcmp(integerResolution, "800x720") == 0)) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("gb integer resolution: want 800x720, got '", integerResolution)), "'")))));
    }
    if (!__btrc_isEmpty(RenderPlanner_targetResolution(gameBoy, "native", 1280, 800))) {
        (failures = (failures + RenderingContractTest_fail("gb native mode must return ''")));
    }
    ResolvedBezel* dualBezel = BezelResolver_resolve(dualScreenSystem, "", assetRoot);
    TapEnvironment* environment = RenderPlanner_tapEnvironment(dualScreenSystem, dualBezel, "1x");
    if ((!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_NATIVE_W"), "400") == 0)) || (!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_NATIVE_H"), "480") == 0))) {
        (failures = (failures + RenderingContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("n3ds combined native: want 400x480, got ", TapEnvironment_valueOf(environment, "SEMU_TAP_NATIVE_W"))), "x")), TapEnvironment_valueOf(environment, "SEMU_TAP_NATIVE_H"))))));
    }
    if ((!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_DUAL"), "1") == 0)) || (!(strcmp(TapEnvironment_valueOf(environment, "SEMU_TAP_ASPECT"), "400:480") == 0))) {
        (failures = (failures + RenderingContractTest_fail("n3ds dual wire values: DUAL=1 ASPECT=400:480")));
    }
    return failures;
}

int RenderingContractTest_run(char* repoRoot) {
    char* sourceRoot = Environment_get("SEMU_SRC_ROOT", PathTools_join(repoRoot, "src/semu"));
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(sourceRoot, log);
    printf("%s\n", "rendering contract test");
    if (!ParseLog_ok(log)) {
        printf("%s\n", "  FAIL contract set does not parse cleanly (see the system suite)");
        int __btrc_ret_699 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_699;
    }
    SystemDefinition* gameBoy = RenderingContractTest_findSystem(contracts, "gb");
    SystemDefinition* gameCube = RenderingContractTest_findSystem(contracts, "gc");
    SystemDefinition* switchSystem = RenderingContractTest_findSystem(contracts, "switch");
    SystemDefinition* dualScreenSystem = RenderingContractTest_findSystem(contracts, "n3ds");
    if (((__btrc_isEmpty(gameBoy->id) || __btrc_isEmpty(gameCube->id)) || __btrc_isEmpty(switchSystem->id)) || __btrc_isEmpty(dualScreenSystem->id)) {
        printf("%s\n", "  FAIL expected systems gb/gc/switch/n3ds in the contract set");
        int __btrc_ret_700 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_700;
    }
    char* assetRoot = BezelResolver_assetRoot(repoRoot);
    int violations = 0;
    (violations = (violations + RenderingContractTest_checkHandheldBezel(gameBoy, assetRoot)));
    (violations = (violations + RenderingContractTest_checkGeometryIndirection(gameCube, assetRoot)));
    (violations = (violations + RenderingContractTest_checkGenericFallback(switchSystem, assetRoot)));
    (violations = (violations + RenderingContractTest_checkSystemKindWire(dualScreenSystem)));
    (violations = (violations + RenderingContractTest_checkShaderMatrix(gameBoy, gameCube)));
    (violations = (violations + RenderingContractTest_checkTargetResolution(gameBoy, dualScreenSystem, assetRoot)));
    if (violations == 0) {
        printf("%s\n", "  OK bezel resolution (gb variants, gc geometry_from, switch fallback)");
        printf("%s\n", "  OK tap environment wire values + system-kind mapping");
        printf("%s\n", "  OK shader selection matrix + path resolution");
        printf("%s\n", "  OK render target resolution (1x/integer/native)");
    }
    int __fstr_702_arg0 = violations;
    int __fstr_702_len = snprintf(NULL, 0, "rendering contract test: %d violation(s)", __fstr_702_arg0);
    char* __fstr_702_buf = __btrc_str_track(((char*)malloc((__fstr_702_len + 1))));
    snprintf(__fstr_702_buf, (__fstr_702_len + 1), "rendering contract test: %d violation(s)", __fstr_702_arg0);
    printf("%s\n", __fstr_702_buf);
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return violations;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

int InputContractTest_check(bool condition, char* what) {
    if (condition) {
        return 0;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("  FAIL: ", what)));
    return 1;
}

int InputContractTest_checkEquals(char* got, char* want, char* what) {
    if (strcmp(got, want) == 0) {
        return 0;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  FAIL: ", what)), ": got '")), got)), "' want '")), want)), "'")));
    return 1;
}

int InputContractTest_checkChordTokens(SteamInputTarget* target) {
    int violations = 0;
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Ctrl+P", target), "LEFT_CONTROL P", "vdf Ctrl+P"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Ctrl+-", target), "LEFT_CONTROL KEYPAD_DASH", "vdf Ctrl+-"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Ctrl++", target), "LEFT_CONTROL KEYPAD_PLUS", "vdf Ctrl++"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Ctrl+Tab", target), "LEFT_CONTROL TAB", "vdf Ctrl+Tab"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Ctrl+Enter", target), "LEFT_CONTROL RETURN", "vdf Ctrl+Enter"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Esc", target), "ESCAPE", "vdf Esc"));
    (violations += InputContractTest_checkEquals(ChordTranslator_vdfTokens("Alt+F4", target), "LEFT_ALT F4", "vdf Alt+F4"));
    return violations;
}

btrc_Vector_string* InputContractTest_normalizedBindings(char* line) {
    btrc_Vector_string* heads = btrc_Vector_string_new();
    btrc_Vector_string* parts = Strings_split(line, "\"binding\" \"");
    for (int partIndex = 1; (partIndex < btrc_Vector_string_size(parts)); (partIndex++)) {
        char* tail = btrc_Vector_string_get(parts, partIndex);
        int quoteIndex = __btrc_indexOf(tail, "\"");
        char* content = ((quoteIndex < 0) ? tail : __btrc_str_track(__btrc_substring(tail, 0, quoteIndex)));
        int commaIndex = __btrc_indexOf(content, ",");
        char* head = ((commaIndex < 0) ? content : __btrc_str_track(__btrc_substring(content, 0, commaIndex)));
        btrc_Vector_string_push(heads, __btrc_str_track(__btrc_trim(head)));
    }
    return heads;
    if (heads != NULL) {
        if ((--heads->__rc) <= 0) {
            btrc_Vector_string_destroy(heads);
        }
    }
}

int InputContractTest_countOf(btrc_Vector_string* values, char* value) {
    int count = 0;
    int __n_704 = btrc_Vector_string_iterLen(values);
    for (int __i_703 = 0; (__i_703 < __n_704); (__i_703++)) {
        char* candidate = btrc_Vector_string_iterGet(values, __i_703);
        if (strcmp(candidate, value) == 0) {
            (count++);
        }
    }
    return count;
}

int InputContractTest_checkBindingSets(char* emittedLine, char* goldenLine, char* what) {
    btrc_Vector_string* emittedHeads = InputContractTest_normalizedBindings(emittedLine);
    btrc_Vector_string* goldenHeads = InputContractTest_normalizedBindings(goldenLine);
    int violations = InputContractTest_check((btrc_Vector_string_size(emittedHeads) == btrc_Vector_string_size(goldenHeads)), __btrc_str_track(__btrc_strcat(what, ": binding count")));
    int __n_706 = btrc_Vector_string_iterLen(goldenHeads);
    for (int __i_705 = 0; (__i_705 < __n_706); (__i_705++)) {
        char* goldenHead = btrc_Vector_string_iterGet(goldenHeads, __i_705);
        (violations += InputContractTest_check((InputContractTest_countOf(emittedHeads, goldenHead) == InputContractTest_countOf(goldenHeads, goldenHead)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(what, ": binding '")), goldenHead)), "'"))));
    }
    return violations;
}

char* InputContractTest_lineContaining(char* text, char* needle) {
    btrc_Vector_string* __iter_707 = Strings_split(text, "\n");
    int __n_709 = btrc_Vector_string_iterLen(__iter_707);
    for (int __i_708 = 0; (__i_708 < __n_709); (__i_708++)) {
        char* line = btrc_Vector_string_iterGet(__iter_707, __i_708);
        if (__btrc_strContains(line, needle)) {
            return line;
        }
    }
    return "";
}

int InputContractTest_compareGolden(char* emittedVdf, char* goldenVdf, char* name) {
    int violations = 0;
    btrc_Vector_string* __iter_710 = Strings_split(goldenVdf, "\n");
    int __n_712 = btrc_Vector_string_iterLen(__iter_710);
    for (int __i_711 = 0; (__i_711 < __n_712); (__i_711++)) {
        char* goldenLine = btrc_Vector_string_iterGet(__iter_710, __i_711);
        if (__btrc_strContains(goldenLine, "\"id\" \"10\"")) {
            break;
        }
        if ((!__btrc_isEmpty(__btrc_str_track(__btrc_trim(goldenLine)))) && (!__btrc_startsWith(goldenLine, "\t\"title\""))) {
            (violations += InputContractTest_check(__btrc_strContains(emittedVdf, goldenLine), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, ": missing passthrough line: ")), __btrc_str_track(__btrc_trim(goldenLine))))));
        }
    }
    (violations += InputContractTest_check(__btrc_strContains(emittedVdf, InputContractTest_lineContaining(goldenVdf, "\"preset\" { \"id\" \"0\"")), __btrc_str_track(__btrc_strcat(name, ": gamepad preset line matches golden"))));
    (violations += InputContractTest_check(__btrc_strContains(emittedVdf, InputContractTest_lineContaining(goldenVdf, "\"settings\"")), __btrc_str_track(__btrc_strcat(name, ": settings line matches golden"))));
    for (int slotIndex = 0; (slotIndex < 6); (slotIndex++)) {
        int __fstr_713_arg0 = slotIndex;
        int __fstr_713_len = snprintf(NULL, 0, "\"touch_menu_button_%d\"", __fstr_713_arg0);
        char* __fstr_713_buf = __btrc_str_track(((char*)malloc((__fstr_713_len + 1))));
        snprintf(__fstr_713_buf, (__fstr_713_len + 1), "\"touch_menu_button_%d\"", __fstr_713_arg0);
        char* slotKey = __fstr_713_buf;
        char* goldenLine = InputContractTest_lineContaining(goldenVdf, slotKey);
        char* emittedLine = InputContractTest_lineContaining(emittedVdf, slotKey);
        if (__btrc_isEmpty(goldenLine) || __btrc_isEmpty(emittedLine)) {
            int __fstr_714_arg0 = slotIndex;
            int __fstr_714_len = snprintf(NULL, 0, ": radial slot %d present in golden and emitted", __fstr_714_arg0);
            char* __fstr_714_buf = __btrc_str_track(((char*)malloc((__fstr_714_len + 1))));
            snprintf(__fstr_714_buf, (__fstr_714_len + 1), ": radial slot %d present in golden and emitted", __fstr_714_arg0);
            (violations += InputContractTest_check(false, __btrc_str_track(__btrc_strcat(name, __fstr_714_buf))));
        } else {
            int __fstr_715_arg0 = slotIndex;
            int __fstr_715_len = snprintf(NULL, 0, ": radial slot %d", __fstr_715_arg0);
            char* __fstr_715_buf = __btrc_str_track(((char*)malloc((__fstr_715_len + 1))));
            snprintf(__fstr_715_buf, (__fstr_715_len + 1), ": radial slot %d", __fstr_715_arg0);
            (violations += InputContractTest_checkBindingSets(emittedLine, goldenLine, __btrc_str_track(__btrc_strcat(name, __fstr_715_buf))));
        }
    }
    return violations;
}

int InputContractTest_checkVdfStructure(char* vdfText, char* name, SteamInputTarget* target, Keymap* keymap, char* title, char* radialName) {
    int violations = 0;
    (violations += InputContractTest_check((!__btrc_isEmpty(vdfText)), __btrc_str_track(__btrc_strcat(name, ": emitted file is empty"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"title\"\t\t\"", title)), "\""))), __btrc_str_track(__btrc_strcat(name, ": title"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"name\" \"", radialName)), "\""))), __btrc_str_track(__btrc_strcat(name, ": radial name"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "\"controller_type\"\t\t\"controller_neptune\""), __btrc_str_track(__btrc_strcat(name, ": controller type"))));
    for (int slotIndex = 0; (slotIndex < btrc_Vector_string_size(target->radialSlots)); (slotIndex++)) {
        int __fstr_716_arg0 = slotIndex;
        int __fstr_716_len = snprintf(NULL, 0, "\"touch_menu_button_%d\"", __fstr_716_arg0);
        char* __fstr_716_buf = __btrc_str_track(((char*)malloc((__fstr_716_len + 1))));
        snprintf(__fstr_716_buf, (__fstr_716_len + 1), "\"touch_menu_button_%d\"", __fstr_716_arg0);
        int __fstr_717_arg0 = slotIndex;
        int __fstr_717_len = snprintf(NULL, 0, ": radial slot %d", __fstr_717_arg0);
        char* __fstr_717_buf = __btrc_str_track(((char*)malloc((__fstr_717_len + 1))));
        snprintf(__fstr_717_buf, (__fstr_717_len + 1), ": radial slot %d", __fstr_717_arg0);
        (violations += InputContractTest_check(__btrc_strContains(vdfText, __fstr_716_buf), __btrc_str_track(__btrc_strcat(name, __fstr_717_buf))));
    }
    int slotCount = btrc_Vector_string_size(target->radialSlots);
    int __fstr_718_arg0 = slotCount;
    int __fstr_718_len = snprintf(NULL, 0, "touch_menu_button_%d", __fstr_718_arg0);
    char* __fstr_718_buf = __btrc_str_track(((char*)malloc((__fstr_718_len + 1))));
    snprintf(__fstr_718_buf, (__fstr_718_len + 1), "touch_menu_button_%d", __fstr_718_arg0);
    (violations += InputContractTest_check((!__btrc_strContains(vdfText, __fstr_718_buf)), __btrc_str_track(__btrc_strcat(name, ": unexpected extra radial slot"))));
    btrc_Vector_KeymapBinding_p1* __iter_719 = keymap->bindings;
    int __n_721 = btrc_Vector_KeymapBinding_p1_iterLen(__iter_719);
    for (int __i_720 = 0; (__i_720 < __n_721); (__i_720++)) {
        KeymapBinding* binding = btrc_Vector_KeymapBinding_p1_iterGet(__iter_719, __i_720);
        char* label = SteamInputTarget_actionLabel(target, binding->actionId);
        char* tokens = ChordTranslator_vdfTokens(Keymap_chordFor(keymap, binding->actionId), target);
        btrc_Vector_string* __iter_722 = Strings_split(tokens, " ");
        int __n_724 = btrc_Vector_string_iterLen(__iter_722);
        for (int __i_723 = 0; (__i_723 < __n_724); (__i_723++)) {
            char* keyName = btrc_Vector_string_iterGet(__iter_722, __i_723);
            (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"key_press ", keyName)), ", ")), label)), ", , \""))), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, ": binding '")), binding->button)), "' key token '")), keyName)), "' (")), label)), ")"))));
        }
    }
    char* quitLabel = SteamInputTarget_actionLabel(target, "app.quit");
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"xinput_button SELECT, ", quitLabel)), ", , \""))), __btrc_str_track(__btrc_strcat(name, ": quit extra xinput_select"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"xinput_button START, ", quitLabel)), ", , \""))), __btrc_str_track(__btrc_strcat(name, ": quit extra xinput_start"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"key_press ESCAPE, ", quitLabel)), ", , \""))), __btrc_str_track(__btrc_strcat(name, ": quit extra escape"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"key_press LEFT_ALT, ", quitLabel)), ", , \""))), __btrc_str_track(__btrc_strcat(name, ": quit extra alt_f4 (alt)"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"key_press F4, ", quitLabel)), ", , \""))), __btrc_str_track(__btrc_strcat(name, ": quit extra alt_f4 (f4)"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "\"preset\" { \"id\" \"0\" \"name\" \"Default\""), __btrc_str_track(__btrc_strcat(name, ": gamepad preset"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "\"preset\" { \"id\" \"1\" \"name\" \"Preset_1000001\""), __btrc_str_track(__btrc_strcat(name, ": hotkey preset"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "\"14\" \"switch active\""), __btrc_str_track(__btrc_strcat(name, ": hotkey switch layer"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "controller_action CHANGE_PRESET 2 0 0, Hotkey, , "), __btrc_str_track(__btrc_strcat(name, ": view long-press preset switch"))));
    (violations += InputContractTest_check(__btrc_strContains(vdfText, "controller_action CHANGE_PRESET 1 0 0, , "), __btrc_str_track(__btrc_strcat(name, ": preset return"))));
    return violations;
}

int InputContractTest_run(char* repoRoot) {
    char* sourceRoot = Environment_get("SEMU_SRC_ROOT", PathTools_join(repoRoot, "src/semu"));
    char* targetDirectory = PathTools_join(sourceRoot, "input/targets/steamdeck");
    char* goldenDirectory = PathTools_join(repoRoot, "garbage/generated/packaging/input/steam-input");
    ParseLog* log = ParseLog_new();
    Keymap* keymap = KeymapParser_parse(Path_readAll(PathTools_join(targetDirectory, "steamdeck.skm")), log);
    SteamInputTarget* target = SteamInputParser_parse(PathTools_join(targetDirectory, "steam_input.json"), log);
    int violations = 0;
    int __fstr_726_arg0 = btrc_Vector_KeymapAction_p1_size(keymap->actions);
    int __fstr_726_arg1 = btrc_Vector_KeymapBinding_p1_size(keymap->bindings);
    int __fstr_726_len = snprintf(NULL, 0, "keymap: %d actions, %d bindings", __fstr_726_arg0, __fstr_726_arg1);
    char* __fstr_726_buf = __btrc_str_track(((char*)malloc((__fstr_726_len + 1))));
    snprintf(__fstr_726_buf, (__fstr_726_len + 1), "keymap: %d actions, %d bindings", __fstr_726_arg0, __fstr_726_arg1);
    printf("%s\n", __fstr_726_buf);
    int __fstr_730_arg0 = btrc_Vector_string_size(target->actionIds);
    int __fstr_730_len = snprintf(NULL, 0, "steam-input: %d vocabulary actions, ", __fstr_730_arg0);
    char* __fstr_730_buf = __btrc_str_track(((char*)malloc((__fstr_730_len + 1))));
    snprintf(__fstr_730_buf, (__fstr_730_len + 1), "steam-input: %d vocabulary actions, ", __fstr_730_arg0);
    int __fstr_731_arg0 = btrc_Vector_string_size(target->radialSlots);
    int __fstr_731_len = snprintf(NULL, 0, "%d radial slots, ", __fstr_731_arg0);
    char* __fstr_731_buf = __btrc_str_track(((char*)malloc((__fstr_731_len + 1))));
    snprintf(__fstr_731_buf, (__fstr_731_len + 1), "%d radial slots, ", __fstr_731_arg0);
    int __fstr_732_arg0 = btrc_Vector_string_size(target->templateIds);
    char* __fstr_732_arg1 = target->defaultTemplateId;
    int __fstr_732_len = snprintf(NULL, 0, "%d templates (default %s)", __fstr_732_arg0, __fstr_732_arg1);
    char* __fstr_732_buf = __btrc_str_track(((char*)malloc((__fstr_732_len + 1))));
    snprintf(__fstr_732_buf, (__fstr_732_len + 1), "%d templates (default %s)", __fstr_732_arg0, __fstr_732_arg1);
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__fstr_730_buf, __fstr_731_buf)), __fstr_732_buf)));
    (violations += InputContractTest_check((btrc_Vector_KeymapAction_p1_size(keymap->actions) == 14), "expected 14 keymap actions"));
    (violations += InputContractTest_check((btrc_Vector_KeymapBinding_p1_size(keymap->bindings) == 13), "expected 13 keymap bindings"));
    (violations += InputContractTest_check((btrc_Vector_string_size(target->actionIds) == 14), "expected 14 vocabulary actions"));
    (violations += InputContractTest_check((btrc_Vector_string_size(target->radialSlots) == 6), "expected 6 radial slots"));
    (violations += InputContractTest_check((btrc_Vector_string_size(target->quitExtraBindings) == 4), "expected 4 quit extra bindings"));
    (violations += InputContractTest_check((btrc_Vector_string_size(target->templateIds) == 2), "expected 2 templates"));
    (violations += InputContractTest_checkEquals(target->defaultTemplateId, "neptune_full", "default template"));
    (violations += InputContractTest_check((!__btrc_isEmpty(target->templatesDirectory)), "expected a linux templates_dir"));
    (violations += InputContractTest_checkEquals(SteamInputTarget_deviceIdentity(target, "retroarch_input_driver"), "sdl2", "device identity lookup"));
    (violations += InputContractTest_checkChordTokens(target));
    char* outputDirectory = PathTools_join(Environment_get("SEMU_TEST_TMP", "/tmp/semu-core-tests"), "input-test");
    SteamdeckInputEmitter_emitTemplates(target, keymap, outputDirectory);
    for (int templateIndex = 0; (templateIndex < btrc_Vector_string_size(target->templateIds)); (templateIndex++)) {
        char* outputName = btrc_Vector_string_get(target->templateOutputs, templateIndex);
        char* vdfPath = PathTools_join(outputDirectory, outputName);
        char* vdfText = Path_readAll(vdfPath);
        (violations += InputContractTest_checkVdfStructure(vdfText, outputName, target, keymap, btrc_Vector_string_get(target->templateTitles, templateIndex), btrc_Vector_string_get(target->templateRadialNames, templateIndex)));
        char* goldenPath = PathTools_join(goldenDirectory, outputName);
        if (FileSystem_isFile(goldenPath)) {
            (violations += InputContractTest_compareGolden(vdfText, Path_readAll(goldenPath), outputName));
        } else {
            (violations += InputContractTest_check(false, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(outputName, ": golden missing: ")), goldenPath))));
        }
        printf("%s\n", __btrc_str_track(__btrc_strcat("emitted ", vdfPath)));
    }
    if (!ParseLog_ok(log)) {
        int __fstr_734_arg0 = btrc_Vector_string_size(log->errors);
        int __fstr_734_len = snprintf(NULL, 0, "FAIL: %d input contract violations", __fstr_734_arg0);
        char* __fstr_734_buf = __btrc_str_track(((char*)malloc((__fstr_734_len + 1))));
        snprintf(__fstr_734_buf, (__fstr_734_len + 1), "FAIL: %d input contract violations", __fstr_734_arg0);
        printf("%s\n", __fstr_734_buf);
        btrc_Vector_string* __iter_735 = log->errors;
        int __n_737 = btrc_Vector_string_iterLen(__iter_735);
        for (int __i_736 = 0; (__i_736 < __n_737); (__i_736++)) {
            char* error = btrc_Vector_string_iterGet(__iter_735, __i_736);
            printf("%s\n", __btrc_str_track(__btrc_strcat("  ", error)));
        }
        (violations += btrc_Vector_string_size(log->errors));
    }
    if (violations == 0) {
        printf("%s\n", "input: keymap + steam-input target + VDF emission valid");
    }
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return violations;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

void GithubPin_init(GithubPin* self) {
    self->__rc = 1;
    (self->key = "");
    (self->owner = "");
    (self->repository = "");
    (self->revision = "");
    (self->narHash = "");
}

GithubPin* GithubPin_new(void) {
    GithubPin* self = ((GithubPin*)malloc(sizeof(GithubPin)));
    memset(self, 0, sizeof(GithubPin));
    GithubPin_init(self);
    return self;
}

void GithubPin_destroy(GithubPin* self) {
    free(self);
}

char* NixPackage_sourcesPath(char* repoRoot) {
    return PathTools_join(repoRoot, "src/semu/emulators/rendering/assets/sources.json");
}

char* NixPackage_bezelsNixPath(char* repoRoot) {
    return PathTools_join(repoRoot, "src/semu/packaging/nix/semu_bezels.nix");
}

char* NixPackage_stringField(JsonValue* objectValue, char* key) {
    JsonValue* value = JsonValue_get(objectValue, key);
    if (JsonValue_isString(value)) {
        return JsonValue_asString(value);
    }
    return "";
}

int NixPackage_checkUpstreamPins(JsonValue* root, char* file, btrc_Vector_GithubPin_p1* pins, ParseLog* log) {
    JsonValue* upstreams = JsonValue_get(root, "upstreams");
    if (!JsonValue_isObject(upstreams)) {
        ParseLog_add(log, file, "upstreams", "missing or not an object");
        return 1;
    }
    int issues = 0;
    btrc_Vector_string* __iter_738 = JsonValue_keys(upstreams);
    int __n_740 = btrc_Vector_string_iterLen(__iter_738);
    for (int __i_739 = 0; (__i_739 < __n_740); (__i_739++)) {
        char* pinKey = btrc_Vector_string_iterGet(__iter_738, __i_739);
        JsonValue* pinValue = JsonValue_get(upstreams, pinKey);
        char* kind = NixPackage_stringField(pinValue, "kind");
        if (strcmp(kind, "github") == 0) {
            char* revision = NixPackage_stringField(pinValue, "rev");
            char* narHash = NixPackage_stringField(pinValue, "nar_hash");
            if (__btrc_isEmpty(revision)) {
                ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("upstreams.", pinKey)), "github pin missing rev");
                (issues = (issues + 1));
            }
            if (__btrc_isEmpty(narHash)) {
                ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("upstreams.", pinKey)), "github pin missing nar_hash");
                (issues = (issues + 1));
            }
            if ((!__btrc_isEmpty(revision)) && (!__btrc_isEmpty(narHash))) {
                GithubPin* pin = GithubPin_new();
                (pin->key = pinKey);
                (pin->owner = NixPackage_stringField(pinValue, "owner"));
                (pin->repository = NixPackage_stringField(pinValue, "repo"));
                (pin->revision = revision);
                (pin->narHash = narHash);
                btrc_Vector_GithubPin_p1_push(pins, pin);
                if (pin != NULL) {
                    if ((--pin->__rc) <= 0) {
                        GithubPin_destroy(pin);
                    }
                }
            }
        } else if (strcmp(kind, "nixpkgs") == 0) {
            if (__btrc_isEmpty(NixPackage_stringField(pinValue, "attr"))) {
                ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("upstreams.", pinKey)), "nixpkgs pin missing attr");
                (issues = (issues + 1));
            }
        } else {
            ParseLog_add(log, file, __btrc_str_track(__btrc_strcat("upstreams.", pinKey)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unknown pin kind '", kind)), "'")));
            (issues = (issues + 1));
        }
    }
    return issues;
}

int NixPackage_checkAssetRecipe(char* assetPath, btrc_Vector_string* recipeKeys, char* file, char* context, ParseLog* log) {
    if (__btrc_isEmpty(assetPath)) {
        return 0;
    }
    if (!__btrc_startsWith(assetPath, "assets/")) {
        ParseLog_add(log, file, context, __btrc_str_track(__btrc_strcat("asset path is not canonical (assets/...): ", assetPath)));
        return 1;
    }
    if (!btrc_Vector_string_contains(recipeKeys, assetPath)) {
        ParseLog_add(log, file, context, __btrc_str_track(__btrc_strcat("no recipe in sources.json assets for ", assetPath)));
        return 1;
    }
    return 0;
}

int NixPackage_checkSystemAssets(SystemDefinition* systemDefinition, btrc_Vector_string* recipeKeys, ParseLog* log) {
    int issues = 0;
    if (systemDefinition->bezels->present) {
        btrc_Vector_BezelVariant_p1* __iter_741 = systemDefinition->bezels->variants;
        int __n_743 = btrc_Vector_BezelVariant_p1_iterLen(__iter_741);
        for (int __i_742 = 0; (__i_742 < __n_743); (__i_742++)) {
            BezelVariant* variant = btrc_Vector_BezelVariant_p1_iterGet(__iter_741, __i_742);
            char* context = __btrc_str_track(__btrc_strcat("bezels.variants.", variant->id));
            (issues = (issues + NixPackage_checkAssetRecipe(variant->art, recipeKeys, systemDefinition->bezels->file, __btrc_str_track(__btrc_strcat(context, ".art")), log)));
            (issues = (issues + NixPackage_checkAssetRecipe(variant->glass, recipeKeys, systemDefinition->bezels->file, __btrc_str_track(__btrc_strcat(context, ".glass")), log)));
        }
    }
    if (systemDefinition->shaders->present) {
        char* shadersFile = systemDefinition->shaders->file;
        (issues = (issues + NixPackage_checkAssetRecipe(systemDefinition->shaders->screen, recipeKeys, shadersFile, "shaders.screen", log)));
        (issues = (issues + NixPackage_checkAssetRecipe(systemDefinition->shaders->composite, recipeKeys, shadersFile, "shaders.composite", log)));
        if (systemDefinition->shaders->hasWidescreen) {
            (issues = (issues + NixPackage_checkAssetRecipe(systemDefinition->shaders->widescreenScreen, recipeKeys, shadersFile, "shaders.widescreen.screen", log)));
            (issues = (issues + NixPackage_checkAssetRecipe(systemDefinition->shaders->widescreenComposite, recipeKeys, shadersFile, "shaders.widescreen.composite", log)));
        }
    }
    return issues;
}

int NixPackage_checkReferencedAssets(char* repoRoot, btrc_Vector_string* recipeKeys, char* sourcesFile, ParseLog* log) {
    int issues = 0;
    ParseLog* contractLog = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(PathTools_join(repoRoot, "src/semu"), contractLog);
    if (!ParseLog_ok(contractLog)) {
        btrc_Vector_string* __iter_744 = contractLog->errors;
        int __n_746 = btrc_Vector_string_iterLen(__iter_744);
        for (int __i_745 = 0; (__i_745 < __n_746); (__i_745++)) {
            char* error = btrc_Vector_string_iterGet(__iter_744, __i_745);
            ParseLog_add(log, "nix_package", "contracts", error);
            (issues = (issues + 1));
        }
    }
    btrc_Vector_SystemDefinition_p1* __iter_747 = contracts->systems;
    int __n_749 = btrc_Vector_SystemDefinition_p1_iterLen(__iter_747);
    for (int __i_748 = 0; (__i_748 < __n_749); (__i_748++)) {
        SystemDefinition* systemDefinition = btrc_Vector_SystemDefinition_p1_iterGet(__iter_747, __i_748);
        (issues = (issues + NixPackage_checkSystemAssets(systemDefinition, recipeKeys, log)));
    }
    btrc_Vector_string* __list_750 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_750, "assets/bezels/generic/4x3.png");
    btrc_Vector_string_push(__list_750, "assets/bezels/generic/16x9.png");
    btrc_Vector_string_push(__list_750, "assets/bezels/generic/dual.png");
    btrc_Vector_string* genericFallbackArts = __list_750;
    int __n_752 = btrc_Vector_string_iterLen(genericFallbackArts);
    for (int __i_751 = 0; (__i_751 < __n_752); (__i_751++)) {
        char* genericArt = btrc_Vector_string_iterGet(genericFallbackArts, __i_751);
        (issues = (issues + NixPackage_checkAssetRecipe(genericArt, recipeKeys, sourcesFile, "generic_fallback", log)));
    }
    if (genericFallbackArts != NULL) {
        if ((--genericFallbackArts->__rc) <= 0) {
            btrc_Vector_string_destroy(genericFallbackArts);
        }
    }
    if (contractLog != NULL) {
        if ((--contractLog->__rc) <= 0) {
            ParseLog_destroy(contractLog);
        }
    }
    return issues;
    if (genericFallbackArts != NULL) {
        if ((--genericFallbackArts->__rc) <= 0) {
            btrc_Vector_string_destroy(genericFallbackArts);
        }
    }
    if (contractLog != NULL) {
        if ((--contractLog->__rc) <= 0) {
            ParseLog_destroy(contractLog);
        }
    }
}

void NixPackage_crossCheckBezelsNix(char* repoRoot, btrc_Vector_GithubPin_p1* pins, ParseLog* log) {
    char* nixPath = NixPackage_bezelsNixPath(repoRoot);
    if (!FileSystem_isFile(nixPath)) {
        ParseLog_add(log, nixPath, "pins", "warning: semu_bezels.nix not found; pin cross-check skipped");
        return;
    }
    char* nixText = Path_readAll(nixPath);
    int __n_754 = btrc_Vector_GithubPin_p1_iterLen(pins);
    for (int __i_753 = 0; (__i_753 < __n_754); (__i_753++)) {
        GithubPin* pin = btrc_Vector_GithubPin_p1_iterGet(pins, __i_753);
        if (!__btrc_strContains(nixText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("repo = \"", pin->repository)), "\"")))) {
            ParseLog_add(log, nixPath, __btrc_str_track(__btrc_strcat("pins.", pin->key)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("warning: repo '", pin->repository)), "' is not fetched by semu_bezels.nix")));
        } else {
            if (!__btrc_strContains(nixText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("rev = \"", pin->revision)), "\"")))) {
                ParseLog_add(log, nixPath, __btrc_str_track(__btrc_strcat("pins.", pin->key)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("warning: rev pin differs from sources.json (", pin->revision)), ")")));
            }
            if (!__btrc_strContains(nixText, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("hash = \"", pin->narHash)), "\"")))) {
                ParseLog_add(log, nixPath, __btrc_str_track(__btrc_strcat("pins.", pin->key)), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("warning: hash pin differs from sources.json (", pin->narHash)), ")")));
            }
        }
    }
}

int NixPackage_verify(char* repoRoot, ParseLog* log) {
    char* sourcesFile = NixPackage_sourcesPath(repoRoot);
    JsonValue* root = JsonValue_readFile(sourcesFile);
    if (JsonValue_isError(root) || (!JsonValue_isObject(root))) {
        ParseLog_add(log, sourcesFile, "(root)", "missing or invalid JSON");
        return 1;
    }
    btrc_Vector_GithubPin_p1* pins = btrc_Vector_GithubPin_p1_new();
    int issues = NixPackage_checkUpstreamPins(root, sourcesFile, pins, log);
    btrc_Vector_string* recipeKeys = btrc_Vector_string_new();
    JsonValue* assets = JsonValue_get(root, "assets");
    if (!JsonValue_isObject(assets)) {
        ParseLog_add(log, sourcesFile, "assets", "missing or not an object");
        (issues = (issues + 1));
    } else {
        btrc_Vector_string* __iter_755 = JsonValue_keys(assets);
        int __n_757 = btrc_Vector_string_iterLen(__iter_755);
        for (int __i_756 = 0; (__i_756 < __n_757); (__i_756++)) {
            char* recipeKey = btrc_Vector_string_iterGet(__iter_755, __i_756);
            btrc_Vector_string_push(recipeKeys, recipeKey);
        }
    }
    (issues = (issues + NixPackage_checkReferencedAssets(repoRoot, recipeKeys, sourcesFile, log)));
    NixPackage_crossCheckBezelsNix(repoRoot, pins, log);
    if (recipeKeys != NULL) {
        if ((--recipeKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(recipeKeys);
        }
    }
    if (pins != NULL) {
        if ((--pins->__rc) <= 0) {
            btrc_Vector_GithubPin_p1_destroy(pins);
        }
    }
    return issues;
    if (recipeKeys != NULL) {
        if ((--recipeKeys->__rc) <= 0) {
            btrc_Vector_string_destroy(recipeKeys);
        }
    }
    if (pins != NULL) {
        if ((--pins->__rc) <= 0) {
            btrc_Vector_GithubPin_p1_destroy(pins);
        }
    }
}

int PackageContractTest_fail(char* message) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("  FAIL ", message)));
    return 1;
}

int PackageContractTest_require(char* text, char* needle, char* what) {
    if (!__btrc_strContains(text, needle)) {
        return PackageContractTest_fail(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(what, " missing: ")), needle)));
    }
    return 0;
}

int PackageContractTest_occurrenceCount(char* text, char* needle) {
    btrc_Vector_string* segments = Strings_split(text, needle);
    return (btrc_Vector_string_size(segments) - 1);
}

bool PackageContractTest_isExecutable(char* path) {
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_run(shell, __btrc_str_track(__btrc_strcat("test -x ", ShellWords_quote(path))), CommandOutput_suppress(), CommandOutput_suppress(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
    bool __btrc_ret_758 = (result->code == 0);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_758;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int PackageContractTest_checkEsSystems(SemuContracts* contracts) {
    int failures = 0;
    char* xml = SystemEmitter_esSystemsXml(contracts, "linux");
    int systemCount = PackageContractTest_occurrenceCount(xml, "  <system>\n");
    if (systemCount != btrc_Vector_SystemDefinition_p1_size(contracts->systems)) {
        int __fstr_759_arg0 = systemCount;
        int __fstr_759_arg1 = btrc_Vector_SystemDefinition_p1_size(contracts->systems);
        int __fstr_759_len = snprintf(NULL, 0, "es_systems block count %d != %d systems", __fstr_759_arg0, __fstr_759_arg1);
        char* __fstr_759_buf = __btrc_str_track(((char*)malloc((__fstr_759_len + 1))));
        snprintf(__fstr_759_buf, (__fstr_759_len + 1), "es_systems block count %d != %d systems", __fstr_759_arg0, __fstr_759_arg1);
        (failures = (failures + PackageContractTest_fail(__fstr_759_buf)));
    }
    if (btrc_Vector_SystemDefinition_p1_size(contracts->systems) != 17) {
        int __fstr_760_arg0 = btrc_Vector_SystemDefinition_p1_size(contracts->systems);
        int __fstr_760_len = snprintf(NULL, 0, "expected 17 system contracts, got %d", __fstr_760_arg0);
        char* __fstr_760_buf = __btrc_str_track(((char*)malloc((__fstr_760_len + 1))));
        snprintf(__fstr_760_buf, (__fstr_760_len + 1), "expected 17 system contracts, got %d", __fstr_760_arg0);
        (failures = (failures + PackageContractTest_fail(__fstr_760_buf)));
    }
    (failures = (failures + PackageContractTest_require(xml, "<command label=\"Gambatte\">%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%</command>", "gb linux command")));
    (failures = (failures + PackageContractTest_require(xml, "<command label=\"Mupen64Plus-Next\">%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mupen64plus_next_libretro.so %ROM%</command>", "n64 linux command")));
    (failures = (failures + PackageContractTest_require(xml, "<command label=\"Flycast\">%EMULATOR_FLYCAST% -config window:fullscreen=yes %ROM%</command>", "dreamcast linux command")));
    (failures = (failures + PackageContractTest_require(xml, "<name>gb</name>", "gb system entry")));
    (failures = (failures + PackageContractTest_require(xml, "<path>%ROMPATH%/gb</path>", "gb rompath")));
    (failures = (failures + PackageContractTest_require(xml, "<extension>.bin .BIN .dmg .DMG .gb .GB .gbs .GBS .7z .7Z .zip .ZIP</extension>", "gb both-case extension list")));
    (failures = (failures + PackageContractTest_require(xml, "<platform>genesis,megadrive</platform>", "genesis multi-platform join")));
    if (__btrc_strContains(xml, "%EMULATOR_ARES%")) {
        (failures = (failures + PackageContractTest_fail("macos-only ares binding leaked into linux es_systems")));
    }
    if (__btrc_strContains(xml, "semu-settings")) {
        (failures = (failures + PackageContractTest_fail("retired semu-settings synthetic system was emitted")));
    }
    char* macosXml = SystemEmitter_esSystemsXml(contracts, "macos");
    (failures = (failures + PackageContractTest_require(macosXml, "<command label=\"ares\">%EMULATOR_ARES% --fullscreen --system &quot;Nintendo 64&quot; %ROM%</command>", "n64 macos ares command")));
    return failures;
}

int PackageContractTest_checkFindRules(SemuContracts* contracts, char* repoRoot) {
    int failures = 0;
    char* rules = SystemEmitter_esFindRulesXml(contracts->emulators, "linux");
    int linuxEmulatorCount = 0;
    btrc_Vector_EmulatorDefinition_p1* __iter_761 = contracts->emulators;
    int __n_763 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_761);
    for (int __i_762 = 0; (__i_762 < __n_763); (__i_762++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_761, __i_762);
        if (EmulatorDefinition_hasOperatingSystem(emulatorDefinition, "linux")) {
            (linuxEmulatorCount = (linuxEmulatorCount + 1));
            (failures = (failures + PackageContractTest_require(rules, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<emulator name=\"", __btrc_str_track(__btrc_toUpper(emulatorDefinition->id)))), "\">")), __btrc_str_track(__btrc_strcat(emulatorDefinition->id, " find rule")))));
            (failures = (failures + PackageContractTest_require(rules, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<rule type=\"systempath\">\n      <entry>semu-", emulatorDefinition->id)), "</entry>")), __btrc_str_track(__btrc_strcat(emulatorDefinition->id, " systempath entry")))));
            (failures = (failures + PackageContractTest_require(rules, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<entry>%ESPATH%/semu-", emulatorDefinition->id)), "</entry>")), __btrc_str_track(__btrc_strcat(emulatorDefinition->id, " %ESPATH% staticpath entry")))));
        }
    }
    if (linuxEmulatorCount != 9) {
        int __fstr_764_arg0 = linuxEmulatorCount;
        int __fstr_764_len = snprintf(NULL, 0, "expected 9 linux emulators, got %d", __fstr_764_arg0);
        char* __fstr_764_buf = __btrc_str_track(((char*)malloc((__fstr_764_len + 1))));
        snprintf(__fstr_764_buf, (__fstr_764_len + 1), "expected 9 linux emulators, got %d", __fstr_764_arg0);
        (failures = (failures + PackageContractTest_fail(__fstr_764_buf)));
    }
    if (__btrc_strContains(rules, "<emulator name=\"ARES\">")) {
        (failures = (failures + PackageContractTest_fail("macos-only ares got a linux find rule")));
    }
    if (__btrc_strContains(rules, repoRoot)) {
        (failures = (failures + PackageContractTest_fail("find rules leaked a bootstrap-machine absolute path")));
    }
    (failures = (failures + PackageContractTest_require(rules, "<core name=\"RETROARCH\">", "retroarch corepath rule")));
    (failures = (failures + PackageContractTest_require(rules, "<entry>%ESPATH%/../lib/retroarch/cores</entry>", "corepath bundled AppImage dir")));
    (failures = (failures + PackageContractTest_require(rules, "<entry>/usr/lib/x86_64-linux-gnu/libretro</entry>", "corepath x86_64")));
    (failures = (failures + PackageContractTest_require(rules, "<entry>/usr/lib/aarch64-linux-gnu/libretro</entry>", "corepath aarch64")));
    (failures = (failures + PackageContractTest_require(rules, "<entry>/usr/lib/libretro</entry>", "corepath generic")));
    if (__btrc_strContains(rules, "${env:") || __btrc_strContains(rules, "${nix_result}")) {
        (failures = (failures + PackageContractTest_fail("find rules leaked unexpandable semu tokens into corepath entries")));
    }
    return failures;
}

int PackageContractTest_checkAppImage(SemuContracts* contracts, char* repoRoot, char* outputRoot) {
    int failures = 0;
    AppImagePackage_emitForEmulators(contracts->emulators, repoRoot, outputRoot);
    char* appRunPath = PathTools_join(outputRoot, "packaging/linux/AppRun");
    if (!FileSystem_isFile(appRunPath)) {
        (failures = (failures + PackageContractTest_fail(__btrc_str_track(__btrc_strcat("AppRun not emitted at ", appRunPath)))));
    } else {
        char* appRun = Path_readAll(appRunPath);
        if (!__btrc_startsWith(appRun, "#!")) {
            (failures = (failures + PackageContractTest_fail("AppRun lacks a shebang")));
        }
        (failures = (failures + PackageContractTest_require(appRun, "APPDIR=", "AppRun APPDIR resolution")));
        (failures = (failures + PackageContractTest_require(appRun, "SEMU_PROJECT_DIR", "AppRun project-dir wiring")));
        if (!PackageContractTest_isExecutable(appRunPath)) {
            (failures = (failures + PackageContractTest_fail("AppRun is not executable")));
        }
    }
    char* desktopPath = PathTools_join(outputRoot, "packaging/linux/semu.desktop");
    if (!FileSystem_isFile(desktopPath)) {
        (failures = (failures + PackageContractTest_fail(__btrc_str_track(__btrc_strcat("semu.desktop not emitted at ", desktopPath)))));
    } else {
        char* desktop = Path_readAll(desktopPath);
        (failures = (failures + PackageContractTest_require(desktop, "[Desktop Entry]", "desktop entry header")));
        (failures = (failures + PackageContractTest_require(desktop, "Exec=AppRun", "desktop Exec line")));
    }
    char* shimPath = PathTools_join(outputRoot, "packaging/linux/bin/semu-retroarch");
    if (!FileSystem_isFile(shimPath)) {
        (failures = (failures + PackageContractTest_fail(__btrc_str_track(__btrc_strcat("retroarch shim not emitted at ", shimPath)))));
    } else if (!PackageContractTest_isExecutable(shimPath)) {
        (failures = (failures + PackageContractTest_fail("retroarch shim is not executable")));
    }
    return failures;
}

int PackageContractTest_checkManifest(SemuContracts* contracts) {
    int failures = 0;
    JsonValue* parsed = JsonValue_parse(SystemEmitter_manifestJson(contracts));
    if (JsonValue_isError(parsed) || (!JsonValue_isObject(parsed))) {
        return PackageContractTest_fail("SystemEmitter.manifestJson is not valid JSON");
    }
    if (JsonValue_size(JsonValue_get(parsed, "systems")) != btrc_Vector_SystemDefinition_p1_size(contracts->systems)) {
        (failures = (failures + PackageContractTest_fail("manifest systems count mismatch")));
    }
    if (JsonValue_size(JsonValue_get(parsed, "emulators")) != btrc_Vector_EmulatorDefinition_p1_size(contracts->emulators)) {
        (failures = (failures + PackageContractTest_fail("manifest emulators count mismatch")));
    }
    if ((!JsonValue_isInt(JsonValue_get(parsed, "schema_version"))) || (JsonValue_asInt(JsonValue_get(parsed, "schema_version")) != 1)) {
        (failures = (failures + PackageContractTest_fail("manifest schema_version != 1")));
    }
    return failures;
}

int PackageContractTest_checkNixLayer(char* repoRoot) {
    int failures = 0;
    ParseLog* nixLog = ParseLog_new();
    int issues = NixPackage_verify(repoRoot, nixLog);
    btrc_Vector_string* __iter_765 = nixLog->errors;
    int __n_767 = btrc_Vector_string_iterLen(__iter_765);
    for (int __i_766 = 0; (__i_766 < __n_767); (__i_766++)) {
        char* entry = btrc_Vector_string_iterGet(__iter_765, __i_766);
        if (__btrc_strContains(entry, "warning:")) {
            printf("%s\n", __btrc_str_track(__btrc_strcat("  WARN ", entry)));
        } else {
            printf("%s\n", __btrc_str_track(__btrc_strcat("  ", entry)));
        }
    }
    if (issues != 0) {
        int __fstr_768_arg0 = issues;
        int __fstr_768_len = snprintf(NULL, 0, "NixPackage.verify reported %d issues (see above)", __fstr_768_arg0);
        char* __fstr_768_buf = __btrc_str_track(((char*)malloc((__fstr_768_len + 1))));
        snprintf(__fstr_768_buf, (__fstr_768_len + 1), "NixPackage.verify reported %d issues (see above)", __fstr_768_arg0);
        (failures = (failures + PackageContractTest_fail(__fstr_768_buf)));
    }
    if (nixLog != NULL) {
        if ((--nixLog->__rc) <= 0) {
            ParseLog_destroy(nixLog);
        }
    }
    return failures;
    if (nixLog != NULL) {
        if ((--nixLog->__rc) <= 0) {
            ParseLog_destroy(nixLog);
        }
    }
}

int PackageContractTest_run(char* repoRoot) {
    int failures = 0;
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(PathTools_join(repoRoot, "src/semu"), log);
    if (!ParseLog_ok(log)) {
        btrc_Vector_string* __iter_769 = log->errors;
        int __n_771 = btrc_Vector_string_iterLen(__iter_769);
        for (int __i_770 = 0; (__i_770 < __n_771); (__i_770++)) {
            char* error = btrc_Vector_string_iterGet(__iter_769, __i_770);
            (failures = (failures + PackageContractTest_fail(__btrc_str_track(__btrc_strcat("parse: ", error)))));
        }
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return failures;
    }
    char* outputRoot = Environment_get("SEMU_PKG_TEST_DIR", PathTools_join(repoRoot, "src/generated/test/pkg-test"));
    (failures = (failures + PackageContractTest_checkEsSystems(contracts)));
    (failures = (failures + PackageContractTest_checkFindRules(contracts, repoRoot)));
    (failures = (failures + PackageContractTest_checkAppImage(contracts, repoRoot, outputRoot)));
    (failures = (failures + PackageContractTest_checkManifest(contracts)));
    (failures = (failures + PackageContractTest_checkNixLayer(repoRoot)));
    if (failures > 0) {
        int __fstr_773_arg0 = failures;
        int __fstr_773_len = snprintf(NULL, 0, "package contract: FAIL (%d violations)", __fstr_773_arg0);
        char* __fstr_773_buf = __btrc_str_track(((char*)malloc((__fstr_773_len + 1))));
        snprintf(__fstr_773_buf, (__fstr_773_len + 1), "package contract: FAIL (%d violations)", __fstr_773_arg0);
        printf("%s\n", __fstr_773_buf);
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return failures;
    }
    printf("%s\n", "package contract: es-de xml + appimage + manifest + nix pins valid");
    int __btrc_ret_774 = 0;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_774;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

int GeneratedTreeTest_fail(char* message) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("  FAIL ", message)));
    return 1;
}

int GeneratedTreeTest_requireFile(char* root, char* relativePath) {
    if (!FileSystem_isFile(PathTools_join(root, relativePath))) {
        return GeneratedTreeTest_fail(__btrc_str_track(__btrc_strcat("expected generated file missing: ", relativePath)));
    }
    return 0;
}

int GeneratedTreeTest_checkArtifactTree(SemuContracts* contracts, char* generatedRoot) {
    int failures = 0;
    (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, "semu.json")));
    (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, "packaging/es-de/custom_systems/es_systems.xml")));
    (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, "packaging/es-de/custom_systems/es_find_rules.xml")));
    (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, "packaging/linux/AppRun")));
    (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, "packaging/linux/semu.desktop")));
    btrc_Vector_EmulatorDefinition_p1* __iter_775 = contracts->emulators;
    int __n_777 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_775);
    for (int __i_776 = 0; (__i_776 < __n_777); (__i_776++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_775, __i_776);
        (failures = (failures + GeneratedTreeTest_requireFile(generatedRoot, __btrc_str_track(__btrc_strcat("packaging/linux/bin/semu-", emulatorDefinition->id)))));
    }
    return failures;
}

int GeneratedTreeTest_checkTreeAudit(char* repoRoot) {
    UnixShell* shell = UnixShell_new();
    ExecResult* audit = UnixShell_run(shell, "make -f tests/Makefile tree-audit", CommandOutput_collect(), CommandOutput_combine(), false, true, false, "", "", repoRoot, CommandEnvironment_empty(), false, "");
    if (audit->code != 0) {
        int __fstr_778_arg0 = audit->code;
        int __fstr_778_len = snprintf(NULL, 0, "tree-audit failed (exit %d):", __fstr_778_arg0);
        char* __fstr_778_buf = __btrc_str_track(((char*)malloc((__fstr_778_len + 1))));
        snprintf(__fstr_778_buf, (__fstr_778_len + 1), "tree-audit failed (exit %d):", __fstr_778_arg0);
        int failures = GeneratedTreeTest_fail(__fstr_778_buf);
        printf("%s\n", audit->out);
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return failures;
    }
    int __btrc_ret_779 = 0;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_779;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int GeneratedTreeTest_run(char* repoRoot) {
    int failures = 0;
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SystemParser_loadAll(PathTools_join(repoRoot, "src/semu"), log);
    if (!ParseLog_ok(log)) {
        btrc_Vector_string* __iter_780 = log->errors;
        int __n_782 = btrc_Vector_string_iterLen(__iter_780);
        for (int __i_781 = 0; (__i_781 < __n_782); (__i_781++)) {
            char* error = btrc_Vector_string_iterGet(__iter_780, __i_781);
            (failures = (failures + GeneratedTreeTest_fail(__btrc_str_track(__btrc_strcat("parse: ", error)))));
        }
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return failures;
    }
    char* generatedRoot = PathTools_join(repoRoot, "src/generated");
    SystemEmitter_emitAll(contracts, generatedRoot);
    AppImagePackage_emitForEmulators(contracts->emulators, repoRoot, generatedRoot);
    (failures = (failures + GeneratedTreeTest_checkArtifactTree(contracts, generatedRoot)));
    (failures = (failures + GeneratedTreeTest_checkTreeAudit(repoRoot)));
    if (failures > 0) {
        int __fstr_784_arg0 = failures;
        int __fstr_784_len = snprintf(NULL, 0, "generated tree: FAIL (%d violations)", __fstr_784_arg0);
        char* __fstr_784_buf = __btrc_str_track(((char*)malloc((__fstr_784_len + 1))));
        snprintf(__fstr_784_buf, (__fstr_784_len + 1), "generated tree: FAIL (%d violations)", __fstr_784_arg0);
        printf("%s\n", __fstr_784_buf);
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return failures;
    }
    printf("%s\n", "generated tree: emission + tree-audit clean");
    int __btrc_ret_785 = 0;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_785;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

char* SemuCli_projectRoot(CliArgs* arguments) {
    return CliArgs_valueAfter(arguments, "--project", FileSystem_currentDirectory());
}

char* SemuCli_sourceRoot(char* project) {
    return PathTools_join(project, "src/semu");
}

char* SemuCli_generatedRoot(char* project) {
    return PathTools_join(project, "src/generated");
}

char* SemuCli_detectOperatingSystem(void) {
    char* forced = Environment_get("SEMU_OS", "");
    if (!__btrc_isEmpty(forced)) {
        return forced;
    }
    if (FileSystem_isDir("/System/Library/CoreServices")) {
        return "macos";
    }
    return "linux";
}

char* SemuCli_detectSession(void) {
    char* forced = Environment_get("SEMU_SESSION", "");
    if (!__btrc_isEmpty(forced)) {
        return forced;
    }
    if (strcmp(__btrc_str_track(__btrc_toLower(Environment_get("XDG_CURRENT_DESKTOP", ""))), "gamescope") == 0) {
        return "gamescope";
    }
    return "desktop";
}

SemuContracts* SemuCli_loadContracts(char* project, ParseLog* log) {
    return SystemParser_loadAll(SemuCli_sourceRoot(project), log);
}

bool SemuCli_reportErrors(ParseLog* log) {
    if (ParseLog_ok(log)) {
        return false;
    }
    int __fstr_787_arg0 = btrc_Vector_string_size(log->errors);
    int __fstr_787_len = snprintf(NULL, 0, "contract violations: %d", __fstr_787_arg0);
    char* __fstr_787_buf = __btrc_str_track(((char*)malloc((__fstr_787_len + 1))));
    snprintf(__fstr_787_buf, (__fstr_787_len + 1), "contract violations: %d", __fstr_787_arg0);
    printf("%s\n", __fstr_787_buf);
    btrc_Vector_string* __iter_788 = log->errors;
    int __n_790 = btrc_Vector_string_iterLen(__iter_788);
    for (int __i_789 = 0; (__i_789 < __n_790); (__i_789++)) {
        char* error = btrc_Vector_string_iterGet(__iter_788, __i_789);
        printf("%s\n", __btrc_str_track(__btrc_strcat("  ", error)));
    }
    return true;
}

int SemuCli_runManifest(CliArgs* arguments, char* project) {
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SemuCli_loadContracts(project, log);
    if (SemuCli_reportErrors(log)) {
        int __btrc_ret_791 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_791;
    }
    char* output = CliArgs_valueAfter(arguments, "--output", PathTools_join(SemuCli_generatedRoot(project), "semu.json"));
    UnixFileSystem_mkdirp(PathTools_dirname(output));
    if (!Path_writeAll(output, SystemEmitter_manifestJson(contracts))) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("error: failed to write manifest: ", output)));
        int __btrc_ret_792 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_792;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("wrote ", output)));
    int __btrc_ret_793 = 0;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_793;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

char* SemuCli_inputTargetDirectory(char* project) {
    return PathTools_join(SemuCli_sourceRoot(project), "input/targets/steamdeck");
}

int SemuCli_runBootstrap(CliArgs* arguments, char* project) {
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SemuCli_loadContracts(project, log);
    if (SemuCli_reportErrors(log)) {
        int __btrc_ret_794 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_794;
    }
    char* targetDirectory = SemuCli_inputTargetDirectory(project);
    char* keymapPath = PathTools_join(targetDirectory, "steamdeck.skm");
    if (!FileSystem_isFile(keymapPath)) {
        ParseLog_add(log, "input/targets/steamdeck/steamdeck.skm", "file", "missing keymap contract");
    }
    Keymap* keymap = KeymapParser_parse(Path_readAll(keymapPath), log);
    SteamInputTarget* steamInput = SteamInputParser_parse(PathTools_join(targetDirectory, "steam_input.json"), log);
    if (SemuCli_reportErrors(log)) {
        int __btrc_ret_795 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        return __btrc_ret_795;
    }
    char* generatedRoot = SemuCli_generatedRoot(project);
    char* packagingRoot = PathTools_join(generatedRoot, "packaging");
    SystemEmitter_emitAll(contracts, generatedRoot);
    printf("%s\n", __btrc_str_track(__btrc_strcat("emitted manifest + es-de configuration -> ", generatedRoot)));
    AppImagePackage_emitForEmulators(contracts->emulators, project, generatedRoot);
    printf("%s\n", __btrc_str_track(__btrc_strcat("emitted appimage entry + launcher shims -> ", PathTools_join(packagingRoot, "linux"))));
    char* steamInputDirectory = PathTools_join(packagingRoot, "input/steam-input");
    SteamdeckInputEmitter_emitTemplates(steamInput, keymap, steamInputDirectory);
    printf("%s\n", __btrc_str_track(__btrc_strcat("emitted steam-input templates -> ", steamInputDirectory)));
    char* profilesRoot = PathTools_join(packagingRoot, "emulators/profiles");
    btrc_Vector_EmulatorDefinition_p1* __iter_796 = contracts->emulators;
    int __n_798 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_796);
    for (int __i_797 = 0; (__i_797 < __n_798); (__i_797++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_796, __i_797);
        EmulatorRegistry_emitProfiles(emulatorDefinition->id, emulatorDefinition, steamInput, keymap, profilesRoot);
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("emitted emulator profiles -> ", profilesRoot)));
    int __btrc_ret_799 = 0;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    return __btrc_ret_799;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
}

EmulatorDefinition* SemuCli_emulatorForId(SemuContracts* contracts, char* emulatorId) {
    btrc_Vector_EmulatorDefinition_p1* __iter_800 = contracts->emulators;
    int __n_802 = btrc_Vector_EmulatorDefinition_p1_iterLen(__iter_800);
    for (int __i_801 = 0; (__i_801 < __n_802); (__i_801++)) {
        EmulatorDefinition* emulatorDefinition = btrc_Vector_EmulatorDefinition_p1_iterGet(__iter_800, __i_801);
        if (strcmp(emulatorDefinition->id, emulatorId) == 0) {
            return emulatorDefinition;
        }
    }
    return EmulatorDefinition_new();
}

int SemuCli_printPlan(LaunchPlan* launchPlan) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("backend: ", launchPlan->backend)));
    btrc_Vector_string* __iter_803 = launchPlan->argumentVector;
    int __n_805 = btrc_Vector_string_iterLen(__iter_803);
    for (int __i_804 = 0; (__i_804 < __n_805); (__i_804++)) {
        char* argument = btrc_Vector_string_iterGet(__iter_803, __i_804);
        printf("%s\n", __btrc_str_track(__btrc_strcat("argv: ", argument)));
    }
    for (int keyIndex = 0; (keyIndex < btrc_Vector_string_size(launchPlan->environmentSetKeys)); (keyIndex++)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("env: ", btrc_Vector_string_get(launchPlan->environmentSetKeys, keyIndex))), "=")), btrc_Vector_string_get(launchPlan->environmentSetValues, keyIndex))));
    }
    btrc_Vector_string* __iter_806 = launchPlan->environmentUnset;
    int __n_808 = btrc_Vector_string_iterLen(__iter_806);
    for (int __i_807 = 0; (__i_807 < __n_808); (__i_807++)) {
        char* key = btrc_Vector_string_iterGet(__iter_806, __i_807);
        printf("%s\n", __btrc_str_track(__btrc_strcat("unset: ", key)));
    }
    if (!__btrc_isEmpty(launchPlan->note)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("note: ", launchPlan->note)));
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("command: ", EmulatorLauncher_shellCommand(launchPlan))));
    return 0;
}

int SemuCli_runLauncher(CliArgs* arguments, char* project) {
    if (CliArgs_count(arguments) < 2) {
        printf("%s\n", "usage: semu launcher <emulator> [--print-plan] [args...] [rom]");
        return 1;
    }
    char* emulatorId = CliArgs_get(arguments, 1);
    bool printPlanRequested = false;
    btrc_Vector_string* systemArguments = btrc_Vector_string_new();
    for (int argumentIndex = 2; (argumentIndex < CliArgs_count(arguments)); (argumentIndex++)) {
        char* value = CliArgs_get(arguments, argumentIndex);
        if (strcmp(value, "--print-plan") == 0) {
            (printPlanRequested = true);
        } else if (strcmp(value, "--project") == 0) {
            (argumentIndex++);
        } else {
            btrc_Vector_string_push(systemArguments, value);
        }
    }
    ParseLog* log = ParseLog_new();
    SemuContracts* contracts = SemuCli_loadContracts(project, log);
    if (SemuCli_reportErrors(log)) {
        int __btrc_ret_809 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        if (systemArguments != NULL) {
            if ((--systemArguments->__rc) <= 0) {
                btrc_Vector_string_destroy(systemArguments);
            }
        }
        return __btrc_ret_809;
    }
    EmulatorDefinition* emulatorDefinition = SemuCli_emulatorForId(contracts, emulatorId);
    if (__btrc_isEmpty(emulatorDefinition->id)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("launcher: unknown emulator '", emulatorId)), "'")));
        int __btrc_ret_810 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        if (systemArguments != NULL) {
            if ((--systemArguments->__rc) <= 0) {
                btrc_Vector_string_destroy(systemArguments);
            }
        }
        return __btrc_ret_810;
    }
    char* operatingSystem = SemuCli_detectOperatingSystem();
    LaunchPlan* launchPlan = EmulatorLauncher_plan(emulatorDefinition, operatingSystem, SemuCli_detectSession(), "", systemArguments);
    if (btrc_Vector_string_size(launchPlan->argumentVector) == 0) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("launcher: ", launchPlan->note)));
        int __btrc_ret_811 = 1;
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        if (systemArguments != NULL) {
            if ((--systemArguments->__rc) <= 0) {
                btrc_Vector_string_destroy(systemArguments);
            }
        }
        return __btrc_ret_811;
    }
    if (printPlanRequested) {
        int __btrc_ret_812 = SemuCli_printPlan(launchPlan);
        if (log != NULL) {
            if ((--log->__rc) <= 0) {
                ParseLog_destroy(log);
            }
        }
        if (systemArguments != NULL) {
            if ((--systemArguments->__rc) <= 0) {
                btrc_Vector_string_destroy(systemArguments);
            }
        }
        return __btrc_ret_812;
    }
    int __btrc_ret_813 = EmulatorExecution_run(emulatorDefinition, operatingSystem, launchPlan, project);
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    if (systemArguments != NULL) {
        if ((--systemArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(systemArguments);
        }
    }
    return __btrc_ret_813;
    if (log != NULL) {
        if ((--log->__rc) <= 0) {
            ParseLog_destroy(log);
        }
    }
    if (systemArguments != NULL) {
        if ((--systemArguments->__rc) <= 0) {
            btrc_Vector_string_destroy(systemArguments);
        }
    }
}

int SemuCli_runTestCore(char* project) {
    int failures = 0;
    printf("%s\n", "== core: system contract");
    (failures = (failures + SystemContractTest_run(project)));
    printf("%s\n", "== core: emulator contract");
    (failures = (failures + EmulatorContractTest_run(project)));
    printf("%s\n", "== core: rendering contract");
    (failures = (failures + RenderingContractTest_run(project)));
    printf("%s\n", "== core: input contract");
    (failures = (failures + InputContractTest_run(project)));
    printf("%s\n", "== core: package contract");
    (failures = (failures + PackageContractTest_run(project)));
    printf("%s\n", "== core: generated tree");
    (failures = (failures + GeneratedTreeTest_run(project)));
    if (failures > 0) {
        int __fstr_815_arg0 = failures;
        int __fstr_815_len = snprintf(NULL, 0, "test core: FAIL (%d violations)", __fstr_815_arg0);
        char* __fstr_815_buf = __btrc_str_track(((char*)malloc((__fstr_815_len + 1))));
        snprintf(__fstr_815_buf, (__fstr_815_len + 1), "test core: FAIL (%d violations)", __fstr_815_arg0);
        printf("%s\n", __fstr_815_buf);
        return 1;
    }
    printf("%s\n", "test core: OK");
    return 0;
}

int SemuCli_printUsage(void) {
    printf("%s\n", "usage: semu <command>");
    printf("%s\n", "  manifest  [--output <path>]                  write src/generated/semu.json");
    printf("%s\n", "  bootstrap [--project <path>]                 emit src/generated/packaging/**");
    printf("%s\n", "  launcher  <emulator> [--print-plan] [args...] [rom]  plan/exec a launch");
    printf("%s\n", "  test core                                    run the contract test suites");
    return 1;
}

int SemuCli_run(CliArgs* arguments, char* repoRoot) {
    char* command = CliArgs_command(arguments);
    if (strcmp(command, "manifest") == 0) {
        return SemuCli_runManifest(arguments, repoRoot);
    }
    if (strcmp(command, "bootstrap") == 0) {
        return SemuCli_runBootstrap(arguments, repoRoot);
    }
    if (strcmp(command, "launcher") == 0) {
        return SemuCli_runLauncher(arguments, repoRoot);
    }
    if (strcmp(command, "test") == 0) {
        if ((CliArgs_count(arguments) > 1) && (strcmp(CliArgs_get(arguments, 1), "core") == 0)) {
            return SemuCli_runTestCore(repoRoot);
        }
        printf("%s\n", "usage: semu test core");
        return 1;
    }
    return SemuCli_printUsage();
}

int main(int argc, char** argv) {
    CliArgs* arguments = CliArgs_new(argc, argv);
    int __btrc_ret_816 = SemuCli_run(arguments, SemuCli_projectRoot(arguments));
    if (arguments != NULL) {
        if ((--arguments->__rc) <= 0) {
            CliArgs_destroy(arguments);
        }
    }
    return __btrc_ret_816;
    if (arguments != NULL) {
        if ((--arguments->__rc) <= 0) {
            CliArgs_destroy(arguments);
        }
    }
}
