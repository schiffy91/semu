#define _DEFAULT_SOURCE
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
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

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

/* btrc try/catch runtime (dynamic) */
static __thread int __btrc_try_cap = 16;
static __thread jmp_buf* __btrc_try_stack = NULL;
static __thread int __btrc_try_top = -1;
static __thread char __btrc_error_msg[1024] = "";

/* Cleanup stack: tracks heap resources to free on exception */
typedef void (*__btrc_cleanup_fn)(void*);
typedef struct { void** ptr_ref; __btrc_cleanup_fn fn; int try_level; } __btrc_cleanup_entry;
static __thread int __btrc_cleanup_cap = 64;
static __thread __btrc_cleanup_entry* __btrc_cleanup_stack = NULL;
static __thread int __btrc_cleanup_top = -1;

static inline void __btrc_run_cleanups(int level) {
    while (__btrc_cleanup_top >= 0 && __btrc_cleanup_stack[__btrc_cleanup_top].try_level >= level) {
        __btrc_cleanup_entry e = __btrc_cleanup_stack[__btrc_cleanup_top--];
        if (e.fn && e.ptr_ref && *e.ptr_ref) { e.fn(*e.ptr_ref); *e.ptr_ref = NULL; }
    }
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

#define _DARWIN_C_SOURCE

typedef struct Console Console;
typedef struct Strings Strings;
typedef struct CliArgs CliArgs;
void CliArgs_destroy(CliArgs* self);
typedef struct CliCommand CliCommand;
typedef struct CliCommandLine CliCommandLine;
typedef struct CliHelp CliHelp;
typedef struct File File;
void File_destroy(File* self);
typedef struct Path Path;
typedef struct UnixPlatform UnixPlatform;
typedef struct Platform Platform;
typedef struct Environment Environment;
typedef struct ProcessStatus ProcessStatus;
typedef struct UnixPipe UnixPipe;
typedef struct UnixProcess UnixProcess;
typedef struct ShellWords ShellWords;
typedef struct CommandOutput CommandOutput;
typedef struct CommandEnvironment CommandEnvironment;
typedef struct ExecResult ExecResult;
typedef struct Command Command;
void Command_destroy(Command* self);
typedef struct UnixShell UnixShell;
void UnixShell_destroy(UnixShell* self);
typedef struct PowerShell PowerShell;
typedef struct FileStatus FileStatus;
void FileStatus_destroy(FileStatus* self);
typedef struct Directory Directory;
void Directory_destroy(Directory* self);
typedef struct UnixFileSystem UnixFileSystem;
typedef struct PathTools PathTools;
typedef struct FileSystem FileSystem;
typedef struct JsonObject JsonObject;
void JsonObject_destroy(JsonObject* self);
typedef struct JsonText JsonText;
typedef struct SemuDefinition SemuDefinition;
void SemuDefinition_destroy(SemuDefinition* self);
typedef struct SemuSystemDefinition SemuSystemDefinition;
typedef struct SemuEmulatorDefinition SemuEmulatorDefinition;
typedef struct SemuBuildPlan SemuBuildPlan;
void SemuBuildPlan_destroy(SemuBuildPlan* self);
typedef struct SemuOwnedPaths SemuOwnedPaths;
typedef struct SemuCompilerLexer SemuCompilerLexer;
typedef struct SemuCompilerParser SemuCompilerParser;
typedef struct SemuMergePolicy SemuMergePolicy;
typedef struct SemuEmulatorDefinitionGenerator SemuEmulatorDefinitionGenerator;
typedef struct SemuTemplate SemuTemplate;
typedef struct SemuEmulatorStateGenerator SemuEmulatorStateGenerator;
typedef struct SemuEmulatorRenderingDefinition SemuEmulatorRenderingDefinition;
typedef struct SemuRenderingDefinition SemuRenderingDefinition;
typedef struct SemuEsDeCommand SemuEsDeCommand;
void SemuEsDeCommand_destroy(SemuEsDeCommand* self);
typedef struct SemuEsDeSystem SemuEsDeSystem;
void SemuEsDeSystem_destroy(SemuEsDeSystem* self);
typedef struct SemuEsDeFindRule SemuEsDeFindRule;
void SemuEsDeFindRule_destroy(SemuEsDeFindRule* self);
typedef struct SemuEsDeGenerator SemuEsDeGenerator;
typedef struct SemuSteamInputTemplate SemuSteamInputTemplate;
void SemuSteamInputTemplate_destroy(SemuSteamInputTemplate* self);
typedef struct SemuSteamInputGenerator SemuSteamInputGenerator;
typedef struct SemuAppImageContent SemuAppImageContent;
void SemuAppImageContent_destroy(SemuAppImageContent* self);
typedef struct SemuAppImageGenerator SemuAppImageGenerator;
typedef struct SemuCompilerResolver SemuCompilerResolver;
typedef struct SemuCompilerChecker SemuCompilerChecker;
typedef struct SemuCompilerProjectWriter SemuCompilerProjectWriter;
typedef struct SemuCompilerGenerator SemuCompilerGenerator;
typedef struct SemuCli SemuCli;
typedef struct SemuApp SemuApp;
typedef struct btrc_Vector_string btrc_Vector_string;
typedef struct btrc_Vector_bool btrc_Vector_bool;
typedef struct btrc_Vector_SemuEsDeCommand btrc_Vector_SemuEsDeCommand;
typedef struct btrc_Vector_SemuEsDeSystem btrc_Vector_SemuEsDeSystem;
typedef struct btrc_Vector_SemuEsDeFindRule btrc_Vector_SemuEsDeFindRule;
typedef struct btrc_Vector_SemuSteamInputTemplate btrc_Vector_SemuSteamInputTemplate;
typedef struct btrc_Vector_SemuAppImageContent btrc_Vector_SemuAppImageContent;
typedef struct btrc_Map_string_string btrc_Map_string_string;
typedef struct btrc_Map_string_bool btrc_Map_string_bool;
char* semuModelFirstRouteEmulator(char* raw);
char* joinPath(char* left, char* right);
void ensureDir(char* path);
char* semuEsDeXmlEscape(char* value);
char* semuEsDeExtensionList(btrc_Vector_string* extensions);
char* semuEsDeCommandXml(SemuEsDeCommand* command);
char* semuEsDeSystemXml(SemuEsDeSystem* system);
char* semuEsDeFindRuleXml(SemuEsDeFindRule* rule);
char* semuSteamInputJsonQ(char* value);
char* semuSteamInputJsonField(char* key, char* value);
char* semuSteamInputJsonStrField(char* key, char* value);
char* semuSteamInputJsonObject(btrc_Vector_string* fields);
char* semuSteamInputJsonStringArray(btrc_Vector_string* values);
char* semuAppImageShellWord(char* value);
char* semuAppImageJsonQ(char* value);
char* semuAppImageContentJson(SemuAppImageContent* content);
char* semuPrettyJsonString(char* value);
char* semuPrettyJsonObject(char* raw);
int semuBuildCommand(CliArgs* args, char* project);
int semuVerifyCommand(CliArgs* args, char* project);
char* Strings_copy(char* s);
char* Strings_replace(char* s, char* old, char* replacement);
btrc_Vector_string* Strings_split(char* s, char* delim);
int Strings_find(char* s, char* sub, int start);
char* Strings_fromInt(int n);
void CliArgs_init(CliArgs* self, int argc, char** argv);
CliArgs* CliArgs_new(int argc, char** argv);
int CliArgs_count(CliArgs* self);
char* CliArgs_get(CliArgs* self, int index);
char* CliArgs_command(CliArgs* self);
bool CliArgs_has(CliArgs* self, char* flag);
char* CliArgs_valueAfter(CliArgs* self, char* flag, char* fallback);
void File_init(File* self, char* path, char* mode);
File* File_new(char* path, char* mode);
bool File_ok(File* self);
char* File_read(File* self);
void File_write(File* self, char* text);
void File_close(File* self);
char* Path_readAll(char* path);
void Path_writeAll(char* path, char* content);
int UnixPlatform_pid(void);
int Platform_pid(void);
char* Environment_get(char* name, char* fallback);
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
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
void Command_init(Command* self, char* command);
Command* Command_new(char* command);
void UnixShell_init(UnixShell* self);
UnixShell* UnixShell_new(void);
char* UnixShell_redactText(char* text, char* sensitive);
void UnixShell_logError(char* message);
char* UnixShell_tempPath(UnixShell* self, char* name);
char* UnixShell_renderEnv(UnixShell* self, btrc_Vector_string* env);
char* UnixShell_withContext(UnixShell* self, char* command, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot);
char* UnixShell_withRedirections(UnixShell* self, char* rendered, char* stdout, char* stderr, char* outFile, char* errFile, char* stdinFile);
ExecResult* UnixShell_run(UnixShell* self, char* command, char* stdout, char* stderr, bool logCommand, bool logFailure, bool throwOnFailure, char* redactSubstring, char* stdin, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot);
char* mkdtemp(char* templatePath);
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
int FileSystem_mkdir(char* path, int mode);
int FileSystem_mkdirp(char* path);
char* FileSystem_currentDirectory(void);
btrc_Vector_string* FileSystem_listDir(char* path);
char* FileSystem_readText(char* path);
void FileSystem_writeText(char* path, char* content);
void JsonObject_init(JsonObject* self);
JsonObject* JsonObject_new(void);
char* JsonObject_escape(char* text);
void JsonObject_setString(JsonObject* self, char* key, char* value);
void JsonObject_setRaw(JsonObject* self, char* key, char* value);
void JsonObject_setBool(JsonObject* self, char* key, bool value);
char* JsonObject_getString(JsonObject* self, char* key, char* fallback);
bool JsonObject_getBool(JsonObject* self, char* key, bool fallback);
char* JsonObject_stringify(JsonObject* self);
JsonObject* JsonObject_parse(char* text);
JsonObject* JsonObject_readFile(char* path);
char* JsonText_slice(char* text, int start, int end);
char* JsonText_unescape(char* text);
int JsonText_stringEnd(char* text, int start);
int JsonText_balancedEnd(char* text, int start);
int JsonText_skipSpaces(char* text, int i);
int JsonText_keyPosition(char* text, char* key);
int JsonText_valueStart(char* text, char* key);
char* JsonText_parseStringValue(char* text, int i, char* fallback);
char* JsonText_field(char* text, char* key, char* fallback);
char* JsonText_objectField(char* text, char* key);
btrc_Vector_string* JsonText_stringArray(char* text, char* key);
btrc_Vector_string* JsonText_objectArray(char* text, char* key);
char* JsonText_expand(char* text, btrc_Map_string_string* args);
void SemuDefinition_init(SemuDefinition* self, char* id, char* path);
SemuDefinition* SemuDefinition_new(char* id, char* path);
bool SemuDefinition_exists(SemuDefinition* self);
JsonObject* SemuDefinition_json(SemuDefinition* self);
char* SemuDefinition_stringField(SemuDefinition* self, char* key, char* fallback);
btrc_Vector_string* SemuDefinition_stringArray(SemuDefinition* self, char* key);
void SemuSystemDefinition_init(SemuSystemDefinition* self, SemuDefinition* source);
SemuSystemDefinition* SemuSystemDefinition_new(SemuDefinition* source);
void SemuEmulatorDefinition_init(SemuEmulatorDefinition* self, SemuDefinition* source);
SemuEmulatorDefinition* SemuEmulatorDefinition_new(SemuDefinition* source);
void SemuBuildPlan_init(SemuBuildPlan* self, char* project, char* mode, char* target, char* emulator);
SemuBuildPlan* SemuBuildPlan_new(char* project, char* mode, char* target, char* emulator);
bool SemuBuildPlan_ok(SemuBuildPlan* self);
void SemuBuildPlan_addError(SemuBuildPlan* self, char* message);
void SemuBuildPlan_addWarning(SemuBuildPlan* self, char* message);
char* SemuOwnedPaths_sourceRoot(char* project);
char* SemuOwnedPaths_home(char* project);
char* SemuOwnedPaths_generated(char* project);
char* SemuOwnedPaths_state(char* project);
char* SemuOwnedPaths_cache(char* project);
char* SemuOwnedPaths_overrides(char* project);
char* SemuOwnedPaths_semuConfig(char* project);
char* SemuOwnedPaths_romsRoot(char* project);
char* SemuOwnedPaths_biosRoot(char* project);
char* SemuOwnedPaths_ps2BiosRoot(char* project);
char* SemuOwnedPaths_mediaRoot(char* project);
char* SemuOwnedPaths_themeRoot(char* project);
char* SemuOwnedPaths_directDefinitionRoot(char* root);
char* SemuOwnedPaths_sourceDefinitionRoot(char* root);
char* SemuOwnedPaths_definitionRoot(char* project);
char* SemuOwnedPaths_targetFile(char* project, char* target);
char* SemuOwnedPaths_systemFile(char* project, char* system);
char* SemuOwnedPaths_emulatorFile(char* project, char* emulator, char* name);
char* SemuOwnedPaths_systemAssetFile(char* project, char* system);
char* SemuOwnedPaths_settingsDefinitionFile(char* project, char* name);
char* SemuOwnedPaths_generatedFile(char* project, char* relative);
void SemuOwnedPaths_writeGenerated(char* project, char* relative, char* content);
btrc_Vector_string* SemuCompilerLexer_jsonIds(char* project, char* directory);
btrc_Vector_string* SemuCompilerLexer_emulatorIds(char* project);
SemuDefinition* SemuCompilerParser_target(char* project, char* target);
SemuSystemDefinition* SemuCompilerParser_system(char* project, char* system);
SemuEmulatorDefinition* SemuCompilerParser_emulator(char* project, char* emulator);
btrc_Vector_string* SemuCompilerParser_systemIds(char* project);
btrc_Vector_string* SemuCompilerParser_emulatorIds(char* project);
btrc_Vector_string* SemuCompilerParser_targetSystemIds(char* project, char* target);
btrc_Vector_string* SemuCompilerParser_targetEmulatorIds(char* project, char* target);
char* SemuMergePolicy_precedenceText(void);
char* SemuEmulatorDefinitionGenerator_plan(char* project, char* target, char* emulator);
char* SemuTemplate_expand(char* text, btrc_Map_string_string* values);
char* SemuTemplate_expandProject(char* project, char* text);
char* SemuEmulatorStateGenerator_stateFile(char* project, char* emulator);
char* SemuEmulatorStateGenerator_templateFile(char* project, char* emulator, char* name);
void SemuEmulatorStateGenerator_write(char* project, char* emulator);
void SemuEmulatorRenderingDefinition_init(SemuEmulatorRenderingDefinition* self, char* raw);
SemuEmulatorRenderingDefinition* SemuEmulatorRenderingDefinition_new(char* raw);
char* SemuEmulatorRenderingDefinition_field(SemuEmulatorRenderingDefinition* self, char* key, char* fallback);
bool SemuEmulatorRenderingDefinition_boolField(SemuEmulatorRenderingDefinition* self, char* key, bool fallback);
SemuEmulatorRenderingDefinition* SemuEmulatorRenderingDefinition_load(char* project, char* emulator);
void SemuRenderingDefinition_init(SemuRenderingDefinition* self, char* raw, char* overrideRaw);
SemuRenderingDefinition* SemuRenderingDefinition_new(char* raw, char* overrideRaw);
char* SemuRenderingDefinition_pathField(SemuRenderingDefinition* self, char* path, char* fallback);
char* SemuRenderingDefinition_viewportField(SemuRenderingDefinition* self, char* key, char* fallback);
char* SemuRenderingDefinition_rendererObject(SemuRenderingDefinition* self, char* key);
char* SemuRenderingDefinition_rendererField(SemuRenderingDefinition* self, char* objectKey, char* fieldKey, char* fallback);
char* SemuRenderingDefinition_nativeSize(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_contentAspect(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_layoutKind(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_scalePolicy(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_dynamicAspectFlag(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_bezelEffectFile(SemuRenderingDefinition* self);
char* SemuRenderingDefinition_shaderEffectFile(SemuRenderingDefinition* self);
SemuRenderingDefinition* SemuRenderingDefinition_load(char* project, char* systemId);
char* SemuRenderingDefinition_pathFieldIn(char* raw, char* path, char* fallback);
char* SemuRenderingDefinition_nestedOverrideJson(char* path, char* value);
char* SemuRenderingDefinition_jsonValue(char* value);
char* SemuRenderingDefinition_cleanValue(char* value);
char* SemuRenderingDefinition_indentJson(char* value);
void SemuEsDeCommand_init(SemuEsDeCommand* self);
SemuEsDeCommand* SemuEsDeCommand_new(void);
void SemuEsDeSystem_init(SemuEsDeSystem* self);
SemuEsDeSystem* SemuEsDeSystem_new(void);
void SemuEsDeFindRule_init(SemuEsDeFindRule* self);
SemuEsDeFindRule* SemuEsDeFindRule_new(void);
char* SemuEsDeGenerator_plan(char* project, char* target);
SemuEsDeCommand* SemuEsDeGenerator_command(char* label, char* command);
SemuEsDeSystem* SemuEsDeGenerator_system(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_SemuEsDeCommand* commands);
SemuEsDeFindRule* SemuEsDeGenerator_emulatorRule(char* name, char* launcherPath);
SemuEsDeFindRule* SemuEsDeGenerator_coreRule(char* name, btrc_Vector_string* paths);
char* SemuEsDeGenerator_settingsXml(char* roms, char* media, char* themes, char* startupSystem, char* startupView, char* theme);
char* SemuEsDeGenerator_systemsXml(btrc_Vector_SemuEsDeSystem* systems);
char* SemuEsDeGenerator_findRulesXml(btrc_Vector_SemuEsDeFindRule* rules);
char* SemuEsDeGenerator_settingsLauncherScript(void);
void SemuSteamInputTemplate_init(SemuSteamInputTemplate* self);
SemuSteamInputTemplate* SemuSteamInputTemplate_new(void);
char* SemuSteamInputGenerator_plan(char* project, char* target);
char* SemuSteamInputGenerator_selectionJson(char* targetId, char* deviceId, char* templateId, btrc_Vector_string* radialActions);
void SemuAppImageContent_init(SemuAppImageContent* self);
SemuAppImageContent* SemuAppImageContent_new(void);
char* SemuAppImageGenerator_plan(char* project, char* target);
SemuAppImageContent* SemuAppImageGenerator_content(char* path, char* text, bool executable);
char* SemuAppImageGenerator_btrcShim(void);
char* SemuAppImageGenerator_emulatorShim(char* emulatorId);
char* SemuAppImageGenerator_desktopEntry(char* name, char* comment, char* icon);
char* SemuAppImageGenerator_manifestJson(btrc_Vector_SemuAppImageContent* contents);
SemuBuildPlan* SemuCompilerResolver_resolve(CliArgs* args, char* project);
void SemuCompilerChecker_check(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_init(SemuCompilerProjectWriter* self);
void SemuCompilerProjectWriter_initializeHome(char* project);
void SemuCompilerProjectWriter_ensureUserConfig(char* project);
char* SemuCompilerProjectWriter_launcherBin(char* project);
char* SemuCompilerProjectWriter_stableLauncherBin(char* project);
char* SemuCompilerProjectWriter_stableLauncherPreamble(void);
char* SemuCompilerProjectWriter_stableEmulatorLauncher(char* emulator);
char* SemuCompilerProjectWriter_stableSettingsLauncher(void);
void SemuCompilerProjectWriter_writeStableLaunchers(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_writeTarget(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_writeRenderAssets(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_copyRenderAssetDirectory(char* project, char* name);
void SemuCompilerProjectWriter_writeRenderHookConfigs(SemuBuildPlan* plan);
char* SemuCompilerProjectWriter_renderHookConfig(SemuBuildPlan* plan, char* emulator);
bool SemuCompilerProjectWriter_stringVectorContains(btrc_Vector_string* values, char* expected);
char* SemuCompilerProjectWriter_renderHostEnv(SemuBuildPlan* plan);
bool SemuCompilerProjectWriter_settingBool(char* project, char* key, bool fallback);
void SemuCompilerProjectWriter_writeEmulatorState(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_writeEsDe(SemuBuildPlan* plan);
btrc_Vector_SemuEsDeCommand* SemuCompilerProjectWriter_commandsForSystem(char* project, char* systemRaw);
char* SemuCompilerProjectWriter_emulatorDisplayName(char* project, char* emulator);
void SemuCompilerProjectWriter_writeSteamInput(SemuBuildPlan* plan);
void SemuCompilerProjectWriter_writeLauncherInventory(SemuBuildPlan* plan);
int SemuCompilerGenerator_generate(SemuBuildPlan* plan);
int SemuCompilerGenerator_fail(SemuBuildPlan* plan);
int SemuCli_run(CliArgs* args);
char* SemuCli_launcherNameFromProgram(char* program);
void SemuCli_usage(void);
SemuBuildPlan* SemuCli_configPlan(char* project, char* target);
int SemuCli_generateConfigs(char* project, char* target);
int SemuCli_config(CliArgs* args, char* project);
int SemuCli_apprun(CliArgs* args, char* project);
char* SemuCli_settingsPath(char* project);
JsonObject* SemuCli_settingsObject(char* project);
char* SemuCli_normalizedKey(char* key);
void SemuCli_writeSettingsObject(char* project, JsonObject* object);
int SemuCli_settings(CliArgs* args, char* project);
int SemuCli_settingsAction(char* project, char* action);
bool SemuCli_shouldApply(CliArgs* args, int start);
int SemuCli_settingsPut(char* project, char* key, char* value);
int SemuCli_settingsToggle(char* project, char* key);
int SemuCli_settingsUi(char* project);
int SemuCli_launcher(CliArgs* args, char* project, char* emulator, int startIndex);
int SemuCli_launcherRouted(CliArgs* args, char* project);
char* SemuCli_renderLauncherArgs(btrc_Vector_string* args);
btrc_Vector_string* SemuCli_launcherDefaultArgs(char* project, char* emulator);
char* SemuCli_launcherEnvPrefix(char* project, char* linux);
char* SemuCli_launcherFlatpakEnv(char* project, char* linux);
btrc_Vector_string* SemuCli_normalizeRetroarchCoreArgs(char* emulator, btrc_Vector_string* args);
char* SemuCli_normalizedRetroarchCore(char* coreDir, char* core);
char* SemuCli_backendCommand(char* executable, char* flatpak, char* renderedArgs, char* envPrefix, char* flatpakEnv);
btrc_Vector_string* SemuCli_sourceHookLauncherArgs(char* project, char* emulator, btrc_Vector_string* args);
char* SemuCli_withQuitWatch(char* command);
char* SemuCli_quitWatchHelper(void);
char* SemuCli_withVisualWrapper(char* project, char* emulator, char* command);
char* SemuCli_withSourceHookEnv(char* project, char* emulator, char* systemId, char* command);
bool SemuCli_sourceHookCurrentlyApplied(char* project, char* emulator, char* systemId);
char* SemuCli_withNixGlOnlyWrapper(char* systemId, char* command);
char* SemuCli_renderBackend(char* project, char* emulator);
char* SemuCli_systemAssetField(char* project, char* systemId, char* field, char* fallback);
bool SemuCli_settingBool(char* project, char* key, bool fallback);
char* SemuCli_settingString(char* project, char* key, char* fallback);
int SemuCli_assets(CliArgs* args, char* project);
int SemuCli_keymap(CliArgs* args, char* project);
int SemuCli_sync(CliArgs* args, char* project);
int SemuCli_syncAction(char* project, char* mode);
int SemuCli_n3ds(char* project, char* mode);
int SemuCli_doctor(char* project);
int SemuCli_bootstrap(char* project);
int SemuCli_deck(char* project);
int SemuCli_e2e(CliArgs* args, char* project);
int SemuCli_manifest(char* project);
typedef bool (*__btrc_fn_bool_string)(char*);
typedef void (*__btrc_fn_void_string)(char*);
typedef char* (*__btrc_fn_string_string)(char*);
typedef char* (*__btrc_fn_string_string_string)(char*, char*);
typedef bool (*__btrc_fn_bool_bool)(bool);
typedef void (*__btrc_fn_void_bool)(bool);
typedef bool (*__btrc_fn_bool_bool_bool)(bool, bool);
typedef bool (*__btrc_fn_bool_SemuEsDeCommand)(SemuEsDeCommand*);
typedef void (*__btrc_fn_void_SemuEsDeCommand)(SemuEsDeCommand*);
typedef SemuEsDeCommand* (*__btrc_fn_SemuEsDeCommand_SemuEsDeCommand)(SemuEsDeCommand*);
typedef SemuEsDeCommand* (*__btrc_fn_SemuEsDeCommand_SemuEsDeCommand_SemuEsDeCommand)(SemuEsDeCommand*, SemuEsDeCommand*);
typedef bool (*__btrc_fn_bool_SemuEsDeSystem)(SemuEsDeSystem*);
typedef void (*__btrc_fn_void_SemuEsDeSystem)(SemuEsDeSystem*);
typedef SemuEsDeSystem* (*__btrc_fn_SemuEsDeSystem_SemuEsDeSystem)(SemuEsDeSystem*);
typedef SemuEsDeSystem* (*__btrc_fn_SemuEsDeSystem_SemuEsDeSystem_SemuEsDeSystem)(SemuEsDeSystem*, SemuEsDeSystem*);
typedef bool (*__btrc_fn_bool_SemuEsDeFindRule)(SemuEsDeFindRule*);
typedef void (*__btrc_fn_void_SemuEsDeFindRule)(SemuEsDeFindRule*);
typedef SemuEsDeFindRule* (*__btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule)(SemuEsDeFindRule*);
typedef SemuEsDeFindRule* (*__btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule_SemuEsDeFindRule)(SemuEsDeFindRule*, SemuEsDeFindRule*);
typedef bool (*__btrc_fn_bool_SemuSteamInputTemplate)(SemuSteamInputTemplate*);
typedef void (*__btrc_fn_void_SemuSteamInputTemplate)(SemuSteamInputTemplate*);
typedef SemuSteamInputTemplate* (*__btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate)(SemuSteamInputTemplate*);
typedef SemuSteamInputTemplate* (*__btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate_SemuSteamInputTemplate)(SemuSteamInputTemplate*, SemuSteamInputTemplate*);
typedef bool (*__btrc_fn_bool_SemuAppImageContent)(SemuAppImageContent*);
typedef void (*__btrc_fn_void_SemuAppImageContent)(SemuAppImageContent*);
typedef SemuAppImageContent* (*__btrc_fn_SemuAppImageContent_SemuAppImageContent)(SemuAppImageContent*);
typedef SemuAppImageContent* (*__btrc_fn_SemuAppImageContent_SemuAppImageContent_SemuAppImageContent)(SemuAppImageContent*, SemuAppImageContent*);

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

struct btrc_Vector_SemuEsDeCommand {
    int __rc;
    SemuEsDeCommand** data;
    int len;
    int cap;
};

struct btrc_Vector_SemuEsDeSystem {
    int __rc;
    SemuEsDeSystem** data;
    int len;
    int cap;
};

struct btrc_Vector_SemuEsDeFindRule {
    int __rc;
    SemuEsDeFindRule** data;
    int len;
    int cap;
};

struct btrc_Vector_SemuSteamInputTemplate {
    int __rc;
    SemuSteamInputTemplate** data;
    int len;
    int cap;
};

struct btrc_Vector_SemuAppImageContent {
    int __rc;
    SemuAppImageContent** data;
    int len;
    int cap;
};

struct btrc_Map_string_string {
    int __rc;
    char** keys;
    char** values;
    bool* occupied;
    int len;
    int cap;
};

struct btrc_Map_string_bool {
    int __rc;
    char** keys;
    bool* values;
    bool* occupied;
    int len;
    int cap;
};

struct Console {
    int __rc;
};

struct Strings {
    int __rc;
};

struct CliArgs {
    int __rc;
    char* program;
    btrc_Vector_string* values;
};

struct CliCommand {
    int __rc;
    char* name;
    btrc_Vector_string* aliases;
};

struct CliCommandLine {
    int __rc;
    btrc_Vector_string* operands;
    btrc_Map_string_string* options;
    btrc_Vector_string* optionNames;
    btrc_Vector_string* optionValues;
    btrc_Vector_string* errors;
};

struct CliHelp {
    int __rc;
    int width;
    btrc_Vector_string* lines;
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

struct UnixPlatform {
    int __rc;
};

struct Platform {
    int __rc;
};

struct Environment {
    int __rc;
};

struct ProcessStatus {
    int __rc;
    int raw;
};

struct UnixPipe {
    int __rc;
    FILE* handle;
    char* command;
};

struct UnixProcess {
    int __rc;
};

struct ShellWords {
    int __rc;
};

struct CommandOutput {
    int __rc;
};

struct CommandEnvironment {
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

struct PowerShell {
    int __rc;
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

struct UnixFileSystem {
    int __rc;
};

struct PathTools {
    int __rc;
};

struct FileSystem {
    int __rc;
};

struct JsonObject {
    int __rc;
    btrc_Map_string_string* values;
    btrc_Map_string_bool* quoted;
};

struct JsonText {
    int __rc;
};

struct SemuDefinition {
    int __rc;
    char* id;
    char* path;
    char* raw;
};

struct SemuSystemDefinition {
    int __rc;
    SemuDefinition* source;
    char* fullname;
    char* romDir;
    char* primaryEmulator;
    btrc_Vector_string* extensions;
    btrc_Vector_string* bios;
};

struct SemuEmulatorDefinition {
    int __rc;
    SemuDefinition* source;
    btrc_Vector_string* servedSystems;
};

struct SemuBuildPlan {
    int __rc;
    char* project;
    char* mode;
    char* target;
    char* emulator;
    btrc_Vector_string* systemIds;
    btrc_Vector_string* emulatorIds;
    btrc_Vector_string* errors;
    btrc_Vector_string* warnings;
    btrc_Vector_string* outputs;
};

struct SemuOwnedPaths {
    int __rc;
};

struct SemuCompilerLexer {
    int __rc;
};

struct SemuCompilerParser {
    int __rc;
};

struct SemuMergePolicy {
    int __rc;
};

struct SemuEmulatorDefinitionGenerator {
    int __rc;
};

struct SemuTemplate {
    int __rc;
};

struct SemuEmulatorStateGenerator {
    int __rc;
};

struct SemuEmulatorRenderingDefinition {
    int __rc;
    char* raw;
    char* renderer;
};

struct SemuRenderingDefinition {
    int __rc;
    char* raw;
    char* overrideRaw;
    char* viewport;
    char* overrideViewport;
    char* renderer;
    char* overrideRenderer;
};

struct SemuEsDeCommand {
    int __rc;
    char* label;
    char* command;
};

struct SemuEsDeSystem {
    int __rc;
    char* id;
    char* fullname;
    char* platform;
    char* theme;
    char* romDir;
    btrc_Vector_string* extensions;
    btrc_Vector_SemuEsDeCommand* commands;
};

struct SemuEsDeFindRule {
    int __rc;
    char* kind;
    char* name;
    btrc_Vector_string* entries;
};

struct SemuEsDeGenerator {
    int __rc;
};

struct SemuSteamInputTemplate {
    int __rc;
    char* id;
    char* title;
    char* source;
    char* destination;
    bool required;
};

struct SemuSteamInputGenerator {
    int __rc;
};

struct SemuAppImageContent {
    int __rc;
    char* path;
    char* text;
    bool executable;
};

struct SemuAppImageGenerator {
    int __rc;
};

struct SemuCompilerResolver {
    int __rc;
};

struct SemuCompilerChecker {
    int __rc;
};

struct SemuCompilerProjectWriter {
    int __rc;
};

struct SemuCompilerGenerator {
    int __rc;
};

struct SemuCli {
    int __rc;
};

struct SemuApp {
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
static char* btrc_Vector_string_pop(btrc_Vector_string* self);
static char* btrc_Vector_string_get(btrc_Vector_string* self, int i);
static void btrc_Vector_string_set(btrc_Vector_string* self, int i, char* val);
static void btrc_Vector_string_free(btrc_Vector_string* self);
static void btrc_Vector_string_remove(btrc_Vector_string* self, int idx);
static void btrc_Vector_string_reverse(btrc_Vector_string* self);
static btrc_Vector_string* btrc_Vector_string_reversed(btrc_Vector_string* self);
static void btrc_Vector_string_swap(btrc_Vector_string* self, int i, int j);
static void btrc_Vector_string_clear(btrc_Vector_string* self);
static void btrc_Vector_string_fill(btrc_Vector_string* self, char* val);
static int btrc_Vector_string_size(btrc_Vector_string* self);
static bool btrc_Vector_string_isEmpty(btrc_Vector_string* self);
static char* btrc_Vector_string_first(btrc_Vector_string* self);
static char* btrc_Vector_string_last(btrc_Vector_string* self);
static btrc_Vector_string* btrc_Vector_string_slice(btrc_Vector_string* self, int start, int end);
static btrc_Vector_string* btrc_Vector_string_take(btrc_Vector_string* self, int n);
static btrc_Vector_string* btrc_Vector_string_drop(btrc_Vector_string* self, int n);
static void btrc_Vector_string_extend(btrc_Vector_string* self, btrc_Vector_string* other);
static void btrc_Vector_string_insert(btrc_Vector_string* self, int idx, char* val);
static bool btrc_Vector_string_contains(btrc_Vector_string* self, char* val);
static int btrc_Vector_string_indexOf(btrc_Vector_string* self, char* val);
static int btrc_Vector_string_lastIndexOf(btrc_Vector_string* self, char* val);
static int btrc_Vector_string_count(btrc_Vector_string* self, char* val);
static void btrc_Vector_string_removeAll(btrc_Vector_string* self, char* val);
static btrc_Vector_string* btrc_Vector_string_distinct(btrc_Vector_string* self);
static void btrc_Vector_string_sort(btrc_Vector_string* self);
static btrc_Vector_string* btrc_Vector_string_sorted(btrc_Vector_string* self);
static char* btrc_Vector_string_min(btrc_Vector_string* self);
static char* btrc_Vector_string_max(btrc_Vector_string* self);
static char* btrc_Vector_string_sum(btrc_Vector_string* self);
static char* btrc_Vector_string_join(btrc_Vector_string* self, char* sep);
static char* btrc_Vector_string_joinToString(btrc_Vector_string* self, char* sep);
static btrc_Vector_string* btrc_Vector_string_filter(btrc_Vector_string* self, __btrc_fn_bool_string pred);
static int btrc_Vector_string_findIndex(btrc_Vector_string* self, __btrc_fn_bool_string pred);
static void btrc_Vector_string_forEach(btrc_Vector_string* self, __btrc_fn_void_string fn);
static btrc_Vector_string* btrc_Vector_string_map(btrc_Vector_string* self, __btrc_fn_string_string fn);
static bool btrc_Vector_string_any(btrc_Vector_string* self, __btrc_fn_bool_string pred);
static bool btrc_Vector_string_all(btrc_Vector_string* self, __btrc_fn_bool_string pred);
static char* btrc_Vector_string_reduce(btrc_Vector_string* self, char* init, __btrc_fn_string_string_string fn);
static btrc_Vector_string* btrc_Vector_string_copy(btrc_Vector_string* self);
static void btrc_Vector_string_removeAt(btrc_Vector_string* self, int idx);
static int btrc_Vector_string_iterLen(btrc_Vector_string* self);
static char* btrc_Vector_string_iterGet(btrc_Vector_string* self, int i);

static void btrc_Vector_bool_init(btrc_Vector_bool* self);
static btrc_Vector_bool* btrc_Vector_bool_new(void);
static void btrc_Vector_bool_destroy(btrc_Vector_bool* self);
static void btrc_Vector_bool_push(btrc_Vector_bool* self, bool val);
static bool btrc_Vector_bool_pop(btrc_Vector_bool* self);
static bool btrc_Vector_bool_get(btrc_Vector_bool* self, int i);
static void btrc_Vector_bool_set(btrc_Vector_bool* self, int i, bool val);
static void btrc_Vector_bool_free(btrc_Vector_bool* self);
static void btrc_Vector_bool_remove(btrc_Vector_bool* self, int idx);
static void btrc_Vector_bool_reverse(btrc_Vector_bool* self);
static btrc_Vector_bool* btrc_Vector_bool_reversed(btrc_Vector_bool* self);
static void btrc_Vector_bool_swap(btrc_Vector_bool* self, int i, int j);
static void btrc_Vector_bool_clear(btrc_Vector_bool* self);
static void btrc_Vector_bool_fill(btrc_Vector_bool* self, bool val);
static int btrc_Vector_bool_size(btrc_Vector_bool* self);
static bool btrc_Vector_bool_isEmpty(btrc_Vector_bool* self);
static bool btrc_Vector_bool_first(btrc_Vector_bool* self);
static bool btrc_Vector_bool_last(btrc_Vector_bool* self);
static btrc_Vector_bool* btrc_Vector_bool_slice(btrc_Vector_bool* self, int start, int end);
static btrc_Vector_bool* btrc_Vector_bool_take(btrc_Vector_bool* self, int n);
static btrc_Vector_bool* btrc_Vector_bool_drop(btrc_Vector_bool* self, int n);
static void btrc_Vector_bool_extend(btrc_Vector_bool* self, btrc_Vector_bool* other);
static void btrc_Vector_bool_insert(btrc_Vector_bool* self, int idx, bool val);
static bool btrc_Vector_bool_contains(btrc_Vector_bool* self, bool val);
static int btrc_Vector_bool_indexOf(btrc_Vector_bool* self, bool val);
static int btrc_Vector_bool_lastIndexOf(btrc_Vector_bool* self, bool val);
static int btrc_Vector_bool_count(btrc_Vector_bool* self, bool val);
static void btrc_Vector_bool_removeAll(btrc_Vector_bool* self, bool val);
static btrc_Vector_bool* btrc_Vector_bool_distinct(btrc_Vector_bool* self);
static void btrc_Vector_bool_sort(btrc_Vector_bool* self);
static btrc_Vector_bool* btrc_Vector_bool_sorted(btrc_Vector_bool* self);
static bool btrc_Vector_bool_min(btrc_Vector_bool* self);
static bool btrc_Vector_bool_max(btrc_Vector_bool* self);
static bool btrc_Vector_bool_sum(btrc_Vector_bool* self);
static char* btrc_Vector_bool_join(btrc_Vector_bool* self, char* sep);
static char* btrc_Vector_bool_joinToString(btrc_Vector_bool* self, char* sep);
static btrc_Vector_bool* btrc_Vector_bool_filter(btrc_Vector_bool* self, __btrc_fn_bool_bool pred);
static int btrc_Vector_bool_findIndex(btrc_Vector_bool* self, __btrc_fn_bool_bool pred);
static void btrc_Vector_bool_forEach(btrc_Vector_bool* self, __btrc_fn_void_bool fn);
static btrc_Vector_bool* btrc_Vector_bool_map(btrc_Vector_bool* self, __btrc_fn_bool_bool fn);
static bool btrc_Vector_bool_any(btrc_Vector_bool* self, __btrc_fn_bool_bool pred);
static bool btrc_Vector_bool_all(btrc_Vector_bool* self, __btrc_fn_bool_bool pred);
static bool btrc_Vector_bool_reduce(btrc_Vector_bool* self, bool init, __btrc_fn_bool_bool_bool fn);
static btrc_Vector_bool* btrc_Vector_bool_copy(btrc_Vector_bool* self);
static void btrc_Vector_bool_removeAt(btrc_Vector_bool* self, int idx);
static int btrc_Vector_bool_iterLen(btrc_Vector_bool* self);
static bool btrc_Vector_bool_iterGet(btrc_Vector_bool* self, int i);

static void btrc_Vector_SemuEsDeCommand_init(btrc_Vector_SemuEsDeCommand* self);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_new(void);
static void btrc_Vector_SemuEsDeCommand_destroy(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_push(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_pop(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_get(btrc_Vector_SemuEsDeCommand* self, int i);
static void btrc_Vector_SemuEsDeCommand_set(btrc_Vector_SemuEsDeCommand* self, int i, SemuEsDeCommand* val);
static void btrc_Vector_SemuEsDeCommand_free(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_remove(btrc_Vector_SemuEsDeCommand* self, int idx);
static void btrc_Vector_SemuEsDeCommand_reverse(btrc_Vector_SemuEsDeCommand* self);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_reversed(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_swap(btrc_Vector_SemuEsDeCommand* self, int i, int j);
static void btrc_Vector_SemuEsDeCommand_clear(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_fill(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static int btrc_Vector_SemuEsDeCommand_size(btrc_Vector_SemuEsDeCommand* self);
static bool btrc_Vector_SemuEsDeCommand_isEmpty(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_first(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_last(btrc_Vector_SemuEsDeCommand* self);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_slice(btrc_Vector_SemuEsDeCommand* self, int start, int end);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_take(btrc_Vector_SemuEsDeCommand* self, int n);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_drop(btrc_Vector_SemuEsDeCommand* self, int n);
static void btrc_Vector_SemuEsDeCommand_extend(btrc_Vector_SemuEsDeCommand* self, btrc_Vector_SemuEsDeCommand* other);
static void btrc_Vector_SemuEsDeCommand_insert(btrc_Vector_SemuEsDeCommand* self, int idx, SemuEsDeCommand* val);
static bool btrc_Vector_SemuEsDeCommand_contains(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static int btrc_Vector_SemuEsDeCommand_indexOf(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static int btrc_Vector_SemuEsDeCommand_lastIndexOf(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static int btrc_Vector_SemuEsDeCommand_count(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static void btrc_Vector_SemuEsDeCommand_removeAll(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_distinct(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_sort(btrc_Vector_SemuEsDeCommand* self);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_sorted(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_min(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_max(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_sum(btrc_Vector_SemuEsDeCommand* self);
static char* btrc_Vector_SemuEsDeCommand_join(btrc_Vector_SemuEsDeCommand* self, char* sep);
static char* btrc_Vector_SemuEsDeCommand_joinToString(btrc_Vector_SemuEsDeCommand* self, char* sep);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_filter(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred);
static int btrc_Vector_SemuEsDeCommand_findIndex(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred);
static void btrc_Vector_SemuEsDeCommand_forEach(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_void_SemuEsDeCommand fn);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_map(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_SemuEsDeCommand_SemuEsDeCommand fn);
static bool btrc_Vector_SemuEsDeCommand_any(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred);
static bool btrc_Vector_SemuEsDeCommand_all(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_reduce(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* init, __btrc_fn_SemuEsDeCommand_SemuEsDeCommand_SemuEsDeCommand fn);
static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_copy(btrc_Vector_SemuEsDeCommand* self);
static void btrc_Vector_SemuEsDeCommand_removeAt(btrc_Vector_SemuEsDeCommand* self, int idx);
static int btrc_Vector_SemuEsDeCommand_iterLen(btrc_Vector_SemuEsDeCommand* self);
static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_iterGet(btrc_Vector_SemuEsDeCommand* self, int i);

static void btrc_Vector_SemuEsDeSystem_init(btrc_Vector_SemuEsDeSystem* self);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_new(void);
static void btrc_Vector_SemuEsDeSystem_destroy(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_push(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_pop(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_get(btrc_Vector_SemuEsDeSystem* self, int i);
static void btrc_Vector_SemuEsDeSystem_set(btrc_Vector_SemuEsDeSystem* self, int i, SemuEsDeSystem* val);
static void btrc_Vector_SemuEsDeSystem_free(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_remove(btrc_Vector_SemuEsDeSystem* self, int idx);
static void btrc_Vector_SemuEsDeSystem_reverse(btrc_Vector_SemuEsDeSystem* self);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_reversed(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_swap(btrc_Vector_SemuEsDeSystem* self, int i, int j);
static void btrc_Vector_SemuEsDeSystem_clear(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_fill(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static int btrc_Vector_SemuEsDeSystem_size(btrc_Vector_SemuEsDeSystem* self);
static bool btrc_Vector_SemuEsDeSystem_isEmpty(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_first(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_last(btrc_Vector_SemuEsDeSystem* self);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_slice(btrc_Vector_SemuEsDeSystem* self, int start, int end);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_take(btrc_Vector_SemuEsDeSystem* self, int n);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_drop(btrc_Vector_SemuEsDeSystem* self, int n);
static void btrc_Vector_SemuEsDeSystem_extend(btrc_Vector_SemuEsDeSystem* self, btrc_Vector_SemuEsDeSystem* other);
static void btrc_Vector_SemuEsDeSystem_insert(btrc_Vector_SemuEsDeSystem* self, int idx, SemuEsDeSystem* val);
static bool btrc_Vector_SemuEsDeSystem_contains(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static int btrc_Vector_SemuEsDeSystem_indexOf(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static int btrc_Vector_SemuEsDeSystem_lastIndexOf(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static int btrc_Vector_SemuEsDeSystem_count(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static void btrc_Vector_SemuEsDeSystem_removeAll(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_distinct(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_sort(btrc_Vector_SemuEsDeSystem* self);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_sorted(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_min(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_max(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_sum(btrc_Vector_SemuEsDeSystem* self);
static char* btrc_Vector_SemuEsDeSystem_join(btrc_Vector_SemuEsDeSystem* self, char* sep);
static char* btrc_Vector_SemuEsDeSystem_joinToString(btrc_Vector_SemuEsDeSystem* self, char* sep);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_filter(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred);
static int btrc_Vector_SemuEsDeSystem_findIndex(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred);
static void btrc_Vector_SemuEsDeSystem_forEach(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_void_SemuEsDeSystem fn);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_map(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_SemuEsDeSystem_SemuEsDeSystem fn);
static bool btrc_Vector_SemuEsDeSystem_any(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred);
static bool btrc_Vector_SemuEsDeSystem_all(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_reduce(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* init, __btrc_fn_SemuEsDeSystem_SemuEsDeSystem_SemuEsDeSystem fn);
static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_copy(btrc_Vector_SemuEsDeSystem* self);
static void btrc_Vector_SemuEsDeSystem_removeAt(btrc_Vector_SemuEsDeSystem* self, int idx);
static int btrc_Vector_SemuEsDeSystem_iterLen(btrc_Vector_SemuEsDeSystem* self);
static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_iterGet(btrc_Vector_SemuEsDeSystem* self, int i);

static void btrc_Vector_SemuEsDeFindRule_init(btrc_Vector_SemuEsDeFindRule* self);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_new(void);
static void btrc_Vector_SemuEsDeFindRule_destroy(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_push(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_pop(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_get(btrc_Vector_SemuEsDeFindRule* self, int i);
static void btrc_Vector_SemuEsDeFindRule_set(btrc_Vector_SemuEsDeFindRule* self, int i, SemuEsDeFindRule* val);
static void btrc_Vector_SemuEsDeFindRule_free(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_remove(btrc_Vector_SemuEsDeFindRule* self, int idx);
static void btrc_Vector_SemuEsDeFindRule_reverse(btrc_Vector_SemuEsDeFindRule* self);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_reversed(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_swap(btrc_Vector_SemuEsDeFindRule* self, int i, int j);
static void btrc_Vector_SemuEsDeFindRule_clear(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_fill(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static int btrc_Vector_SemuEsDeFindRule_size(btrc_Vector_SemuEsDeFindRule* self);
static bool btrc_Vector_SemuEsDeFindRule_isEmpty(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_first(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_last(btrc_Vector_SemuEsDeFindRule* self);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_slice(btrc_Vector_SemuEsDeFindRule* self, int start, int end);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_take(btrc_Vector_SemuEsDeFindRule* self, int n);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_drop(btrc_Vector_SemuEsDeFindRule* self, int n);
static void btrc_Vector_SemuEsDeFindRule_extend(btrc_Vector_SemuEsDeFindRule* self, btrc_Vector_SemuEsDeFindRule* other);
static void btrc_Vector_SemuEsDeFindRule_insert(btrc_Vector_SemuEsDeFindRule* self, int idx, SemuEsDeFindRule* val);
static bool btrc_Vector_SemuEsDeFindRule_contains(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static int btrc_Vector_SemuEsDeFindRule_indexOf(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static int btrc_Vector_SemuEsDeFindRule_lastIndexOf(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static int btrc_Vector_SemuEsDeFindRule_count(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static void btrc_Vector_SemuEsDeFindRule_removeAll(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_distinct(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_sort(btrc_Vector_SemuEsDeFindRule* self);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_sorted(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_min(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_max(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_sum(btrc_Vector_SemuEsDeFindRule* self);
static char* btrc_Vector_SemuEsDeFindRule_join(btrc_Vector_SemuEsDeFindRule* self, char* sep);
static char* btrc_Vector_SemuEsDeFindRule_joinToString(btrc_Vector_SemuEsDeFindRule* self, char* sep);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_filter(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred);
static int btrc_Vector_SemuEsDeFindRule_findIndex(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred);
static void btrc_Vector_SemuEsDeFindRule_forEach(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_void_SemuEsDeFindRule fn);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_map(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule fn);
static bool btrc_Vector_SemuEsDeFindRule_any(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred);
static bool btrc_Vector_SemuEsDeFindRule_all(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_reduce(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* init, __btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule_SemuEsDeFindRule fn);
static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_copy(btrc_Vector_SemuEsDeFindRule* self);
static void btrc_Vector_SemuEsDeFindRule_removeAt(btrc_Vector_SemuEsDeFindRule* self, int idx);
static int btrc_Vector_SemuEsDeFindRule_iterLen(btrc_Vector_SemuEsDeFindRule* self);
static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_iterGet(btrc_Vector_SemuEsDeFindRule* self, int i);

static void btrc_Vector_SemuSteamInputTemplate_init(btrc_Vector_SemuSteamInputTemplate* self);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_new(void);
static void btrc_Vector_SemuSteamInputTemplate_destroy(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_push(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_pop(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_get(btrc_Vector_SemuSteamInputTemplate* self, int i);
static void btrc_Vector_SemuSteamInputTemplate_set(btrc_Vector_SemuSteamInputTemplate* self, int i, SemuSteamInputTemplate* val);
static void btrc_Vector_SemuSteamInputTemplate_free(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_remove(btrc_Vector_SemuSteamInputTemplate* self, int idx);
static void btrc_Vector_SemuSteamInputTemplate_reverse(btrc_Vector_SemuSteamInputTemplate* self);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_reversed(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_swap(btrc_Vector_SemuSteamInputTemplate* self, int i, int j);
static void btrc_Vector_SemuSteamInputTemplate_clear(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_fill(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static int btrc_Vector_SemuSteamInputTemplate_size(btrc_Vector_SemuSteamInputTemplate* self);
static bool btrc_Vector_SemuSteamInputTemplate_isEmpty(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_first(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_last(btrc_Vector_SemuSteamInputTemplate* self);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_slice(btrc_Vector_SemuSteamInputTemplate* self, int start, int end);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_take(btrc_Vector_SemuSteamInputTemplate* self, int n);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_drop(btrc_Vector_SemuSteamInputTemplate* self, int n);
static void btrc_Vector_SemuSteamInputTemplate_extend(btrc_Vector_SemuSteamInputTemplate* self, btrc_Vector_SemuSteamInputTemplate* other);
static void btrc_Vector_SemuSteamInputTemplate_insert(btrc_Vector_SemuSteamInputTemplate* self, int idx, SemuSteamInputTemplate* val);
static bool btrc_Vector_SemuSteamInputTemplate_contains(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static int btrc_Vector_SemuSteamInputTemplate_indexOf(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static int btrc_Vector_SemuSteamInputTemplate_lastIndexOf(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static int btrc_Vector_SemuSteamInputTemplate_count(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static void btrc_Vector_SemuSteamInputTemplate_removeAll(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_distinct(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_sort(btrc_Vector_SemuSteamInputTemplate* self);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_sorted(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_min(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_max(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_sum(btrc_Vector_SemuSteamInputTemplate* self);
static char* btrc_Vector_SemuSteamInputTemplate_join(btrc_Vector_SemuSteamInputTemplate* self, char* sep);
static char* btrc_Vector_SemuSteamInputTemplate_joinToString(btrc_Vector_SemuSteamInputTemplate* self, char* sep);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_filter(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred);
static int btrc_Vector_SemuSteamInputTemplate_findIndex(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred);
static void btrc_Vector_SemuSteamInputTemplate_forEach(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_void_SemuSteamInputTemplate fn);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_map(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate fn);
static bool btrc_Vector_SemuSteamInputTemplate_any(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred);
static bool btrc_Vector_SemuSteamInputTemplate_all(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_reduce(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* init, __btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate_SemuSteamInputTemplate fn);
static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_copy(btrc_Vector_SemuSteamInputTemplate* self);
static void btrc_Vector_SemuSteamInputTemplate_removeAt(btrc_Vector_SemuSteamInputTemplate* self, int idx);
static int btrc_Vector_SemuSteamInputTemplate_iterLen(btrc_Vector_SemuSteamInputTemplate* self);
static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_iterGet(btrc_Vector_SemuSteamInputTemplate* self, int i);

static void btrc_Vector_SemuAppImageContent_init(btrc_Vector_SemuAppImageContent* self);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_new(void);
static void btrc_Vector_SemuAppImageContent_destroy(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_push(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_pop(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_get(btrc_Vector_SemuAppImageContent* self, int i);
static void btrc_Vector_SemuAppImageContent_set(btrc_Vector_SemuAppImageContent* self, int i, SemuAppImageContent* val);
static void btrc_Vector_SemuAppImageContent_free(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_remove(btrc_Vector_SemuAppImageContent* self, int idx);
static void btrc_Vector_SemuAppImageContent_reverse(btrc_Vector_SemuAppImageContent* self);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_reversed(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_swap(btrc_Vector_SemuAppImageContent* self, int i, int j);
static void btrc_Vector_SemuAppImageContent_clear(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_fill(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static int btrc_Vector_SemuAppImageContent_size(btrc_Vector_SemuAppImageContent* self);
static bool btrc_Vector_SemuAppImageContent_isEmpty(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_first(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_last(btrc_Vector_SemuAppImageContent* self);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_slice(btrc_Vector_SemuAppImageContent* self, int start, int end);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_take(btrc_Vector_SemuAppImageContent* self, int n);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_drop(btrc_Vector_SemuAppImageContent* self, int n);
static void btrc_Vector_SemuAppImageContent_extend(btrc_Vector_SemuAppImageContent* self, btrc_Vector_SemuAppImageContent* other);
static void btrc_Vector_SemuAppImageContent_insert(btrc_Vector_SemuAppImageContent* self, int idx, SemuAppImageContent* val);
static bool btrc_Vector_SemuAppImageContent_contains(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static int btrc_Vector_SemuAppImageContent_indexOf(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static int btrc_Vector_SemuAppImageContent_lastIndexOf(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static int btrc_Vector_SemuAppImageContent_count(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static void btrc_Vector_SemuAppImageContent_removeAll(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_distinct(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_sort(btrc_Vector_SemuAppImageContent* self);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_sorted(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_min(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_max(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_sum(btrc_Vector_SemuAppImageContent* self);
static char* btrc_Vector_SemuAppImageContent_join(btrc_Vector_SemuAppImageContent* self, char* sep);
static char* btrc_Vector_SemuAppImageContent_joinToString(btrc_Vector_SemuAppImageContent* self, char* sep);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_filter(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred);
static int btrc_Vector_SemuAppImageContent_findIndex(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred);
static void btrc_Vector_SemuAppImageContent_forEach(btrc_Vector_SemuAppImageContent* self, __btrc_fn_void_SemuAppImageContent fn);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_map(btrc_Vector_SemuAppImageContent* self, __btrc_fn_SemuAppImageContent_SemuAppImageContent fn);
static bool btrc_Vector_SemuAppImageContent_any(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred);
static bool btrc_Vector_SemuAppImageContent_all(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_reduce(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* init, __btrc_fn_SemuAppImageContent_SemuAppImageContent_SemuAppImageContent fn);
static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_copy(btrc_Vector_SemuAppImageContent* self);
static void btrc_Vector_SemuAppImageContent_removeAt(btrc_Vector_SemuAppImageContent* self, int idx);
static int btrc_Vector_SemuAppImageContent_iterLen(btrc_Vector_SemuAppImageContent* self);
static SemuAppImageContent* btrc_Vector_SemuAppImageContent_iterGet(btrc_Vector_SemuAppImageContent* self, int i);

static void btrc_Map_string_string_init(btrc_Map_string_string* self);
static btrc_Map_string_string* btrc_Map_string_string_new(void);
static void btrc_Map_string_string_destroy(btrc_Map_string_string* self);
static void btrc_Map_string_string_resize(btrc_Map_string_string* self);
static void btrc_Map_string_string_put(btrc_Map_string_string* self, char* key, char* value);
static void btrc_Map_string_string_putMoved(btrc_Map_string_string* self, char* key, char* value);
static void btrc_Map_string_string_putRetained(btrc_Map_string_string* self, char* key, char* value);
static char* btrc_Map_string_string_get(btrc_Map_string_string* self, char* key);
static char* btrc_Map_string_string_getOrDefault(btrc_Map_string_string* self, char* key, char* fallback);
static bool btrc_Map_string_string_has(btrc_Map_string_string* self, char* key);
static bool btrc_Map_string_string_contains(btrc_Map_string_string* self, char* key);
static void btrc_Map_string_string_putIfAbsent(btrc_Map_string_string* self, char* key, char* value);
static void btrc_Map_string_string_free(btrc_Map_string_string* self);
static void btrc_Map_string_string_remove(btrc_Map_string_string* self, char* key);
static void btrc_Map_string_string_clear(btrc_Map_string_string* self);
static int btrc_Map_string_string_size(btrc_Map_string_string* self);
static bool btrc_Map_string_string_isEmpty(btrc_Map_string_string* self);
static btrc_Vector_string* btrc_Map_string_string_keys(btrc_Map_string_string* self);
static btrc_Vector_string* btrc_Map_string_string_values(btrc_Map_string_string* self);
static bool btrc_Map_string_string_containsValue(btrc_Map_string_string* self, char* value);
static void btrc_Map_string_string_set(btrc_Map_string_string* self, char* key, char* value);
static void btrc_Map_string_string_merge(btrc_Map_string_string* self, btrc_Map_string_string* other);
static int btrc_Map_string_string_iterLen(btrc_Map_string_string* self);
static char* btrc_Map_string_string_iterGet(btrc_Map_string_string* self, int n);
static char* btrc_Map_string_string_iterValueAt(btrc_Map_string_string* self, int n);

static void btrc_Map_string_bool_init(btrc_Map_string_bool* self);
static btrc_Map_string_bool* btrc_Map_string_bool_new(void);
static void btrc_Map_string_bool_destroy(btrc_Map_string_bool* self);
static void btrc_Map_string_bool_resize(btrc_Map_string_bool* self);
static void btrc_Map_string_bool_put(btrc_Map_string_bool* self, char* key, bool value);
static void btrc_Map_string_bool_putMoved(btrc_Map_string_bool* self, char* key, bool value);
static void btrc_Map_string_bool_putRetained(btrc_Map_string_bool* self, char* key, bool value);
static bool btrc_Map_string_bool_get(btrc_Map_string_bool* self, char* key);
static bool btrc_Map_string_bool_getOrDefault(btrc_Map_string_bool* self, char* key, bool fallback);
static bool btrc_Map_string_bool_has(btrc_Map_string_bool* self, char* key);
static bool btrc_Map_string_bool_contains(btrc_Map_string_bool* self, char* key);
static void btrc_Map_string_bool_putIfAbsent(btrc_Map_string_bool* self, char* key, bool value);
static void btrc_Map_string_bool_free(btrc_Map_string_bool* self);
static void btrc_Map_string_bool_remove(btrc_Map_string_bool* self, char* key);
static void btrc_Map_string_bool_clear(btrc_Map_string_bool* self);
static int btrc_Map_string_bool_size(btrc_Map_string_bool* self);
static bool btrc_Map_string_bool_isEmpty(btrc_Map_string_bool* self);
static btrc_Vector_string* btrc_Map_string_bool_keys(btrc_Map_string_bool* self);
static btrc_Vector_bool* btrc_Map_string_bool_values(btrc_Map_string_bool* self);
static bool btrc_Map_string_bool_containsValue(btrc_Map_string_bool* self, bool value);
static void btrc_Map_string_bool_set(btrc_Map_string_bool* self, char* key, bool value);
static void btrc_Map_string_bool_merge(btrc_Map_string_bool* self, btrc_Map_string_bool* other);
static int btrc_Map_string_bool_iterLen(btrc_Map_string_bool* self);
static char* btrc_Map_string_bool_iterGet(btrc_Map_string_bool* self, int n);
static bool btrc_Map_string_bool_iterValueAt(btrc_Map_string_bool* self, int n);

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

static char* btrc_Vector_string_pop(btrc_Vector_string* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
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

static void btrc_Vector_string_remove(btrc_Vector_string* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_string_reverse(btrc_Vector_string* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        char* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_string* btrc_Vector_string_reversed(btrc_Vector_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_string_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_string_swap(btrc_Vector_string* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    char* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_string_clear(btrc_Vector_string* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    (self->len = 0);
}

static void btrc_Vector_string_fill(btrc_Vector_string* self, char* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (self->data[i] = val);
    }
}

static int btrc_Vector_string_size(btrc_Vector_string* self) {
    return self->len;
}

static bool btrc_Vector_string_isEmpty(btrc_Vector_string* self) {
    return (self->len == 0);
}

static char* btrc_Vector_string_first(btrc_Vector_string* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static char* btrc_Vector_string_last(btrc_Vector_string* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_string* btrc_Vector_string_slice(btrc_Vector_string* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_string_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_string* btrc_Vector_string_take(btrc_Vector_string* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_string_slice(self, 0, n);
}

static btrc_Vector_string* btrc_Vector_string_drop(btrc_Vector_string* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_string_slice(self, n, self->len);
}

static void btrc_Vector_string_extend(btrc_Vector_string* self, btrc_Vector_string* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_string_push(self, other->data[i]);
    }
}

static void btrc_Vector_string_insert(btrc_Vector_string* self, int idx, char* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((char**)__btrc_safe_realloc(self->data, (sizeof(char*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_string_contains(btrc_Vector_string* self, char* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_string_indexOf(btrc_Vector_string* self, char* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_string_lastIndexOf(btrc_Vector_string* self, char* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_string_count(btrc_Vector_string* self, char* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_string_removeAll(btrc_Vector_string* self, char* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        }
    }
    (self->len = j);
}

static btrc_Vector_string* btrc_Vector_string_distinct(btrc_Vector_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_string_contains(result, self->data[i])) {
            btrc_Vector_string_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_string_sort(btrc_Vector_string* self) {
    for (int i = 1; (i < self->len); (i++)) {
        char* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_string* btrc_Vector_string_sorted(btrc_Vector_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_string_push(result, self->data[i]);
    }
    btrc_Vector_string_sort(result);
    return result;
}

static char* btrc_Vector_string_min(btrc_Vector_string* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    char* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static char* btrc_Vector_string_max(btrc_Vector_string* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    char* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
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

static char* btrc_Vector_string_joinToString(btrc_Vector_string* self, char* sep) {
    return btrc_Vector_string_join(self, sep);
}

static btrc_Vector_string* btrc_Vector_string_filter(btrc_Vector_string* self, __btrc_fn_bool_string pred) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_string_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_string_findIndex(btrc_Vector_string* self, __btrc_fn_bool_string pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_string_forEach(btrc_Vector_string* self, __btrc_fn_void_string fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_string* btrc_Vector_string_map(btrc_Vector_string* self, __btrc_fn_string_string fn) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_string_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_string_any(btrc_Vector_string* self, __btrc_fn_bool_string pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_string_all(btrc_Vector_string* self, __btrc_fn_bool_string pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static char* btrc_Vector_string_reduce(btrc_Vector_string* self, char* init, __btrc_fn_string_string_string fn) {
    char* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_string* btrc_Vector_string_copy(btrc_Vector_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_string_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_string_removeAt(btrc_Vector_string* self, int idx) {
    btrc_Vector_string_remove(self, idx);
}

static int btrc_Vector_string_iterLen(btrc_Vector_string* self) {
    return self->len;
}

static char* btrc_Vector_string_iterGet(btrc_Vector_string* self, int i) {
    return self->data[i];
}

static void btrc_Vector_bool_init(btrc_Vector_bool* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_bool* btrc_Vector_bool_new(void) {
    btrc_Vector_bool* self = ((btrc_Vector_bool*)malloc(sizeof(btrc_Vector_bool)));
    memset(self, 0, sizeof(btrc_Vector_bool));
    btrc_Vector_bool_init(self);
    return self;
}

static void btrc_Vector_bool_destroy(btrc_Vector_bool* self) {
    free(self);
}

static void btrc_Vector_bool_push(btrc_Vector_bool* self, bool val) {
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((bool*)__btrc_safe_realloc(self->data, (sizeof(bool) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static bool btrc_Vector_bool_pop(btrc_Vector_bool* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static bool btrc_Vector_bool_get(btrc_Vector_bool* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_bool_set(btrc_Vector_bool* self, int i, bool val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (self->data[i] = val);
}

static void btrc_Vector_bool_free(btrc_Vector_bool* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_bool_remove(btrc_Vector_bool* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_bool_reverse(btrc_Vector_bool* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        bool tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_bool* btrc_Vector_bool_reversed(btrc_Vector_bool* self) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_bool_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_bool_swap(btrc_Vector_bool* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    bool tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_bool_clear(btrc_Vector_bool* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    (self->len = 0);
}

static void btrc_Vector_bool_fill(btrc_Vector_bool* self, bool val) {
    for (int i = 0; (i < self->len); (i++)) {
        (self->data[i] = val);
    }
}

static int btrc_Vector_bool_size(btrc_Vector_bool* self) {
    return self->len;
}

static bool btrc_Vector_bool_isEmpty(btrc_Vector_bool* self) {
    return (self->len == 0);
}

static bool btrc_Vector_bool_first(btrc_Vector_bool* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static bool btrc_Vector_bool_last(btrc_Vector_bool* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_bool* btrc_Vector_bool_slice(btrc_Vector_bool* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_bool_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_bool* btrc_Vector_bool_take(btrc_Vector_bool* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_bool_slice(self, 0, n);
}

static btrc_Vector_bool* btrc_Vector_bool_drop(btrc_Vector_bool* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_bool_slice(self, n, self->len);
}

static void btrc_Vector_bool_extend(btrc_Vector_bool* self, btrc_Vector_bool* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_bool_push(self, other->data[i]);
    }
}

static void btrc_Vector_bool_insert(btrc_Vector_bool* self, int idx, bool val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((bool*)__btrc_safe_realloc(self->data, (sizeof(bool) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_bool_contains(btrc_Vector_bool* self, bool val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_bool_indexOf(btrc_Vector_bool* self, bool val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_bool_lastIndexOf(btrc_Vector_bool* self, bool val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_bool_count(btrc_Vector_bool* self, bool val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_bool_removeAll(btrc_Vector_bool* self, bool val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        }
    }
    (self->len = j);
}

static btrc_Vector_bool* btrc_Vector_bool_distinct(btrc_Vector_bool* self) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_bool_contains(result, self->data[i])) {
            btrc_Vector_bool_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_bool_sort(btrc_Vector_bool* self) {
    for (int i = 1; (i < self->len); (i++)) {
        bool key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_bool* btrc_Vector_bool_sorted(btrc_Vector_bool* self) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_bool_push(result, self->data[i]);
    }
    btrc_Vector_bool_sort(result);
    return result;
}

static bool btrc_Vector_bool_min(btrc_Vector_bool* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    bool m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static bool btrc_Vector_bool_max(btrc_Vector_bool* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    bool m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static bool btrc_Vector_bool_sum(btrc_Vector_bool* self) {
    bool s = ((bool)0);
    for (int i = 0; (i < self->len); (i++)) {
        (s = (s + self->data[i]));
    }
    return s;
}

static btrc_Vector_bool* btrc_Vector_bool_filter(btrc_Vector_bool* self, __btrc_fn_bool_bool pred) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_bool_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_bool_findIndex(btrc_Vector_bool* self, __btrc_fn_bool_bool pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_bool_forEach(btrc_Vector_bool* self, __btrc_fn_void_bool fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_bool* btrc_Vector_bool_map(btrc_Vector_bool* self, __btrc_fn_bool_bool fn) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_bool_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_bool_any(btrc_Vector_bool* self, __btrc_fn_bool_bool pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_bool_all(btrc_Vector_bool* self, __btrc_fn_bool_bool pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static bool btrc_Vector_bool_reduce(btrc_Vector_bool* self, bool init, __btrc_fn_bool_bool_bool fn) {
    bool acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_bool* btrc_Vector_bool_copy(btrc_Vector_bool* self) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_bool_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_bool_removeAt(btrc_Vector_bool* self, int idx) {
    btrc_Vector_bool_remove(self, idx);
}

static int btrc_Vector_bool_iterLen(btrc_Vector_bool* self) {
    return self->len;
}

static bool btrc_Vector_bool_iterGet(btrc_Vector_bool* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuEsDeCommand_init(btrc_Vector_SemuEsDeCommand* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_new(void) {
    btrc_Vector_SemuEsDeCommand* self = ((btrc_Vector_SemuEsDeCommand*)malloc(sizeof(btrc_Vector_SemuEsDeCommand)));
    memset(self, 0, sizeof(btrc_Vector_SemuEsDeCommand));
    btrc_Vector_SemuEsDeCommand_init(self);
    return self;
}

static void btrc_Vector_SemuEsDeCommand_destroy(btrc_Vector_SemuEsDeCommand* self) {
    free(self);
}

static void btrc_Vector_SemuEsDeCommand_push(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeCommand**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeCommand*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_pop(btrc_Vector_SemuEsDeCommand* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_get(btrc_Vector_SemuEsDeCommand* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuEsDeCommand_set(btrc_Vector_SemuEsDeCommand* self, int i, SemuEsDeCommand* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuEsDeCommand_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_SemuEsDeCommand_free(btrc_Vector_SemuEsDeCommand* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeCommand_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuEsDeCommand_remove(btrc_Vector_SemuEsDeCommand* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuEsDeCommand_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuEsDeCommand_reverse(btrc_Vector_SemuEsDeCommand* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuEsDeCommand* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_reversed(btrc_Vector_SemuEsDeCommand* self) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeCommand_swap(btrc_Vector_SemuEsDeCommand* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuEsDeCommand* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuEsDeCommand_clear(btrc_Vector_SemuEsDeCommand* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeCommand_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuEsDeCommand_fill(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (val->__rc++);
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeCommand_destroy(self->data[i]);
            }
        }
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuEsDeCommand_size(btrc_Vector_SemuEsDeCommand* self) {
    return self->len;
}

static bool btrc_Vector_SemuEsDeCommand_isEmpty(btrc_Vector_SemuEsDeCommand* self) {
    return (self->len == 0);
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_first(btrc_Vector_SemuEsDeCommand* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_last(btrc_Vector_SemuEsDeCommand* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_slice(btrc_Vector_SemuEsDeCommand* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_take(btrc_Vector_SemuEsDeCommand* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeCommand_slice(self, 0, n);
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_drop(btrc_Vector_SemuEsDeCommand* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeCommand_slice(self, n, self->len);
}

static void btrc_Vector_SemuEsDeCommand_extend(btrc_Vector_SemuEsDeCommand* self, btrc_Vector_SemuEsDeCommand* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuEsDeCommand_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuEsDeCommand_insert(btrc_Vector_SemuEsDeCommand* self, int idx, SemuEsDeCommand* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeCommand**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeCommand*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuEsDeCommand_contains(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuEsDeCommand_indexOf(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeCommand_lastIndexOf(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeCommand_count(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuEsDeCommand_removeAll(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeCommand_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_distinct(btrc_Vector_SemuEsDeCommand* self) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuEsDeCommand_contains(result, self->data[i])) {
            btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuEsDeCommand_sort(btrc_Vector_SemuEsDeCommand* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuEsDeCommand* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_sorted(btrc_Vector_SemuEsDeCommand* self) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
    }
    btrc_Vector_SemuEsDeCommand_sort(result);
    return result;
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_min(btrc_Vector_SemuEsDeCommand* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuEsDeCommand* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_max(btrc_Vector_SemuEsDeCommand* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuEsDeCommand* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_filter(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuEsDeCommand_findIndex(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuEsDeCommand_forEach(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_void_SemuEsDeCommand fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_map(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_SemuEsDeCommand_SemuEsDeCommand fn) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeCommand_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuEsDeCommand_any(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuEsDeCommand_all(btrc_Vector_SemuEsDeCommand* self, __btrc_fn_bool_SemuEsDeCommand pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_reduce(btrc_Vector_SemuEsDeCommand* self, SemuEsDeCommand* init, __btrc_fn_SemuEsDeCommand_SemuEsDeCommand_SemuEsDeCommand fn) {
    SemuEsDeCommand* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_copy(btrc_Vector_SemuEsDeCommand* self) {
    btrc_Vector_SemuEsDeCommand* result = btrc_Vector_SemuEsDeCommand_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeCommand_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeCommand_removeAt(btrc_Vector_SemuEsDeCommand* self, int idx) {
    btrc_Vector_SemuEsDeCommand_remove(self, idx);
}

static int btrc_Vector_SemuEsDeCommand_iterLen(btrc_Vector_SemuEsDeCommand* self) {
    return self->len;
}

static SemuEsDeCommand* btrc_Vector_SemuEsDeCommand_iterGet(btrc_Vector_SemuEsDeCommand* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuEsDeSystem_init(btrc_Vector_SemuEsDeSystem* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_new(void) {
    btrc_Vector_SemuEsDeSystem* self = ((btrc_Vector_SemuEsDeSystem*)malloc(sizeof(btrc_Vector_SemuEsDeSystem)));
    memset(self, 0, sizeof(btrc_Vector_SemuEsDeSystem));
    btrc_Vector_SemuEsDeSystem_init(self);
    return self;
}

static void btrc_Vector_SemuEsDeSystem_destroy(btrc_Vector_SemuEsDeSystem* self) {
    free(self);
}

static void btrc_Vector_SemuEsDeSystem_push(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeSystem**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeSystem*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_pop(btrc_Vector_SemuEsDeSystem* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_get(btrc_Vector_SemuEsDeSystem* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuEsDeSystem_set(btrc_Vector_SemuEsDeSystem* self, int i, SemuEsDeSystem* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuEsDeSystem_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_SemuEsDeSystem_free(btrc_Vector_SemuEsDeSystem* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeSystem_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuEsDeSystem_remove(btrc_Vector_SemuEsDeSystem* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuEsDeSystem_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuEsDeSystem_reverse(btrc_Vector_SemuEsDeSystem* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuEsDeSystem* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_reversed(btrc_Vector_SemuEsDeSystem* self) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeSystem_swap(btrc_Vector_SemuEsDeSystem* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuEsDeSystem* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuEsDeSystem_clear(btrc_Vector_SemuEsDeSystem* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeSystem_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuEsDeSystem_fill(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (val->__rc++);
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeSystem_destroy(self->data[i]);
            }
        }
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuEsDeSystem_size(btrc_Vector_SemuEsDeSystem* self) {
    return self->len;
}

static bool btrc_Vector_SemuEsDeSystem_isEmpty(btrc_Vector_SemuEsDeSystem* self) {
    return (self->len == 0);
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_first(btrc_Vector_SemuEsDeSystem* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_last(btrc_Vector_SemuEsDeSystem* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_slice(btrc_Vector_SemuEsDeSystem* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_take(btrc_Vector_SemuEsDeSystem* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeSystem_slice(self, 0, n);
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_drop(btrc_Vector_SemuEsDeSystem* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeSystem_slice(self, n, self->len);
}

static void btrc_Vector_SemuEsDeSystem_extend(btrc_Vector_SemuEsDeSystem* self, btrc_Vector_SemuEsDeSystem* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuEsDeSystem_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuEsDeSystem_insert(btrc_Vector_SemuEsDeSystem* self, int idx, SemuEsDeSystem* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeSystem**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeSystem*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuEsDeSystem_contains(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuEsDeSystem_indexOf(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeSystem_lastIndexOf(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeSystem_count(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuEsDeSystem_removeAll(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeSystem_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_distinct(btrc_Vector_SemuEsDeSystem* self) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuEsDeSystem_contains(result, self->data[i])) {
            btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuEsDeSystem_sort(btrc_Vector_SemuEsDeSystem* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuEsDeSystem* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_sorted(btrc_Vector_SemuEsDeSystem* self) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
    }
    btrc_Vector_SemuEsDeSystem_sort(result);
    return result;
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_min(btrc_Vector_SemuEsDeSystem* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuEsDeSystem* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_max(btrc_Vector_SemuEsDeSystem* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuEsDeSystem* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_filter(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuEsDeSystem_findIndex(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuEsDeSystem_forEach(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_void_SemuEsDeSystem fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_map(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_SemuEsDeSystem_SemuEsDeSystem fn) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeSystem_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuEsDeSystem_any(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuEsDeSystem_all(btrc_Vector_SemuEsDeSystem* self, __btrc_fn_bool_SemuEsDeSystem pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_reduce(btrc_Vector_SemuEsDeSystem* self, SemuEsDeSystem* init, __btrc_fn_SemuEsDeSystem_SemuEsDeSystem_SemuEsDeSystem fn) {
    SemuEsDeSystem* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_copy(btrc_Vector_SemuEsDeSystem* self) {
    btrc_Vector_SemuEsDeSystem* result = btrc_Vector_SemuEsDeSystem_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeSystem_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeSystem_removeAt(btrc_Vector_SemuEsDeSystem* self, int idx) {
    btrc_Vector_SemuEsDeSystem_remove(self, idx);
}

static int btrc_Vector_SemuEsDeSystem_iterLen(btrc_Vector_SemuEsDeSystem* self) {
    return self->len;
}

static SemuEsDeSystem* btrc_Vector_SemuEsDeSystem_iterGet(btrc_Vector_SemuEsDeSystem* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuEsDeFindRule_init(btrc_Vector_SemuEsDeFindRule* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_new(void) {
    btrc_Vector_SemuEsDeFindRule* self = ((btrc_Vector_SemuEsDeFindRule*)malloc(sizeof(btrc_Vector_SemuEsDeFindRule)));
    memset(self, 0, sizeof(btrc_Vector_SemuEsDeFindRule));
    btrc_Vector_SemuEsDeFindRule_init(self);
    return self;
}

static void btrc_Vector_SemuEsDeFindRule_destroy(btrc_Vector_SemuEsDeFindRule* self) {
    free(self);
}

static void btrc_Vector_SemuEsDeFindRule_push(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeFindRule**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeFindRule*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_pop(btrc_Vector_SemuEsDeFindRule* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_get(btrc_Vector_SemuEsDeFindRule* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuEsDeFindRule_set(btrc_Vector_SemuEsDeFindRule* self, int i, SemuEsDeFindRule* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuEsDeFindRule_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_SemuEsDeFindRule_free(btrc_Vector_SemuEsDeFindRule* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeFindRule_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuEsDeFindRule_remove(btrc_Vector_SemuEsDeFindRule* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuEsDeFindRule_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuEsDeFindRule_reverse(btrc_Vector_SemuEsDeFindRule* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuEsDeFindRule* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_reversed(btrc_Vector_SemuEsDeFindRule* self) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeFindRule_swap(btrc_Vector_SemuEsDeFindRule* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuEsDeFindRule* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuEsDeFindRule_clear(btrc_Vector_SemuEsDeFindRule* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeFindRule_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuEsDeFindRule_fill(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (val->__rc++);
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeFindRule_destroy(self->data[i]);
            }
        }
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuEsDeFindRule_size(btrc_Vector_SemuEsDeFindRule* self) {
    return self->len;
}

static bool btrc_Vector_SemuEsDeFindRule_isEmpty(btrc_Vector_SemuEsDeFindRule* self) {
    return (self->len == 0);
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_first(btrc_Vector_SemuEsDeFindRule* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_last(btrc_Vector_SemuEsDeFindRule* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_slice(btrc_Vector_SemuEsDeFindRule* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_take(btrc_Vector_SemuEsDeFindRule* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeFindRule_slice(self, 0, n);
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_drop(btrc_Vector_SemuEsDeFindRule* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuEsDeFindRule_slice(self, n, self->len);
}

static void btrc_Vector_SemuEsDeFindRule_extend(btrc_Vector_SemuEsDeFindRule* self, btrc_Vector_SemuEsDeFindRule* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuEsDeFindRule_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuEsDeFindRule_insert(btrc_Vector_SemuEsDeFindRule* self, int idx, SemuEsDeFindRule* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuEsDeFindRule**)__btrc_safe_realloc(self->data, (sizeof(SemuEsDeFindRule*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuEsDeFindRule_contains(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuEsDeFindRule_indexOf(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeFindRule_lastIndexOf(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuEsDeFindRule_count(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuEsDeFindRule_removeAll(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuEsDeFindRule_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_distinct(btrc_Vector_SemuEsDeFindRule* self) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuEsDeFindRule_contains(result, self->data[i])) {
            btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuEsDeFindRule_sort(btrc_Vector_SemuEsDeFindRule* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuEsDeFindRule* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_sorted(btrc_Vector_SemuEsDeFindRule* self) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
    }
    btrc_Vector_SemuEsDeFindRule_sort(result);
    return result;
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_min(btrc_Vector_SemuEsDeFindRule* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuEsDeFindRule* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_max(btrc_Vector_SemuEsDeFindRule* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuEsDeFindRule* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_filter(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuEsDeFindRule_findIndex(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuEsDeFindRule_forEach(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_void_SemuEsDeFindRule fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_map(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule fn) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeFindRule_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuEsDeFindRule_any(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuEsDeFindRule_all(btrc_Vector_SemuEsDeFindRule* self, __btrc_fn_bool_SemuEsDeFindRule pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_reduce(btrc_Vector_SemuEsDeFindRule* self, SemuEsDeFindRule* init, __btrc_fn_SemuEsDeFindRule_SemuEsDeFindRule_SemuEsDeFindRule fn) {
    SemuEsDeFindRule* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_copy(btrc_Vector_SemuEsDeFindRule* self) {
    btrc_Vector_SemuEsDeFindRule* result = btrc_Vector_SemuEsDeFindRule_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuEsDeFindRule_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuEsDeFindRule_removeAt(btrc_Vector_SemuEsDeFindRule* self, int idx) {
    btrc_Vector_SemuEsDeFindRule_remove(self, idx);
}

static int btrc_Vector_SemuEsDeFindRule_iterLen(btrc_Vector_SemuEsDeFindRule* self) {
    return self->len;
}

static SemuEsDeFindRule* btrc_Vector_SemuEsDeFindRule_iterGet(btrc_Vector_SemuEsDeFindRule* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuSteamInputTemplate_init(btrc_Vector_SemuSteamInputTemplate* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_new(void) {
    btrc_Vector_SemuSteamInputTemplate* self = ((btrc_Vector_SemuSteamInputTemplate*)malloc(sizeof(btrc_Vector_SemuSteamInputTemplate)));
    memset(self, 0, sizeof(btrc_Vector_SemuSteamInputTemplate));
    btrc_Vector_SemuSteamInputTemplate_init(self);
    return self;
}

static void btrc_Vector_SemuSteamInputTemplate_destroy(btrc_Vector_SemuSteamInputTemplate* self) {
    free(self);
}

static void btrc_Vector_SemuSteamInputTemplate_push(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuSteamInputTemplate**)__btrc_safe_realloc(self->data, (sizeof(SemuSteamInputTemplate*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_pop(btrc_Vector_SemuSteamInputTemplate* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_get(btrc_Vector_SemuSteamInputTemplate* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuSteamInputTemplate_set(btrc_Vector_SemuSteamInputTemplate* self, int i, SemuSteamInputTemplate* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuSteamInputTemplate_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_SemuSteamInputTemplate_free(btrc_Vector_SemuSteamInputTemplate* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuSteamInputTemplate_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuSteamInputTemplate_remove(btrc_Vector_SemuSteamInputTemplate* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuSteamInputTemplate_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuSteamInputTemplate_reverse(btrc_Vector_SemuSteamInputTemplate* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuSteamInputTemplate* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_reversed(btrc_Vector_SemuSteamInputTemplate* self) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuSteamInputTemplate_swap(btrc_Vector_SemuSteamInputTemplate* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuSteamInputTemplate* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuSteamInputTemplate_clear(btrc_Vector_SemuSteamInputTemplate* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuSteamInputTemplate_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuSteamInputTemplate_fill(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (val->__rc++);
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuSteamInputTemplate_destroy(self->data[i]);
            }
        }
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuSteamInputTemplate_size(btrc_Vector_SemuSteamInputTemplate* self) {
    return self->len;
}

static bool btrc_Vector_SemuSteamInputTemplate_isEmpty(btrc_Vector_SemuSteamInputTemplate* self) {
    return (self->len == 0);
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_first(btrc_Vector_SemuSteamInputTemplate* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_last(btrc_Vector_SemuSteamInputTemplate* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_slice(btrc_Vector_SemuSteamInputTemplate* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_take(btrc_Vector_SemuSteamInputTemplate* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuSteamInputTemplate_slice(self, 0, n);
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_drop(btrc_Vector_SemuSteamInputTemplate* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuSteamInputTemplate_slice(self, n, self->len);
}

static void btrc_Vector_SemuSteamInputTemplate_extend(btrc_Vector_SemuSteamInputTemplate* self, btrc_Vector_SemuSteamInputTemplate* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuSteamInputTemplate_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuSteamInputTemplate_insert(btrc_Vector_SemuSteamInputTemplate* self, int idx, SemuSteamInputTemplate* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuSteamInputTemplate**)__btrc_safe_realloc(self->data, (sizeof(SemuSteamInputTemplate*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuSteamInputTemplate_contains(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuSteamInputTemplate_indexOf(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuSteamInputTemplate_lastIndexOf(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuSteamInputTemplate_count(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuSteamInputTemplate_removeAll(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuSteamInputTemplate_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_distinct(btrc_Vector_SemuSteamInputTemplate* self) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuSteamInputTemplate_contains(result, self->data[i])) {
            btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuSteamInputTemplate_sort(btrc_Vector_SemuSteamInputTemplate* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuSteamInputTemplate* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_sorted(btrc_Vector_SemuSteamInputTemplate* self) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
    }
    btrc_Vector_SemuSteamInputTemplate_sort(result);
    return result;
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_min(btrc_Vector_SemuSteamInputTemplate* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuSteamInputTemplate* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_max(btrc_Vector_SemuSteamInputTemplate* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuSteamInputTemplate* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_filter(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuSteamInputTemplate_findIndex(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuSteamInputTemplate_forEach(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_void_SemuSteamInputTemplate fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_map(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate fn) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuSteamInputTemplate_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuSteamInputTemplate_any(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuSteamInputTemplate_all(btrc_Vector_SemuSteamInputTemplate* self, __btrc_fn_bool_SemuSteamInputTemplate pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_reduce(btrc_Vector_SemuSteamInputTemplate* self, SemuSteamInputTemplate* init, __btrc_fn_SemuSteamInputTemplate_SemuSteamInputTemplate_SemuSteamInputTemplate fn) {
    SemuSteamInputTemplate* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_copy(btrc_Vector_SemuSteamInputTemplate* self) {
    btrc_Vector_SemuSteamInputTemplate* result = btrc_Vector_SemuSteamInputTemplate_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuSteamInputTemplate_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuSteamInputTemplate_removeAt(btrc_Vector_SemuSteamInputTemplate* self, int idx) {
    btrc_Vector_SemuSteamInputTemplate_remove(self, idx);
}

static int btrc_Vector_SemuSteamInputTemplate_iterLen(btrc_Vector_SemuSteamInputTemplate* self) {
    return self->len;
}

static SemuSteamInputTemplate* btrc_Vector_SemuSteamInputTemplate_iterGet(btrc_Vector_SemuSteamInputTemplate* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuAppImageContent_init(btrc_Vector_SemuAppImageContent* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_new(void) {
    btrc_Vector_SemuAppImageContent* self = ((btrc_Vector_SemuAppImageContent*)malloc(sizeof(btrc_Vector_SemuAppImageContent)));
    memset(self, 0, sizeof(btrc_Vector_SemuAppImageContent));
    btrc_Vector_SemuAppImageContent_init(self);
    return self;
}

static void btrc_Vector_SemuAppImageContent_destroy(btrc_Vector_SemuAppImageContent* self) {
    free(self);
}

static void btrc_Vector_SemuAppImageContent_push(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuAppImageContent**)__btrc_safe_realloc(self->data, (sizeof(SemuAppImageContent*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_pop(btrc_Vector_SemuAppImageContent* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_get(btrc_Vector_SemuAppImageContent* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuAppImageContent_set(btrc_Vector_SemuAppImageContent* self, int i, SemuAppImageContent* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuAppImageContent_destroy(self->data[i]);
        }
    }
    (self->data[i] = val);
}

static void btrc_Vector_SemuAppImageContent_free(btrc_Vector_SemuAppImageContent* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuAppImageContent_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuAppImageContent_remove(btrc_Vector_SemuAppImageContent* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuAppImageContent_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuAppImageContent_reverse(btrc_Vector_SemuAppImageContent* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuAppImageContent* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_reversed(btrc_Vector_SemuAppImageContent* self) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuAppImageContent_swap(btrc_Vector_SemuAppImageContent* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuAppImageContent* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuAppImageContent_clear(btrc_Vector_SemuAppImageContent* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuAppImageContent_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuAppImageContent_fill(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    for (int i = 0; (i < self->len); (i++)) {
        (val->__rc++);
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuAppImageContent_destroy(self->data[i]);
            }
        }
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuAppImageContent_size(btrc_Vector_SemuAppImageContent* self) {
    return self->len;
}

static bool btrc_Vector_SemuAppImageContent_isEmpty(btrc_Vector_SemuAppImageContent* self) {
    return (self->len == 0);
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_first(btrc_Vector_SemuAppImageContent* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_last(btrc_Vector_SemuAppImageContent* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_slice(btrc_Vector_SemuAppImageContent* self, int start, int end) {
    if (start < 0) {
        (start = (self->len + start));
    }
    if (end < 0) {
        (end = (self->len + end));
    }
    if (start < 0) {
        (start = 0);
    }
    if (end > self->len) {
        (end = self->len);
    }
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_take(btrc_Vector_SemuAppImageContent* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuAppImageContent_slice(self, 0, n);
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_drop(btrc_Vector_SemuAppImageContent* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuAppImageContent_slice(self, n, self->len);
}

static void btrc_Vector_SemuAppImageContent_extend(btrc_Vector_SemuAppImageContent* self, btrc_Vector_SemuAppImageContent* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuAppImageContent_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuAppImageContent_insert(btrc_Vector_SemuAppImageContent* self, int idx, SemuAppImageContent* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuAppImageContent**)__btrc_safe_realloc(self->data, (sizeof(SemuAppImageContent*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuAppImageContent_contains(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuAppImageContent_indexOf(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuAppImageContent_lastIndexOf(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuAppImageContent_count(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuAppImageContent_removeAll(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuAppImageContent_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_distinct(btrc_Vector_SemuAppImageContent* self) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuAppImageContent_contains(result, self->data[i])) {
            btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuAppImageContent_sort(btrc_Vector_SemuAppImageContent* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuAppImageContent* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_sorted(btrc_Vector_SemuAppImageContent* self) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
    }
    btrc_Vector_SemuAppImageContent_sort(result);
    return result;
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_min(btrc_Vector_SemuAppImageContent* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuAppImageContent* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_max(btrc_Vector_SemuAppImageContent* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuAppImageContent* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_filter(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuAppImageContent_findIndex(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuAppImageContent_forEach(btrc_Vector_SemuAppImageContent* self, __btrc_fn_void_SemuAppImageContent fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_map(btrc_Vector_SemuAppImageContent* self, __btrc_fn_SemuAppImageContent_SemuAppImageContent fn) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuAppImageContent_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuAppImageContent_any(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuAppImageContent_all(btrc_Vector_SemuAppImageContent* self, __btrc_fn_bool_SemuAppImageContent pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_reduce(btrc_Vector_SemuAppImageContent* self, SemuAppImageContent* init, __btrc_fn_SemuAppImageContent_SemuAppImageContent_SemuAppImageContent fn) {
    SemuAppImageContent* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuAppImageContent* btrc_Vector_SemuAppImageContent_copy(btrc_Vector_SemuAppImageContent* self) {
    btrc_Vector_SemuAppImageContent* result = btrc_Vector_SemuAppImageContent_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuAppImageContent_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuAppImageContent_removeAt(btrc_Vector_SemuAppImageContent* self, int idx) {
    btrc_Vector_SemuAppImageContent_remove(self, idx);
}

static int btrc_Vector_SemuAppImageContent_iterLen(btrc_Vector_SemuAppImageContent* self) {
    return self->len;
}

static SemuAppImageContent* btrc_Vector_SemuAppImageContent_iterGet(btrc_Vector_SemuAppImageContent* self, int i) {
    return self->data[i];
}

static void btrc_Map_string_string_init(btrc_Map_string_string* self) {
    self->__rc = 1;
    (self->cap = 16);
    (self->len = 0);
    (self->keys = ((char**)__btrc_safe_calloc(16, sizeof(char*))));
    (self->values = ((char**)__btrc_safe_calloc(16, sizeof(char*))));
    (self->occupied = ((bool*)__btrc_safe_calloc(16, sizeof(bool))));
}

static btrc_Map_string_string* btrc_Map_string_string_new(void) {
    btrc_Map_string_string* self = ((btrc_Map_string_string*)malloc(sizeof(btrc_Map_string_string)));
    memset(self, 0, sizeof(btrc_Map_string_string));
    btrc_Map_string_string_init(self);
    return self;
}

static void btrc_Map_string_string_destroy(btrc_Map_string_string* self) {
    free(self);
}

static void btrc_Map_string_string_resize(btrc_Map_string_string* self) {
    int old_cap = self->cap;
    char** old_keys = self->keys;
    char** old_values = self->values;
    bool* old_occupied = self->occupied;
    (self->cap = (self->cap * 2));
    (self->len = 0);
    (self->keys = ((char**)__btrc_safe_calloc(self->cap, sizeof(char*))));
    (self->values = ((char**)__btrc_safe_calloc(self->cap, sizeof(char*))));
    (self->occupied = ((bool*)__btrc_safe_calloc(self->cap, sizeof(bool))));
    for (int i = 0; (i < old_cap); (i++)) {
        if (old_occupied[i]) {
            btrc_Map_string_string_putMoved(self, old_keys[i], old_values[i]);
        }
    }
    free(old_keys);
    free(old_values);
    free(old_occupied);
}

static void btrc_Map_string_string_put(btrc_Map_string_string* self, char* key, char* value) {
    if ((self->len * 4) >= (self->cap * 3)) {
        btrc_Map_string_string_resize(self);
    }
    btrc_Map_string_string_putRetained(self, key, value);
}

static void btrc_Map_string_string_putMoved(btrc_Map_string_string* self, char* key, char* value) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->values[idx] = value);
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
    (self->keys[idx] = key);
    (self->values[idx] = value);
    (self->occupied[idx] = true);
    (self->len++);
}

static void btrc_Map_string_string_putRetained(btrc_Map_string_string* self, char* key, char* value) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->values[idx] = value);
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
    (self->keys[idx] = key);
    (self->values[idx] = value);
    (self->occupied[idx] = true);
    (self->len++);
}

static char* btrc_Map_string_string_get(btrc_Map_string_string* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return self->values[idx];
        }
        (idx = ((idx + 1) % self->cap));
    }
    fprintf(stderr, "Map key not found\n");
    exit(1);
    return self->values[0];
}

static char* btrc_Map_string_string_getOrDefault(btrc_Map_string_string* self, char* key, char* fallback) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return self->values[idx];
        }
        (idx = ((idx + 1) % self->cap));
    }
    return fallback;
}

static bool btrc_Map_string_string_has(btrc_Map_string_string* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return true;
        }
        (idx = ((idx + 1) % self->cap));
    }
    return false;
}

static bool btrc_Map_string_string_contains(btrc_Map_string_string* self, char* key) {
    return btrc_Map_string_string_has(self, key);
}

static void btrc_Map_string_string_putIfAbsent(btrc_Map_string_string* self, char* key, char* value) {
    if (!btrc_Map_string_string_has(self, key)) {
        btrc_Map_string_string_put(self, key, value);
    }
}

static void btrc_Map_string_string_free(btrc_Map_string_string* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
        }
    }
    free(self->keys);
    free(self->values);
    free(self->occupied);
    (self->keys = NULL);
    (self->values = NULL);
    (self->occupied = NULL);
    (self->cap = 0);
    (self->len = 0);
}

static void btrc_Map_string_string_remove(btrc_Map_string_string* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->occupied[idx] = false);
            (self->len--);
            unsigned int j = ((idx + 1) % self->cap);
            while (self->occupied[j]) {
                char* rk = self->keys[j];
                char* rv = self->values[j];
                (self->occupied[j] = false);
                (self->len--);
                btrc_Map_string_string_putMoved(self, rk, rv);
                (j = ((j + 1) % self->cap));
            }
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
}

static void btrc_Map_string_string_clear(btrc_Map_string_string* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            (self->occupied[i] = false);
        }
    }
    (self->len = 0);
}

static int btrc_Map_string_string_size(btrc_Map_string_string* self) {
    return self->len;
}

static bool btrc_Map_string_string_isEmpty(btrc_Map_string_string* self) {
    return (self->len == 0);
}

static btrc_Vector_string* btrc_Map_string_string_keys(btrc_Map_string_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            btrc_Vector_string_push(result, self->keys[i]);
        }
    }
    return result;
}

static btrc_Vector_string* btrc_Map_string_string_values(btrc_Map_string_string* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            btrc_Vector_string_push(result, self->values[i]);
        }
    }
    return result;
}

static bool btrc_Map_string_string_containsValue(btrc_Map_string_string* self, char* value) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i] && __btrc_eq(self->values[i], value)) {
            return true;
        }
    }
    return false;
}

static void btrc_Map_string_string_set(btrc_Map_string_string* self, char* key, char* value) {
    btrc_Map_string_string_put(self, key, value);
}

static void btrc_Map_string_string_merge(btrc_Map_string_string* self, btrc_Map_string_string* other) {
    for (int i = 0; (i < other->cap); (i++)) {
        if (other->occupied[i]) {
            btrc_Map_string_string_put(self, other->keys[i], other->values[i]);
        }
    }
}

static int btrc_Map_string_string_iterLen(btrc_Map_string_string* self) {
    return self->len;
}

static char* btrc_Map_string_string_iterGet(btrc_Map_string_string* self, int n) {
    int count = 0;
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            if (count == n) {
                return self->keys[i];
            }
            (count++);
        }
    }
    fprintf(stderr, "Map iterGet: index out of bounds\n");
    exit(1);
    return self->keys[0];
}

static char* btrc_Map_string_string_iterValueAt(btrc_Map_string_string* self, int n) {
    int count = 0;
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            if (count == n) {
                return self->values[i];
            }
            (count++);
        }
    }
    fprintf(stderr, "Map iterValueAt: index out of bounds\n");
    exit(1);
    return self->values[0];
}

static void btrc_Map_string_bool_init(btrc_Map_string_bool* self) {
    self->__rc = 1;
    (self->cap = 16);
    (self->len = 0);
    (self->keys = ((char**)__btrc_safe_calloc(16, sizeof(char*))));
    (self->values = ((bool*)__btrc_safe_calloc(16, sizeof(bool))));
    (self->occupied = ((bool*)__btrc_safe_calloc(16, sizeof(bool))));
}

static btrc_Map_string_bool* btrc_Map_string_bool_new(void) {
    btrc_Map_string_bool* self = ((btrc_Map_string_bool*)malloc(sizeof(btrc_Map_string_bool)));
    memset(self, 0, sizeof(btrc_Map_string_bool));
    btrc_Map_string_bool_init(self);
    return self;
}

static void btrc_Map_string_bool_destroy(btrc_Map_string_bool* self) {
    free(self);
}

static void btrc_Map_string_bool_resize(btrc_Map_string_bool* self) {
    int old_cap = self->cap;
    char** old_keys = self->keys;
    bool* old_values = self->values;
    bool* old_occupied = self->occupied;
    (self->cap = (self->cap * 2));
    (self->len = 0);
    (self->keys = ((char**)__btrc_safe_calloc(self->cap, sizeof(char*))));
    (self->values = ((bool*)__btrc_safe_calloc(self->cap, sizeof(bool))));
    (self->occupied = ((bool*)__btrc_safe_calloc(self->cap, sizeof(bool))));
    for (int i = 0; (i < old_cap); (i++)) {
        if (old_occupied[i]) {
            btrc_Map_string_bool_putMoved(self, old_keys[i], old_values[i]);
        }
    }
    free(old_keys);
    free(old_values);
    free(old_occupied);
}

static void btrc_Map_string_bool_put(btrc_Map_string_bool* self, char* key, bool value) {
    if ((self->len * 4) >= (self->cap * 3)) {
        btrc_Map_string_bool_resize(self);
    }
    btrc_Map_string_bool_putRetained(self, key, value);
}

static void btrc_Map_string_bool_putMoved(btrc_Map_string_bool* self, char* key, bool value) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->values[idx] = value);
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
    (self->keys[idx] = key);
    (self->values[idx] = value);
    (self->occupied[idx] = true);
    (self->len++);
}

static void btrc_Map_string_bool_putRetained(btrc_Map_string_bool* self, char* key, bool value) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->values[idx] = value);
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
    (self->keys[idx] = key);
    (self->values[idx] = value);
    (self->occupied[idx] = true);
    (self->len++);
}

static bool btrc_Map_string_bool_get(btrc_Map_string_bool* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return self->values[idx];
        }
        (idx = ((idx + 1) % self->cap));
    }
    fprintf(stderr, "Map key not found\n");
    exit(1);
    return self->values[0];
}

static bool btrc_Map_string_bool_getOrDefault(btrc_Map_string_bool* self, char* key, bool fallback) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return self->values[idx];
        }
        (idx = ((idx + 1) % self->cap));
    }
    return fallback;
}

static bool btrc_Map_string_bool_has(btrc_Map_string_bool* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            return true;
        }
        (idx = ((idx + 1) % self->cap));
    }
    return false;
}

static bool btrc_Map_string_bool_contains(btrc_Map_string_bool* self, char* key) {
    return btrc_Map_string_bool_has(self, key);
}

static void btrc_Map_string_bool_putIfAbsent(btrc_Map_string_bool* self, char* key, bool value) {
    if (!btrc_Map_string_bool_has(self, key)) {
        btrc_Map_string_bool_put(self, key, value);
    }
}

static void btrc_Map_string_bool_free(btrc_Map_string_bool* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
        }
    }
    free(self->keys);
    free(self->values);
    free(self->occupied);
    (self->keys = NULL);
    (self->values = NULL);
    (self->occupied = NULL);
    (self->cap = 0);
    (self->len = 0);
}

static void btrc_Map_string_bool_remove(btrc_Map_string_bool* self, char* key) {
    unsigned int idx = (__btrc_hash(key) % self->cap);
    while (self->occupied[idx]) {
        if (__btrc_eq(self->keys[idx], key)) {
            (self->occupied[idx] = false);
            (self->len--);
            unsigned int j = ((idx + 1) % self->cap);
            while (self->occupied[j]) {
                char* rk = self->keys[j];
                bool rv = self->values[j];
                (self->occupied[j] = false);
                (self->len--);
                btrc_Map_string_bool_putMoved(self, rk, rv);
                (j = ((j + 1) % self->cap));
            }
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
}

static void btrc_Map_string_bool_clear(btrc_Map_string_bool* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            (self->occupied[i] = false);
        }
    }
    (self->len = 0);
}

static int btrc_Map_string_bool_size(btrc_Map_string_bool* self) {
    return self->len;
}

static bool btrc_Map_string_bool_isEmpty(btrc_Map_string_bool* self) {
    return (self->len == 0);
}

static btrc_Vector_string* btrc_Map_string_bool_keys(btrc_Map_string_bool* self) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            btrc_Vector_string_push(result, self->keys[i]);
        }
    }
    return result;
}

static btrc_Vector_bool* btrc_Map_string_bool_values(btrc_Map_string_bool* self) {
    btrc_Vector_bool* result = btrc_Vector_bool_new();
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            btrc_Vector_bool_push(result, self->values[i]);
        }
    }
    return result;
}

static bool btrc_Map_string_bool_containsValue(btrc_Map_string_bool* self, bool value) {
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i] && __btrc_eq(self->values[i], value)) {
            return true;
        }
    }
    return false;
}

static void btrc_Map_string_bool_set(btrc_Map_string_bool* self, char* key, bool value) {
    btrc_Map_string_bool_put(self, key, value);
}

static void btrc_Map_string_bool_merge(btrc_Map_string_bool* self, btrc_Map_string_bool* other) {
    for (int i = 0; (i < other->cap); (i++)) {
        if (other->occupied[i]) {
            btrc_Map_string_bool_put(self, other->keys[i], other->values[i]);
        }
    }
}

static int btrc_Map_string_bool_iterLen(btrc_Map_string_bool* self) {
    return self->len;
}

static char* btrc_Map_string_bool_iterGet(btrc_Map_string_bool* self, int n) {
    int count = 0;
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            if (count == n) {
                return self->keys[i];
            }
            (count++);
        }
    }
    fprintf(stderr, "Map iterGet: index out of bounds\n");
    exit(1);
    return self->keys[0];
}

static bool btrc_Map_string_bool_iterValueAt(btrc_Map_string_bool* self, int n) {
    int count = 0;
    for (int i = 0; (i < self->cap); (i++)) {
        if (self->occupied[i]) {
            if (count == n) {
                return self->values[i];
            }
            (count++);
        }
    }
    fprintf(stderr, "Map iterValueAt: index out of bounds\n");
    exit(1);
    return self->values[0];
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

char* Strings_fromInt(int n) {
    char* buf = ((char*)malloc(32));
    snprintf(buf, 32, "%d", n);
    return buf;
}

void CliArgs_init(CliArgs* self, int argc, char** argv) {
    self->__rc = 1;
    (self->program = ((argc > 0) ? Strings_copy(argv[0]) : ""));
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    btrc_Vector_string* __list_3 = btrc_Vector_string_new();
    (self->values = __list_3);
    btrc_Vector_string* __list_2 = btrc_Vector_string_new();
    (__list_2->__rc++);
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

bool CliArgs_has(CliArgs* self, char* flag) {
    int __n_5 = btrc_Vector_string_iterLen(self->values);
    for (int __i_4 = 0; (__i_4 < __n_5); (__i_4++)) {
        char* value = btrc_Vector_string_iterGet(self->values, __i_4);
        if (strcmp(value, flag) == 0) {
            return true;
        }
    }
    return false;
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

void File_write(File* self, char* text) {
    if (!self->is_open) {
        return;
    }
    fputs(text, self->handle);
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
        char* __btrc_ret_32 = "";
        if (f != NULL) {
            if ((--f->__rc) <= 0) {
                File_destroy(f);
            }
        }
        return __btrc_ret_32;
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

void Path_writeAll(char* path, char* content) {
    File* f = File_new(path, "w");
    if (!File_ok(f)) {
        if (f != NULL) {
            if ((--f->__rc) <= 0) {
                File_destroy(f);
            }
        }
        return;
    }
    File_write(f, content);
    File_close(f);
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
    for (int __i_33 = 0; (name[__i_33] != '\0'); (__i_33++)) {
        char ch = name[__i_33];
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
    for (int __i_34 = 0; (raw[__i_34] != '\0'); (__i_34++)) {
        char ch = raw[__i_34];
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
        char* __fstr_35_arg0 = item;
        int __fstr_35_len = snprintf(NULL, 0, "invalid environment assignment: %s", __fstr_35_arg0);
        char* __fstr_35_buf = __btrc_str_track(((char*)malloc((__fstr_35_len + 1))));
        snprintf(__fstr_35_buf, (__fstr_35_len + 1), "invalid environment assignment: %s", __fstr_35_arg0);
        __btrc_throw(__fstr_35_buf);
    }
    char* name = __btrc_str_track(__btrc_substring(item, 0, split));
    if (!ShellWords_isEnvName(name)) {
        char* __fstr_36_arg0 = name;
        int __fstr_36_len = snprintf(NULL, 0, "invalid environment variable name: %s", __fstr_36_arg0);
        char* __fstr_36_buf = __btrc_str_track(((char*)malloc((__fstr_36_len + 1))));
        snprintf(__fstr_36_buf, (__fstr_36_len + 1), "invalid environment variable name: %s", __fstr_36_arg0);
        __btrc_throw(__fstr_36_buf);
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

void Command_init(Command* self, char* command) {
    self->__rc = 1;
    (self->command = command);
    if (self->arguments != NULL) {
        if ((--self->arguments->__rc) <= 0) {
            btrc_Vector_string_free(self->arguments);
        }
    }
    btrc_Vector_string* __list_38 = btrc_Vector_string_new();
    (self->arguments = __list_38);
    btrc_Vector_string* __list_37 = btrc_Vector_string_new();
    (__list_37->__rc++);
}

Command* Command_new(char* command) {
    Command* self = ((Command*)malloc(sizeof(Command)));
    memset(self, 0, sizeof(Command));
    Command_init(self, command);
    return self;
}

void Command_destroy(Command* self) {
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
    char* __fstr_43_arg0 = base;
    char* __fstr_43_arg1 = separator;
    int __fstr_43_arg2 = Platform_pid();
    int __fstr_43_arg3 = self->tempId;
    char* __fstr_43_arg4 = name;
    int __fstr_43_len = snprintf(NULL, 0, "%s%sbtrc-process-%d-%d.%s.XXXXXX", __fstr_43_arg0, __fstr_43_arg1, __fstr_43_arg2, __fstr_43_arg3, __fstr_43_arg4);
    char* __fstr_43_buf = __btrc_str_track(((char*)malloc((__fstr_43_len + 1))));
    snprintf(__fstr_43_buf, (__fstr_43_len + 1), "%s%sbtrc-process-%d-%d.%s.XXXXXX", __fstr_43_arg0, __fstr_43_arg1, __fstr_43_arg2, __fstr_43_arg3, __fstr_43_arg4);
    char* templatePath = __fstr_43_buf;
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
    int __n_45 = btrc_Vector_string_iterLen(env);
    for (int __i_44 = 0; (__i_44 < __n_45); (__i_44++)) {
        char* item = btrc_Vector_string_iterGet(env, __i_44);
        btrc_Vector_string_push(parts, ShellWords_envAssignment(item));
    }
    return btrc_Vector_string_join(parts, " ");
}

char* UnixShell_withContext(UnixShell* self, char* command, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot) {
    char* rendered = command;
    char* envPrefix = UnixShell_renderEnv(self, env);
    if (!__btrc_isEmpty(envPrefix)) {
        char* __fstr_46_arg0 = envPrefix;
        char* __fstr_46_arg1 = rendered;
        int __fstr_46_len = snprintf(NULL, 0, "%s %s", __fstr_46_arg0, __fstr_46_arg1);
        char* __fstr_46_buf = __btrc_str_track(((char*)malloc((__fstr_46_len + 1))));
        snprintf(__fstr_46_buf, (__fstr_46_len + 1), "%s %s", __fstr_46_arg0, __fstr_46_arg1);
        (rendered = __fstr_46_buf);
    }
    if (sudo) {
        char* __fstr_47_arg0 = rendered;
        int __fstr_47_len = snprintf(NULL, 0, "sudo %s", __fstr_47_arg0);
        char* __fstr_47_buf = __btrc_str_track(((char*)malloc((__fstr_47_len + 1))));
        snprintf(__fstr_47_buf, (__fstr_47_len + 1), "sudo %s", __fstr_47_arg0);
        (rendered = __fstr_47_buf);
    }
    if (!__btrc_isEmpty(cwd)) {
        char* __fstr_48_arg0 = ShellWords_quote(cwd);
        char* __fstr_48_arg1 = rendered;
        int __fstr_48_len = snprintf(NULL, 0, "cd %s && %s", __fstr_48_arg0, __fstr_48_arg1);
        char* __fstr_48_buf = __btrc_str_track(((char*)malloc((__fstr_48_len + 1))));
        snprintf(__fstr_48_buf, (__fstr_48_len + 1), "cd %s && %s", __fstr_48_arg0, __fstr_48_arg1);
        (rendered = __fstr_48_buf);
    }
    char* root = (__btrc_isEmpty(chroot) ? self->chrootPath : chroot);
    if (!__btrc_isEmpty(root)) {
        char* __fstr_49_arg0 = ShellWords_quote(root);
        char* __fstr_49_arg1 = ShellWords_quote(rendered);
        int __fstr_49_len = snprintf(NULL, 0, "nixos-enter --root %s --command %s", __fstr_49_arg0, __fstr_49_arg1);
        char* __fstr_49_buf = __btrc_str_track(((char*)malloc((__fstr_49_len + 1))));
        snprintf(__fstr_49_buf, (__fstr_49_len + 1), "nixos-enter --root %s --command %s", __fstr_49_arg0, __fstr_49_arg1);
        (rendered = __fstr_49_buf);
    }
    return rendered;
}

char* UnixShell_withRedirections(UnixShell* self, char* rendered, char* stdout, char* stderr, char* outFile, char* errFile, char* stdinFile) {
    char* __fstr_50_arg0 = rendered;
    int __fstr_50_len = snprintf(NULL, 0, "( %s )", __fstr_50_arg0);
    char* __fstr_50_buf = __btrc_str_track(((char*)malloc((__fstr_50_len + 1))));
    snprintf(__fstr_50_buf, (__fstr_50_len + 1), "( %s )", __fstr_50_arg0);
    char* command = __fstr_50_buf;
    if (!__btrc_isEmpty(stdinFile)) {
        char* __fstr_51_arg0 = command;
        char* __fstr_51_arg1 = ShellWords_quote(stdinFile);
        int __fstr_51_len = snprintf(NULL, 0, "%s < %s", __fstr_51_arg0, __fstr_51_arg1);
        char* __fstr_51_buf = __btrc_str_track(((char*)malloc((__fstr_51_len + 1))));
        snprintf(__fstr_51_buf, (__fstr_51_len + 1), "%s < %s", __fstr_51_arg0, __fstr_51_arg1);
        (command = __fstr_51_buf);
    }
    if (strcmp(stdout, CommandOutput_collect()) == 0) {
        char* __fstr_52_arg0 = command;
        char* __fstr_52_arg1 = ShellWords_quote(outFile);
        int __fstr_52_len = snprintf(NULL, 0, "%s > %s", __fstr_52_arg0, __fstr_52_arg1);
        char* __fstr_52_buf = __btrc_str_track(((char*)malloc((__fstr_52_len + 1))));
        snprintf(__fstr_52_buf, (__fstr_52_len + 1), "%s > %s", __fstr_52_arg0, __fstr_52_arg1);
        (command = __fstr_52_buf);
    } else if (strcmp(stdout, CommandOutput_suppress()) == 0) {
        char* __fstr_53_arg0 = command;
        int __fstr_53_len = snprintf(NULL, 0, "%s > /dev/null", __fstr_53_arg0);
        char* __fstr_53_buf = __btrc_str_track(((char*)malloc((__fstr_53_len + 1))));
        snprintf(__fstr_53_buf, (__fstr_53_len + 1), "%s > /dev/null", __fstr_53_arg0);
        (command = __fstr_53_buf);
    }
    if (strcmp(stderr, CommandOutput_combine()) == 0) {
        char* __fstr_54_arg0 = command;
        int __fstr_54_len = snprintf(NULL, 0, "%s 2>&1", __fstr_54_arg0);
        char* __fstr_54_buf = __btrc_str_track(((char*)malloc((__fstr_54_len + 1))));
        snprintf(__fstr_54_buf, (__fstr_54_len + 1), "%s 2>&1", __fstr_54_arg0);
        (command = __fstr_54_buf);
    } else if (strcmp(stderr, CommandOutput_collect()) == 0) {
        char* __fstr_55_arg0 = command;
        char* __fstr_55_arg1 = ShellWords_quote(errFile);
        int __fstr_55_len = snprintf(NULL, 0, "%s 2> %s", __fstr_55_arg0, __fstr_55_arg1);
        char* __fstr_55_buf = __btrc_str_track(((char*)malloc((__fstr_55_len + 1))));
        snprintf(__fstr_55_buf, (__fstr_55_len + 1), "%s 2> %s", __fstr_55_arg0, __fstr_55_arg1);
        (command = __fstr_55_buf);
    } else if (strcmp(stderr, CommandOutput_suppress()) == 0) {
        char* __fstr_56_arg0 = command;
        int __fstr_56_len = snprintf(NULL, 0, "%s 2>/dev/null", __fstr_56_arg0);
        char* __fstr_56_buf = __btrc_str_track(((char*)malloc((__fstr_56_len + 1))));
        snprintf(__fstr_56_buf, (__fstr_56_len + 1), "%s 2>/dev/null", __fstr_56_arg0);
        (command = __fstr_56_buf);
    }
    return command;
}

ExecResult* UnixShell_run(UnixShell* self, char* command, char* stdout, char* stderr, bool logCommand, bool logFailure, bool throwOnFailure, char* redactSubstring, char* stdin, char* cwd, btrc_Vector_string* env, bool sudo, char* chroot) {
    if (!CommandOutput_valid(stdout)) {
        char* __fstr_57_arg0 = stdout;
        int __fstr_57_len = snprintf(NULL, 0, "invalid stdout mode: %s", __fstr_57_arg0);
        char* __fstr_57_buf = __btrc_str_track(((char*)malloc((__fstr_57_len + 1))));
        snprintf(__fstr_57_buf, (__fstr_57_len + 1), "invalid stdout mode: %s", __fstr_57_arg0);
        __btrc_throw(__fstr_57_buf);
    }
    if (!CommandOutput_valid(stderr)) {
        char* __fstr_58_arg0 = stderr;
        int __fstr_58_len = snprintf(NULL, 0, "invalid stderr mode: %s", __fstr_58_arg0);
        char* __fstr_58_buf = __btrc_str_track(((char*)malloc((__fstr_58_len + 1))));
        snprintf(__fstr_58_buf, (__fstr_58_len + 1), "invalid stderr mode: %s", __fstr_58_arg0);
        __btrc_throw(__fstr_58_buf);
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
        char* __fstr_60_arg0 = visible;
        int __fstr_60_len = snprintf(NULL, 0, "LOG: %s", __fstr_60_arg0);
        char* __fstr_60_buf = __btrc_str_track(((char*)malloc((__fstr_60_len + 1))));
        snprintf(__fstr_60_buf, (__fstr_60_len + 1), "LOG: %s", __fstr_60_arg0);
        UnixShell_logError(__fstr_60_buf);
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
        int __fstr_62_arg0 = code;
        char* __fstr_62_arg1 = UnixShell_redactText(rendered, redactSubstring);
        int __fstr_62_len = snprintf(NULL, 0, "Command failed (%d): %s", __fstr_62_arg0, __fstr_62_arg1);
        char* __fstr_62_buf = __btrc_str_track(((char*)malloc((__fstr_62_len + 1))));
        snprintf(__fstr_62_buf, (__fstr_62_len + 1), "Command failed (%d): %s", __fstr_62_arg0, __fstr_62_arg1);
        UnixShell_logError(__fstr_62_buf);
    }
    if (throwOnFailure && (code != 0)) {
        int __fstr_63_arg0 = code;
        char* __fstr_63_arg1 = UnixShell_redactText(rendered, redactSubstring);
        int __fstr_63_len = snprintf(NULL, 0, "Command failed (%d): %s", __fstr_63_arg0, __fstr_63_arg1);
        char* __fstr_63_buf = __btrc_str_track(((char*)malloc((__fstr_63_len + 1))));
        snprintf(__fstr_63_buf, (__fstr_63_len + 1), "Command failed (%d): %s", __fstr_63_arg0, __fstr_63_arg1);
        __btrc_throw(__fstr_63_buf);
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

void Directory_destroy(Directory* self) {
    free(self);
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
        char* __fstr_70_arg0 = left;
        char* __fstr_70_arg1 = right;
        int __fstr_70_len = snprintf(NULL, 0, "%s%s", __fstr_70_arg0, __fstr_70_arg1);
        char* __fstr_70_buf = __btrc_str_track(((char*)malloc((__fstr_70_len + 1))));
        snprintf(__fstr_70_buf, (__fstr_70_len + 1), "%s%s", __fstr_70_arg0, __fstr_70_arg1);
        return __fstr_70_buf;
    }
    char* __fstr_71_arg0 = left;
    char* __fstr_71_arg1 = right;
    int __fstr_71_len = snprintf(NULL, 0, "%s/%s", __fstr_71_arg0, __fstr_71_arg1);
    char* __fstr_71_buf = __btrc_str_track(((char*)malloc((__fstr_71_len + 1))));
    snprintf(__fstr_71_buf, (__fstr_71_len + 1), "%s/%s", __fstr_71_arg0, __fstr_71_arg1);
    return __fstr_71_buf;
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

int FileSystem_mkdir(char* path, int mode) {
    return UnixFileSystem_mkdirPath(path, mode);
}

int FileSystem_mkdirp(char* path) {
    return UnixFileSystem_mkdirp(path);
}

char* FileSystem_currentDirectory(void) {
    return UnixFileSystem_currentDirectory();
}

btrc_Vector_string* FileSystem_listDir(char* path) {
    Directory* dir = Directory_new(path);
    btrc_Vector_string* result = Directory_entries(dir);
    if (dir != NULL) {
        if ((--dir->__rc) <= 0) {
            Directory_destroy(dir);
        }
    }
    return result;
    if (dir != NULL) {
        if ((--dir->__rc) <= 0) {
            Directory_destroy(dir);
        }
    }
}

char* FileSystem_readText(char* path) {
    return Path_readAll(path);
}

void FileSystem_writeText(char* path, char* content) {
    Path_writeAll(path, content);
}

void JsonObject_init(JsonObject* self) {
    self->__rc = 1;
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Map_string_string_free(self->values);
        }
    }
    (self->values = btrc_Map_string_string_new());
    (btrc_Map_string_string_new()->__rc++);
    if (self->quoted != NULL) {
        if ((--self->quoted->__rc) <= 0) {
            btrc_Map_string_bool_free(self->quoted);
        }
    }
    (self->quoted = btrc_Map_string_bool_new());
    (btrc_Map_string_bool_new()->__rc++);
}

JsonObject* JsonObject_new(void) {
    JsonObject* self = ((JsonObject*)malloc(sizeof(JsonObject)));
    memset(self, 0, sizeof(JsonObject));
    JsonObject_init(self);
    return self;
}

void JsonObject_destroy(JsonObject* self) {
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Map_string_string_free(self->values);
        }
    }
    if (self->quoted != NULL) {
        if ((--self->quoted->__rc) <= 0) {
            btrc_Map_string_bool_free(self->quoted);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* JsonObject_escape(char* text) {
    if (text == NULL) {
        return "";
    }
    char* escaped = Strings_replace(text, "\\", "\\\\");
    (escaped = Strings_replace(escaped, "\"", "\\\""));
    (escaped = Strings_replace(escaped, "\n", "\\n"));
    (escaped = Strings_replace(escaped, "\r", "\\r"));
    (escaped = Strings_replace(escaped, "\t", "\\t"));
    return escaped;
}

void JsonObject_setString(JsonObject* self, char* key, char* value) {
    btrc_Map_string_string_put(self->values, key, value);
    btrc_Map_string_bool_put(self->quoted, key, true);
}

void JsonObject_setRaw(JsonObject* self, char* key, char* value) {
    btrc_Map_string_string_put(self->values, key, value);
    btrc_Map_string_bool_put(self->quoted, key, false);
}

void JsonObject_setBool(JsonObject* self, char* key, bool value) {
    btrc_Map_string_string_put(self->values, key, (value ? "true" : "false"));
    btrc_Map_string_bool_put(self->quoted, key, false);
}

char* JsonObject_getString(JsonObject* self, char* key, char* fallback) {
    if (!btrc_Map_string_string_has(self->values, key)) {
        return fallback;
    }
    return btrc_Map_string_string_get(self->values, key);
}

bool JsonObject_getBool(JsonObject* self, char* key, bool fallback) {
    if (!btrc_Map_string_string_has(self->values, key)) {
        return fallback;
    }
    char* value = btrc_Map_string_string_get(self->values, key);
    if (strcmp(value, "true") == 0) {
        return true;
    }
    if (strcmp(value, "false") == 0) {
        return false;
    }
    return fallback;
}

char* JsonObject_stringify(JsonObject* self) {
    btrc_Vector_string* fields = btrc_Vector_string_new();
    int __n_73 = btrc_Map_string_string_iterLen(self->values);
    for (int __i_72 = 0; (__i_72 < __n_73); (__i_72++)) {
        char* key = btrc_Map_string_string_iterGet(self->values, __i_72);
        char* value = btrc_Map_string_string_iterValueAt(self->values, __i_72);
        char* escapedKey = JsonObject_escape(key);
        char* field = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", escapedKey)), "\":"));
        if (btrc_Map_string_bool_getOrDefault(self->quoted, key, true)) {
            char* escapedValue = JsonObject_escape(value);
            (field = __btrc_str_track(__btrc_strcat(field, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", escapedValue)), "\"")))));
        } else {
            (field = __btrc_str_track(__btrc_strcat(field, value)));
        }
        btrc_Vector_string_push(fields, field);
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", btrc_Vector_string_join(fields, ","))), "}"));
}

JsonObject* JsonObject_parse(char* text) {
    JsonObject* json = JsonObject_new();
    int len = ((int)strlen(text));
    int depth = 0;
    int i = 0;
    while (i < len) {
        (i = JsonText_skipSpaces(text, i));
        if (i >= len) {
            break;
        }
        char c = text[i];
        if ((c == '{') || (c == '[')) {
            (depth++);
            (i++);
            continue;
        }
        if ((c == '}') || (c == ']')) {
            (depth--);
            (i++);
            continue;
        }
        if ((c != ((char)34)) || (depth != 1)) {
            (i++);
            continue;
        }
        int keyStart = (i + 1);
        int keyEnd = JsonText_stringEnd(text, keyStart);
        int afterKey = JsonText_skipSpaces(text, (keyEnd + 1));
        if ((afterKey >= len) || (text[afterKey] != ':')) {
            (i = (keyEnd + 1));
            continue;
        }
        char* key = JsonText_unescape(JsonText_slice(text, keyStart, keyEnd));
        (i = JsonText_skipSpaces(text, (afterKey + 1)));
        if (i >= len) {
            break;
        }
        if (text[i] == ((char)34)) {
            int valueStart = (i + 1);
            int valueEnd = JsonText_stringEnd(text, valueStart);
            JsonObject_setString(json, key, JsonText_unescape(JsonText_slice(text, valueStart, valueEnd)));
            (i = (valueEnd + 1));
        } else if ((text[i] == '{') || (text[i] == '[')) {
            int valueEnd = JsonText_balancedEnd(text, i);
            JsonObject_setRaw(json, key, JsonText_slice(text, i, valueEnd));
            (i = valueEnd);
        } else {
            int valueStart = i;
            while (((i < len) && (text[i] != ',')) && (text[i] != '}')) {
                (i++);
            }
            int valueEnd = i;
            while ((valueEnd > valueStart) && ((((text[(valueEnd - 1)] == ' ') || (text[(valueEnd - 1)] == '\n')) || (text[(valueEnd - 1)] == '\t')) || (text[(valueEnd - 1)] == '\r'))) {
                (valueEnd--);
            }
            JsonObject_setRaw(json, key, JsonText_slice(text, valueStart, valueEnd));
        }
    }
    return json;
    if (json != NULL) {
        if ((--json->__rc) <= 0) {
            JsonObject_destroy(json);
        }
    }
}

JsonObject* JsonObject_readFile(char* path) {
    return JsonObject_parse(Path_readAll(path));
}

char* JsonText_slice(char* text, int start, int end) {
    return __btrc_str_track(__btrc_substring(text, start, (end - start)));
}

char* JsonText_unescape(char* text) {
    char* result = "";
    bool escaped = false;
    for (int i = 0; (i < ((int)strlen(text))); (i++)) {
        char* current = __btrc_str_track(__btrc_substring(text, i, 1));
        if (escaped) {
            if (strcmp(current, "n") == 0) {
                (result = __btrc_str_track(__btrc_strcat(result, "\n")));
            } else if (strcmp(current, "r") == 0) {
                (result = __btrc_str_track(__btrc_strcat(result, "\r")));
            } else if (strcmp(current, "t") == 0) {
                (result = __btrc_str_track(__btrc_strcat(result, "\t")));
            } else {
                (result = __btrc_str_track(__btrc_strcat(result, current)));
            }
            (escaped = false);
            continue;
        }
        if (strcmp(current, "\\") == 0) {
            (escaped = true);
            continue;
        }
        (result = __btrc_str_track(__btrc_strcat(result, current)));
    }
    if (escaped) {
        (result = __btrc_str_track(__btrc_strcat(result, "\\")));
    }
    return result;
}

int JsonText_stringEnd(char* text, int start) {
    int len = ((int)strlen(text));
    bool escaped = false;
    int i = start;
    while (i < len) {
        if ((!escaped) && (text[i] == ((char)34))) {
            return i;
        }
        (escaped = ((!escaped) && (text[i] == '\\')));
        if (text[i] != '\\') {
            (escaped = false);
        }
        (i++);
    }
    return len;
}

int JsonText_balancedEnd(char* text, int start) {
    int len = ((int)strlen(text));
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int j = start; (j < len); (j++)) {
        char c = text[j];
        if (inString) {
            if ((!escaped) && (c == ((char)34))) {
                (inString = false);
            }
            (escaped = ((!escaped) && (c == '\\')));
            if (c != '\\') {
                (escaped = false);
            }
            continue;
        }
        if (c == ((char)34)) {
            (inString = true);
            (escaped = false);
            continue;
        }
        if ((c == '{') || (c == '[')) {
            (depth++);
            continue;
        }
        if ((c == '}') || (c == ']')) {
            (depth--);
            if (depth == 0) {
                return (j + 1);
            }
        }
    }
    return len;
}

int JsonText_skipSpaces(char* text, int i) {
    int len = ((int)strlen(text));
    while ((i < len) && ((((text[i] == ' ') || (text[i] == '\n')) || (text[i] == '\t')) || (text[i] == '\r'))) {
        (i++);
    }
    return i;
}

int JsonText_keyPosition(char* text, char* key) {
    int len = ((int)strlen(text));
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int i = 0; (i < len); (i++)) {
        char c = text[i];
        if (inString) {
            if ((!escaped) && (c == ((char)34))) {
                (inString = false);
            }
            (escaped = ((!escaped) && (c == '\\')));
            if (c != '\\') {
                (escaped = false);
            }
            continue;
        }
        if (c == ((char)34)) {
            if (depth == 1) {
                int keyStart = (i + 1);
                int keyEnd = JsonText_stringEnd(text, keyStart);
                char* candidate = JsonText_unescape(JsonText_slice(text, keyStart, keyEnd));
                int after = JsonText_skipSpaces(text, (keyEnd + 1));
                if (((after < len) && (text[after] == ':')) && (strcmp(candidate, key) == 0)) {
                    return i;
                }
                (i = keyEnd);
                continue;
            }
            (inString = true);
            (escaped = false);
            continue;
        }
        if ((c == '{') || (c == '[')) {
            (depth++);
            continue;
        }
        if ((c == '}') || (c == ']')) {
            (depth--);
        }
    }
    return (-1);
}

int JsonText_valueStart(char* text, char* key) {
    int keyPos = JsonText_keyPosition(text, key);
    if (keyPos < 0) {
        return (-1);
    }
    int keyEnd = JsonText_stringEnd(text, (keyPos + 1));
    int colon = JsonText_skipSpaces(text, (keyEnd + 1));
    if ((colon >= ((int)strlen(text))) || (text[colon] != ':')) {
        return (-1);
    }
    return JsonText_skipSpaces(text, (colon + 1));
}

char* JsonText_parseStringValue(char* text, int i, char* fallback) {
    int len = ((int)strlen(text));
    (i = JsonText_skipSpaces(text, i));
    if ((i >= len) || (text[i] != ((char)34))) {
        return fallback;
    }
    (i++);
    int start = i;
    int end = JsonText_stringEnd(text, start);
    return JsonText_unescape(JsonText_slice(text, start, end));
}

char* JsonText_field(char* text, char* key, char* fallback) {
    int i = JsonText_valueStart(text, key);
    if (i < 0) {
        return fallback;
    }
    int len = ((int)strlen(text));
    if ((i < len) && (text[i] == ((char)34))) {
        return JsonText_parseStringValue(text, i, fallback);
    }
    int start = i;
    while ((((i < len) && (text[i] != ',')) && (text[i] != '}')) && (text[i] != ']')) {
        (i++);
    }
    char* raw = __btrc_str_track(__btrc_trim(JsonText_slice(text, start, i)));
    if (__btrc_isEmpty(raw)) {
        return fallback;
    }
    return raw;
}

char* JsonText_objectField(char* text, char* key) {
    int i = JsonText_valueStart(text, key);
    int len = ((int)strlen(text));
    if (((i < 0) || (i >= len)) || (text[i] != '{')) {
        return "";
    }
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int j = i; (j < len); (j++)) {
        char c = text[j];
        if (inString) {
            if ((!escaped) && (c == ((char)34))) {
                (inString = false);
            }
            (escaped = ((!escaped) && (c == '\\')));
            if (c != '\\') {
                (escaped = false);
            }
            continue;
        }
        if (c == ((char)34)) {
            (inString = true);
            (escaped = false);
            continue;
        }
        if (c == '{') {
            (depth++);
            continue;
        }
        if (c == '}') {
            (depth--);
            if (depth == 0) {
                return JsonText_slice(text, i, (j + 1));
            }
        }
    }
    return "";
}

btrc_Vector_string* JsonText_stringArray(char* text, char* key) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    int i = JsonText_valueStart(text, key);
    int len = ((int)strlen(text));
    if (((i < 0) || (i >= len)) || (text[i] != '[')) {
        return result;
    }
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    int start = (-1);
    for (int j = (i + 1); (j < len); (j++)) {
        char c = text[j];
        if (inString) {
            if ((!escaped) && (c == ((char)34))) {
                if ((depth == 0) && (start >= 0)) {
                    btrc_Vector_string_push(result, JsonText_unescape(JsonText_slice(text, start, j)));
                }
                (inString = false);
                (start = (-1));
            }
            (escaped = ((!escaped) && (c == '\\')));
            if (c != '\\') {
                (escaped = false);
            }
            continue;
        }
        if (c == ((char)34)) {
            (inString = true);
            (escaped = false);
            (start = (j + 1));
            continue;
        }
        if ((c == '{') || (c == '[')) {
            (depth++);
            continue;
        }
        if (c == '}') {
            (depth--);
            continue;
        }
        if (c == ']') {
            if (depth == 0) {
                break;
            }
            (depth--);
        }
    }
    return result;
}

btrc_Vector_string* JsonText_objectArray(char* text, char* key) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    int i = JsonText_valueStart(text, key);
    int len = ((int)strlen(text));
    if (((i < 0) || (i >= len)) || (text[i] != '[')) {
        return result;
    }
    int depth = 0;
    int start = (-1);
    bool inString = false;
    bool escaped = false;
    for (int j = (i + 1); (j < len); (j++)) {
        char c = text[j];
        if (inString) {
            if ((!escaped) && (c == ((char)34))) {
                (inString = false);
            }
            (escaped = ((!escaped) && (c == '\\')));
            if (c != '\\') {
                (escaped = false);
            }
            continue;
        }
        if (c == ((char)34)) {
            (inString = true);
            (escaped = false);
            continue;
        }
        if (c == ']') {
            if (depth == 0) {
                break;
            }
            (depth--);
            continue;
        }
        if ((c == '{') || (c == '[')) {
            if ((depth == 0) && (c == '{')) {
                (start = j);
            }
            (depth++);
            continue;
        }
        if (c == '}') {
            (depth--);
            if ((depth == 0) && (start >= 0)) {
                btrc_Vector_string_push(result, JsonText_slice(text, start, (j + 1)));
                (start = (-1));
            }
        }
    }
    return result;
}

char* JsonText_expand(char* text, btrc_Map_string_string* args) {
    char* result = Strings_copy(text);
    int __n_79 = btrc_Map_string_string_iterLen(args);
    for (int __i_78 = 0; (__i_78 < __n_79); (__i_78++)) {
        char* key = btrc_Map_string_string_iterGet(args, __i_78);
        char* value = btrc_Map_string_string_iterValueAt(args, __i_78);
        (result = Strings_replace(result, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{{", key)), "}}")), value));
    }
    int __n_81 = btrc_Map_string_string_iterLen(args);
    for (int __i_80 = 0; (__i_80 < __n_81); (__i_80++)) {
        char* key = btrc_Map_string_string_iterGet(args, __i_80);
        char* value = btrc_Map_string_string_iterValueAt(args, __i_80);
        (result = Strings_replace(result, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("${", key)), "}")), value));
    }
    return result;
}

char* semuModelFirstRouteEmulator(char* raw) {
    char* routeObject = JsonText_objectField(raw, "routes");
    btrc_Vector_string* routes = JsonText_objectArray(routeObject, "linux");
    if (routes->len == 0) {
        (routes = JsonText_objectArray(routeObject, "macos"));
    }
    if (routes->len == 0) {
        return "";
    }
    return JsonText_field(btrc_Vector_string_get(routes, 0), "emulator", "");
}

void SemuDefinition_init(SemuDefinition* self, char* id, char* path) {
    self->__rc = 1;
    (self->id = id);
    (self->path = path);
    (self->raw = (FileSystem_isFile(path) ? FileSystem_readText(path) : ""));
}

SemuDefinition* SemuDefinition_new(char* id, char* path) {
    SemuDefinition* self = ((SemuDefinition*)malloc(sizeof(SemuDefinition)));
    memset(self, 0, sizeof(SemuDefinition));
    SemuDefinition_init(self, id, path);
    return self;
}

void SemuDefinition_destroy(SemuDefinition* self) {
    free(self);
}

bool SemuDefinition_exists(SemuDefinition* self) {
    return (((int)strlen(self->raw)) > 0);
}

JsonObject* SemuDefinition_json(SemuDefinition* self) {
    if (!SemuDefinition_exists(self)) {
        return JsonObject_parse("{}");
    }
    return JsonObject_readFile(self->path);
}

char* SemuDefinition_stringField(SemuDefinition* self, char* key, char* fallback) {
    return JsonObject_getString(SemuDefinition_json(self), key, fallback);
}

btrc_Vector_string* SemuDefinition_stringArray(SemuDefinition* self, char* key) {
    return JsonText_stringArray(self->raw, key);
}

void SemuSystemDefinition_init(SemuSystemDefinition* self, SemuDefinition* source) {
    self->__rc = 1;
    if (self->source != NULL) {
        if ((--self->source->__rc) <= 0) {
            SemuDefinition_destroy(self->source);
        }
    }
    (self->source = source);
    (source->__rc++);
    (self->fullname = SemuDefinition_stringField(source, "fullname", source->id));
    (self->romDir = SemuDefinition_stringField(source, "rom_dir", source->id));
    if (self->extensions != NULL) {
        if ((--self->extensions->__rc) <= 0) {
            btrc_Vector_string_free(self->extensions);
        }
    }
    (self->extensions = SemuDefinition_stringArray(source, "extensions"));
    (SemuDefinition_stringArray(source, "extensions")->__rc++);
    if (self->bios != NULL) {
        if ((--self->bios->__rc) <= 0) {
            btrc_Vector_string_free(self->bios);
        }
    }
    (self->bios = SemuDefinition_stringArray(source, "bios"));
    (SemuDefinition_stringArray(source, "bios")->__rc++);
    (self->primaryEmulator = semuModelFirstRouteEmulator(source->raw));
}

SemuSystemDefinition* SemuSystemDefinition_new(SemuDefinition* source) {
    SemuSystemDefinition* self = ((SemuSystemDefinition*)malloc(sizeof(SemuSystemDefinition)));
    memset(self, 0, sizeof(SemuSystemDefinition));
    SemuSystemDefinition_init(self, source);
    return self;
}

void SemuEmulatorDefinition_init(SemuEmulatorDefinition* self, SemuDefinition* source) {
    self->__rc = 1;
    if (self->source != NULL) {
        if ((--self->source->__rc) <= 0) {
            SemuDefinition_destroy(self->source);
        }
    }
    (self->source = source);
    (source->__rc++);
    if (self->servedSystems != NULL) {
        if ((--self->servedSystems->__rc) <= 0) {
            btrc_Vector_string_free(self->servedSystems);
        }
    }
    (self->servedSystems = SemuDefinition_stringArray(source, "servedSystems"));
    (SemuDefinition_stringArray(source, "servedSystems")->__rc++);
}

SemuEmulatorDefinition* SemuEmulatorDefinition_new(SemuDefinition* source) {
    SemuEmulatorDefinition* self = ((SemuEmulatorDefinition*)malloc(sizeof(SemuEmulatorDefinition)));
    memset(self, 0, sizeof(SemuEmulatorDefinition));
    SemuEmulatorDefinition_init(self, source);
    return self;
}

void SemuBuildPlan_init(SemuBuildPlan* self, char* project, char* mode, char* target, char* emulator) {
    self->__rc = 1;
    (self->project = project);
    (self->mode = mode);
    (self->target = target);
    (self->emulator = emulator);
    if (self->systemIds != NULL) {
        if ((--self->systemIds->__rc) <= 0) {
            btrc_Vector_string_free(self->systemIds);
        }
    }
    btrc_Vector_string* __list_83 = btrc_Vector_string_new();
    (self->systemIds = __list_83);
    btrc_Vector_string* __list_82 = btrc_Vector_string_new();
    (__list_82->__rc++);
    if (self->emulatorIds != NULL) {
        if ((--self->emulatorIds->__rc) <= 0) {
            btrc_Vector_string_free(self->emulatorIds);
        }
    }
    btrc_Vector_string* __list_85 = btrc_Vector_string_new();
    (self->emulatorIds = __list_85);
    btrc_Vector_string* __list_84 = btrc_Vector_string_new();
    (__list_84->__rc++);
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            btrc_Vector_string_free(self->errors);
        }
    }
    btrc_Vector_string* __list_87 = btrc_Vector_string_new();
    (self->errors = __list_87);
    btrc_Vector_string* __list_86 = btrc_Vector_string_new();
    (__list_86->__rc++);
    if (self->warnings != NULL) {
        if ((--self->warnings->__rc) <= 0) {
            btrc_Vector_string_free(self->warnings);
        }
    }
    btrc_Vector_string* __list_89 = btrc_Vector_string_new();
    (self->warnings = __list_89);
    btrc_Vector_string* __list_88 = btrc_Vector_string_new();
    (__list_88->__rc++);
    if (self->outputs != NULL) {
        if ((--self->outputs->__rc) <= 0) {
            btrc_Vector_string_free(self->outputs);
        }
    }
    btrc_Vector_string* __list_91 = btrc_Vector_string_new();
    (self->outputs = __list_91);
    btrc_Vector_string* __list_90 = btrc_Vector_string_new();
    (__list_90->__rc++);
}

SemuBuildPlan* SemuBuildPlan_new(char* project, char* mode, char* target, char* emulator) {
    SemuBuildPlan* self = ((SemuBuildPlan*)malloc(sizeof(SemuBuildPlan)));
    memset(self, 0, sizeof(SemuBuildPlan));
    SemuBuildPlan_init(self, project, mode, target, emulator);
    return self;
}

void SemuBuildPlan_destroy(SemuBuildPlan* self) {
    if (self->systemIds != NULL) {
        if ((--self->systemIds->__rc) <= 0) {
            btrc_Vector_string_free(self->systemIds);
        }
    }
    if (self->emulatorIds != NULL) {
        if ((--self->emulatorIds->__rc) <= 0) {
            btrc_Vector_string_free(self->emulatorIds);
        }
    }
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            btrc_Vector_string_free(self->errors);
        }
    }
    if (self->warnings != NULL) {
        if ((--self->warnings->__rc) <= 0) {
            btrc_Vector_string_free(self->warnings);
        }
    }
    if (self->outputs != NULL) {
        if ((--self->outputs->__rc) <= 0) {
            btrc_Vector_string_free(self->outputs);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

bool SemuBuildPlan_ok(SemuBuildPlan* self) {
    return (self->errors->len == 0);
}

void SemuBuildPlan_addError(SemuBuildPlan* self, char* message) {
    btrc_Vector_string_push(self->errors, message);
}

void SemuBuildPlan_addWarning(SemuBuildPlan* self, char* message) {
    btrc_Vector_string_push(self->warnings, message);
}

char* joinPath(char* left, char* right) {
    return PathTools_join(left, right);
}

void ensureDir(char* path) {
    if (!__btrc_isEmpty(path)) {
        FileSystem_mkdirp(path);
    }
}

char* SemuOwnedPaths_sourceRoot(char* project) {
    char* configured = Environment_get("SEMU_ASSET_ROOT", "");
    if ((!__btrc_isEmpty(configured)) && FileSystem_isDir(configured)) {
        return configured;
    }
    if (FileSystem_isDir(joinPath(project, "config"))) {
        return project;
    }
    return FileSystem_currentDirectory();
}

char* SemuOwnedPaths_home(char* project) {
    return Environment_get("SEMU_HOME", joinPath(project, ".semu"));
}

char* SemuOwnedPaths_generated(char* project) {
    return joinPath(SemuOwnedPaths_home(project), "generated");
}

char* SemuOwnedPaths_state(char* project) {
    return joinPath(SemuOwnedPaths_home(project), "state");
}

char* SemuOwnedPaths_cache(char* project) {
    return joinPath(SemuOwnedPaths_home(project), "cache");
}

char* SemuOwnedPaths_overrides(char* project) {
    return joinPath(SemuOwnedPaths_home(project), "overrides");
}

char* SemuOwnedPaths_semuConfig(char* project) {
    return joinPath(SemuOwnedPaths_home(project), "semu.json");
}

char* SemuOwnedPaths_romsRoot(char* project) {
    char* configured = Environment_get("SEMU_ROMS_DIR", Environment_get("SEMU_ROMS", ""));
    if (!__btrc_isEmpty(configured)) {
        return configured;
    }
    char* userConfig = SemuOwnedPaths_semuConfig(project);
    if (FileSystem_isFile(userConfig)) {
        char* configuredRoms = JsonText_field(FileSystem_readText(userConfig), "roms_dir", "");
        if (!__btrc_isEmpty(configuredRoms)) {
            return configuredRoms;
        }
    }
    btrc_Vector_string* __list_92 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_92, "/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs");
    btrc_Vector_string_push(__list_92, "/run/media/deck/SD/Emulation/ROMs");
    btrc_Vector_string_push(__list_92, "/run/media/deck/SD/ROMs");
    btrc_Vector_string_push(__list_92, "/run/media/deck/SD");
    btrc_Vector_string_push(__list_92, "/mnt/SD/Emulation/ES-DE/ES-DE/ROMs");
    btrc_Vector_string_push(__list_92, "/mnt/SD/Emulation/ROMs");
    btrc_Vector_string_push(__list_92, "/mnt/SD/ROMs");
    btrc_Vector_string_push(__list_92, "/mnt/SD");
    btrc_Vector_string_push(__list_92, joinPath(project, "ES-DE/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_92, joinPath(project, "ROMs"));
    btrc_Vector_string* candidates = __list_92;
    int __n_94 = btrc_Vector_string_iterLen(candidates);
    for (int __i_93 = 0; (__i_93 < __n_94); (__i_93++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_93);
        if (FileSystem_isDir(candidate)) {
            return candidate;
        }
    }
    return joinPath(project, "ES-DE/ES-DE/ROMs");
}

char* SemuOwnedPaths_biosRoot(char* project) {
    char* configured = Environment_get("SEMU_BIOS_DIR", "");
    if (!__btrc_isEmpty(configured)) {
        return configured;
    }
    char* roms = SemuOwnedPaths_romsRoot(project);
    char* romsParent = PathTools_dirname(roms);
    char* esdeRoot = PathTools_dirname(romsParent);
    char* emulationRoot = PathTools_dirname(esdeRoot);
    btrc_Vector_string* __list_95 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_95, joinPath(roms, "bios"));
    btrc_Vector_string_push(__list_95, joinPath(romsParent, "bios"));
    btrc_Vector_string_push(__list_95, joinPath(esdeRoot, "bios"));
    btrc_Vector_string_push(__list_95, joinPath(emulationRoot, "RetroArch/config/system"));
    btrc_Vector_string_push(__list_95, joinPath(emulationRoot, "PCSX2/config/bios"));
    btrc_Vector_string_push(__list_95, joinPath(emulationRoot, "bios"));
    btrc_Vector_string_push(__list_95, "/run/media/deck/SD/Emulation/RetroArch/config/system");
    btrc_Vector_string_push(__list_95, "/run/media/deck/SD/Emulation/PCSX2/config/bios");
    btrc_Vector_string_push(__list_95, "/mnt/SD/Emulation/RetroArch/config/system");
    btrc_Vector_string_push(__list_95, "/mnt/SD/Emulation/PCSX2/config/bios");
    btrc_Vector_string_push(__list_95, joinPath(project, "BIOS"));
    btrc_Vector_string_push(__list_95, joinPath(project, "bios"));
    btrc_Vector_string* candidates = __list_95;
    int __n_97 = btrc_Vector_string_iterLen(candidates);
    for (int __i_96 = 0; (__i_96 < __n_97); (__i_96++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_96);
        if (FileSystem_isDir(candidate)) {
            return candidate;
        }
    }
    return joinPath(roms, "bios");
}

char* SemuOwnedPaths_ps2BiosRoot(char* project) {
    char* configured = Environment_get("SEMU_PS2_BIOS_DIR", "");
    if (!__btrc_isEmpty(configured)) {
        return configured;
    }
    char* roms = SemuOwnedPaths_romsRoot(project);
    char* romsParent = PathTools_dirname(roms);
    char* esdeRoot = PathTools_dirname(romsParent);
    char* emulationRoot = PathTools_dirname(esdeRoot);
    btrc_Vector_string* __list_98 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_98, joinPath(emulationRoot, "PCSX2/config/bios"));
    btrc_Vector_string_push(__list_98, "/run/media/deck/SD/Emulation/PCSX2/config/bios");
    btrc_Vector_string_push(__list_98, "/mnt/SD/Emulation/PCSX2/config/bios");
    btrc_Vector_string_push(__list_98, joinPath(SemuOwnedPaths_biosRoot(project), "ps2"));
    btrc_Vector_string_push(__list_98, SemuOwnedPaths_biosRoot(project));
    btrc_Vector_string* candidates = __list_98;
    int __n_100 = btrc_Vector_string_iterLen(candidates);
    for (int __i_99 = 0; (__i_99 < __n_100); (__i_99++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_99);
        if (FileSystem_isDir(candidate)) {
            return candidate;
        }
    }
    return joinPath(SemuOwnedPaths_biosRoot(project), "ps2");
}

char* SemuOwnedPaths_mediaRoot(char* project) {
    return Environment_get("SEMU_MEDIA_DIR", joinPath(project, "ES-DE/downloaded_media"));
}

char* SemuOwnedPaths_themeRoot(char* project) {
    return Environment_get("SEMU_THEME_DIR", joinPath(SemuOwnedPaths_generated(project), "themes"));
}

char* SemuOwnedPaths_directDefinitionRoot(char* root) {
    if (FileSystem_isFile(joinPath(joinPath(root, "targets"), "steam-deck.json"))) {
        return root;
    }
    return "";
}

char* SemuOwnedPaths_sourceDefinitionRoot(char* root) {
    char* config = joinPath(root, "config");
    if (FileSystem_isFile(joinPath(joinPath(config, "targets"), "steam-deck.json"))) {
        return config;
    }
    return "";
}

char* SemuOwnedPaths_definitionRoot(char* project) {
    char* configured = Environment_get("SEMU_DEFINITION_ROOT", "");
    char* configuredRoot = ((((int)strlen(configured)) > 0) ? SemuOwnedPaths_directDefinitionRoot(configured) : "");
    if ((((int)strlen(configuredRoot)) == 0) && (((int)strlen(configured)) > 0)) {
        (configuredRoot = SemuOwnedPaths_sourceDefinitionRoot(configured));
    }
    if (((int)strlen(configuredRoot)) > 0) {
        return configuredRoot;
    }
    char* projectRoot = SemuOwnedPaths_sourceDefinitionRoot(project);
    if (((int)strlen(projectRoot)) > 0) {
        return projectRoot;
    }
    char* root = SemuOwnedPaths_sourceRoot(project);
    char* asset = SemuOwnedPaths_sourceDefinitionRoot(root);
    if (((int)strlen(asset)) > 0) {
        return asset;
    }
    char* cwd = SemuOwnedPaths_sourceDefinitionRoot(".");
    if (((int)strlen(cwd)) > 0) {
        return cwd;
    }
    return joinPath(project, "config");
}

char* SemuOwnedPaths_targetFile(char* project, char* target) {
    return joinPath(joinPath(SemuOwnedPaths_definitionRoot(project), "targets"), __btrc_str_track(__btrc_strcat(target, ".json")));
}

char* SemuOwnedPaths_systemFile(char* project, char* system) {
    return joinPath(joinPath(SemuOwnedPaths_definitionRoot(project), "systems"), __btrc_str_track(__btrc_strcat(system, ".json")));
}

char* SemuOwnedPaths_emulatorFile(char* project, char* emulator, char* name) {
    return joinPath(joinPath(joinPath(SemuOwnedPaths_definitionRoot(project), "emulators"), emulator), name);
}

char* SemuOwnedPaths_systemAssetFile(char* project, char* system) {
    return joinPath(joinPath(joinPath(SemuOwnedPaths_definitionRoot(project), "assets"), "systems"), __btrc_str_track(__btrc_strcat(system, ".json")));
}

char* SemuOwnedPaths_settingsDefinitionFile(char* project, char* name) {
    return joinPath(joinPath(SemuOwnedPaths_definitionRoot(project), "settings"), name);
}

char* SemuOwnedPaths_generatedFile(char* project, char* relative) {
    return joinPath(SemuOwnedPaths_generated(project), relative);
}

void SemuOwnedPaths_writeGenerated(char* project, char* relative, char* content) {
    char* path = SemuOwnedPaths_generatedFile(project, relative);
    ensureDir(PathTools_dirname(path));
    FileSystem_writeText(path, content);
}

btrc_Vector_string* SemuCompilerLexer_jsonIds(char* project, char* directory) {
    btrc_Vector_string* ids = btrc_Vector_string_new();
    char* root = joinPath(SemuOwnedPaths_definitionRoot(project), directory);
    if (!FileSystem_isDir(root)) {
        return ids;
    }
    int __n_102 = btrc_Vector_string_iterLen(FileSystem_listDir(root));
    for (int __i_101 = 0; (__i_101 < __n_102); (__i_101++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(root), __i_101);
        if (!__btrc_endsWith(name, ".json")) {
            continue;
        }
        char* id = __btrc_str_track(__btrc_substring(name, 0, (((int)strlen(name)) - 5)));
        btrc_Vector_string_push(ids, id);
    }
    return ids;
}

btrc_Vector_string* SemuCompilerLexer_emulatorIds(char* project) {
    btrc_Vector_string* ids = btrc_Vector_string_new();
    char* root = joinPath(SemuOwnedPaths_definitionRoot(project), "emulators");
    if (!FileSystem_isDir(root)) {
        return ids;
    }
    int __n_104 = btrc_Vector_string_iterLen(FileSystem_listDir(root));
    for (int __i_103 = 0; (__i_103 < __n_104); (__i_103++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(root), __i_103);
        char* path = joinPath(root, name);
        if (FileSystem_isDir(path) && FileSystem_isFile(joinPath(path, "emulator.json"))) {
            btrc_Vector_string_push(ids, name);
        }
    }
    return ids;
}

SemuDefinition* SemuCompilerParser_target(char* project, char* target) {
    return SemuDefinition_new(target, SemuOwnedPaths_targetFile(project, target));
}

SemuSystemDefinition* SemuCompilerParser_system(char* project, char* system) {
    return SemuSystemDefinition_new(SemuDefinition_new(system, SemuOwnedPaths_systemFile(project, system)));
}

SemuEmulatorDefinition* SemuCompilerParser_emulator(char* project, char* emulator) {
    return SemuEmulatorDefinition_new(SemuDefinition_new(emulator, SemuOwnedPaths_emulatorFile(project, emulator, "emulator.json")));
}

btrc_Vector_string* SemuCompilerParser_systemIds(char* project) {
    return SemuCompilerLexer_jsonIds(project, "systems");
}

btrc_Vector_string* SemuCompilerParser_emulatorIds(char* project) {
    return SemuCompilerLexer_emulatorIds(project);
}

btrc_Vector_string* SemuCompilerParser_targetSystemIds(char* project, char* target) {
    SemuDefinition* definition = SemuCompilerParser_target(project, target);
    return JsonText_stringArray(definition->raw, "systems");
}

btrc_Vector_string* SemuCompilerParser_targetEmulatorIds(char* project, char* target) {
    SemuDefinition* definition = SemuCompilerParser_target(project, target);
    return JsonText_stringArray(definition->raw, "emulator_ids");
}

char* SemuMergePolicy_precedenceText(void) {
    return "repo defaults < target defaults < $SEMU_HOME/semu.json < $SEMU_HOME/overrides/** < CLI flags";
}

char* SemuEmulatorDefinitionGenerator_plan(char* project, char* target, char* emulator) {
    SemuEmulatorDefinition* definition = SemuCompilerParser_emulator(project, emulator);
    JsonObject* metadata = SemuDefinition_json(definition->source);
    char* launchPath = SemuOwnedPaths_emulatorFile(project, emulator, "launch.json");
    char* launchRaw = (FileSystem_isFile(launchPath) ? FileSystem_readText(launchPath) : "{}");
    char* linux = JsonText_objectField(launchRaw, "linux");
    char* launcher = JsonText_field(linux, "launcher", "");
    char* flatpak = JsonText_field(linux, "flatpakId", "");
    char* executable = JsonText_field(linux, "executable", "");
    char* romMode = JsonText_field(JsonText_objectField(launchRaw, "romArg"), "mode", "");
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("generate emulator=", JsonObject_getString(metadata, "id", emulator))), " target=")), target)), " project=")), project)), " launcher=semu-")), emulator));
    if (((int)strlen(launcher)) > 0) {
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, " backend=")), launcher)));
    }
    if (((int)strlen(flatpak)) > 0) {
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, " flatpak=")), flatpak)));
    }
    if (((int)strlen(executable)) > 0) {
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, " executable=")), executable)));
    }
    if (((int)strlen(romMode)) > 0) {
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, " romArg=")), romMode)));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, " systems=")), btrc_Vector_string_join(definition->servedSystems, ",")));
}

char* SemuTemplate_expand(char* text, btrc_Map_string_string* values) {
    return JsonText_expand(text, values);
}

char* SemuTemplate_expandProject(char* project, char* text) {
    btrc_Map_string_string* values = btrc_Map_string_string_new();
    btrc_Map_string_string_put(values, "project", project);
    btrc_Map_string_string_put(values, "semu_home", SemuOwnedPaths_home(project));
    btrc_Map_string_string_put(values, "generated", SemuOwnedPaths_generated(project));
    btrc_Map_string_string_put(values, "state", SemuOwnedPaths_state(project));
    btrc_Map_string_string_put(values, "cache", SemuOwnedPaths_cache(project));
    btrc_Map_string_string_put(values, "roms", SemuOwnedPaths_romsRoot(project));
    btrc_Map_string_string_put(values, "bios", SemuOwnedPaths_biosRoot(project));
    btrc_Map_string_string_put(values, "ps2_bios", SemuOwnedPaths_ps2BiosRoot(project));
    return SemuTemplate_expand(text, values);
}

char* SemuEmulatorStateGenerator_stateFile(char* project, char* emulator) {
    return SemuOwnedPaths_emulatorFile(project, emulator, "state.json");
}

char* SemuEmulatorStateGenerator_templateFile(char* project, char* emulator, char* name) {
    return joinPath(PathTools_dirname(SemuEmulatorStateGenerator_stateFile(project, emulator)), joinPath("templates", name));
}

void SemuEmulatorStateGenerator_write(char* project, char* emulator) {
    char* statePath = SemuEmulatorStateGenerator_stateFile(project, emulator);
    if (!FileSystem_isFile(statePath)) {
        return;
    }
    char* raw = FileSystem_readText(statePath);
    btrc_Vector_string* directories = JsonText_stringArray(raw, "directories");
    int __n_106 = btrc_Vector_string_iterLen(directories);
    for (int __i_105 = 0; (__i_105 < __n_106); (__i_105++)) {
        char* directory = btrc_Vector_string_iterGet(directories, __i_105);
        ensureDir(SemuOwnedPaths_generatedFile(project, directory));
    }
    btrc_Vector_string* cleanDirectories = JsonText_stringArray(raw, "clean_directories");
    int __n_108 = btrc_Vector_string_iterLen(cleanDirectories);
    for (int __i_107 = 0; (__i_107 < __n_108); (__i_107++)) {
        char* directory = btrc_Vector_string_iterGet(cleanDirectories, __i_107);
        char* generatedDirectory = SemuOwnedPaths_generatedFile(project, directory);
        if ((!__btrc_isEmpty(generatedDirectory)) && __btrc_startsWith(generatedDirectory, SemuOwnedPaths_generated(project))) {
            UnixShell* shell = UnixShell_new();
            UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("rm -rf ", ShellWords_quote(generatedDirectory))), " && mkdir -p ")), ShellWords_quote(generatedDirectory))), CommandOutput_stream(), CommandOutput_stream(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
        }
    }
    btrc_Vector_string* templates = JsonText_objectArray(raw, "templates");
    int __n_110 = btrc_Vector_string_iterLen(templates);
    for (int __i_109 = 0; (__i_109 < __n_110); (__i_109++)) {
        char* entry = btrc_Vector_string_iterGet(templates, __i_109);
        char* source = JsonText_field(entry, "source", "");
        char* output = JsonText_field(entry, "output", "");
        if (__btrc_isEmpty(source) || __btrc_isEmpty(output)) {
            continue;
        }
        char* sourcePath = SemuEmulatorStateGenerator_templateFile(project, emulator, source);
        if (!FileSystem_isFile(sourcePath)) {
            continue;
        }
        char* rendered = SemuTemplate_expandProject(project, FileSystem_readText(sourcePath));
        SemuOwnedPaths_writeGenerated(project, output, rendered);
    }
    btrc_Vector_string* copies = JsonText_objectArray(raw, "copies");
    int __n_112 = btrc_Vector_string_iterLen(copies);
    for (int __i_111 = 0; (__i_111 < __n_112); (__i_111++)) {
        char* entry = btrc_Vector_string_iterGet(copies, __i_111);
        char* output = JsonText_field(entry, "output", "");
        if (__btrc_isEmpty(output)) {
            continue;
        }
        btrc_Vector_string* sources = JsonText_stringArray(entry, "sources");
        char* source = JsonText_field(entry, "source", "");
        if (!__btrc_isEmpty(source)) {
            btrc_Vector_string_push(sources, source);
        }
        int __n_114 = btrc_Vector_string_iterLen(sources);
        for (int __i_113 = 0; (__i_113 < __n_114); (__i_113++)) {
            char* candidate = btrc_Vector_string_iterGet(sources, __i_113);
            char* sourcePath = SemuTemplate_expandProject(project, candidate);
            if (FileSystem_isFile(sourcePath)) {
                SemuOwnedPaths_writeGenerated(project, output, FileSystem_readText(sourcePath));
                break;
            }
        }
    }
    btrc_Vector_string* directoryCopies = JsonText_objectArray(raw, "directory_copies");
    int __n_116 = btrc_Vector_string_iterLen(directoryCopies);
    for (int __i_115 = 0; (__i_115 < __n_116); (__i_115++)) {
        char* entry = btrc_Vector_string_iterGet(directoryCopies, __i_115);
        char* output = JsonText_field(entry, "output", "");
        if (__btrc_isEmpty(output)) {
            continue;
        }
        btrc_Vector_string* sources = JsonText_stringArray(entry, "sources");
        char* source = JsonText_field(entry, "source", "");
        if (!__btrc_isEmpty(source)) {
            btrc_Vector_string_push(sources, source);
        }
        int __n_118 = btrc_Vector_string_iterLen(sources);
        for (int __i_117 = 0; (__i_117 < __n_118); (__i_117++)) {
            char* candidate = btrc_Vector_string_iterGet(sources, __i_117);
            char* sourcePath = SemuTemplate_expandProject(project, candidate);
            if (FileSystem_isDir(sourcePath)) {
                char* outputPath = SemuOwnedPaths_generatedFile(project, output);
                ensureDir(PathTools_dirname(outputPath));
                UnixShell* shell = UnixShell_new();
                UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("rm -rf ", ShellWords_quote(outputPath))), " && mkdir -p ")), ShellWords_quote(PathTools_dirname(outputPath)))), " && cp -a ")), ShellWords_quote(sourcePath))), " ")), ShellWords_quote(outputPath))), CommandOutput_stream(), CommandOutput_stream(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
                if (shell != NULL) {
                    if ((--shell->__rc) <= 0) {
                        UnixShell_destroy(shell);
                    }
                }
                break;
                if (shell != NULL) {
                    if ((--shell->__rc) <= 0) {
                        UnixShell_destroy(shell);
                    }
                }
            }
        }
    }
    btrc_Vector_string* directoryLinks = JsonText_objectArray(raw, "directory_links");
    int __n_120 = btrc_Vector_string_iterLen(directoryLinks);
    for (int __i_119 = 0; (__i_119 < __n_120); (__i_119++)) {
        char* entry = btrc_Vector_string_iterGet(directoryLinks, __i_119);
        char* output = JsonText_field(entry, "output", "");
        if (__btrc_isEmpty(output)) {
            continue;
        }
        btrc_Vector_string* sources = JsonText_stringArray(entry, "sources");
        char* source = JsonText_field(entry, "source", "");
        if (!__btrc_isEmpty(source)) {
            btrc_Vector_string_push(sources, source);
        }
        int __n_122 = btrc_Vector_string_iterLen(sources);
        for (int __i_121 = 0; (__i_121 < __n_122); (__i_121++)) {
            char* candidate = btrc_Vector_string_iterGet(sources, __i_121);
            char* sourcePath = SemuTemplate_expandProject(project, candidate);
            if (FileSystem_isDir(sourcePath)) {
                char* outputPath = SemuOwnedPaths_generatedFile(project, output);
                ensureDir(PathTools_dirname(outputPath));
                UnixShell* shell = UnixShell_new();
                UnixShell_run(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("rm -rf ", ShellWords_quote(outputPath))), " && mkdir -p ")), ShellWords_quote(PathTools_dirname(outputPath)))), " && ln -s ")), ShellWords_quote(sourcePath))), " ")), ShellWords_quote(outputPath))), CommandOutput_stream(), CommandOutput_stream(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
                if (shell != NULL) {
                    if ((--shell->__rc) <= 0) {
                        UnixShell_destroy(shell);
                    }
                }
                break;
                if (shell != NULL) {
                    if ((--shell->__rc) <= 0) {
                        UnixShell_destroy(shell);
                    }
                }
            }
        }
    }
}

void SemuEmulatorRenderingDefinition_init(SemuEmulatorRenderingDefinition* self, char* raw) {
    self->__rc = 1;
    (self->raw = raw);
    (self->renderer = JsonText_objectField(raw, "semu_renderer"));
}

SemuEmulatorRenderingDefinition* SemuEmulatorRenderingDefinition_new(char* raw) {
    SemuEmulatorRenderingDefinition* self = ((SemuEmulatorRenderingDefinition*)malloc(sizeof(SemuEmulatorRenderingDefinition)));
    memset(self, 0, sizeof(SemuEmulatorRenderingDefinition));
    SemuEmulatorRenderingDefinition_init(self, raw);
    return self;
}

char* SemuEmulatorRenderingDefinition_field(SemuEmulatorRenderingDefinition* self, char* key, char* fallback) {
    if (__btrc_isEmpty(self->renderer)) {
        return fallback;
    }
    char* value = SemuRenderingDefinition_cleanValue(JsonText_field(self->renderer, key, fallback));
    return (__btrc_isEmpty(value) ? fallback : value);
}

bool SemuEmulatorRenderingDefinition_boolField(SemuEmulatorRenderingDefinition* self, char* key, bool fallback) {
    char* value = SemuEmulatorRenderingDefinition_field(self, key, (fallback ? "true" : "false"));
    if (strcmp(value, "true") == 0) {
        return true;
    }
    if (strcmp(value, "false") == 0) {
        return false;
    }
    return fallback;
}

SemuEmulatorRenderingDefinition* SemuEmulatorRenderingDefinition_load(char* project, char* emulator) {
    char* path = SemuOwnedPaths_emulatorFile(project, emulator, "rendering.json");
    return SemuEmulatorRenderingDefinition_new((FileSystem_isFile(path) ? FileSystem_readText(path) : "{}"));
}

void SemuRenderingDefinition_init(SemuRenderingDefinition* self, char* raw, char* overrideRaw) {
    self->__rc = 1;
    (self->raw = raw);
    (self->overrideRaw = overrideRaw);
    (self->viewport = JsonText_objectField(raw, "content_viewport"));
    (self->overrideViewport = JsonText_objectField(overrideRaw, "content_viewport"));
    (self->renderer = JsonText_objectField(raw, "renderer"));
    (self->overrideRenderer = JsonText_objectField(overrideRaw, "renderer"));
}

SemuRenderingDefinition* SemuRenderingDefinition_new(char* raw, char* overrideRaw) {
    SemuRenderingDefinition* self = ((SemuRenderingDefinition*)malloc(sizeof(SemuRenderingDefinition)));
    memset(self, 0, sizeof(SemuRenderingDefinition));
    SemuRenderingDefinition_init(self, raw, overrideRaw);
    return self;
}

char* SemuRenderingDefinition_pathField(SemuRenderingDefinition* self, char* path, char* fallback) {
    char* value = SemuRenderingDefinition_cleanValue(SemuRenderingDefinition_pathFieldIn(self->overrideRaw, path, ""));
    return (__btrc_isEmpty(value) ? SemuRenderingDefinition_pathFieldIn(self->raw, path, fallback) : value);
}

char* SemuRenderingDefinition_viewportField(SemuRenderingDefinition* self, char* key, char* fallback) {
    char* value = SemuRenderingDefinition_cleanValue(JsonText_field(self->overrideViewport, key, ""));
    if (!__btrc_isEmpty(value)) {
        return value;
    }
    return JsonText_field(self->viewport, key, fallback);
}

char* SemuRenderingDefinition_rendererObject(SemuRenderingDefinition* self, char* key) {
    char* overrideObject = JsonText_objectField(self->overrideRenderer, key);
    if (!__btrc_isEmpty(overrideObject)) {
        return overrideObject;
    }
    return JsonText_objectField(self->renderer, key);
}

char* SemuRenderingDefinition_rendererField(SemuRenderingDefinition* self, char* objectKey, char* fieldKey, char* fallback) {
    char* object = SemuRenderingDefinition_rendererObject(self, objectKey);
    if (__btrc_isEmpty(object)) {
        return fallback;
    }
    return SemuRenderingDefinition_cleanValue(JsonText_field(object, fieldKey, fallback));
}

char* SemuRenderingDefinition_nativeSize(SemuRenderingDefinition* self) {
    char* native = JsonText_objectField(self->overrideViewport, "native_size");
    if (__btrc_isEmpty(native)) {
        (native = JsonText_objectField(self->viewport, "native_size"));
    }
    char* width = JsonText_field(native, "width", "");
    char* height = JsonText_field(native, "height", "");
    if (__btrc_isEmpty(width) || __btrc_isEmpty(height)) {
        return "";
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(width, "x")), height));
}

char* SemuRenderingDefinition_contentAspect(SemuRenderingDefinition* self) {
    return SemuRenderingDefinition_viewportField(self, "content_aspect", SemuRenderingDefinition_viewportField(self, "default_aspect", "4:3"));
}

char* SemuRenderingDefinition_layoutKind(SemuRenderingDefinition* self) {
    char* layout = JsonText_objectField(self->viewport, "layout");
    return JsonText_field(layout, "kind", "single");
}

char* SemuRenderingDefinition_scalePolicy(SemuRenderingDefinition* self) {
    return SemuRenderingDefinition_viewportField(self, "scale_policy", "aspect_preserve");
}

char* SemuRenderingDefinition_dynamicAspectFlag(SemuRenderingDefinition* self) {
    char* value = SemuRenderingDefinition_viewportField(self, "dynamic_aspect", "false");
    return ((strcmp(value, "true") == 0) ? "1" : "0");
}

char* SemuRenderingDefinition_bezelEffectFile(SemuRenderingDefinition* self) {
    return SemuRenderingDefinition_rendererField(self, "bezel", "composition_effect_file", "");
}

char* SemuRenderingDefinition_shaderEffectFile(SemuRenderingDefinition* self) {
    return SemuRenderingDefinition_rendererField(self, "shader", "effect_file", "");
}

SemuRenderingDefinition* SemuRenderingDefinition_load(char* project, char* systemId) {
    char* path = SemuOwnedPaths_systemAssetFile(project, systemId);
    char* overridePath = joinPath(joinPath(SemuOwnedPaths_overrides(project), "rendering"), __btrc_str_track(__btrc_strcat(systemId, ".json")));
    if (!FileSystem_isFile(path)) {
        return SemuRenderingDefinition_new("{}", (FileSystem_isFile(overridePath) ? FileSystem_readText(overridePath) : "{}"));
    }
    return SemuRenderingDefinition_new(FileSystem_readText(path), (FileSystem_isFile(overridePath) ? FileSystem_readText(overridePath) : "{}"));
}

char* SemuRenderingDefinition_pathFieldIn(char* raw, char* path, char* fallback) {
    if (__btrc_isEmpty(raw) || __btrc_isEmpty(path)) {
        return fallback;
    }
    btrc_Vector_string* parts = Strings_split(path, ".");
    if (parts->len == 0) {
        return fallback;
    }
    char* object = raw;
    for (int i = 0; (i < (parts->len - 1)); (i++)) {
        (object = JsonText_objectField(object, btrc_Vector_string_get(parts, i)));
        if (__btrc_isEmpty(object)) {
            return fallback;
        }
    }
    return SemuRenderingDefinition_cleanValue(JsonText_field(object, btrc_Vector_string_get(parts, (parts->len - 1)), fallback));
}

char* SemuRenderingDefinition_nestedOverrideJson(char* path, char* value) {
    btrc_Vector_string* parts = Strings_split(path, ".");
    if (parts->len == 0) {
        return "{\n}\n";
    }
    char* rendered = SemuRenderingDefinition_jsonValue(value);
    for (int i = (parts->len - 1); (i >= 0); (i--)) {
        (rendered = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{\n  \"", JsonObject_escape(btrc_Vector_string_get(parts, i)))), "\": ")), SemuRenderingDefinition_indentJson(rendered))), "\n}")));
    }
    return __btrc_str_track(__btrc_strcat(rendered, "\n"));
}

char* SemuRenderingDefinition_jsonValue(char* value) {
    if (((strcmp(value, "true") == 0) || (strcmp(value, "false") == 0)) || (strcmp(value, "null") == 0)) {
        return value;
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", JsonObject_escape(value))), "\""));
}

char* SemuRenderingDefinition_cleanValue(char* value) {
    return ((strcmp(value, "null") == 0) ? "" : value);
}

char* SemuRenderingDefinition_indentJson(char* value) {
    btrc_Vector_string* lines = Strings_split(value, "\n");
    btrc_Vector_string* rendered = btrc_Vector_string_new();
    int __n_124 = btrc_Vector_string_iterLen(lines);
    for (int __i_123 = 0; (__i_123 < __n_124); (__i_123++)) {
        char* line = btrc_Vector_string_iterGet(lines, __i_123);
        if (!__btrc_isEmpty(line)) {
            btrc_Vector_string_push(rendered, line);
        }
    }
    if (rendered->len == 0) {
        return value;
    }
    return btrc_Vector_string_join(rendered, "\n  ");
}

void SemuEsDeCommand_init(SemuEsDeCommand* self) {
    self->__rc = 1;
    (self->label = "");
    (self->command = "");
}

SemuEsDeCommand* SemuEsDeCommand_new(void) {
    SemuEsDeCommand* self = ((SemuEsDeCommand*)malloc(sizeof(SemuEsDeCommand)));
    memset(self, 0, sizeof(SemuEsDeCommand));
    SemuEsDeCommand_init(self);
    return self;
}

void SemuEsDeCommand_destroy(SemuEsDeCommand* self) {
    free(self);
}

void SemuEsDeSystem_init(SemuEsDeSystem* self) {
    self->__rc = 1;
    (self->id = "");
    (self->fullname = "");
    (self->platform = "");
    (self->theme = "");
    (self->romDir = "");
    if (self->extensions != NULL) {
        if ((--self->extensions->__rc) <= 0) {
            btrc_Vector_string_free(self->extensions);
        }
    }
    btrc_Vector_string* __list_126 = btrc_Vector_string_new();
    (self->extensions = __list_126);
    btrc_Vector_string* __list_125 = btrc_Vector_string_new();
    (__list_125->__rc++);
    if (self->commands != NULL) {
        if ((--self->commands->__rc) <= 0) {
            btrc_Vector_SemuEsDeCommand_free(self->commands);
        }
    }
    btrc_Vector_SemuEsDeCommand* __list_128 = btrc_Vector_SemuEsDeCommand_new();
    (self->commands = __list_128);
    btrc_Vector_SemuEsDeCommand* __list_127 = btrc_Vector_SemuEsDeCommand_new();
    (__list_127->__rc++);
}

SemuEsDeSystem* SemuEsDeSystem_new(void) {
    SemuEsDeSystem* self = ((SemuEsDeSystem*)malloc(sizeof(SemuEsDeSystem)));
    memset(self, 0, sizeof(SemuEsDeSystem));
    SemuEsDeSystem_init(self);
    return self;
}

void SemuEsDeSystem_destroy(SemuEsDeSystem* self) {
    if (self->extensions != NULL) {
        if ((--self->extensions->__rc) <= 0) {
            btrc_Vector_string_free(self->extensions);
        }
    }
    if (self->commands != NULL) {
        if ((--self->commands->__rc) <= 0) {
            btrc_Vector_SemuEsDeCommand_free(self->commands);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void SemuEsDeFindRule_init(SemuEsDeFindRule* self) {
    self->__rc = 1;
    (self->kind = "emulator");
    (self->name = "");
    if (self->entries != NULL) {
        if ((--self->entries->__rc) <= 0) {
            btrc_Vector_string_free(self->entries);
        }
    }
    btrc_Vector_string* __list_130 = btrc_Vector_string_new();
    (self->entries = __list_130);
    btrc_Vector_string* __list_129 = btrc_Vector_string_new();
    (__list_129->__rc++);
}

SemuEsDeFindRule* SemuEsDeFindRule_new(void) {
    SemuEsDeFindRule* self = ((SemuEsDeFindRule*)malloc(sizeof(SemuEsDeFindRule)));
    memset(self, 0, sizeof(SemuEsDeFindRule));
    SemuEsDeFindRule_init(self);
    return self;
}

void SemuEsDeFindRule_destroy(SemuEsDeFindRule* self) {
    if (self->entries != NULL) {
        if ((--self->entries->__rc) <= 0) {
            btrc_Vector_string_free(self->entries);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* SemuEsDeGenerator_plan(char* project, char* target) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("generate esde target=", target)), " project=")), project)), " outputs=ES-DE/es_settings.xml,ES-DE/custom_systems/es_systems.xml,.semu/generated/bin"));
}

SemuEsDeCommand* SemuEsDeGenerator_command(char* label, char* command) {
    SemuEsDeCommand* entry = SemuEsDeCommand_new();
    (entry->label = label);
    (entry->command = command);
    return entry;
    if (entry != NULL) {
        if ((--entry->__rc) <= 0) {
            SemuEsDeCommand_destroy(entry);
        }
    }
}

SemuEsDeSystem* SemuEsDeGenerator_system(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_SemuEsDeCommand* commands) {
    SemuEsDeSystem* system = SemuEsDeSystem_new();
    (system->id = id);
    (system->fullname = fullname);
    (system->platform = platform);
    (system->theme = theme);
    (system->romDir = romDir);
    if (system->extensions != NULL) {
        if ((--system->extensions->__rc) <= 0) {
            btrc_Vector_string_free(system->extensions);
        }
    }
    (system->extensions = extensions);
    (extensions->__rc++);
    if (system->commands != NULL) {
        if ((--system->commands->__rc) <= 0) {
            btrc_Vector_SemuEsDeCommand_free(system->commands);
        }
    }
    (system->commands = commands);
    (commands->__rc++);
    return system;
    if (system != NULL) {
        if ((--system->__rc) <= 0) {
            SemuEsDeSystem_destroy(system);
        }
    }
}

SemuEsDeFindRule* SemuEsDeGenerator_emulatorRule(char* name, char* launcherPath) {
    SemuEsDeFindRule* rule = SemuEsDeFindRule_new();
    (rule->kind = "emulator");
    (rule->name = name);
    if (rule->entries != NULL) {
        if ((--rule->entries->__rc) <= 0) {
            btrc_Vector_string_free(rule->entries);
        }
    }
    btrc_Vector_string* __list_132 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_132, launcherPath);
    (rule->entries = __list_132);
    btrc_Vector_string* __list_131 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_131, launcherPath);
    (__list_131->__rc++);
    return rule;
    if (rule != NULL) {
        if ((--rule->__rc) <= 0) {
            SemuEsDeFindRule_destroy(rule);
        }
    }
}

SemuEsDeFindRule* SemuEsDeGenerator_coreRule(char* name, btrc_Vector_string* paths) {
    SemuEsDeFindRule* rule = SemuEsDeFindRule_new();
    (rule->kind = "core");
    (rule->name = name);
    if (rule->entries != NULL) {
        if ((--rule->entries->__rc) <= 0) {
            btrc_Vector_string_free(rule->entries);
        }
    }
    (rule->entries = paths);
    (paths->__rc++);
    return rule;
    if (rule != NULL) {
        if ((--rule->__rc) <= 0) {
            SemuEsDeFindRule_destroy(rule);
        }
    }
}

char* SemuEsDeGenerator_settingsXml(char* roms, char* media, char* themes, char* startupSystem, char* startupView, char* theme) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<string name=\"ROMDirectory\" value=\"")), semuEsDeXmlEscape(roms))), "\" />\n")), "<string name=\"MediaDirectory\" value=\"")), semuEsDeXmlEscape(media))), "\" />\n")), "<string name=\"UserThemeDirectory\" value=\"")), semuEsDeXmlEscape(themes))), "\" />\n")), "<string name=\"ApplicationUpdaterFrequency\" value=\"never\" />\n")), "<string name=\"SaveGamelistsMode\" value=\"always\" />\n")), "<string name=\"CreatePlaceholderSystemDirectories\" value=\"false\" />\n")), "<string name=\"InputControllerType\" value=\"xbox\" />\n")), "<string name=\"StartupSystem\" value=\"")), semuEsDeXmlEscape(startupSystem))), "\" />\n")), "<string name=\"StartupView\" value=\"")), semuEsDeXmlEscape(startupView))), "\" />\n")), "<string name=\"Theme\" value=\"")), semuEsDeXmlEscape(theme))), "\" />\n"));
}

char* SemuEsDeGenerator_systemsXml(btrc_Vector_SemuEsDeSystem* systems) {
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by Semu compiler ES-DE generator -->\n")), "<systemList>\n"));
    int __n_134 = btrc_Vector_SemuEsDeSystem_iterLen(systems);
    for (int __i_133 = 0; (__i_133 < __n_134); (__i_133++)) {
        SemuEsDeSystem* system = btrc_Vector_SemuEsDeSystem_iterGet(systems, __i_133);
        (result = __btrc_str_track(__btrc_strcat(result, semuEsDeSystemXml(system))));
    }
    return __btrc_str_track(__btrc_strcat(result, "</systemList>\n"));
}

char* SemuEsDeGenerator_findRulesXml(btrc_Vector_SemuEsDeFindRule* rules) {
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by Semu compiler ES-DE generator -->\n")), "<ruleList>\n"));
    int __n_136 = btrc_Vector_SemuEsDeFindRule_iterLen(rules);
    for (int __i_135 = 0; (__i_135 < __n_136); (__i_135++)) {
        SemuEsDeFindRule* rule = btrc_Vector_SemuEsDeFindRule_iterGet(rules, __i_135);
        (result = __btrc_str_track(__btrc_strcat(result, semuEsDeFindRuleXml(rule))));
    }
    return __btrc_str_track(__btrc_strcat(result, "</ruleList>\n"));
}

char* SemuEsDeGenerator_settingsLauncherScript(void) {
    btrc_Vector_string* __list_137 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_137, "#!/usr/bin/env sh");
    btrc_Vector_string_push(__list_137, "set -eu");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "here=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd -P)\"");
    btrc_Vector_string_push(__list_137, "if [ -z \"${SEMU_PROJECT_DIR:-}\" ]; then");
    btrc_Vector_string_push(__list_137, "  project=\"$(CDPATH= cd -- \"$here/../../..\" && pwd -P)\"");
    btrc_Vector_string_push(__list_137, "  export SEMU_PROJECT_DIR=\"$project\"");
    btrc_Vector_string_push(__list_137, "else");
    btrc_Vector_string_push(__list_137, "  project=\"$SEMU_PROJECT_DIR\"");
    btrc_Vector_string_push(__list_137, "fi");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "entry=\"${1:-}\"");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "usable_runner() {");
    btrc_Vector_string_push(__list_137, "  [ -n \"$1\" ] && [ -x \"$1\" ] || return 1");
    btrc_Vector_string_push(__list_137, "  case \"$1\" in");
    btrc_Vector_string_push(__list_137, "    *.AppImage|*.appimage) return 1 ;;");
    btrc_Vector_string_push(__list_137, "  esac");
    btrc_Vector_string_push(__list_137, "  return 0");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "find_runner() {");
    btrc_Vector_string_push(__list_137, "  for candidate in \"${SEMU_CLI:-}\" \"$here/semu\" \"$here/semu-btrc\" \"${SEMU_ACTIVE_LAUNCHER_BIN:-}/semu\" \"${SEMU_ASSET_ROOT:-}/result-full/bin/semu\" \"$HOME/semu/result-full/bin/semu\" \"${SEMU_ASSET_ROOT:-}/build/out/semu\" \"$project/build/out/semu\" \"$HOME/Applications/Semu/semu\" \"$(command -v semu-btrc 2>/dev/null || true)\" \"$(command -v semu 2>/dev/null || true)\"; do");
    btrc_Vector_string_push(__list_137, "    usable_runner \"$candidate\" || continue");
    btrc_Vector_string_push(__list_137, "    printf '%s\\n' \"$candidate\"");
    btrc_Vector_string_push(__list_137, "    return 0");
    btrc_Vector_string_push(__list_137, "  done");
    btrc_Vector_string_push(__list_137, "  return 1");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "runner=\"$(find_runner || true)\"");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "settings_cli_missing() {");
    btrc_Vector_string_push(__list_137, "  printf '%s\\n' \"Semu Settings: generated semu-btrc or semu CLI not found.\" >&2");
    btrc_Vector_string_push(__list_137, "  return 127");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "run_settings() {");
    btrc_Vector_string_push(__list_137, "  if [ -n \"$runner\" ]; then");
    btrc_Vector_string_push(__list_137, "    if [ -n \"$entry\" ] && [ -f \"$entry\" ]; then");
    btrc_Vector_string_push(__list_137, "      \"$runner\" settings entry \"$entry\" --project \"$project\"");
    btrc_Vector_string_push(__list_137, "    else");
    btrc_Vector_string_push(__list_137, "      \"$runner\" settings ui --project \"$project\"");
    btrc_Vector_string_push(__list_137, "    fi");
    btrc_Vector_string_push(__list_137, "  else");
    btrc_Vector_string_push(__list_137, "    settings_cli_missing");
    btrc_Vector_string_push(__list_137, "  fi");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "exec_settings() {");
    btrc_Vector_string_push(__list_137, "  if [ -n \"$runner\" ]; then");
    btrc_Vector_string_push(__list_137, "    if [ -n \"$entry\" ] && [ -f \"$entry\" ]; then");
    btrc_Vector_string_push(__list_137, "      exec \"$runner\" settings entry \"$entry\" --project \"$project\"");
    btrc_Vector_string_push(__list_137, "    fi");
    btrc_Vector_string_push(__list_137, "    exec \"$runner\" settings ui --project \"$project\"");
    btrc_Vector_string_push(__list_137, "  fi");
    btrc_Vector_string_push(__list_137, "  settings_cli_missing");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "open_terminal() {");
    btrc_Vector_string_push(__list_137, "  if [ -z \"$runner\" ]; then");
    btrc_Vector_string_push(__list_137, "    settings_cli_missing");
    btrc_Vector_string_push(__list_137, "    return 127");
    btrc_Vector_string_push(__list_137, "  fi");
    btrc_Vector_string_push(__list_137, "  terminal=\"$1\"");
    btrc_Vector_string_push(__list_137, "  if [ -n \"$entry\" ] && [ -f \"$entry\" ]; then");
    btrc_Vector_string_push(__list_137, "    case \"$terminal\" in");
    btrc_Vector_string_push(__list_137, "      konsole) exec \"$terminal\" --fullscreen -e \"$runner\" settings entry \"$entry\" --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "      xterm) exec \"$terminal\" -fullscreen -e \"$runner\" settings entry \"$entry\" --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "      gnome-terminal|kgx) exec \"$terminal\" -- \"$runner\" settings entry \"$entry\" --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "      *) exec \"$terminal\" -e \"$runner\" settings entry \"$entry\" --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "    esac");
    btrc_Vector_string_push(__list_137, "  fi");
    btrc_Vector_string_push(__list_137, "  case \"$terminal\" in");
    btrc_Vector_string_push(__list_137, "    konsole) exec \"$terminal\" --fullscreen -e \"$runner\" settings ui --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "    xterm) exec \"$terminal\" -fullscreen -e \"$runner\" settings ui --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "    gnome-terminal|kgx) exec \"$terminal\" -- \"$runner\" settings ui --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "    *) exec \"$terminal\" -e \"$runner\" settings ui --project \"$project\" ;;");
    btrc_Vector_string_push(__list_137, "  esac");
    btrc_Vector_string_push(__list_137, "}");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "action=\"ui\"");
    btrc_Vector_string_push(__list_137, "if [ -n \"$entry\" ] && [ -f \"$entry\" ]; then");
    btrc_Vector_string_push(__list_137, "  IFS= read -r action < \"$entry\" || action=\"\"");
    btrc_Vector_string_push(__list_137, "fi");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "case \"$action\" in");
    btrc_Vector_string_push(__list_137, "  ui|ui\\ *)");
    btrc_Vector_string_push(__list_137, "    if [ \"${SEMU_SETTINGS_NO_TERMINAL:-0}\" != \"1\" ] && [ ! -t 0 ]; then");
    btrc_Vector_string_push(__list_137, "      semu_home=\"${SEMU_HOME:-$project/.semu}\"");
    btrc_Vector_string_push(__list_137, "      status_dir=\"$semu_home/state\"");
    btrc_Vector_string_push(__list_137, "      mkdir -p \"$status_dir\" 2>/dev/null || status_dir=\"${TMPDIR:-/tmp}\"");
    btrc_Vector_string_push(__list_137, "      status_file=\"$status_dir/semu-settings-last.txt\"");
    btrc_Vector_string_push(__list_137, "      status=0");
    btrc_Vector_string_push(__list_137, "      run_settings > \"$status_file\" 2>&1 || status=$?");
    btrc_Vector_string_push(__list_137, "      if command -v kdialog >/dev/null 2>&1; then");
    btrc_Vector_string_push(__list_137, "        kdialog --title \"Semu Settings\" --textbox \"$status_file\" 900 650 || true");
    btrc_Vector_string_push(__list_137, "        exit \"$status\"");
    btrc_Vector_string_push(__list_137, "      fi");
    btrc_Vector_string_push(__list_137, "      if command -v zenity >/dev/null 2>&1; then");
    btrc_Vector_string_push(__list_137, "        zenity --title=\"Semu Settings\" --text-info --width=900 --height=650 --filename=\"$status_file\" || true");
    btrc_Vector_string_push(__list_137, "        exit \"$status\"");
    btrc_Vector_string_push(__list_137, "      fi");
    btrc_Vector_string_push(__list_137, "      for terminal in konsole xterm gnome-terminal kgx alacritty kitty foot x-terminal-emulator; do");
    btrc_Vector_string_push(__list_137, "        command -v \"$terminal\" >/dev/null 2>&1 || continue");
    btrc_Vector_string_push(__list_137, "        open_terminal \"$terminal\"");
    btrc_Vector_string_push(__list_137, "      done");
    btrc_Vector_string_push(__list_137, "    fi");
    btrc_Vector_string_push(__list_137, "    ;;");
    btrc_Vector_string_push(__list_137, "esac");
    btrc_Vector_string_push(__list_137, "");
    btrc_Vector_string_push(__list_137, "exec_settings");
    btrc_Vector_string* lines = __list_137;
    return __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
}

char* semuEsDeXmlEscape(char* value) {
    char* result = Strings_replace(value, "&", "&amp;");
    (result = Strings_replace(result, "\"", "&quot;"));
    (result = Strings_replace(result, "<", "&lt;"));
    (result = Strings_replace(result, ">", "&gt;"));
    return result;
}

char* semuEsDeExtensionList(btrc_Vector_string* extensions) {
    btrc_Vector_string* values = btrc_Vector_string_new();
    int __n_139 = btrc_Vector_string_iterLen(extensions);
    for (int __i_138 = 0; (__i_138 < __n_139); (__i_138++)) {
        char* extension = btrc_Vector_string_iterGet(extensions, __i_138);
        btrc_Vector_string_push(values, extension);
        btrc_Vector_string_push(values, __btrc_str_track(__btrc_toUpper(extension)));
    }
    return btrc_Vector_string_join(values, " ");
}

char* semuEsDeCommandXml(SemuEsDeCommand* command) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    <command label=\"", semuEsDeXmlEscape(command->label))), "\">")), semuEsDeXmlEscape(command->command))), "</command>\n"));
}

char* semuEsDeSystemXml(SemuEsDeSystem* system) {
    char* path = (__btrc_startsWith(system->romDir, "/") ? semuEsDeXmlEscape(system->romDir) : __btrc_str_track(__btrc_strcat("%ROMPATH%/", semuEsDeXmlEscape(system->romDir))));
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <system>\n", "    <name>")), semuEsDeXmlEscape(system->id))), "</name>\n")), "    <fullname>")), semuEsDeXmlEscape(system->fullname))), "</fullname>\n")), "    <path>")), path)), "</path>\n")), "    <extension>")), semuEsDeXmlEscape(semuEsDeExtensionList(system->extensions)))), "</extension>\n"));
    int __n_141 = btrc_Vector_SemuEsDeCommand_iterLen(system->commands);
    for (int __i_140 = 0; (__i_140 < __n_141); (__i_140++)) {
        SemuEsDeCommand* command = btrc_Vector_SemuEsDeCommand_iterGet(system->commands, __i_140);
        (result = __btrc_str_track(__btrc_strcat(result, semuEsDeCommandXml(command))));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "    <platform>")), semuEsDeXmlEscape(system->platform))), "</platform>\n")), "    <theme>")), semuEsDeXmlEscape(system->theme))), "</theme>\n")), "  </system>\n"));
}

char* semuEsDeFindRuleXml(SemuEsDeFindRule* rule) {
    char* ruleType = ((strcmp(rule->kind, "core") == 0) ? "corepath" : "staticpath");
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <", rule->kind)), " name=\"")), semuEsDeXmlEscape(rule->name))), "\">\n")), "    <rule type=\"")), ruleType)), "\">\n"));
    int __n_143 = btrc_Vector_string_iterLen(rule->entries);
    for (int __i_142 = 0; (__i_142 < __n_143); (__i_142++)) {
        char* entry = btrc_Vector_string_iterGet(rule->entries, __i_142);
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "      <entry>")), semuEsDeXmlEscape(entry))), "</entry>\n")));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "    </rule>\n")), "  </")), rule->kind)), ">\n"));
}

void SemuSteamInputTemplate_init(SemuSteamInputTemplate* self) {
    self->__rc = 1;
    (self->id = "");
    (self->title = "");
    (self->source = "");
    (self->destination = "");
    (self->required = false);
}

SemuSteamInputTemplate* SemuSteamInputTemplate_new(void) {
    SemuSteamInputTemplate* self = ((SemuSteamInputTemplate*)malloc(sizeof(SemuSteamInputTemplate)));
    memset(self, 0, sizeof(SemuSteamInputTemplate));
    SemuSteamInputTemplate_init(self);
    return self;
}

void SemuSteamInputTemplate_destroy(SemuSteamInputTemplate* self) {
    free(self);
}

char* SemuSteamInputGenerator_plan(char* project, char* target) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("generate steam_input target=", target)), " project=")), project)), " outputs=config/input/steam-input"));
}

char* SemuSteamInputGenerator_selectionJson(char* targetId, char* deviceId, char* templateId, btrc_Vector_string* radialActions) {
    btrc_Vector_string* __list_147 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_147, semuSteamInputJsonStrField("target", targetId));
    btrc_Vector_string_push(__list_147, semuSteamInputJsonStrField("device", deviceId));
    btrc_Vector_string_push(__list_147, semuSteamInputJsonStrField("template", templateId));
    btrc_Vector_string_push(__list_147, semuSteamInputJsonField("radial_actions", semuSteamInputJsonStringArray(radialActions)));
    return semuSteamInputJsonObject(__list_147);
}

char* semuSteamInputJsonQ(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", Strings_replace(Strings_replace(value, "\\", "\\\\"), "\"", "\\\""))), "\""));
}

char* semuSteamInputJsonField(char* key, char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(semuSteamInputJsonQ(key), ": ")), value));
}

char* semuSteamInputJsonStrField(char* key, char* value) {
    return semuSteamInputJsonField(key, semuSteamInputJsonQ(value));
}

char* semuSteamInputJsonObject(btrc_Vector_string* fields) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", btrc_Vector_string_join(fields, ", "))), "}\n"));
}

char* semuSteamInputJsonStringArray(btrc_Vector_string* values) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    int __n_149 = btrc_Vector_string_iterLen(values);
    for (int __i_148 = 0; (__i_148 < __n_149); (__i_148++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_148);
        btrc_Vector_string_push(out, semuSteamInputJsonQ(value));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[", btrc_Vector_string_join(out, ", "))), "]"));
}

void SemuAppImageContent_init(SemuAppImageContent* self) {
    self->__rc = 1;
    (self->path = "");
    (self->text = "");
    (self->executable = false);
}

SemuAppImageContent* SemuAppImageContent_new(void) {
    SemuAppImageContent* self = ((SemuAppImageContent*)malloc(sizeof(SemuAppImageContent)));
    memset(self, 0, sizeof(SemuAppImageContent));
    SemuAppImageContent_init(self);
    return self;
}

void SemuAppImageContent_destroy(SemuAppImageContent* self) {
    free(self);
}

char* SemuAppImageGenerator_plan(char* project, char* target) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("generate appimage target=", target)), " project=")), project)), " outputs=.semu/generated/packaging/appimage/manifest.json,build/packaging/linux/AppRun,build/packaging/linux/build-appimage.sh"));
}

SemuAppImageContent* SemuAppImageGenerator_content(char* path, char* text, bool executable) {
    SemuAppImageContent* content = SemuAppImageContent_new();
    (content->path = path);
    (content->text = text);
    (content->executable = executable);
    return content;
    if (content != NULL) {
        if ((--content->__rc) <= 0) {
            SemuAppImageContent_destroy(content);
        }
    }
}

char* SemuAppImageGenerator_btrcShim(void) {
    return "#!/usr/bin/env sh\nset -eu\nhere=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd -P)\"\n\nusable_cli() {\n  [ -n \"$1\" ] && [ -x \"$1\" ] || return 1\n  case \"$1\" in\n    *.AppImage|*.appimage) return 1 ;;\n  esac\n  return 0\n}\n\nfind_cli() {\n  for candidate in \
      \"${SEMU_CLI:-}\" \
      \"$here/semu\" \
      \"${APPDIR:-}/usr/bin/semu\" \
      \"${SEMU_ASSET_ROOT:-}/result-full/bin/semu\" \
      \"$HOME/semu/result-full/bin/semu\" \
      \"${SEMU_PROJECT_DIR:-}/build/out/semu\" \
      \"${SEMU_ASSET_ROOT:-}/build/out/semu\" \
      \"$HOME/semu/build/out/semu\" \
      \"$HOME/Applications/Semu/semu\" \
      \"$(command -v semu || true)\"; do\n    usable_cli \"$candidate\" || continue\n    printf '%s\n' \"$candidate\"\n    return 0\n  done\n  return 1\n}\n\ncli=\"$(find_cli || true)\"\n\nif [ -z \"$cli\" ] || [ ! -x \"$cli\" ]; then\n  echo \"semu-btrc: compiled BTRC CLI not found\" >&2\n  exit 127\nfi\n\nexec \"$cli\" \"$@\"\n";
}

char* SemuAppImageGenerator_emulatorShim(char* emulatorId) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\nexec \"$(dirname \"$0\")/semu-btrc\" launcher ", semuAppImageShellWord(emulatorId))), " \"$@\"\n"));
}

char* SemuAppImageGenerator_desktopEntry(char* name, char* comment, char* icon) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Desktop Entry]\n", "Name=")), name)), "\n")), "Comment=")), comment)), "\n")), "Exec=AppRun\n")), "Icon=")), icon)), "\n")), "Type=Application\n")), "Categories=Game;Emulator;\n")), "Terminal=false\n"));
}

char* SemuAppImageGenerator_manifestJson(btrc_Vector_SemuAppImageContent* contents) {
    btrc_Vector_string* items = btrc_Vector_string_new();
    int __n_152 = btrc_Vector_SemuAppImageContent_iterLen(contents);
    for (int __i_151 = 0; (__i_151 < __n_152); (__i_151++)) {
        SemuAppImageContent* content = btrc_Vector_SemuAppImageContent_iterGet(contents, __i_151);
        btrc_Vector_string_push(items, semuAppImageContentJson(content));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", "\"schema_version\": 1, ")), "\"contents\": [")), btrc_Vector_string_join(items, ", "))), "]")), "}\n"));
}

char* semuAppImageShellWord(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("'", Strings_replace(value, "'", "'\"'\"'"))), "'"));
}

char* semuAppImageJsonQ(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", Strings_replace(Strings_replace(value, "\\", "\\\\"), "\"", "\\\""))), "\""));
}

char* semuAppImageContentJson(SemuAppImageContent* content) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", "\"path\": ")), semuAppImageJsonQ(content->path))), ", ")), "\"executable\": ")), (content->executable ? "true" : "false"))), "}"));
}

SemuBuildPlan* SemuCompilerResolver_resolve(CliArgs* args, char* project) {
    char* mode = "target";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    char* target = CliArgs_valueAfter(args, "--target", "steam-deck");
    char* emulator = CliArgs_valueAfter(args, "--emulator", "");
    if (((strcmp(mode, "target") == 0) && (CliArgs_count(args) > 2)) && (!__btrc_startsWith(CliArgs_get(args, 2), "--"))) {
        (target = CliArgs_get(args, 2));
    }
    if (((strcmp(mode, "emulator") == 0) && (CliArgs_count(args) > 2)) && (!__btrc_startsWith(CliArgs_get(args, 2), "--"))) {
        (emulator = CliArgs_get(args, 2));
    }
    SemuBuildPlan* plan = SemuBuildPlan_new(project, mode, target, emulator);
    if (plan->systemIds != NULL) {
        if ((--plan->systemIds->__rc) <= 0) {
            btrc_Vector_string_free(plan->systemIds);
        }
    }
    (plan->systemIds = SemuCompilerParser_targetSystemIds(project, target));
    (SemuCompilerParser_targetSystemIds(project, target)->__rc++);
    if (plan->systemIds->len == 0) {
        if (plan->systemIds != NULL) {
            if ((--plan->systemIds->__rc) <= 0) {
                btrc_Vector_string_free(plan->systemIds);
            }
        }
        (plan->systemIds = SemuCompilerParser_systemIds(project));
        (SemuCompilerParser_systemIds(project)->__rc++);
    }
    if (plan->emulatorIds != NULL) {
        if ((--plan->emulatorIds->__rc) <= 0) {
            btrc_Vector_string_free(plan->emulatorIds);
        }
    }
    (plan->emulatorIds = SemuCompilerParser_targetEmulatorIds(project, target));
    (SemuCompilerParser_targetEmulatorIds(project, target)->__rc++);
    if (plan->emulatorIds->len == 0) {
        if (plan->emulatorIds != NULL) {
            if ((--plan->emulatorIds->__rc) <= 0) {
                btrc_Vector_string_free(plan->emulatorIds);
            }
        }
        (plan->emulatorIds = SemuCompilerParser_emulatorIds(project));
        (SemuCompilerParser_emulatorIds(project)->__rc++);
    }
    return plan;
    if (plan != NULL) {
        if ((--plan->__rc) <= 0) {
            SemuBuildPlan_destroy(plan);
        }
    }
}

void SemuCompilerChecker_check(SemuBuildPlan* plan) {
    if (!FileSystem_isFile(SemuOwnedPaths_targetFile(plan->project, plan->target))) {
        char* __fstr_154_arg0 = plan->target;
        int __fstr_154_len = snprintf(NULL, 0, "target definition not found: config/targets/%s.json", __fstr_154_arg0);
        char* __fstr_154_buf = __btrc_str_track(((char*)malloc((__fstr_154_len + 1))));
        snprintf(__fstr_154_buf, (__fstr_154_len + 1), "target definition not found: config/targets/%s.json", __fstr_154_arg0);
        SemuBuildPlan_addError(plan, __fstr_154_buf);
    }
    if (strcmp(plan->mode, "emulator") == 0) {
        if (((int)strlen(plan->emulator)) == 0) {
            SemuBuildPlan_addError(plan, "build emulator requires an emulator id");
        } else if ((!(strcmp(plan->emulator, "all") == 0)) && (!FileSystem_isFile(SemuOwnedPaths_emulatorFile(plan->project, plan->emulator, "emulator.json")))) {
            char* __fstr_156_arg0 = plan->emulator;
            int __fstr_156_len = snprintf(NULL, 0, "emulator definition not found: config/emulators/%s/emulator.json", __fstr_156_arg0);
            char* __fstr_156_buf = __btrc_str_track(((char*)malloc((__fstr_156_len + 1))));
            snprintf(__fstr_156_buf, (__fstr_156_len + 1), "emulator definition not found: config/emulators/%s/emulator.json", __fstr_156_arg0);
            SemuBuildPlan_addError(plan, __fstr_156_buf);
        }
    }
    if (plan->systemIds->len == 0) {
        SemuBuildPlan_addError(plan, "no system definitions found under config/systems/");
    }
    if (plan->emulatorIds->len == 0) {
        SemuBuildPlan_addError(plan, "no emulator definitions found under config/emulators/");
    }
    int __n_158 = btrc_Vector_string_iterLen(plan->systemIds);
    for (int __i_157 = 0; (__i_157 < __n_158); (__i_157++)) {
        char* system = btrc_Vector_string_iterGet(plan->systemIds, __i_157);
        SemuSystemDefinition* definition = SemuCompilerParser_system(plan->project, system);
        if (!SemuDefinition_exists(definition->source)) {
            char* __fstr_160_arg0 = system;
            int __fstr_160_len = snprintf(NULL, 0, "system definition disappeared while compiling: %s", __fstr_160_arg0);
            char* __fstr_160_buf = __btrc_str_track(((char*)malloc((__fstr_160_len + 1))));
            snprintf(__fstr_160_buf, (__fstr_160_len + 1), "system definition disappeared while compiling: %s", __fstr_160_arg0);
            SemuBuildPlan_addError(plan, __fstr_160_buf);
            continue;
        }
        if (((int)strlen(definition->primaryEmulator)) == 0) {
            char* __fstr_162_arg0 = system;
            int __fstr_162_len = snprintf(NULL, 0, "system %s has no primary route emulator", __fstr_162_arg0);
            char* __fstr_162_buf = __btrc_str_track(((char*)malloc((__fstr_162_len + 1))));
            snprintf(__fstr_162_buf, (__fstr_162_len + 1), "system %s has no primary route emulator", __fstr_162_arg0);
            SemuBuildPlan_addWarning(plan, __fstr_162_buf);
        }
        if (definition->extensions->len == 0) {
            char* __fstr_164_arg0 = system;
            int __fstr_164_len = snprintf(NULL, 0, "system %s has no ROM extensions", __fstr_164_arg0);
            char* __fstr_164_buf = __btrc_str_track(((char*)malloc((__fstr_164_len + 1))));
            snprintf(__fstr_164_buf, (__fstr_164_len + 1), "system %s has no ROM extensions", __fstr_164_arg0);
            SemuBuildPlan_addWarning(plan, __fstr_164_buf);
        }
    }
}

void SemuCompilerProjectWriter_init(SemuCompilerProjectWriter* self) {
    self->__rc = 1;
}

void SemuCompilerProjectWriter_initializeHome(char* project) {
    ensureDir(project);
    ensureDir(SemuOwnedPaths_home(project));
    ensureDir(SemuOwnedPaths_generated(project));
    ensureDir(SemuOwnedPaths_state(project));
    ensureDir(SemuOwnedPaths_cache(project));
    ensureDir(SemuOwnedPaths_overrides(project));
    ensureDir(joinPath(SemuOwnedPaths_overrides(project), "systems"));
    ensureDir(joinPath(SemuOwnedPaths_overrides(project), "emulators"));
    ensureDir(joinPath(SemuOwnedPaths_overrides(project), "input"));
    ensureDir(joinPath(SemuOwnedPaths_overrides(project), "rendering"));
    SemuCompilerProjectWriter_ensureUserConfig(project);
}

void SemuCompilerProjectWriter_ensureUserConfig(char* project) {
    char* path = SemuOwnedPaths_semuConfig(project);
    if (FileSystem_isFile(path)) {
        return;
    }
    char* source = SemuOwnedPaths_settingsDefinitionFile(project, "semu-settings.json");
    char* text = (FileSystem_isFile(source) ? FileSystem_readText(source) : "{\n  \"schema_version\": 1\n}\n");
    ensureDir(PathTools_dirname(path));
    FileSystem_writeText(path, semuPrettyJsonObject(text));
}

char* SemuCompilerProjectWriter_launcherBin(char* project) {
    char* configured = Environment_get("SEMU_LAUNCHER_BIN", "");
    if (!__btrc_isEmpty(configured)) {
        return configured;
    }
    return SemuCompilerProjectWriter_stableLauncherBin(project);
}

char* SemuCompilerProjectWriter_stableLauncherBin(char* project) {
    return SemuOwnedPaths_generatedFile(project, "bin");
}

char* SemuCompilerProjectWriter_stableLauncherPreamble(void) {
    return "#!/usr/bin/env sh\nset -eu\nhere=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd -P)\"\nif [ -z \"${SEMU_PROJECT_DIR:-}\" ]; then\n  project=\"$(CDPATH= cd -- \"$here/../../..\" && pwd -P)\"\n  export SEMU_PROJECT_DIR=\"$project\"\nfi\nactive=\"${SEMU_ACTIVE_LAUNCHER_BIN:-}\"\nrunner=\"\"\nusable_runner() {\n  [ -n \"$1\" ] && [ -x \"$1\" ] || return 1\n  case \"$1\" in\n    *.AppImage|*.appimage) return 1 ;;\n  esac\n  return 0\n}\nfind_runner() {\n  for candidate in \
      \"${SEMU_CLI:-}\" \
      \"$here/semu\" \
      \"$here/semu-btrc\" \
      \"${SEMU_ASSET_ROOT:-}/result-full/bin/semu\" \
      \"$HOME/semu/result-full/bin/semu\" \
      \"${SEMU_ASSET_ROOT:-}/build/out/semu\" \
      \"$project/build/out/semu\" \
      \"$HOME/Applications/Semu/semu\" \
      \"$(command -v semu-btrc 2>/dev/null || true)\" \
      \"$(command -v semu 2>/dev/null || true)\"; do\n    usable_runner \"$candidate\" || continue\n    printf '%s\n' \"$candidate\"\n    return 0\n  done\n  return 1\n}\n";
}

char* SemuCompilerProjectWriter_stableEmulatorLauncher(char* emulator) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(SemuCompilerProjectWriter_stableLauncherPreamble(), "if [ -n \"$active\" ] && [ -x \"$active/semu-")), emulator)), "\" ]; then exec \"$active/semu-")), emulator)), "\" \"$@\"; fi\n")), "runner=\"$(find_runner || true)\"\n")), "if [ -n \"$runner\" ]; then exec \"$runner\" semu-")), emulator)), " \"$@\"; fi\n")), "echo \"Semu launcher: generated semu CLI not found for ")), emulator)), "\" >&2\n")), "exit 127\n"));
}

char* SemuCompilerProjectWriter_stableSettingsLauncher(void) {
    return SemuEsDeGenerator_settingsLauncherScript();
}

void SemuCompilerProjectWriter_writeStableLaunchers(SemuBuildPlan* plan) {
    char* bin = SemuCompilerProjectWriter_stableLauncherBin(plan->project);
    ensureDir(bin);
    int __n_166 = btrc_Vector_string_iterLen(plan->emulatorIds);
    for (int __i_165 = 0; (__i_165 < __n_166); (__i_165++)) {
        char* emulator = btrc_Vector_string_iterGet(plan->emulatorIds, __i_165);
        FileSystem_writeText(joinPath(bin, __btrc_str_track(__btrc_strcat("semu-", emulator))), SemuCompilerProjectWriter_stableEmulatorLauncher(emulator));
        FileSystem_chmod(joinPath(bin, __btrc_str_track(__btrc_strcat("semu-", emulator))), 493);
    }
    FileSystem_writeText(joinPath(bin, "semu-settings"), SemuCompilerProjectWriter_stableSettingsLauncher());
    FileSystem_chmod(joinPath(bin, "semu-settings"), 493);
}

void SemuCompilerProjectWriter_writeTarget(SemuBuildPlan* plan) {
    SemuCompilerProjectWriter_initializeHome(plan->project);
    SemuCompilerProjectWriter_writeEmulatorState(plan);
    SemuCompilerProjectWriter_writeRenderAssets(plan);
    SemuCompilerProjectWriter_writeStableLaunchers(plan);
    SemuCompilerProjectWriter_writeEsDe(plan);
    SemuCompilerProjectWriter_writeSteamInput(plan);
    SemuCompilerProjectWriter_writeLauncherInventory(plan);
}

void SemuCompilerProjectWriter_writeRenderAssets(SemuBuildPlan* plan) {
    SemuCompilerProjectWriter_copyRenderAssetDirectory(plan->project, "reshade");
    SemuCompilerProjectWriter_copyRenderAssetDirectory(plan->project, "slang");
    SemuOwnedPaths_writeGenerated(plan->project, "assets/rendering/host-env", SemuCompilerProjectWriter_renderHostEnv(plan));
    SemuCompilerProjectWriter_writeRenderHookConfigs(plan);
}

void SemuCompilerProjectWriter_copyRenderAssetDirectory(char* project, char* name) {
    char* source = joinPath(SemuOwnedPaths_sourceRoot(project), __btrc_str_track(__btrc_strcat("assets/rendering/", name)));
    char* target = SemuOwnedPaths_generatedFile(project, __btrc_str_track(__btrc_strcat("assets/rendering/", name)));
    ensureDir(target);
    if (FileSystem_isDir(source)) {
        int __n_168 = btrc_Vector_string_iterLen(FileSystem_listDir(source));
        for (int __i_167 = 0; (__i_167 < __n_168); (__i_167++)) {
            char* fileName = btrc_Vector_string_iterGet(FileSystem_listDir(source), __i_167);
            char* sourcePath = joinPath(source, fileName);
            if (FileSystem_isFile(sourcePath)) {
                FileSystem_writeText(joinPath(target, fileName), FileSystem_readText(sourcePath));
            }
        }
    }
}

void SemuCompilerProjectWriter_writeRenderHookConfigs(SemuBuildPlan* plan) {
    int __n_170 = btrc_Vector_string_iterLen(plan->emulatorIds);
    for (int __i_169 = 0; (__i_169 < __n_170); (__i_169++)) {
        char* emulator = btrc_Vector_string_iterGet(plan->emulatorIds, __i_169);
        SemuOwnedPaths_writeGenerated(plan->project, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("assets/rendering/hooks/", emulator)), ".json")), SemuCompilerProjectWriter_renderHookConfig(plan, emulator));
    }
}

char* SemuCompilerProjectWriter_renderHookConfig(SemuBuildPlan* plan, char* emulator) {
    SemuEmulatorRenderingDefinition* emulatorRendering = SemuEmulatorRenderingDefinition_load(plan->project, emulator);
    char* implementationStatus = SemuEmulatorRenderingDefinition_field(emulatorRendering, "implementation_status", "unknown");
    char* packageStatus = SemuEmulatorRenderingDefinition_field(emulatorRendering, "source_package_status", "unknown");
    char* scope = SemuEmulatorRenderingDefinition_field(emulatorRendering, "hook_scope", SemuEmulatorRenderingDefinition_field(emulatorRendering, "planned_hook_scope", "game_framebuffer"));
    char* hookPoint = SemuEmulatorRenderingDefinition_field(emulatorRendering, "hook_point", SemuEmulatorRenderingDefinition_field(emulatorRendering, "planned_hook_point", ""));
    bool requiresDeckProof = SemuEmulatorRenderingDefinition_boolField(emulatorRendering, "requires_deck_proof", false);
    bool shaders = SemuCompilerProjectWriter_settingBool(plan->project, "visual_crt_shaders", true);
    bool bezels = SemuCompilerProjectWriter_settingBool(plan->project, "visual_bezels", true);
    char* scale = (SemuCompilerProjectWriter_settingBool(plan->project, "visual_integer_scaling", true) ? "integer" : "stretch");
    btrc_Vector_string* servedSystems = SemuCompilerParser_emulator(plan->project, emulator)->servedSystems;
    btrc_Vector_string* hookSystems = btrc_Vector_string_new();
    int __n_172 = btrc_Vector_string_iterLen(plan->systemIds);
    for (int __i_171 = 0; (__i_171 < __n_172); (__i_171++)) {
        char* systemId = btrc_Vector_string_iterGet(plan->systemIds, __i_171);
        if (SemuCompilerProjectWriter_stringVectorContains(servedSystems, systemId)) {
            btrc_Vector_string_push(hookSystems, systemId);
        }
    }
    btrc_Vector_string* __list_173 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_173, "{");
    btrc_Vector_string_push(__list_173, "  \"schema_version\": 1,");
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"emulator\": ", semuPrettyJsonString(emulator))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"target\": ", semuPrettyJsonString(plan->target))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"implementation_status\": ", semuPrettyJsonString(implementationStatus))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"source_package_status\": ", semuPrettyJsonString(packageStatus))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"requires_deck_proof\": ", (requiresDeckProof ? "true" : "false"))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"scope\": ", semuPrettyJsonString(scope))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"hook_point\": ", semuPrettyJsonString(hookPoint))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"shaders_enabled\": ", (shaders ? "true" : "false"))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"bezels_enabled\": ", (bezels ? "true" : "false"))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"scale\": ", semuPrettyJsonString(scale))), ",")));
    btrc_Vector_string_push(__list_173, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  \"generated_root\": ", semuPrettyJsonString(SemuOwnedPaths_generatedFile(plan->project, "assets/rendering")))), ",")));
    btrc_Vector_string_push(__list_173, "  \"systems\": [");
    btrc_Vector_string* lines = __list_173;
    int index = 0;
    int __n_175 = btrc_Vector_string_iterLen(hookSystems);
    for (int __i_174 = 0; (__i_174 < __n_175); (__i_174++)) {
        char* systemId = btrc_Vector_string_iterGet(hookSystems, __i_174);
        SemuRenderingDefinition* rendering = SemuRenderingDefinition_load(plan->project, systemId);
        char* row = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    {", "\"id\": ")), semuPrettyJsonString(systemId))), ", \"shader_effect_file\": ")), semuPrettyJsonString(SemuRenderingDefinition_shaderEffectFile(rendering)))), ", \"bezel_effect_file\": ")), semuPrettyJsonString(SemuRenderingDefinition_bezelEffectFile(rendering)))), ", \"shader_enabled_by_default\": ")), SemuRenderingDefinition_rendererField(rendering, "shader", "enabled_by_default", "false"))), ", \"bezel_enabled_by_default\": ")), SemuRenderingDefinition_rendererField(rendering, "bezel", "enabled_by_default", "false"))), ", \"native_size\": ")), semuPrettyJsonString(SemuRenderingDefinition_nativeSize(rendering)))), ", \"content_aspect\": ")), semuPrettyJsonString(SemuRenderingDefinition_contentAspect(rendering)))), ", \"layout\": ")), semuPrettyJsonString(SemuRenderingDefinition_layoutKind(rendering)))), "}"));
        (index++);
        if (index < hookSystems->len) {
            (row = __btrc_str_track(__btrc_strcat(row, ",")));
        }
        btrc_Vector_string_push(lines, row);
    }
    btrc_Vector_string_push(lines, "  ]");
    btrc_Vector_string_push(lines, "}");
    return __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
}

bool SemuCompilerProjectWriter_stringVectorContains(btrc_Vector_string* values, char* expected) {
    int __n_177 = btrc_Vector_string_iterLen(values);
    for (int __i_176 = 0; (__i_176 < __n_177); (__i_176++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_176);
        if (strcmp(value, expected) == 0) {
            return true;
        }
    }
    return false;
}

char* SemuCompilerProjectWriter_renderHostEnv(SemuBuildPlan* plan) {
    bool shaders = SemuCompilerProjectWriter_settingBool(plan->project, "visual_crt_shaders", true);
    bool bezels = SemuCompilerProjectWriter_settingBool(plan->project, "visual_bezels", true);
    char* scale = (SemuCompilerProjectWriter_settingBool(plan->project, "visual_integer_scaling", true) ? "integer" : "stretch");
    btrc_Vector_string* __list_178 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_178, __btrc_str_track(__btrc_strcat("SEMU_HOST_RENDER_SHADERS=", (shaders ? "1" : "0"))));
    btrc_Vector_string_push(__list_178, __btrc_str_track(__btrc_strcat("SEMU_HOST_RENDER_BEZELS=", (bezels ? "1" : "0"))));
    btrc_Vector_string_push(__list_178, __btrc_str_track(__btrc_strcat("SEMU_HOST_RENDER_SCALE=", ShellWords_quote(scale))));
    btrc_Vector_string_push(__list_178, "semu_host_render_effect_name() {");
    btrc_Vector_string_push(__list_178, "  case \"$1:$2\" in");
    btrc_Vector_string* lines = __list_178;
    int __n_180 = btrc_Vector_string_iterLen(plan->systemIds);
    for (int __i_179 = 0; (__i_179 < __n_180); (__i_179++)) {
        char* systemId = btrc_Vector_string_iterGet(plan->systemIds, __i_179);
        SemuRenderingDefinition* rendering = SemuRenderingDefinition_load(plan->project, systemId);
        char* effect = SemuRenderingDefinition_bezelEffectFile(rendering);
        char* shader = SemuRenderingDefinition_shaderEffectFile(rendering);
        if (!__btrc_isEmpty(effect)) {
            btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    ", ShellWords_quote(__btrc_str_track(__btrc_strcat(systemId, ":bezel"))))), ") printf '%s\\n' ")), ShellWords_quote(effect))), " ;;")));
        }
        if (!__btrc_isEmpty(shader)) {
            btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    ", ShellWords_quote(__btrc_str_track(__btrc_strcat(systemId, ":shader"))))), ") printf '%s\\n' ")), ShellWords_quote(shader))), " ;;")));
        }
    }
    btrc_Vector_string_push(lines, "    *) printf '%s\\n' '' ;;");
    btrc_Vector_string_push(lines, "  esac");
    btrc_Vector_string_push(lines, "}");
    return __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
}

bool SemuCompilerProjectWriter_settingBool(char* project, char* key, bool fallback) {
    char* path = SemuOwnedPaths_semuConfig(project);
    if (!FileSystem_isFile(path)) {
        return fallback;
    }
    JsonObject* object = JsonObject_parse(FileSystem_readText(path));
    return JsonObject_getBool(object, key, fallback);
}

void SemuCompilerProjectWriter_writeEmulatorState(SemuBuildPlan* plan) {
    int __n_182 = btrc_Vector_string_iterLen(plan->emulatorIds);
    for (int __i_181 = 0; (__i_181 < __n_182); (__i_181++)) {
        char* emulator = btrc_Vector_string_iterGet(plan->emulatorIds, __i_181);
        SemuEmulatorStateGenerator_write(plan->project, emulator);
    }
}

void SemuCompilerProjectWriter_writeEsDe(SemuBuildPlan* plan) {
    btrc_Vector_SemuEsDeSystem* systems = btrc_Vector_SemuEsDeSystem_new();
    int __n_184 = btrc_Vector_string_iterLen(plan->systemIds);
    for (int __i_183 = 0; (__i_183 < __n_184); (__i_183++)) {
        char* systemId = btrc_Vector_string_iterGet(plan->systemIds, __i_183);
        SemuDefinition* definition = SemuCompilerParser_system(plan->project, systemId)->source;
        btrc_Vector_SemuEsDeCommand* commands = SemuCompilerProjectWriter_commandsForSystem(plan->project, definition->raw);
        btrc_Vector_SemuEsDeSystem_push(systems, SemuEsDeGenerator_system(systemId, SemuDefinition_stringField(definition, "fullname", systemId), SemuDefinition_stringField(definition, "platform", systemId), SemuDefinition_stringField(definition, "theme", systemId), SemuDefinition_stringField(definition, "rom_dir", systemId), SemuDefinition_stringArray(definition, "extensions"), commands));
    }
    btrc_Vector_SemuEsDeFindRule* rules = btrc_Vector_SemuEsDeFindRule_new();
    int __n_186 = btrc_Vector_string_iterLen(plan->emulatorIds);
    for (int __i_185 = 0; (__i_185 < __n_186); (__i_185++)) {
        char* emulatorId = btrc_Vector_string_iterGet(plan->emulatorIds, __i_185);
        btrc_Vector_SemuEsDeFindRule_push(rules, SemuEsDeGenerator_emulatorRule(__btrc_str_track(__btrc_toUpper(emulatorId)), joinPath(SemuCompilerProjectWriter_launcherBin(plan->project), __btrc_str_track(__btrc_strcat("semu-", emulatorId)))));
    }
    btrc_Vector_string* __list_188 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_188, Environment_get("SEMU_RETROARCH_CORE_DIR", "/usr/lib/libretro"));
    btrc_Vector_string_push(__list_188, "/usr/lib/x86_64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_188, "/usr/lib/aarch64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_188, "/usr/lib/libretro");
    btrc_Vector_SemuEsDeFindRule_push(rules, SemuEsDeGenerator_coreRule("RETROARCH", __list_188));
    char* customSystems = joinPath(plan->project, "ES-DE/custom_systems");
    ensureDir(customSystems);
    FileSystem_writeText(joinPath(customSystems, "es_systems.xml"), SemuEsDeGenerator_systemsXml(systems));
    FileSystem_writeText(joinPath(customSystems, "es_find_rules.xml"), SemuEsDeGenerator_findRulesXml(rules));
    FileSystem_writeText(joinPath(plan->project, "ES-DE/es_settings.xml"), SemuEsDeGenerator_settingsXml(SemuOwnedPaths_romsRoot(plan->project), SemuOwnedPaths_mediaRoot(plan->project), SemuOwnedPaths_themeRoot(plan->project), "gb", "system", "linear"));
    char* userHome = Environment_get("HOME", "");
    if (!__btrc_isEmpty(userHome)) {
        char* userCustom = joinPath(userHome, "ES-DE/custom_systems");
        char* userSettings = joinPath(userHome, "ES-DE/settings");
        ensureDir(userCustom);
        ensureDir(userSettings);
        FileSystem_writeText(joinPath(userCustom, "es_systems.xml"), SemuEsDeGenerator_systemsXml(systems));
        FileSystem_writeText(joinPath(userCustom, "es_find_rules.xml"), SemuEsDeGenerator_findRulesXml(rules));
        FileSystem_writeText(joinPath(userSettings, "es_settings.xml"), SemuEsDeGenerator_settingsXml(SemuOwnedPaths_romsRoot(plan->project), SemuOwnedPaths_mediaRoot(plan->project), SemuOwnedPaths_themeRoot(plan->project), "gb", "system", "linear"));
    }
}

btrc_Vector_SemuEsDeCommand* SemuCompilerProjectWriter_commandsForSystem(char* project, char* systemRaw) {
    btrc_Vector_SemuEsDeCommand* commands = btrc_Vector_SemuEsDeCommand_new();
    char* routeRoot = JsonText_objectField(systemRaw, "routes");
    btrc_Vector_string* routes = JsonText_objectArray(routeRoot, "linux");
    if (routes->len == 0) {
        (routes = JsonText_objectArray(routeRoot, "macos"));
    }
    int __n_190 = btrc_Vector_string_iterLen(routes);
    for (int __i_189 = 0; (__i_189 < __n_190); (__i_189++)) {
        char* route = btrc_Vector_string_iterGet(routes, __i_189);
        char* emulator = JsonText_field(route, "emulator", "");
        if (__btrc_isEmpty(emulator)) {
            continue;
        }
        char* core = JsonText_field(route, "core", "");
        char* label = SemuCompilerProjectWriter_emulatorDisplayName(project, emulator);
        char* systemId = JsonText_field(systemRaw, "id", "");
        char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("env SEMU_SYSTEM=", systemId)), " %EMULATOR_")), __btrc_str_track(__btrc_toUpper(emulator)))), "%"));
        if ((strcmp(emulator, "retroarch") == 0) && (!__btrc_isEmpty(core))) {
            (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, " -f -L %CORE_RETROARCH%/")), core)));
        }
        (command = __btrc_str_track(__btrc_strcat(command, " %ROM%")));
        btrc_Vector_SemuEsDeCommand_push(commands, SemuEsDeGenerator_command(label, command));
    }
    return commands;
}

char* SemuCompilerProjectWriter_emulatorDisplayName(char* project, char* emulator) {
    char* path = SemuOwnedPaths_emulatorFile(project, emulator, "emulator.json");
    if (!FileSystem_isFile(path)) {
        return emulator;
    }
    return JsonText_field(FileSystem_readText(path), "name", emulator);
}

void SemuCompilerProjectWriter_writeSteamInput(SemuBuildPlan* plan) {
    btrc_Vector_string* __list_192 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_192, "save");
    btrc_Vector_string_push(__list_192, "load");
    btrc_Vector_string_push(__list_192, "quit");
    btrc_Vector_string_push(__list_192, "menu");
    btrc_Vector_string_push(__list_192, "screenshot");
    btrc_Vector_string_push(__list_192, "escape");
    SemuOwnedPaths_writeGenerated(plan->project, "input/steam-input/selection.json", SemuSteamInputGenerator_selectionJson(plan->target, "steam-deck", "neptune-full", __list_192));
}

void SemuCompilerProjectWriter_writeLauncherInventory(SemuBuildPlan* plan) {
    btrc_Vector_SemuAppImageContent* contents = btrc_Vector_SemuAppImageContent_new();
    btrc_Vector_SemuAppImageContent_push(contents, SemuAppImageGenerator_content("usr/bin/semu-btrc", SemuAppImageGenerator_btrcShim(), true));
    btrc_Vector_SemuAppImageContent_push(contents, SemuAppImageGenerator_content("usr/bin/semu-render", "", true));
    btrc_Vector_SemuAppImageContent_push(contents, SemuAppImageGenerator_content("semu.desktop", SemuAppImageGenerator_desktopEntry("Semu", "Deterministic emulation environment", "semu"), false));
    int __n_194 = btrc_Vector_string_iterLen(plan->emulatorIds);
    for (int __i_193 = 0; (__i_193 < __n_194); (__i_193++)) {
        char* emulator = btrc_Vector_string_iterGet(plan->emulatorIds, __i_193);
        btrc_Vector_SemuAppImageContent_push(contents, SemuAppImageGenerator_content(__btrc_str_track(__btrc_strcat("usr/bin/semu-", emulator)), SemuAppImageGenerator_emulatorShim(emulator), true));
    }
    SemuOwnedPaths_writeGenerated(plan->project, "packaging/appimage/manifest.json", SemuAppImageGenerator_manifestJson(contents));
}

int SemuCompilerGenerator_generate(SemuBuildPlan* plan) {
    char* __fstr_197_arg0 = plan->mode;
    char* __fstr_197_arg1 = plan->target;
    char* __fstr_197_arg2 = plan->project;
    int __fstr_197_len = snprintf(NULL, 0, "semu build %s target=%s project=%s", __fstr_197_arg0, __fstr_197_arg1, __fstr_197_arg2);
    char* __fstr_197_buf = __btrc_str_track(((char*)malloc((__fstr_197_len + 1))));
    snprintf(__fstr_197_buf, (__fstr_197_len + 1), "semu build %s target=%s project=%s", __fstr_197_arg0, __fstr_197_arg1, __fstr_197_arg2);
    printf("%s\n", __fstr_197_buf);
    printf("%s\n", __btrc_str_track(__btrc_strcat("ownership=", SemuMergePolicy_precedenceText())));
    char* __fstr_200_arg0 = Strings_fromInt(plan->systemIds->len);
    char* __fstr_200_arg1 = Strings_fromInt(plan->emulatorIds->len);
    int __fstr_200_len = snprintf(NULL, 0, "systems=%s emulators=%s", __fstr_200_arg0, __fstr_200_arg1);
    char* __fstr_200_buf = __btrc_str_track(((char*)malloc((__fstr_200_len + 1))));
    snprintf(__fstr_200_buf, (__fstr_200_len + 1), "systems=%s emulators=%s", __fstr_200_arg0, __fstr_200_arg1);
    printf("%s\n", __fstr_200_buf);
    if (strcmp(plan->mode, "emulator") == 0) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("emulator=", plan->emulator)));
        if (strcmp(plan->emulator, "all") == 0) {
            int __n_202 = btrc_Vector_string_iterLen(plan->emulatorIds);
            for (int __i_201 = 0; (__i_201 < __n_202); (__i_201++)) {
                char* emulator = btrc_Vector_string_iterGet(plan->emulatorIds, __i_201);
                printf("%s\n", SemuEmulatorDefinitionGenerator_plan(plan->project, plan->target, emulator));
            }
        } else {
            printf("%s\n", SemuEmulatorDefinitionGenerator_plan(plan->project, plan->target, plan->emulator));
        }
    } else if (strcmp(plan->mode, "configs") == 0) {
        SemuCompilerProjectWriter_writeTarget(plan);
        printf("%s\n", "OK configs compiled from Semu-owned state");
    } else {
        SemuCompilerProjectWriter_writeTarget(plan);
        printf("%s\n", SemuEsDeGenerator_plan(plan->project, plan->target));
        printf("%s\n", SemuSteamInputGenerator_plan(plan->project, plan->target));
        printf("%s\n", SemuAppImageGenerator_plan(plan->project, plan->target));
    }
    int __n_204 = btrc_Vector_string_iterLen(plan->warnings);
    for (int __i_203 = 0; (__i_203 < __n_204); (__i_203++)) {
        char* warning = btrc_Vector_string_iterGet(plan->warnings, __i_203);
        printf("%s\n", __btrc_str_track(__btrc_strcat("warning: ", warning)));
    }
    int __n_206 = btrc_Vector_string_iterLen(plan->outputs);
    for (int __i_205 = 0; (__i_205 < __n_206); (__i_205++)) {
        char* output = btrc_Vector_string_iterGet(plan->outputs, __i_205);
        printf("%s\n", output);
    }
    return 0;
}

int SemuCompilerGenerator_fail(SemuBuildPlan* plan) {
    int __n_208 = btrc_Vector_string_iterLen(plan->errors);
    for (int __i_207 = 0; (__i_207 < __n_208); (__i_207++)) {
        char* error = btrc_Vector_string_iterGet(plan->errors, __i_207);
        printf("%s\n", __btrc_str_track(__btrc_strcat("error 0:0 ", error)));
    }
    int __n_210 = btrc_Vector_string_iterLen(plan->warnings);
    for (int __i_209 = 0; (__i_209 < __n_210); (__i_209++)) {
        char* warning = btrc_Vector_string_iterGet(plan->warnings, __i_209);
        printf("%s\n", __btrc_str_track(__btrc_strcat("warning: ", warning)));
    }
    return 1;
}

char* semuPrettyJsonString(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", JsonObject_escape(value))), "\""));
}

char* semuPrettyJsonObject(char* raw) {
    JsonObject* object = JsonObject_parse(raw);
    btrc_Vector_string* __list_211 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_211, "{");
    btrc_Vector_string* lines = __list_211;
    int index = 0;
    int __n_213 = btrc_Map_string_string_iterLen(object->values);
    for (int __i_212 = 0; (__i_212 < __n_213); (__i_212++)) {
        char* key = btrc_Map_string_string_iterGet(object->values, __i_212);
        char* value = btrc_Map_string_string_iterValueAt(object->values, __i_212);
        char* rendered = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  ", semuPrettyJsonString(key))), ": "));
        if (btrc_Map_string_bool_getOrDefault(object->quoted, key, true)) {
            (rendered = __btrc_str_track(__btrc_strcat(rendered, semuPrettyJsonString(value))));
        } else {
            (rendered = __btrc_str_track(__btrc_strcat(rendered, value)));
        }
        (index++);
        if (index < object->values->len) {
            (rendered = __btrc_str_track(__btrc_strcat(rendered, ",")));
        }
        btrc_Vector_string_push(lines, rendered);
    }
    btrc_Vector_string_push(lines, "}");
    return __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(lines, "\n"), "\n"));
}

int semuBuildCommand(CliArgs* args, char* project) {
    SemuBuildPlan* plan = SemuCompilerResolver_resolve(args, project);
    SemuCompilerChecker_check(plan);
    if (!SemuBuildPlan_ok(plan)) {
        return SemuCompilerGenerator_fail(plan);
    }
    return SemuCompilerGenerator_generate(plan);
}

int semuVerifyCommand(CliArgs* args, char* project) {
    SemuBuildPlan* plan = SemuCompilerResolver_resolve(args, project);
    SemuCompilerChecker_check(plan);
    if (!SemuBuildPlan_ok(plan)) {
        return SemuCompilerGenerator_fail(plan);
    }
    char* __fstr_216_arg0 = plan->mode;
    char* __fstr_216_arg1 = plan->target;
    char* __fstr_216_arg2 = plan->project;
    int __fstr_216_len = snprintf(NULL, 0, "semu verify %s target=%s project=%s", __fstr_216_arg0, __fstr_216_arg1, __fstr_216_arg2);
    char* __fstr_216_buf = __btrc_str_track(((char*)malloc((__fstr_216_len + 1))));
    snprintf(__fstr_216_buf, (__fstr_216_len + 1), "semu verify %s target=%s project=%s", __fstr_216_arg0, __fstr_216_arg1, __fstr_216_arg2);
    printf("%s\n", __fstr_216_buf);
    char* __fstr_219_arg0 = Strings_fromInt(plan->systemIds->len);
    char* __fstr_219_arg1 = Strings_fromInt(plan->emulatorIds->len);
    int __fstr_219_len = snprintf(NULL, 0, "systems=%s emulators=%s", __fstr_219_arg0, __fstr_219_arg1);
    char* __fstr_219_buf = __btrc_str_track(((char*)malloc((__fstr_219_len + 1))));
    snprintf(__fstr_219_buf, (__fstr_219_len + 1), "systems=%s emulators=%s", __fstr_219_arg0, __fstr_219_arg1);
    printf("%s\n", __fstr_219_buf);
    int __n_221 = btrc_Vector_string_iterLen(plan->warnings);
    for (int __i_220 = 0; (__i_220 < __n_221); (__i_220++)) {
        char* warning = btrc_Vector_string_iterGet(plan->warnings, __i_220);
        printf("%s\n", __btrc_str_track(__btrc_strcat("warning: ", warning)));
    }
    printf("%s\n", "OK compiler definitions");
    return 0;
}

int SemuCli_run(CliArgs* args) {
    char* command = CliArgs_command(args);
    char* project = CliArgs_valueAfter(args, "--project", Environment_get("SEMU_PROJECT_DIR", "."));
    char* programLauncher = SemuCli_launcherNameFromProgram(args->program);
    if (!__btrc_isEmpty(programLauncher)) {
        return SemuCli_launcher(args, project, programLauncher, 0);
    }
    if (strcmp(command, "build") == 0) {
        return semuBuildCommand(args, project);
    }
    if (strcmp(command, "verify") == 0) {
        return semuVerifyCommand(args, project);
    }
    if (strcmp(command, "config") == 0) {
        return SemuCli_config(args, project);
    }
    if (strcmp(command, "apprun") == 0) {
        return SemuCli_apprun(args, project);
    }
    if (strcmp(command, "settings") == 0) {
        return SemuCli_settings(args, project);
    }
    if (strcmp(command, "assets") == 0) {
        return SemuCli_assets(args, project);
    }
    if (strcmp(command, "keymap") == 0) {
        return SemuCli_keymap(args, project);
    }
    if (strcmp(command, "sync") == 0) {
        return SemuCli_sync(args, project);
    }
    if (strcmp(command, "launcher") == 0) {
        if ((CliArgs_count(args) > 1) && (strcmp(CliArgs_get(args, 1), "routed") == 0)) {
            return SemuCli_launcherRouted(args, project);
        }
        char* emulator = ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "");
        return SemuCli_launcher(args, project, emulator, 2);
    }
    if (strcmp(command, "doctor") == 0) {
        return SemuCli_doctor(project);
    }
    if (strcmp(command, "bootstrap") == 0) {
        return SemuCli_bootstrap(project);
    }
    if (strcmp(command, "deck") == 0) {
        return SemuCli_deck(project);
    }
    if (strcmp(command, "e2e") == 0) {
        return SemuCli_e2e(args, project);
    }
    if ((strcmp(command, "manifest") == 0) || __btrc_isEmpty(command)) {
        return SemuCli_manifest(project);
    }
    SemuCli_usage();
    return 1;
}

char* SemuCli_launcherNameFromProgram(char* program) {
    char* base = PathTools_basename(program);
    if (!__btrc_startsWith(base, "semu-")) {
        return "";
    }
    if ((((strcmp(base, "semu") == 0) || (strcmp(base, "semu-btrc") == 0)) || (strcmp(base, "semu-flatpak") == 0)) || (strcmp(base, "semu-settings") == 0)) {
        return "";
    }
    return Strings_replace(base, "semu-", "");
}

void SemuCli_usage(void) {
    printf("%s\n", "semu build target steam-deck --project PATH");
    printf("%s\n", "semu build configs --project PATH");
    printf("%s\n", "semu settings ui|get|put|toggle|apply");
    printf("%s\n", "semu assets get|put SYSTEM FIELD [VALUE]");
    printf("%s\n", "semu launcher EMULATOR [args...]");
}

SemuBuildPlan* SemuCli_configPlan(char* project, char* target) {
    SemuBuildPlan* plan = SemuBuildPlan_new(project, "configs", target, "");
    if (plan->systemIds != NULL) {
        if ((--plan->systemIds->__rc) <= 0) {
            btrc_Vector_string_free(plan->systemIds);
        }
    }
    (plan->systemIds = SemuCompilerParser_targetSystemIds(project, target));
    (SemuCompilerParser_targetSystemIds(project, target)->__rc++);
    if (plan->systemIds->len == 0) {
        if (plan->systemIds != NULL) {
            if ((--plan->systemIds->__rc) <= 0) {
                btrc_Vector_string_free(plan->systemIds);
            }
        }
        (plan->systemIds = SemuCompilerParser_systemIds(project));
        (SemuCompilerParser_systemIds(project)->__rc++);
    }
    if (plan->emulatorIds != NULL) {
        if ((--plan->emulatorIds->__rc) <= 0) {
            btrc_Vector_string_free(plan->emulatorIds);
        }
    }
    (plan->emulatorIds = SemuCompilerParser_targetEmulatorIds(project, target));
    (SemuCompilerParser_targetEmulatorIds(project, target)->__rc++);
    if (plan->emulatorIds->len == 0) {
        if (plan->emulatorIds != NULL) {
            if ((--plan->emulatorIds->__rc) <= 0) {
                btrc_Vector_string_free(plan->emulatorIds);
            }
        }
        (plan->emulatorIds = SemuCompilerParser_emulatorIds(project));
        (SemuCompilerParser_emulatorIds(project)->__rc++);
    }
    return plan;
    if (plan != NULL) {
        if ((--plan->__rc) <= 0) {
            SemuBuildPlan_destroy(plan);
        }
    }
}

int SemuCli_generateConfigs(char* project, char* target) {
    SemuBuildPlan* plan = SemuCli_configPlan(project, target);
    SemuCompilerChecker_check(plan);
    if (!SemuBuildPlan_ok(plan)) {
        return SemuCompilerGenerator_fail(plan);
    }
    SemuCompilerProjectWriter_writeTarget(plan);
    return 0;
}

int SemuCli_config(CliArgs* args, char* project) {
    char* sub = ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "");
    if (strcmp(sub, "env") == 0) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_HOME=", ShellWords_quote(SemuOwnedPaths_home(project)))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_PROJECT_DIR=", ShellWords_quote(project))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_ROMS_DIR=", ShellWords_quote(CliArgs_valueAfter(args, "--roms", SemuOwnedPaths_romsRoot(project))))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_BIOS_DIR=", ShellWords_quote(SemuOwnedPaths_biosRoot(project)))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_GENERATED_DIR=", ShellWords_quote(SemuOwnedPaths_generated(project)))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("SEMU_ESDE_SETTINGS_COMMAND=", ShellWords_quote(SemuOwnedPaths_generatedFile(project, "bin/semu-settings")))));
        return 0;
    }
    if ((strcmp(sub, "apply") == 0) || (strcmp(sub, "compile") == 0)) {
        return SemuCli_generateConfigs(project, CliArgs_valueAfter(args, "--target", "steam-deck"));
    }
    SemuCli_usage();
    return 1;
}

int SemuCli_apprun(CliArgs* args, char* project) {
    char* sub = ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "");
    if (strcmp(sub, "prepare") == 0) {
        int status = SemuCli_generateConfigs(project, CliArgs_valueAfter(args, "--target", "steam-deck"));
        if (status == 0) {
            printf("%s\n", "OK apprun prepare");
        }
        return status;
    }
    return SemuCli_config(args, project);
}

char* SemuCli_settingsPath(char* project) {
    SemuCompilerProjectWriter_initializeHome(project);
    return SemuOwnedPaths_semuConfig(project);
}

JsonObject* SemuCli_settingsObject(char* project) {
    return JsonObject_parse(FileSystem_readText(SemuCli_settingsPath(project)));
}

char* SemuCli_normalizedKey(char* key) {
    return Strings_replace(key, ".", "_");
}

void SemuCli_writeSettingsObject(char* project, JsonObject* object) {
    FileSystem_writeText(SemuCli_settingsPath(project), semuPrettyJsonObject(JsonObject_stringify(object)));
}

int SemuCli_settings(CliArgs* args, char* project) {
    char* sub = ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "ui");
    if (strcmp(sub, "ui") == 0) {
        return SemuCli_settingsUi(project);
    }
    if (strcmp(sub, "entry") == 0) {
        if (CliArgs_count(args) < 3) {
            printf("%s\n", "error 0:0 settings entry needs PATH");
            return 1;
        }
        return SemuCli_settingsAction(project, __btrc_str_track(__btrc_trim(FileSystem_readText(CliArgs_get(args, 2)))));
    }
    if (strcmp(sub, "get") == 0) {
        if (CliArgs_count(args) < 3) {
            printf("%s\n", "error 0:0 settings get needs KEY");
            return 1;
        }
        JsonObject* object = SemuCli_settingsObject(project);
        printf("%s\n", JsonObject_getString(object, SemuCli_normalizedKey(CliArgs_get(args, 2)), ""));
        return 0;
    }
    if (strcmp(sub, "put") == 0) {
        if (CliArgs_count(args) < 4) {
            printf("%s\n", "error 0:0 settings put needs KEY VALUE");
            return 1;
        }
        int status = SemuCli_settingsPut(project, CliArgs_get(args, 2), CliArgs_get(args, 3));
        if (status != 0) {
            return status;
        }
        if (SemuCli_shouldApply(args, 4)) {
            return SemuCli_generateConfigs(project, "steam-deck");
        }
        return 0;
    }
    if (strcmp(sub, "toggle") == 0) {
        if (CliArgs_count(args) < 3) {
            printf("%s\n", "error 0:0 settings toggle needs KEY");
            return 1;
        }
        int status = SemuCli_settingsToggle(project, CliArgs_get(args, 2));
        if (status != 0) {
            return status;
        }
        if (SemuCli_shouldApply(args, 3)) {
            return SemuCli_generateConfigs(project, "steam-deck");
        }
        return 0;
    }
    if ((strcmp(sub, "apply") == 0) || (strcmp(sub, "compile") == 0)) {
        return SemuCli_generateConfigs(project, "steam-deck");
    }
    if (strcmp(sub, "sync") == 0) {
        return SemuCli_syncAction(project, ((CliArgs_count(args) > 2) ? CliArgs_get(args, 2) : "status"));
    }
    if (strcmp(sub, "n3ds") == 0) {
        return SemuCli_n3ds(project, ((CliArgs_count(args) > 2) ? CliArgs_get(args, 2) : "status"));
    }
    return SemuCli_settingsAction(project, sub);
}

int SemuCli_settingsAction(char* project, char* action) {
    btrc_Vector_string* parts = Strings_split(action, " ");
    if (parts->len == 0) {
        printf("%s\n", "error 0:0 empty settings action");
        return 1;
    }
    char* verb = btrc_Vector_string_get(parts, 0);
    if (strcmp(verb, "ui") == 0) {
        return SemuCli_settingsUi(project);
    }
    if ((strcmp(verb, "apply") == 0) || (strcmp(verb, "compile") == 0)) {
        return SemuCli_generateConfigs(project, "steam-deck");
    }
    if ((strcmp(verb, "put") == 0) && (parts->len >= 3)) {
        int status = SemuCli_settingsPut(project, btrc_Vector_string_get(parts, 1), btrc_Vector_string_get(parts, 2));
        if (status != 0) {
            return status;
        }
        if ((parts->len > 3) && (strcmp(btrc_Vector_string_get(parts, 3), "apply") == 0)) {
            return SemuCli_generateConfigs(project, "steam-deck");
        }
        return 0;
    }
    if ((strcmp(verb, "toggle") == 0) && (parts->len >= 2)) {
        int status = SemuCli_settingsToggle(project, btrc_Vector_string_get(parts, 1));
        if (status != 0) {
            return status;
        }
        if ((parts->len > 2) && (strcmp(btrc_Vector_string_get(parts, 2), "apply") == 0)) {
            return SemuCli_generateConfigs(project, "steam-deck");
        }
        return 0;
    }
    if (strcmp(verb, "sync") == 0) {
        return SemuCli_syncAction(project, ((parts->len > 1) ? btrc_Vector_string_get(parts, 1) : "status"));
    }
    if (strcmp(verb, "n3ds") == 0) {
        return SemuCli_n3ds(project, ((parts->len > 1) ? btrc_Vector_string_get(parts, 1) : "status"));
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("error 0:0 unknown settings action: ", action)));
    return 1;
}

bool SemuCli_shouldApply(CliArgs* args, int start) {
    for (int i = start; (i < CliArgs_count(args)); (i++)) {
        char* arg = CliArgs_get(args, i);
        if ((strcmp(arg, "apply") == 0) || (strcmp(arg, "--apply") == 0)) {
            return true;
        }
    }
    return false;
}

int SemuCli_settingsPut(char* project, char* key, char* value) {
    JsonObject* object = SemuCli_settingsObject(project);
    char* normalized = SemuCli_normalizedKey(key);
    if ((strcmp(value, "true") == 0) || (strcmp(value, "false") == 0)) {
        JsonObject_setRaw(object, normalized, value);
    } else {
        JsonObject_setString(object, normalized, value);
    }
    SemuCli_writeSettingsObject(project, object);
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("OK ", normalized)), "=")), value)));
    return 0;
}

int SemuCli_settingsToggle(char* project, char* key) {
    JsonObject* object = SemuCli_settingsObject(project);
    char* normalized = SemuCli_normalizedKey(key);
    bool current = JsonObject_getBool(object, normalized, false);
    JsonObject_setBool(object, normalized, (!current));
    SemuCli_writeSettingsObject(project, object);
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("OK ", normalized)), "=")), ((!current) ? "true" : "false"))));
    return 0;
}

int SemuCli_settingsUi(char* project) {
    JsonObject* object = SemuCli_settingsObject(project);
    printf("%s\n", "Semu Settings");
    printf("%s\n", __btrc_str_track(__btrc_strcat("Project: ", project)));
    printf("%s\n", __btrc_str_track(__btrc_strcat("ROMs: ", SemuOwnedPaths_romsRoot(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Integer scaling: ", JsonObject_getString(object, "visual_integer_scaling", "true"))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Render backend: ", JsonObject_getString(object, "visual_render_backend", "auto"))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("CRT shaders: ", JsonObject_getString(object, "visual_crt_shaders", "true"))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Bezels: ", JsonObject_getString(object, "visual_bezels", "true"))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Syncthing: ", JsonObject_getString(object, "sync_enabled", "false"))));
    printf("%s\n", "");
    printf("%s\n", "Commands:");
    printf("%s\n", __btrc_str_track(__btrc_strcat("  semu settings put roms.dir PATH --project ", ShellWords_quote(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("  semu settings toggle visual.bezels apply --project ", ShellWords_quote(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("  semu settings apply --project ", ShellWords_quote(project))));
    return 0;
}

int SemuCli_launcher(CliArgs* args, char* project, char* emulator, int startIndex) {
    if (__btrc_isEmpty(emulator)) {
        printf("%s\n", "error 0:0 launcher needs emulator id");
        return 1;
    }
    char* launchFile = SemuOwnedPaths_emulatorFile(project, emulator, "launch.json");
    if (!FileSystem_isFile(launchFile)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("error 0:0 emulator launch definition not found: ", emulator)));
        return 1;
    }
    char* raw = FileSystem_readText(launchFile);
    char* linux = JsonText_objectField(raw, "linux");
    char* executable = JsonText_field(linux, "executable", emulator);
    char* flatpak = JsonText_field(linux, "flatpakId", "");
    char* envPrefix = SemuCli_launcherEnvPrefix(project, linux);
    char* flatpakEnv = SemuCli_launcherFlatpakEnv(project, linux);
    btrc_Vector_string* passthrough = SemuCli_launcherDefaultArgs(project, emulator);
    (passthrough = SemuCli_sourceHookLauncherArgs(project, emulator, passthrough));
    for (int i = startIndex; (i < CliArgs_count(args)); (i++)) {
        btrc_Vector_string_push(passthrough, CliArgs_get(args, i));
    }
    (passthrough = SemuCli_normalizeRetroarchCoreArgs(emulator, passthrough));
    char* renderedArgs = SemuCli_renderLauncherArgs(passthrough);
    char* command = SemuCli_backendCommand(executable, flatpak, renderedArgs, envPrefix, flatpakEnv);
    (command = SemuCli_withVisualWrapper(project, emulator, command));
    (command = SemuCli_withQuitWatch(command));
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_run(shell, command, CommandOutput_stream(), CommandOutput_stream(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
    int __btrc_ret_222 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_222;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int SemuCli_launcherRouted(CliArgs* args, char* project) {
    if (CliArgs_count(args) < 4) {
        printf("%s\n", "error 0:0 launcher routed needs EMULATOR EXECUTABLE");
        return 1;
    }
    char* emulator = CliArgs_get(args, 2);
    char* executable = CliArgs_get(args, 3);
    btrc_Vector_string* routedArgs = SemuCli_launcherDefaultArgs(project, emulator);
    (routedArgs = SemuCli_sourceHookLauncherArgs(project, emulator, routedArgs));
    for (int i = 4; (i < CliArgs_count(args)); (i++)) {
        btrc_Vector_string_push(routedArgs, CliArgs_get(args, i));
    }
    (routedArgs = SemuCli_normalizeRetroarchCoreArgs(emulator, routedArgs));
    char* linux = JsonText_objectField(FileSystem_readText(SemuOwnedPaths_emulatorFile(project, emulator, "launch.json")), "linux");
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("exec ", SemuCli_launcherEnvPrefix(project, linux))), ShellWords_quote(executable))), " ")), SemuCli_renderLauncherArgs(routedArgs)));
    (command = SemuCli_withVisualWrapper(project, emulator, command));
    (command = SemuCli_withQuitWatch(command));
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_run(shell, command, CommandOutput_stream(), CommandOutput_stream(), false, false, false, "", "", "", CommandEnvironment_empty(), false, "");
    int __btrc_ret_223 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_223;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

char* SemuCli_renderLauncherArgs(btrc_Vector_string* args) {
    btrc_Vector_string* rendered = btrc_Vector_string_new();
    int __n_225 = btrc_Vector_string_iterLen(args);
    for (int __i_224 = 0; (__i_224 < __n_225); (__i_224++)) {
        char* arg = btrc_Vector_string_iterGet(args, __i_224);
        btrc_Vector_string_push(rendered, ShellWords_quote(arg));
    }
    return btrc_Vector_string_join(rendered, " ");
}

btrc_Vector_string* SemuCli_launcherDefaultArgs(char* project, char* emulator) {
    char* launchFile = SemuOwnedPaths_emulatorFile(project, emulator, "launch.json");
    if (!FileSystem_isFile(launchFile)) {
        btrc_Vector_string* empty = btrc_Vector_string_new();
        return empty;
    }
    char* linux = JsonText_objectField(FileSystem_readText(launchFile), "linux");
    btrc_Vector_string* rawDefaults = JsonText_stringArray(linux, "defaultArgs");
    btrc_Vector_string* defaults = btrc_Vector_string_new();
    int __n_227 = btrc_Vector_string_iterLen(rawDefaults);
    for (int __i_226 = 0; (__i_226 < __n_227); (__i_226++)) {
        char* value = btrc_Vector_string_iterGet(rawDefaults, __i_226);
        btrc_Vector_string_push(defaults, SemuTemplate_expandProject(project, value));
    }
    return defaults;
}

char* SemuCli_launcherEnvPrefix(char* project, char* linux) {
    char* raw = JsonText_objectField(linux, "env");
    if (__btrc_isEmpty(raw)) {
        return "";
    }
    JsonObject* object = JsonObject_parse(raw);
    btrc_Vector_string* entries = btrc_Vector_string_new();
    int __n_229 = btrc_Map_string_string_iterLen(object->values);
    for (int __i_228 = 0; (__i_228 < __n_229); (__i_228++)) {
        char* key = btrc_Map_string_string_iterGet(object->values, __i_228);
        char* value = btrc_Map_string_string_iterValueAt(object->values, __i_228);
        btrc_Vector_string_push(entries, ShellWords_quote(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(key, "=")), SemuTemplate_expandProject(project, value)))));
    }
    if (entries->len == 0) {
        return "";
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("env ", btrc_Vector_string_join(entries, " "))), " "));
}

char* SemuCli_launcherFlatpakEnv(char* project, char* linux) {
    char* raw = JsonText_objectField(linux, "env");
    if (__btrc_isEmpty(raw)) {
        return "";
    }
    JsonObject* object = JsonObject_parse(raw);
    btrc_Vector_string* entries = btrc_Vector_string_new();
    int __n_231 = btrc_Map_string_string_iterLen(object->values);
    for (int __i_230 = 0; (__i_230 < __n_231); (__i_230++)) {
        char* key = btrc_Map_string_string_iterGet(object->values, __i_230);
        char* value = btrc_Map_string_string_iterValueAt(object->values, __i_230);
        btrc_Vector_string_push(entries, ShellWords_quote(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("--env=", key)), "=")), SemuTemplate_expandProject(project, value)))));
    }
    return btrc_Vector_string_join(entries, " ");
}

btrc_Vector_string* SemuCli_normalizeRetroarchCoreArgs(char* emulator, btrc_Vector_string* args) {
    if (!(strcmp(emulator, "retroarch") == 0)) {
        return args;
    }
    char* coreDir = Environment_get("SEMU_RETROARCH_CORE_DIR", "");
    if (__btrc_isEmpty(coreDir)) {
        return args;
    }
    btrc_Vector_string* normalized = btrc_Vector_string_new();
    bool nextIsCore = false;
    int __n_233 = btrc_Vector_string_iterLen(args);
    for (int __i_232 = 0; (__i_232 < __n_233); (__i_232++)) {
        char* arg = btrc_Vector_string_iterGet(args, __i_232);
        if (nextIsCore) {
            btrc_Vector_string_push(normalized, SemuCli_normalizedRetroarchCore(coreDir, arg));
            (nextIsCore = false);
        } else {
            btrc_Vector_string_push(normalized, arg);
            (nextIsCore = ((strcmp(arg, "-L") == 0) || (strcmp(arg, "--libretro") == 0)));
        }
    }
    return normalized;
}

char* SemuCli_normalizedRetroarchCore(char* coreDir, char* core) {
    if ((__btrc_startsWith(core, "%CORE_RETROARCH%") || __btrc_startsWith(core, "/tmp/.mount_")) || (!FileSystem_isFile(core))) {
        char* basename = PathTools_basename(core);
        if (!__btrc_isEmpty(basename)) {
            char* replacement = joinPath(coreDir, basename);
            if (FileSystem_isFile(replacement)) {
                return replacement;
            }
        }
    }
    return core;
}

char* SemuCli_backendCommand(char* executable, char* flatpak, char* renderedArgs, char* envPrefix, char* flatpakEnv) {
    char* quotedExecutable = ShellWords_quote(executable);
    char* quotedFlatpak = ShellWords_quote(flatpak);
    if (!__btrc_isEmpty(flatpak)) {
        char* envArgs = (__btrc_isEmpty(flatpakEnv) ? "" : __btrc_str_track(__btrc_strcat(flatpakEnv, " ")));
        return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if command -v ", quotedExecutable)), " >/dev/null 2>&1; then exec ")), envPrefix)), quotedExecutable)), " ")), renderedArgs)), "; else exec flatpak run ")), envArgs)), quotedFlatpak)), " ")), renderedArgs)), "; fi"));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("exec ", envPrefix)), quotedExecutable)), " ")), renderedArgs));
}

btrc_Vector_string* SemuCli_sourceHookLauncherArgs(char* project, char* emulator, btrc_Vector_string* args) {
    return args;
}

char* SemuCli_withQuitWatch(char* command) {
    char* helper = "";
    if (strcmp(Environment_get("SEMU_DISABLE_QUIT_WATCH", "0"), "1") == 0) {
        return command;
    }
    (helper = SemuCli_quitWatchHelper());
    if (!__btrc_isEmpty(helper)) {
        return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("exec ", ShellWords_quote(helper))), " -- sh -lc ")), ShellWords_quote(command)));
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if command -v semu-quit-watch >/dev/null 2>&1; then exec semu-quit-watch -- sh -lc ", ShellWords_quote(command))), "; else echo 'semu: semu-quit-watch unavailable; set SEMU_DISABLE_QUIT_WATCH=1 to launch without Start+Select quit' >&2; exit 127; fi"));
}

char* SemuCli_quitWatchHelper(void) {
    char* configured = Environment_get("SEMU_QUIT_WATCH_BIN", "");
    if ((!__btrc_isEmpty(configured)) && FileSystem_isFile(configured)) {
        return configured;
    }
    char* activeBin = Environment_get("SEMU_ACTIVE_LAUNCHER_BIN", "");
    if (!__btrc_isEmpty(activeBin)) {
        char* activeHelper = joinPath(activeBin, "semu-quit-watch");
        if (FileSystem_isFile(activeHelper)) {
            return activeHelper;
        }
    }
    char* cli = Environment_get("SEMU_CLI", "");
    if (!__btrc_isEmpty(cli)) {
        char* cliHelper = joinPath(PathTools_dirname(cli), "semu-quit-watch");
        if (FileSystem_isFile(cliHelper)) {
            return cliHelper;
        }
    }
    return "";
}

char* SemuCli_withVisualWrapper(char* project, char* emulator, char* command) {
    char* systemId = Environment_get("SEMU_SYSTEM", "");
    if (strcmp(Environment_get("SEMU_DISABLE_RENDER_WRAPPER", Environment_get("SEMU_DISABLE_GAMESCOPE_PRESENTATION", "0")), "1") == 0) {
        return command;
    }
    char* renderBackend = SemuCli_renderBackend(project, emulator);
    if (((strcmp(renderBackend, "none") == 0) || (strcmp(renderBackend, "disabled") == 0)) || (strcmp(renderBackend, "native_fullscreen_only") == 0)) {
        return command;
    }
    if ((strcmp(renderBackend, "source_hook") == 0) || (strcmp(renderBackend, "emulator_hook") == 0)) {
        return SemuCli_withSourceHookEnv(project, emulator, systemId, command);
    }
    if ((strcmp(renderBackend, "nixgl_only") == 0) || (strcmp(renderBackend, "native") == 0)) {
        return SemuCli_withNixGlOnlyWrapper(systemId, command);
    }
    char* hostRenderMode = SemuCli_systemAssetField(project, systemId, "host_render_mode", "");
    if (strcmp(hostRenderMode, "none") == 0) {
        return command;
    }
    if (strcmp(hostRenderMode, "nixgl_only") == 0) {
        return SemuCli_withNixGlOnlyWrapper(systemId, command);
    }
    bool shaders = SemuCli_settingBool(project, "visual_crt_shaders", true);
    bool bezels = SemuCli_settingBool(project, "visual_bezels", true);
    if ((!shaders) && (!bezels)) {
        return SemuCli_withNixGlOnlyWrapper(systemId, command);
    }
    char* scale = (SemuCli_settingBool(project, "visual_integer_scaling", true) ? "integer" : "stretch");
    SemuRenderingDefinition* rendering = SemuRenderingDefinition_load(project, systemId);
    char* effect = SemuRenderingDefinition_bezelEffectFile(rendering);
    char* shaderEffect = SemuRenderingDefinition_shaderEffectFile(rendering);
    char* nativeSize = SemuRenderingDefinition_nativeSize(rendering);
    char* contentAspect = SemuRenderingDefinition_contentAspect(rendering);
    char* layout = SemuRenderingDefinition_layoutKind(rendering);
    char* scalePolicy = SemuRenderingDefinition_scalePolicy(rendering);
    char* aspectPolicy = SemuRenderingDefinition_viewportField(rendering, "default_aspect", contentAspect);
    char* dynamicAspect = SemuRenderingDefinition_dynamicAspectFlag(rendering);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if command -v semu-render >/dev/null 2>&1; then exec env SEMU_RENDER_SHADERS=", (shaders ? "1" : "0"))), " SEMU_RENDER_BEZELS=")), (bezels ? "1" : "0"))), " SEMU_RENDER_BACKEND=")), ShellWords_quote(renderBackend))), " SEMU_RENDER_SCALE=")), ShellWords_quote(scale))), " SEMU_RENDER_EFFECT_FILE=")), ShellWords_quote(effect))), " SEMU_RENDER_SHADER_FILE=")), ShellWords_quote(shaderEffect))), " SEMU_RENDER_NATIVE_SIZE=")), ShellWords_quote(nativeSize))), " SEMU_RENDER_CONTENT_ASPECT=")), ShellWords_quote(contentAspect))), " SEMU_RENDER_LAYOUT=")), ShellWords_quote(layout))), " SEMU_RENDER_SCALE_POLICY=")), ShellWords_quote(scalePolicy))), " SEMU_RENDER_ASPECT_POLICY=")), ShellWords_quote(aspectPolicy))), " SEMU_RENDER_DYNAMIC_ASPECT=")), ShellWords_quote(dynamicAspect))), " semu-render --system ")), ShellWords_quote(systemId))), " -- sh -lc ")), ShellWords_quote(command))), "; else ")), command)), "; fi"));
}

char* SemuCli_withSourceHookEnv(char* project, char* emulator, char* systemId, char* command) {
    SemuRenderingDefinition* rendering = SemuRenderingDefinition_load(project, systemId);
    bool shaders = ((SemuCli_settingBool(project, "visual_crt_shaders", true) && (strcmp(SemuRenderingDefinition_rendererField(rendering, "shader", "enabled_by_default", "false"), "true") == 0)) && (!__btrc_isEmpty(SemuRenderingDefinition_shaderEffectFile(rendering))));
    bool bezels = ((SemuCli_settingBool(project, "visual_bezels", true) && (strcmp(SemuRenderingDefinition_rendererField(rendering, "bezel", "enabled_by_default", "false"), "true") == 0)) && (!__btrc_isEmpty(SemuRenderingDefinition_bezelEffectFile(rendering))));
    char* scale = (SemuCli_settingBool(project, "visual_integer_scaling", true) ? "integer" : "stretch");
    char* marker = (SemuCli_sourceHookCurrentlyApplied(project, emulator, systemId) ? __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("printf '%s\\n' ", ShellWords_quote(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("semu source hook applied emulator=", emulator)), " system=")), systemId))))), " >&2; ")) : "");
    char* hooked = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(marker, "env SEMU_RENDER_HOOK=1")), " SEMU_RENDER_HOOK_CONFIG=")), ShellWords_quote(SemuOwnedPaths_generatedFile(project, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("assets/rendering/hooks/", emulator)), ".json")))))), " SEMU_RENDER_HOOK_PROOF=")), ShellWords_quote(SemuOwnedPaths_generatedFile(project, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("assets/rendering/hooks/", emulator)), ".proof")))))), " SEMU_RENDER_HOOK_ROOT=")), ShellWords_quote(SemuOwnedPaths_generatedFile(project, "assets/rendering")))), " SEMU_RENDER_SHADERS=")), (shaders ? "1" : "0"))), " SEMU_RENDER_BEZELS=")), (bezels ? "1" : "0"))), " SEMU_RENDER_SCALE=")), ShellWords_quote(scale))), " SEMU_SYSTEM=")), ShellWords_quote(systemId))), " sh -lc ")), ShellWords_quote(command)));
    return SemuCli_withNixGlOnlyWrapper(systemId, hooked);
}

bool SemuCli_sourceHookCurrentlyApplied(char* project, char* emulator, char* systemId) {
    if (__btrc_isEmpty(systemId)) {
        return false;
    }
    if (!(strcmp(SemuCli_renderBackend(project, emulator), "source_hook") == 0)) {
        return false;
    }
    return FileSystem_isFile(SemuOwnedPaths_generatedFile(project, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("assets/rendering/hooks/", emulator)), ".json"))));
}

char* SemuCli_withNixGlOnlyWrapper(char* systemId, char* command) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if command -v semu-render >/dev/null 2>&1; then exec env SEMU_RENDER_SHADERS=0 SEMU_RENDER_BEZELS=0", " semu-render --system ")), ShellWords_quote(systemId))), " -- sh -lc ")), ShellWords_quote(command))), "; else ")), command)), "; fi"));
}

char* SemuCli_renderBackend(char* project, char* emulator) {
    char* envBackend = Environment_get("SEMU_RENDER_BACKEND", "");
    if (!__btrc_isEmpty(envBackend)) {
        return envBackend;
    }
    char* setting = SemuCli_settingString(project, "visual_render_backend", "auto");
    if ((!__btrc_isEmpty(setting)) && (!(strcmp(setting, "auto") == 0))) {
        return setting;
    }
    char* declared = SemuEmulatorRenderingDefinition_field(SemuEmulatorRenderingDefinition_load(project, emulator), "preferred_backend", "");
    return (__btrc_isEmpty(declared) ? "native_fullscreen_only" : declared);
}

char* SemuCli_systemAssetField(char* project, char* systemId, char* field, char* fallback) {
    char* path = SemuOwnedPaths_systemAssetFile(project, systemId);
    if (!FileSystem_isFile(path)) {
        return fallback;
    }
    return JsonText_field(FileSystem_readText(path), field, fallback);
}

bool SemuCli_settingBool(char* project, char* key, bool fallback) {
    char* path = SemuCli_settingsPath(project);
    if (!FileSystem_isFile(path)) {
        return fallback;
    }
    JsonObject* object = JsonObject_parse(FileSystem_readText(path));
    return JsonObject_getBool(object, SemuCli_normalizedKey(key), fallback);
}

char* SemuCli_settingString(char* project, char* key, char* fallback) {
    char* path = SemuCli_settingsPath(project);
    if (!FileSystem_isFile(path)) {
        return fallback;
    }
    JsonObject* object = JsonObject_parse(FileSystem_readText(path));
    return JsonObject_getString(object, SemuCli_normalizedKey(key), fallback);
}

int SemuCli_assets(CliArgs* args, char* project) {
    if (CliArgs_count(args) < 2) {
        printf("%s\n", "asset commands: get SYSTEM FIELD | put SYSTEM FIELD VALUE --apply");
        return 0;
    }
    char* sub = CliArgs_get(args, 1);
    if ((strcmp(sub, "get") == 0) && (CliArgs_count(args) >= 4)) {
        SemuRenderingDefinition* rendering = SemuRenderingDefinition_load(project, CliArgs_get(args, 2));
        printf("%s\n", SemuRenderingDefinition_pathField(rendering, CliArgs_get(args, 3), ""));
        return 0;
    }
    if ((strcmp(sub, "put") == 0) && (CliArgs_count(args) >= 5)) {
        char* system = CliArgs_get(args, 2);
        char* field = CliArgs_get(args, 3);
        char* value = CliArgs_get(args, 4);
        char* overrideDir = joinPath(SemuOwnedPaths_overrides(project), "rendering");
        ensureDir(overrideDir);
        FileSystem_writeText(joinPath(overrideDir, __btrc_str_track(__btrc_strcat(system, ".json"))), SemuRenderingDefinition_nestedOverrideJson(field, value));
        if (CliArgs_has(args, "--apply")) {
            return SemuCli_generateConfigs(project, "steam-deck");
        }
        return 0;
    }
    return 1;
}

int SemuCli_keymap(CliArgs* args, char* project) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("Semu input actions are declared in ", joinPath(SemuOwnedPaths_definitionRoot(project), "input/actions.json"))));
    return 0;
}

int SemuCli_sync(CliArgs* args, char* project) {
    return SemuCli_syncAction(project, ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "status"));
}

int SemuCli_syncAction(char* project, char* mode) {
    if (strcmp(mode, "open") == 0) {
        printf("%s\n", "Syncthing UI: http://127.0.0.1:8384");
        return 0;
    }
    if (strcmp(mode, "toggle") == 0) {
        return SemuCli_settingsToggle(project, "sync.enabled");
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("Syncthing config lives in ", SemuOwnedPaths_settingsDefinitionFile(project, "sync.json"))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Project: ", project)));
    return 0;
}

int SemuCli_n3ds(char* project, char* mode) {
    char* root = joinPath(SemuOwnedPaths_romsRoot(project), "n3ds");
    printf("%s\n", __btrc_str_track(__btrc_strcat("3DS ROM root: ", root)));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Mode: ", mode)));
    return 0;
}

int SemuCli_doctor(char* project) {
    printf("%s\n", "Semu doctor");
    printf("%s\n", __btrc_str_track(__btrc_strcat("Project: ", project)));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Definition root: ", SemuOwnedPaths_definitionRoot(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("ROM root: ", SemuOwnedPaths_romsRoot(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("BIOS root: ", SemuOwnedPaths_biosRoot(project))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("Generated root: ", SemuOwnedPaths_generated(project))));
    return 0;
}

int SemuCli_bootstrap(char* project) {
    return SemuCli_generateConfigs(project, "steam-deck");
}

int SemuCli_deck(char* project) {
    return SemuCli_doctor(project);
}

int SemuCli_e2e(CliArgs* args, char* project) {
    char* sub = ((CliArgs_count(args) > 1) ? CliArgs_get(args, 1) : "status");
    if ((strcmp(sub, "status") == 0) || (strcmp(sub, "graph") == 0)) {
        printf("%s\n", "compiler-build");
        printf("%s\n", "compiler-verify");
        return 0;
    }
    return SemuCli_generateConfigs(project, "steam-deck");
}

int SemuCli_manifest(char* project) {
    SemuBuildPlan* plan = SemuCli_configPlan(project, "steam-deck");
    printf("%s\n", "semu compiler manifest");
    printf("%s\n", __btrc_str_track(__btrc_strcat("systems=", Strings_fromInt(plan->systemIds->len))));
    printf("%s\n", __btrc_str_track(__btrc_strcat("emulators=", Strings_fromInt(plan->emulatorIds->len))));
    return 0;
}

int main(int argc, char** argv) {
    CliArgs* args = CliArgs_new(argc, argv);
    int __btrc_ret_234 = SemuCli_run(args);
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
    return __btrc_ret_234;
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
}
