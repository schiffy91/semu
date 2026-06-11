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
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
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

static inline int __btrc_div_int(int a, int b) {
    if (b == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
    return a / b;
}

static inline int __btrc_mod_int(int a, int b) {
    if (b == 0) { fprintf(stderr, "Modulo by zero\n"); exit(1); }
    return a % b;
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

typedef struct Strings Strings;
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
typedef struct ExecResult ExecResult;
typedef struct Command Command;
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
typedef struct CliArgs CliArgs;
void CliArgs_destroy(CliArgs* self);
typedef struct CliCommand CliCommand;
typedef struct Console Console;
typedef struct GraphNode GraphNode;
void GraphNode_destroy(GraphNode* self);
typedef struct ExecutionGraph ExecutionGraph;
void ExecutionGraph_destroy(ExecutionGraph* self);
typedef struct GraphParser GraphParser;
typedef struct GraphCli GraphCli;
typedef struct GraphReport GraphReport;
typedef struct GraphTraversal GraphTraversal;
void GraphTraversal_destroy(GraphTraversal* self);
typedef struct SemuJson SemuJson;
typedef struct SemuShell SemuShell;
typedef struct SemuManifestCompiler SemuManifestCompiler;
typedef struct SystemCatalog SystemCatalog;
void SystemCatalog_destroy(SystemCatalog* self);
typedef struct SemuKeymapLanguage SemuKeymapLanguage;
typedef struct KeymapErrors KeymapErrors;
void KeymapErrors_destroy(KeymapErrors* self);
typedef struct KeymapTokens KeymapTokens;
void KeymapTokens_destroy(KeymapTokens* self);
typedef struct KeymapIr KeymapIr;
void KeymapIr_destroy(KeymapIr* self);
typedef struct KeymapParser KeymapParser;
void KeymapParser_destroy(KeymapParser* self);
typedef struct SemuPaths SemuPaths;
typedef struct SemuLauncher SemuLauncher;
typedef struct SemuEmulatorMetadata SemuEmulatorMetadata;
typedef struct SemuEmulatorSandbox SemuEmulatorSandbox;
typedef struct SemuEmulatorProfiles SemuEmulatorProfiles;
typedef struct SemuProjectBootstrap SemuProjectBootstrap;
typedef struct SemuDoctor SemuDoctor;
typedef struct BinaryEditor BinaryEditor;
void BinaryEditor_destroy(BinaryEditor* self);
typedef struct BinaryReader BinaryReader;
void BinaryReader_destroy(BinaryReader* self);
typedef struct SemuN3dsTools SemuN3dsTools;
typedef struct N3dsRomCheck N3dsRomCheck;
void N3dsRomCheck_destroy(N3dsRomCheck* self);
typedef struct SemuScreenshotVerification SemuScreenshotVerification;
typedef struct SemuSyncRuntime SemuSyncRuntime;
typedef struct SemuAppImageSmoke SemuAppImageSmoke;
typedef struct SemuDeckTests SemuDeckTests;
typedef struct SemuE2eCommand SemuE2eCommand;
typedef struct SemuE2eOperation SemuE2eOperation;
void SemuE2eOperation_destroy(SemuE2eOperation* self);
typedef struct SemuE2eSpec SemuE2eSpec;
void SemuE2eSpec_destroy(SemuE2eSpec* self);
typedef struct SemuE2eParser SemuE2eParser;
typedef struct SemuE2eRunner SemuE2eRunner;
void SemuE2eRunner_destroy(SemuE2eRunner* self);
typedef struct SemuE2eGraphRunner SemuE2eGraphRunner;
void SemuE2eGraphRunner_destroy(SemuE2eGraphRunner* self);
typedef struct SemuSmokeTests SemuSmokeTests;
typedef struct SemuCliCommands SemuCliCommands;
typedef struct SemuApp SemuApp;
typedef struct btrc_Vector_string btrc_Vector_string;
typedef struct btrc_Vector_bool btrc_Vector_bool;
typedef struct btrc_Vector_GraphNode btrc_Vector_GraphNode;
typedef struct btrc_Vector_int btrc_Vector_int;
typedef struct btrc_Vector_SemuE2eOperation btrc_Vector_SemuE2eOperation;
typedef struct btrc_Map_string_string btrc_Map_string_string;
typedef struct btrc_Map_string_bool btrc_Map_string_bool;
char* jsonQ(char* value);
char* jsonField(char* key, char* value);
char* jsonStrField(char* key, char* value);
char* jsonBoolField(char* key, bool value);
char* jsonObject(btrc_Vector_string* fields);
char* jsonArray(btrc_Vector_string* values);
char* jsonStringArray(btrc_Vector_string* values);
void writeJsonPrettyFile(char* path, char* text);
char* commandPath(char* name);
bool commandExists(char* name);
char* commandSpec(char* label, char* command);
char* platformCommands(char* platform, btrc_Vector_string* commands);
char* systemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile);
char* biosSpec(char* id, char* system, bool required, btrc_Vector_string* files, char* target, char* match, char* note);
char* steamInputTemplate(char* id, char* title, char* source, char* note);
char* xmlEscape(char* value);
char* esExtensionList(btrc_Vector_string* extensions);
char* esCommandXml(char* commandJson);
char* esSystemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands);
char* manifestPaths(void);
char* esDe(void);
char* controllerModel(char* id, char* label, char* vendor, char* layout, btrc_Vector_string* capabilities, btrc_Vector_string* defaultOutputs, char* preferredBackend, char* gyroPolicy, char* note);
char* emulationBackend(char* id, char* label, char* layer, btrc_Vector_string* emits, btrc_Vector_string* requires, bool automated, bool visual, char* note);
char* verificationProfile(char* id, char* label, char* backend, btrc_Vector_string* controllers, btrc_Vector_string* checks, bool automated, bool visual);
char* screenshotToolSpec(char* id, char* command, btrc_Vector_string* args, btrc_Vector_string* requires, char* note);
char* screenshotHookSpec(char* id, char* lifecycle, char* output, char* enabledBy, int delayMs, btrc_Vector_string* emulators, char* note);
char* screenshotVerificationProfile(void);
char* inputStack(void);
char* controllerProfiles(void);
char* syncFolderSpec(char* id, char* label, char* path, bool enabled, bool watch, int rescanSeconds);
char* syncProfile(void);
char* biosEntries(void);
char* systemEntries(void);
char* buildJson(void);
SystemCatalog* systemCatalog(void);
bool isIdentChar(char c);
bool isSpaceChar(char c);
char* defaultKeymapSource(void);
char* actionAlias(char* id);
char* actionLabel(char* id);
char* actionNote(char* id);
btrc_Vector_string* actionSystems(char* id);
int lastPlusIndex(char* text);
char* keymapCommandKeyPart(char* command);
char* keymapCommandModifierPart(char* command);
KeymapTokens* lexKeymap(char* source, KeymapErrors* errors);
bool keymapIrHasAction(KeymapIr* ir, char* id);
btrc_Vector_string* requiredKeymapActions(void);
bool allowedModifier(char* modifier);
void validateKeymapIr(KeymapIr* ir, KeymapErrors* errors);
KeymapIr* compileKeymap(char* source, KeymapErrors* errors);
KeymapIr* defaultKeymapIr(void);
char* keymapIrActionJson(KeymapIr* ir, int index);
char* keymapIrBindingJson(KeymapIr* ir, int index);
char* keymapIrJson(KeymapIr* ir);
char* irActionCommand(KeymapIr* ir, char* id);
char* irBindingCombo(KeymapIr* ir, char* id);
char* keymapsJson(void);
char* hotkeyFromKeymapIr(KeymapIr* ir, char* actionId);
char* hotkeySpec(char* id, char* label, char* combo, char* command, btrc_Vector_string* systems, char* note);
char* steamDeckHotkeys(void);
char* retroarchKeyName(char* key);
char* keymapCommandKey(KeymapIr* ir, char* actionId);
char* renderRetroArchKeymap(KeymapIr* ir);
char* dolphinChord(KeymapIr* ir, char* actionId);
char* renderDolphinKeymap(KeymapIr* ir);
char* pcsx2KeyName(char* key);
char* pcsx2Chord(KeymapIr* ir, char* actionId);
char* renderPcsx2Keymap(KeymapIr* ir);
char* renderKeymap(KeymapIr* ir, char* target);
bool isKeymapTarget(char* target);
char* joinPath(char* left, char* right);
char* contentRoot(char* project);
char* romsRoot(char* project);
char* syncRoot(char* project);
char* syncConfigPath(char* project);
char* syncthingHome(char* project);
char* syncthingConfigDir(char* project);
char* syncthingDataDir(char* project);
char* syncScriptsDir(char* project);
char* syncLogDir(char* project);
char* biosRoot(char* project);
char* customSystemsRoot(char* project);
char* esDeProfileCustomSystemsRoot(char* project);
char* emulatorProfilesRoot(char* project);
char* emulatorProfilePath(char* project, char* relative);
char* linuxPackagingRoot(char* project);
char* linuxLauncherBin(char* project);
char* keymapsRoot(char* project);
char* keymapSourcePath(char* project);
char* homeDir(void);
char* systemdUserDir(void);
char* applicationsDir(void);
char* expandProjectTemplate(char* project, char* value);
JsonObject* syncConfig(char* project);
char* syncString(char* project, char* key, char* fallback);
bool syncBool(char* project, char* key, bool fallback);
int syncInt(char* project, char* key, int fallback);
char* configuredRomsRoot(char* project);
char* normalizeRomsRoot(char* romsDir);
char* configuredEmulationRoot(char* project);
btrc_Vector_string* declaredSystemIds(void);
btrc_Vector_string* declaredRomDirs(void);
btrc_Vector_string* controllerProfileFiles(void);
char* esSettingsXmlWithPaths(char* roms, char* media, char* themes);
char* esSettingsXmlForProject(char* project);
void ensureDir(char* path);
char* launcherFlatpakId(char* emulator);
bool launcherUsesX11(char* emulator);
btrc_Vector_string* launcherPresetArgs(char* emulator);
bool launcherIsUtilityArg(char* arg);
bool launcherIsUtilityInvocation(btrc_Vector_string* emulatorArgs);
bool launcherHasFlag(btrc_Vector_string* emulatorArgs, char* flag);
bool launcherDefaultAlreadyPresent(char* emulator, char* arg, btrc_Vector_string* emulatorArgs);
bool launcherDefaultTakesValue(char* arg);
btrc_Vector_string* launcherArgsWithDefaults(char* emulator, btrc_Vector_string* emulatorArgs);
char* launcherFlatpakStateRoot(char* project, char* emulator);
char* launcherRoutedStateRoot(char* project, char* emulator);
void launcherCopyDirContents(char* source, char* destination);
void launcherCopyFile(char* source, char* destination);
void launcherCopyProfileDir(char* project, char* relative, char* destination);
void launcherCopyProfileFile(char* project, char* relative, char* destination);
void launcherCopyStateDir(char* project, char* relative, char* destination);
char* launcherRetroArchSystemDir(char* project);
char* launcherRetroArchConfigValue(char* key, char* value);
char* launcherRetroArchConfig(char* project);
bool launcherUsesNixGl(char* emulator);
char* launcherNixGlCandidate(char* path);
char* launcherNixGlWrapper(char* project);
char* launcherRoutedExecutableCommand(char* project, char* emulator, char* executable);
char* launcherRoutedEnvironment(char* project, char* emulator, char* home, char* configRoot, char* dataRoot, char* cacheRoot);
char* launcherRoutedDisplaySetup(void);
char* launcherQuitWatchCommand(char* childCommand);
void launcherWriteRetroArchConfig(char* project, char* configRoot);
void launcherSeedRoutedState(char* project, char* emulator, char* configRoot, char* dataRoot);
int launcherRunRouted(char* project, char* emulator, char* executable, btrc_Vector_string* emulatorArgs);
int launcherRunFlatpak(char* project, char* emulator, char* flatpakId, btrc_Vector_string* emulatorArgs);
int launcherRunEmulator(char* project, char* emulator, btrc_Vector_string* emulatorArgs);
int launcherCommand(CliArgs* args, char* project);
btrc_Vector_string* linuxLauncherNames(void);
btrc_Vector_string* lowercaseValues(btrc_Vector_string* values);
char* semuLauncherName(char* emulator);
btrc_Vector_string* retroarchCoreSearchPaths(void);
char* launcherEntries(void);
char* stripTrailingSlashes(char* path);
char* sandboxResolveTarget(char* scratch, char* target);
char* sandboxProjectEmulatorDir(char* project, char* emulator);
void sandboxSymlink(char* linkPath, char* sourcePath);
void sandboxApplyLink(char* emuDir, char* scratch, char* entry, char* linuxTarget);
bool sandboxApplyKnownLinks(char* key, char* emuDir, char* scratch);
char* currentUid(void);
char* launchRomsRoot(char* project);
char* xdgRunDir(void);
btrc_Vector_string* cliTail(CliArgs* args, int start);
btrc_Vector_string* vectorTail(btrc_Vector_string* values, int start);
btrc_Vector_string* launcherPassthroughArgs(CliArgs* args);
char* shellAppend(char* command, char* arg);
char* shellAppendAll(char* command, btrc_Vector_string* args);
int sandboxLaunch(char* project, char* emulator, char* executable, btrc_Vector_string* emulatorArgs);
int sandboxPrepareCommand(CliArgs* args, char* project);
int sandboxCommand(CliArgs* args, char* project);
char* textLines(btrc_Vector_string* items);
KeymapIr* projectKeymapIr(char* project);
char* retroArchProfileText(char* project, KeymapIr* ir);
char* dolphinGcpadProfileText(void);
char* dolphinHotkeysProfileText(KeymapIr* ir);
char* dolphinWiimoteProfileText(bool classic);
char* pcsx2ProfileText(KeymapIr* ir);
char* cemuProfileText(void);
char* ryujinxProfileText(void);
void writeGeneratedManifest(char* output);
char* bundledText(char* project, char* relative, char* fallback);
char* renderBundledTemplate(char* project, char* relative, btrc_Map_string_string* values, char* fallback);
char* assetRoot(char* project);
void seedBundledFileFromRoot(char* root, char* project, char* relative, bool executable);
void seedBundledFile(char* project, char* relative, bool executable);
void seedBundledDirFiles(char* project, char* relative, bool executable);
char* portableShellHeader(void);
void writeExecutableText(char* path, char* text);
char* linuxSandboxShimText(void);
char* linuxBtrcShimText(void);
char* linuxFlatpakShimText(void);
char* linuxLauncherShimText(char* emulator);
void seedPortableLinuxShims(char* project);
void seedLinuxAssets(char* project);
void writeProfile(char* project, char* relative, char* text);
void seedEmulatorDefaults(char* project);
void seedKeymapDefaults(char* project);
char* syncDefaultConfigText(char* project, char* romsDir);
void writeSyncDefaults(char* project, char* romsDir);
void ensureRomDirsAt(char* root);
void ensureProjectRomDirs(char* project);
void ensureProjectContentDirs(char* project);
void bootstrapSteamDeck(char* project);
char* esLauncherBin(char* project);
char* appendRetroArchCoreRule(char* rules, char* corePath);
char* esFindRulesXml(char* project, char* launcherBin);
void writeEsDeFiles(char* project);
char* esSettingsXmlForRuntime(char* project, char* romsDir);
void writeEsDeRuntimeFiles(char* project, char* userHome);
char* steamInputKeyName(char* key);
char* steamInputActionLabel(char* id);
char* steamInputKeyBindings(char* command, char* label);
char* steamInputXInputBinding(char* button, char* label);
char* steamInputActionBindings(KeymapIr* ir, char* actionId);
char* steamInputTemplateToken(char* actionId);
void steamInputPutAction(btrc_Map_string_string* values, KeymapIr* ir, char* actionId);
char* steamInputTemplateVdf(char* title, bool full, KeymapIr* ir, char* project);
void writeSteamInputTemplates(char* project);
bool allPresent(char* target, btrc_Vector_string* files);
bool anyPresent(char* target, btrc_Vector_string* files);
void reportPath(char* label, char* path);
void reportBios(char* id, char* target, btrc_Vector_string* files, bool required, bool anyMatch);
bool biosOkInAnyTarget(btrc_Vector_string* targets, btrc_Vector_string* files, bool anyMatch);
void reportBiosTargets(char* id, btrc_Vector_string* targets, btrc_Vector_string* files, bool required, bool anyMatch);
void reportFile(char* label, char* path);
int braceBalance(char* text);
void reportSteamInputTemplate(char* label, char* path);
void doctorSteamDeck(char* project);
bool n3dsRomName(char* name);
bool n3dsArchiveName(char* name);
bool exefsLooksDecrypted(char* text);
N3dsRomCheck* checkN3dsRom(char* path);
btrc_Vector_string* n3dsInputFiles(char* input);
bool copyFilePath(char* source, char* destination);
bool n3dsNoCryptoFlag(int flags);
int patchN3dsNoCryptoFlags(char* path);
bool fixN3dsNoCryptoFile(char* input, char* output);
void reportN3dsRomPreflight(char* project);
char* n3dsNoCryptoInputArg(CliArgs* args, int startIndex);
void printN3dsNoCryptoCheckSummary(int total, int ok, int needsFix, int encrypted, int invalid, int unknown);
int n3dsNoCryptoCommand(CliArgs* args, int startIndex);
int utilitiesCommand(CliArgs* args);
int decrypt3dsNoCryptoCommand(CliArgs* args, int startIndex);
char* verificationRoot(char* project);
char* screenshotConfigPath(char* project);
char* screenshotDefaultConfigText(void);
void writeScreenshotDefaults(char* project);
JsonObject* screenshotConfig(char* project);
char* screenshotString(char* project, char* key, char* fallback);
bool screenshotBool(char* project, char* key, bool fallback);
int screenshotInt(char* project, char* key, int fallback);
bool envEnabled(char* value);
bool screenshotHooksEnabled(char* project);
bool screenshotHookEnabled(char* project, char* hook);
char* screenshotDelaySeconds(char* project);
char* screenshotSafeName(char* value);
char* screenshotExpandPathTemplate(char* project, char* value, char* emulator, char* hook);
char* screenshotExpandCommandTemplate(char* project, char* value, char* emulator, char* hook, char* output);
char* screenshotCapturePath(char* project, char* emulator, char* hook);
char* screenshotAutoTool(void);
char* screenshotConfiguredTool(char* project);
char* screenshotCaptureCommand(char* project, char* emulator, char* hook, char* output);
bool screenshotCaptureTo(char* project, char* emulator, char* hook, char* output);
bool screenshotCapture(char* project, char* emulator, char* hook);
void screenshotCaptureHook(char* project, char* emulator, char* hook);
void screenshotScheduleHook(char* project, char* emulator, char* hook);
void doctorScreenshotHooks(char* project);
char* syncFolderPath(char* project, char* id);
btrc_Vector_string* syncFolderIds(void);
char* syncFolderLabel(char* id);
bool syncFolderEnabled(char* project, char* id);
bool syncFolderWatch(char* project, char* id);
int syncFolderRescan(char* project, char* id);
char* syncGuiAddress(char* project);
char* serviceExecutable(void);
char* syncSyncthingExecutable(void);
bool syncSyncthingAvailable(void);
char* syncServeCommand(char* project);
char* syncServiceText(char* project);
char* syncForceScriptText(char* project);
char* syncForceServiceText(char* project);
char* syncTimerText(void);
void writeSyncSystemdUnits(char* project);
char* deckDesktopText(char* project);
void writeDeckDesktopEntry(char* project);
char* syncConfigXmlPath(char* project);
char* findXmlValue(char* text, char* startTag, char* endTag);
char* syncApiKey(char* project);
char* syncScanUrl(char* project, char* target);
void syncGenerateIfNeeded(char* project);
bool syncSystemctl(char* verb, char* unit);
bool syncWaitForApi(char* project);
bool syncAddFolder(char* project, char* id);
bool syncConfigureFolders(char* project);
bool syncSetup(char* project);
char* semuStateRoot(char* project);
char* lifecycleStatePath(char* project);
char* lifecycleBackupsRoot(char* project);
char* upgradeBackupPath(char* project);
void removePath(char* path);
void writeLifecycleState(char* project, char* action);
void lifecycleReconfigure(char* project, char* romsDir);
void lifecycleInstall(char* project, char* romsDir);
void lifecycleUninstall(char* project, bool purgeGenerated, bool purgeState);
bool lifecycleChangeKeymap(char* project, char* actionId, char* command);
void lifecycleUpgrade(char* project);
void lifecycleStatus(char* project);
bool syncStart(char* project);
bool syncServe(char* project);
bool syncStop(char* project);
bool syncAutostart(char* project, bool enabled);
bool syncForce(char* project, char* target);
void syncStatus(char* project);
void syncTray(char* project);
void syncOpen(char* project);
void doctorSync(char* project);
void e2eWriteExecutable(char* path, char* text);
char* e2eFakeEsdeAppImageText(void);
char* e2eFakeAppImageToolText(void);
char* e2eFakeNixText(void);
btrc_Vector_string* appImageExpectedBins(void);
void e2eWriteFakeNixPackage(char* root, bool includeBwrap);
bool e2eExpectStatus(int expected, char* command);
bool e2eFileContains(char* path, char* expected, char* label);
int e2eCountArgLines(char* text, char* expected);
char* e2eBuildAppImageCommand(char* project, char* binDir, char* nixPackage, char* esde, char* output);
int e2eAppImageSmoke(CliArgs* args);
void deckMaybeRun(char* command);
char* deckBrewExecutable(void);
void deckInstallHostPackages(void);
void deckBuildGeneratedCli(char* project);
int deckProvisionCommand(CliArgs* args, char* project);
bool deckHasGraphicalSession(void);
bool deckHasScreenshotTool(void);
int deckVerifyEmulatorsCommand(CliArgs* args, char* project);
int deckVerifyInputCommand(char* project, bool strict);
bool deckWaitForSyncHealth(char* project);
int deckVerifySyncCommand(char* project);
int e2eCommand(CliArgs* args);
void e2eFatal(char* message);
char* e2eExpandArgs(char* text, btrc_Map_string_string* args);
btrc_Vector_string* semuE2eOperationCatalog(void);
int e2ePayloadAudit(char* project);
btrc_Map_string_string* e2eGraphArgs(CliArgs* args, int startIndex);
btrc_Vector_string* e2eGraphTargets(CliArgs* args, int startIndex);
int e2eGraphCommand(CliArgs* args);
bool e2eOk(bool condition, char* message);
bool e2eContains(char* text, char* expected, char* message);
char* e2eTempDir(char* label);
void e2eSeedFile(char* path);
bool e2eAssertLink(char* linkPath, char* targetPath);
ExecResult* e2eRunLifecycle(char* exe, char* home, char* mode, char* project, char* romsDir);
ExecResult* e2eRunLifecycleChange(char* exe, char* home, char* project, char* actionId, char* commandText);
ExecResult* e2eRunLifecycleArgs(char* exe, char* home, char* project, btrc_Vector_string* lifecycleArgs);
ExecResult* e2eRunSync(char* exe, char* home, char* binDir, char* project, btrc_Vector_string* syncArgs);
ExecResult* e2eRunSteamInput(char* exe, char* home, char* project, btrc_Vector_string* steamInputArgs);
ExecResult* e2eRunLauncher(char* exe, char* home, char* project, char* roms, char* binDir, char* captureDir, char* bwrapPath, char* emulator, btrc_Vector_string* emulatorArgs);
ExecResult* e2eRunRoutedLauncher(char* exe, char* home, char* project, char* roms, char* binDir, char* captureDir, char* emulator, char* routedExe, btrc_Vector_string* emulatorArgs);
bool e2eRunOk(ExecResult* result, char* label);
bool e2eWaitForFile(char* path, char* label);
bool e2eCatalogConsistency(char* project, char* launcherBin);
int e2eLifecycleSmoke(CliArgs* args);
bool e2ePrepareSandbox(char* project, char* scratchRoot, char* emulator);
int e2eSandboxSmoke(void);
void e2eWriteSyncFakes(char* binDir);
int e2eSyncSmoke(CliArgs* args);
int e2eLauncherSmoke(CliArgs* args);
bool e2eWriteFakeN3dsRom(char* path, bool noCrypto, bool decrypted);
ExecResult* e2eRunN3dsNoCrypto(char* exe, char* input, char* outputDir, btrc_Vector_string* extraArgs);
ExecResult* e2eRunDecrypt3dsNoCrypto(char* exe, char* input, btrc_Vector_string* extraArgs);
int e2eN3dsNoCryptoSmoke(CliArgs* args);
int e2eShellSyntaxSmoke(char* project);
void printUsage(void);
int keymapCommand(CliArgs* args);
int screenshotCommand(CliArgs* args, char* project);
int syncCommand(CliArgs* args, char* project);
int lifecycleCommand(CliArgs* args, char* project);
int deckLaunch(char* project);
int deckCommand(CliArgs* args, char* project);
int appRunCommand(CliArgs* args, char* project);
char* steamInputTemplateDir(CliArgs* args);
void copySteamInputTemplate(char* project, char* destination, char* name);
int steamInputCommand(CliArgs* args, char* project);
int configCommand(CliArgs* args, char* project);
char* launcherNameFromProgram(char* program);
char* Strings_copy(char* s);
char* Strings_replace(char* s, char* old, char* replacement);
btrc_Vector_string* Strings_split(char* s, char* delim);
bool Strings_isDigit(char c);
int Strings_toInt(char* s);
int Strings_find(char* s, char* sub, int start);
char* Strings_removePrefix(char* s, char* prefix);
char* Strings_fromInt(int n);
void File_init(File* self, char* path, char* mode);
File* File_new(char* path, char* mode);
bool File_ok(File* self);
char* File_read(File* self);
void File_write(File* self, char* text);
void File_close(File* self);
char* Path_readAll(char* path);
void Path_writeAll(char* path, char* content);
int UnixPlatform_euid(void);
int Platform_euid(void);
char* Environment_get(char* name, char* fallback);
bool Environment_has(char* name);
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
void ProcessStatus_init(ProcessStatus* self, int raw);
ProcessStatus* ProcessStatus_new(int raw);
int ProcessStatus_code(ProcessStatus* self);
void UnixPipe_init(UnixPipe* self, char* command);
UnixPipe* UnixPipe_new(char* command);
bool UnixPipe_ok(UnixPipe* self);
char* UnixPipe_readAll(UnixPipe* self);
ProcessStatus* UnixPipe_close(UnixPipe* self);
ProcessStatus* UnixProcess_system(char* command);
UnixPipe* UnixProcess_pipe(char* command);
bool ShellWords_isSafeArgChar(char c);
bool ShellWords_isSafeArg(char* raw);
char* ShellWords_quote(char* raw);
char* ShellWords_redact(char* text, char* sensitive);
void ExecResult_init(ExecResult* self, int code, char* out, char* err, char* command);
ExecResult* ExecResult_new(int code, char* out, char* err, char* command);
bool ExecResult_ok(ExecResult* self);
char* ExecResult_stdout(ExecResult* self);
char* ExecResult_trimmed(ExecResult* self);
void Command_init(Command* self, char* executable);
Command* Command_new(char* executable);
Command* Command_arg(Command* self, char* value);
Command* Command_flag(Command* self, char* name, char* value);
Command* Command_envVar(Command* self, char* name, char* value);
Command* Command_capture(Command* self, bool enabled);
Command* Command_check(Command* self, bool enabled);
char* Command_renderEnv(Command* self, char* item);
char* Command_render(Command* self);
void UnixShell_init(UnixShell* self);
UnixShell* UnixShell_new(void);
char* UnixShell_redactText(char* text, char* sensitive);
ExecResult* UnixShell_run(UnixShell* self, char* command);
ExecResult* UnixShell_runUnchecked(UnixShell* self, char* command);
ExecResult* UnixShell_runCommand(UnixShell* self, Command* command);
ExecResult* UnixShell_runRaw(UnixShell* self, char* command, bool captureOutput, bool checkStatus, char* sensitive);
char* mkdtemp(char* templatePath);
void FileStatus_init(FileStatus* self, char* path);
FileStatus* FileStatus_new(char* path);
bool FileStatus_exists(FileStatus* self);
bool FileStatus_isDir(FileStatus* self);
bool FileStatus_isFile(FileStatus* self);
bool FileStatus_isSymlink(FileStatus* self);
void Directory_init(Directory* self, char* path);
Directory* Directory_new(char* path);
btrc_Vector_string* Directory_entries(Directory* self);
int UnixFileSystem_statusCode(int raw);
int UnixFileSystem_chmodPath(char* path, int mode);
int UnixFileSystem_mkdirPath(char* path, int mode);
int UnixFileSystem_runShell(char* command);
int UnixFileSystem_mkdirp(char* path);
int UnixFileSystem_removeRecursive(char* path);
int UnixFileSystem_symlinkPath(char* target, char* linkPath);
char* UnixFileSystem_readLink(char* path);
char* UnixFileSystem_tempDir(char* prefix);
char* PathTools_shellQuote(char* raw);
char* PathTools_basename(char* path);
char* PathTools_dirname(char* path);
char* PathTools_join(char* left, char* right);
bool FileSystem_exists(char* path);
bool FileSystem_isDir(char* path);
bool FileSystem_isFile(char* path);
bool FileSystem_isSymlink(char* path);
int FileSystem_chmod(char* path, int mode);
int FileSystem_mkdir(char* path, int mode);
int FileSystem_mkdirp(char* path);
int FileSystem_removeRecursive(char* path);
int FileSystem_symlink(char* target, char* linkPath);
char* FileSystem_readLink(char* path);
char* FileSystem_tempDir(char* prefix);
btrc_Vector_string* FileSystem_listDir(char* path);
char* FileSystem_readText(char* path);
void FileSystem_writeText(char* path, char* content);
void JsonObject_init(JsonObject* self);
JsonObject* JsonObject_new(void);
char* JsonObject_escape(char* text);
char* JsonObject_unescape(char* text);
void JsonObject_setString(JsonObject* self, char* key, char* value);
void JsonObject_setRaw(JsonObject* self, char* key, char* value);
char* JsonObject_getString(JsonObject* self, char* key, char* fallback);
bool JsonObject_getBool(JsonObject* self, char* key, bool fallback);
int JsonObject_getInt(JsonObject* self, char* key, int fallback);
char* JsonObject_stringify(JsonObject* self);
int JsonObject_skipSpaces(char* text, int i);
char* JsonObject_slice(char* text, int start, int end);
int JsonObject_stringEnd(char* text, int start);
JsonObject* JsonObject_parse(char* text);
JsonObject* JsonObject_readFile(char* path);
int JsonText_skipSpaces(char* text, int i);
int JsonText_keyPosition(char* text, char* key);
int JsonText_valueStart(char* text, char* key);
char* JsonText_parseStringValue(char* text, int i, char* fallback);
char* JsonText_field(char* text, char* key, char* fallback);
int JsonText_intField(char* text, char* key, int fallback);
char* JsonText_objectField(char* text, char* key);
btrc_Vector_string* JsonText_stringArray(char* text, char* key);
btrc_Vector_string* JsonText_objectArray(char* text, char* key);
btrc_Map_string_string* JsonText_objectMap(char* objectText);
btrc_Map_string_string* JsonText_argsObject(char* text);
char* JsonText_expand(char* text, btrc_Map_string_string* args);
void JsonText_putArgPair(btrc_Map_string_string* result, char* pair);
void CliArgs_init(CliArgs* self, int argc, char** argv);
CliArgs* CliArgs_new(int argc, char** argv);
int CliArgs_count(CliArgs* self);
char* CliArgs_get(CliArgs* self, int index);
char* CliArgs_command(CliArgs* self);
bool CliArgs_has(CliArgs* self, char* flag);
char* CliArgs_valueAfter(CliArgs* self, char* flag, char* fallback);
void Console_log(char* msg);
void Console_error(char* msg);
void GraphNode_init(GraphNode* self);
GraphNode* GraphNode_new(void);
void ExecutionGraph_init(ExecutionGraph* self);
ExecutionGraph* ExecutionGraph_new(void);
char* ExecutionGraph_resolvePath(ExecutionGraph* self, char* base, char* value);
char* ExecutionGraph_resolvedWorkspaceRoot(ExecutionGraph* self);
char* ExecutionGraph_resolvedSpecPath(ExecutionGraph* self, GraphNode* node);
GraphNode* ExecutionGraph_node(ExecutionGraph* self, char* id);
btrc_Vector_string* ExecutionGraph_defaultTargets(ExecutionGraph* self);
GraphNode* GraphParser_node(char* objectText);
ExecutionGraph* GraphParser_readFile(char* path);
btrc_Map_string_string* GraphCli_args(CliArgs* args, int startIndex);
btrc_Vector_string* GraphCli_targets(CliArgs* args, int startIndex);
void GraphReport_list(ExecutionGraph* graph);
void GraphTraversal_init(GraphTraversal* self, ExecutionGraph* graph);
GraphTraversal* GraphTraversal_new(ExecutionGraph* graph);
int GraphTraversal_visit(GraphTraversal* self, char* id);
btrc_Vector_string* GraphTraversal_order(GraphTraversal* self, btrc_Vector_string* targets);
bool GraphTraversal_ok(GraphTraversal* self);
void SystemCatalog_init(SystemCatalog* self);
SystemCatalog* SystemCatalog_new(void);
void SystemCatalog_addSystem(SystemCatalog* self, char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile);
char* SystemCatalog_systemsJson(SystemCatalog* self);
char* SystemCatalog_esSystemsXml(SystemCatalog* self);
void KeymapErrors_init(KeymapErrors* self);
KeymapErrors* KeymapErrors_new(void);
void KeymapErrors_error(KeymapErrors* self, int line, int column, char* message);
int KeymapErrors_count(KeymapErrors* self);
void KeymapTokens_init(KeymapTokens* self);
KeymapTokens* KeymapTokens_new(void);
void KeymapTokens_push(KeymapTokens* self, char* kind, char* text, int line, int column);
int KeymapTokens_count(KeymapTokens* self);
void KeymapIr_init(KeymapIr* self);
KeymapIr* KeymapIr_new(void);
void KeymapIr_addAction(KeymapIr* self, char* id, char* command);
void KeymapIr_addBinding(KeymapIr* self, char* combo, char* action);
void KeymapParser_init(KeymapParser* self, KeymapTokens* tokens, KeymapErrors* errors);
KeymapParser* KeymapParser_new(KeymapTokens* tokens, KeymapErrors* errors);
char* KeymapParser_kind(KeymapParser* self);
char* KeymapParser_text(KeymapParser* self);
void KeymapParser_errorHere(KeymapParser* self, char* message);
bool KeymapParser_accept(KeymapParser* self, char* kind);
void KeymapParser_skipLine(KeymapParser* self);
char* KeymapParser_valueToken(KeymapParser* self, char* expected);
char* KeymapParser_chordCommand(KeymapParser* self);
KeymapIr* KeymapParser_parse(KeymapParser* self);
void BinaryEditor_init(BinaryEditor* self, char* path, char* mode);
BinaryEditor* BinaryEditor_new(char* path, char* mode);
bool BinaryEditor_ok(BinaryEditor* self);
bool BinaryEditor_seek(BinaryEditor* self, long offset);
int BinaryEditor_readU8(BinaryEditor* self, long offset);
long BinaryEditor_readLe32(BinaryEditor* self, long offset);
char* BinaryEditor_readAscii(BinaryEditor* self, long offset, int count);
bool BinaryEditor_writeU8(BinaryEditor* self, long offset, int value);
bool BinaryEditor_writeLe32(BinaryEditor* self, long offset, long value);
bool BinaryEditor_writeAscii(BinaryEditor* self, long offset, char* text);
void BinaryEditor_close(BinaryEditor* self);
void BinaryReader_init(BinaryReader* self, char* path);
BinaryReader* BinaryReader_new(char* path);
bool BinaryReader_ok(BinaryReader* self);
bool BinaryReader_seek(BinaryReader* self, long offset);
int BinaryReader_readU8(BinaryReader* self, long offset);
long BinaryReader_readLe32(BinaryReader* self, long offset);
char* BinaryReader_readAscii(BinaryReader* self, long offset, int count);
void BinaryReader_close(BinaryReader* self);
void N3dsRomCheck_init(N3dsRomCheck* self);
N3dsRomCheck* N3dsRomCheck_new(void);
void SemuE2eOperation_init(SemuE2eOperation* self);
SemuE2eOperation* SemuE2eOperation_new(void);
void SemuE2eOperation_expandArgs(SemuE2eOperation* self, btrc_Map_string_string* args);
void SemuE2eSpec_init(SemuE2eSpec* self);
SemuE2eSpec* SemuE2eSpec_new(void);
void SemuE2eSpec_setArg(SemuE2eSpec* self, char* key, char* value);
char* SemuE2eSpec_stateDir(SemuE2eSpec* self);
char* SemuE2eSpec_stateHashFile(SemuE2eSpec* self);
char* SemuE2eSpec_parentHashFile(SemuE2eSpec* self);
char* SemuE2eSpec_resolveParentHash(SemuE2eSpec* self);
char* SemuE2eSpec_operationsMaterial(SemuE2eSpec* self);
char* SemuE2eSpec_hashMaterial(SemuE2eSpec* self);
void SemuE2eSpec_computeStateHash(SemuE2eSpec* self);
void SemuE2eSpec_setDerivedArgs(SemuE2eSpec* self);
void SemuE2eSpec_refreshDerivedArgs(SemuE2eSpec* self);
void SemuE2eSpec_expandArgs(SemuE2eSpec* self);
void SemuE2eSpec_recordState(SemuE2eSpec* self);
char* SemuE2eParser_field(char* text, char* key, char* fallback);
int SemuE2eParser_intField(char* text, char* key, int fallback);
btrc_Vector_string* SemuE2eParser_objectArray(char* text, char* key);
btrc_Map_string_string* SemuE2eParser_argsObject(char* text);
SemuE2eOperation* SemuE2eParser_operation(char* objectText);
btrc_Vector_SemuE2eOperation* SemuE2eParser_operations(char* text);
SemuE2eSpec* SemuE2eParser_readSpecFile(char* path);
ExecutionGraph* SemuE2eParser_readGraphFile(char* path);
void SemuE2eRunner_init(SemuE2eRunner* self, SemuE2eSpec* spec, char* workspaceRoot, char* program);
SemuE2eRunner* SemuE2eRunner_new(SemuE2eSpec* spec, char* workspaceRoot, char* program);
void SemuE2eRunner_fail(SemuE2eRunner* self, char* message);
bool SemuE2eRunner_outputMatches(SemuE2eRunner* self, ExecResult* result, char* expect);
void SemuE2eRunner_assertResult(SemuE2eRunner* self, char* label, ExecResult* result, char* expect);
ExecResult* SemuE2eRunner_runWorkspaceCommandWithTimeout(SemuE2eRunner* self, char* command, int timeout);
ExecResult* SemuE2eRunner_runWorkspaceCommand(SemuE2eRunner* self, char* command);
void SemuE2eRunner_runOperation(SemuE2eRunner* self, SemuE2eOperation* op);
int SemuE2eRunner_run(SemuE2eRunner* self);
void SemuE2eGraphRunner_init(SemuE2eGraphRunner* self, ExecutionGraph* graph, btrc_Map_string_string* args, char* program);
SemuE2eGraphRunner* SemuE2eGraphRunner_new(ExecutionGraph* graph, btrc_Map_string_string* args, char* program);
char* SemuE2eGraphRunner_sourceHash(SemuE2eGraphRunner* self);
void SemuE2eGraphRunner_applyStructuralOverrides(SemuE2eGraphRunner* self, SemuE2eSpec* spec, btrc_Map_string_string* overrides);
SemuE2eSpec* SemuE2eGraphRunner_specFor(SemuE2eGraphRunner* self, GraphNode* node);
bool SemuE2eGraphRunner_force(SemuE2eGraphRunner* self);
bool SemuE2eGraphRunner_ready(SemuE2eGraphRunner* self, SemuE2eSpec* spec);
void SemuE2eGraphRunner_list(SemuE2eGraphRunner* self);
void SemuE2eGraphRunner_status(SemuE2eGraphRunner* self);
int SemuE2eGraphRunner_operationCoverage(SemuE2eGraphRunner* self);
int SemuE2eGraphRunner_run(SemuE2eGraphRunner* self, btrc_Vector_string* targets);
typedef bool (*__btrc_fn_bool_string)(char*);
typedef void (*__btrc_fn_void_string)(char*);
typedef char* (*__btrc_fn_string_string)(char*);
typedef char* (*__btrc_fn_string_string_string)(char*, char*);
typedef bool (*__btrc_fn_bool_bool)(bool);
typedef void (*__btrc_fn_void_bool)(bool);
typedef bool (*__btrc_fn_bool_bool_bool)(bool, bool);
typedef bool (*__btrc_fn_bool_GraphNode)(GraphNode*);
typedef void (*__btrc_fn_void_GraphNode)(GraphNode*);
typedef GraphNode* (*__btrc_fn_GraphNode_GraphNode)(GraphNode*);
typedef GraphNode* (*__btrc_fn_GraphNode_GraphNode_GraphNode)(GraphNode*, GraphNode*);
typedef bool (*__btrc_fn_bool_int)(int);
typedef void (*__btrc_fn_void_int)(int);
typedef int (*__btrc_fn_int_int)(int);
typedef int (*__btrc_fn_int_int_int)(int, int);
typedef bool (*__btrc_fn_bool_SemuE2eOperation)(SemuE2eOperation*);
typedef void (*__btrc_fn_void_SemuE2eOperation)(SemuE2eOperation*);
typedef SemuE2eOperation* (*__btrc_fn_SemuE2eOperation_SemuE2eOperation)(SemuE2eOperation*);
typedef SemuE2eOperation* (*__btrc_fn_SemuE2eOperation_SemuE2eOperation_SemuE2eOperation)(SemuE2eOperation*, SemuE2eOperation*);

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

struct btrc_Vector_GraphNode {
    int __rc;
    GraphNode** data;
    int len;
    int cap;
};

struct btrc_Vector_int {
    int __rc;
    int* data;
    int len;
    int cap;
};

struct btrc_Vector_SemuE2eOperation {
    int __rc;
    SemuE2eOperation** data;
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

struct Strings {
    int __rc;
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

struct ExecResult {
    int __rc;
    int code;
    char* out;
    char* err;
    char* command;
};

struct Command {
    int __rc;
    char* executable;
    btrc_Vector_string* args;
    btrc_Vector_string* env;
    bool useSudo;
    bool captureOutput;
    bool checkStatus;
    bool mergeStderr;
    char* sensitive;
};

struct UnixShell {
    int __rc;
    bool logCommands;
    char* chrootPath;
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

struct Console {
    int __rc;
};

struct GraphNode {
    int __rc;
    char* id;
    char* specPath;
    btrc_Vector_string* after;
    btrc_Map_string_string* args;
};

struct ExecutionGraph {
    int __rc;
    char* name;
    char* path;
    char* baseDir;
    char* workspaceRoot;
    btrc_Vector_string* defaults;
    btrc_Vector_GraphNode* nodes;
};

struct GraphParser {
    int __rc;
};

struct GraphCli {
    int __rc;
};

struct GraphReport {
    int __rc;
};

struct GraphTraversal {
    int __rc;
    ExecutionGraph* graph;
    btrc_Vector_string* done;
    btrc_Vector_string* visiting;
    btrc_Vector_string* ordered;
    char* error;
};

struct SemuJson {
    int __rc;
};

struct SemuShell {
    int __rc;
};

struct SemuManifestCompiler {
    int __rc;
};

struct SystemCatalog {
    int __rc;
    btrc_Vector_string* ids;
    btrc_Vector_string* romDirs;
    btrc_Vector_string* jsonSpecs;
    btrc_Vector_string* esSystemSpecs;
};

struct SemuKeymapLanguage {
    int __rc;
};

struct KeymapErrors {
    int __rc;
    btrc_Vector_string* levels;
    btrc_Vector_int* lines;
    btrc_Vector_int* columns;
    btrc_Vector_string* messages;
};

struct KeymapTokens {
    int __rc;
    btrc_Vector_string* kinds;
    btrc_Vector_string* texts;
    btrc_Vector_int* lines;
    btrc_Vector_int* columns;
};

struct KeymapIr {
    int __rc;
    btrc_Vector_string* actionIds;
    btrc_Vector_string* actionCommands;
    btrc_Vector_string* bindingCombos;
    btrc_Vector_string* bindingActions;
};

struct KeymapParser {
    int __rc;
    KeymapTokens* tokens;
    KeymapErrors* errors;
    int index;
};

struct SemuPaths {
    int __rc;
};

struct SemuLauncher {
    int __rc;
};

struct SemuEmulatorMetadata {
    int __rc;
};

struct SemuEmulatorSandbox {
    int __rc;
};

struct SemuEmulatorProfiles {
    int __rc;
};

struct SemuProjectBootstrap {
    int __rc;
};

struct SemuDoctor {
    int __rc;
};

struct BinaryEditor {
    int __rc;
    FILE* handle;
    bool opened;
};

struct BinaryReader {
    int __rc;
    FILE* handle;
    bool opened;
};

struct SemuN3dsTools {
    int __rc;
};

struct N3dsRomCheck {
    int __rc;
    char* status;
    char* note;
    int partitions;
};

struct SemuScreenshotVerification {
    int __rc;
};

struct SemuSyncRuntime {
    int __rc;
};

struct SemuAppImageSmoke {
    int __rc;
};

struct SemuDeckTests {
    int __rc;
};

struct SemuE2eCommand {
    int __rc;
};

struct SemuE2eOperation {
    int __rc;
    char* kind;
    char* command;
    char* expect;
    char* name;
    char* path;
    int timeout;
};

struct SemuE2eSpec {
    int __rc;
    char* name;
    char* state;
    char* parentState;
    char* stateRoot;
    char* stateMaterial;
    char* parentHash;
    char* stateHash;
    char* stateHashShort;
    btrc_Map_string_string* args;
    btrc_Vector_SemuE2eOperation* operations;
};

struct SemuE2eParser {
    int __rc;
};

struct SemuE2eRunner {
    int __rc;
    SemuE2eSpec* spec;
    char* workspaceRoot;
    char* program;
    int failures;
};

struct SemuE2eGraphRunner {
    int __rc;
    ExecutionGraph* graph;
    btrc_Map_string_string* args;
    char* sourceHashValue;
    char* program;
};

struct SemuSmokeTests {
    int __rc;
};

struct SemuCliCommands {
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

static void btrc_Vector_GraphNode_init(btrc_Vector_GraphNode* self);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_new(void);
static void btrc_Vector_GraphNode_destroy(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_push(btrc_Vector_GraphNode* self, GraphNode* val);
static GraphNode* btrc_Vector_GraphNode_pop(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_get(btrc_Vector_GraphNode* self, int i);
static void btrc_Vector_GraphNode_set(btrc_Vector_GraphNode* self, int i, GraphNode* val);
static void btrc_Vector_GraphNode_free(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_remove(btrc_Vector_GraphNode* self, int idx);
static void btrc_Vector_GraphNode_reverse(btrc_Vector_GraphNode* self);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_reversed(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_swap(btrc_Vector_GraphNode* self, int i, int j);
static void btrc_Vector_GraphNode_clear(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_fill(btrc_Vector_GraphNode* self, GraphNode* val);
static int btrc_Vector_GraphNode_size(btrc_Vector_GraphNode* self);
static bool btrc_Vector_GraphNode_isEmpty(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_first(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_last(btrc_Vector_GraphNode* self);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_slice(btrc_Vector_GraphNode* self, int start, int end);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_take(btrc_Vector_GraphNode* self, int n);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_drop(btrc_Vector_GraphNode* self, int n);
static void btrc_Vector_GraphNode_extend(btrc_Vector_GraphNode* self, btrc_Vector_GraphNode* other);
static void btrc_Vector_GraphNode_insert(btrc_Vector_GraphNode* self, int idx, GraphNode* val);
static bool btrc_Vector_GraphNode_contains(btrc_Vector_GraphNode* self, GraphNode* val);
static int btrc_Vector_GraphNode_indexOf(btrc_Vector_GraphNode* self, GraphNode* val);
static int btrc_Vector_GraphNode_lastIndexOf(btrc_Vector_GraphNode* self, GraphNode* val);
static int btrc_Vector_GraphNode_count(btrc_Vector_GraphNode* self, GraphNode* val);
static void btrc_Vector_GraphNode_removeAll(btrc_Vector_GraphNode* self, GraphNode* val);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_distinct(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_sort(btrc_Vector_GraphNode* self);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_sorted(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_min(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_max(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_sum(btrc_Vector_GraphNode* self);
static char* btrc_Vector_GraphNode_join(btrc_Vector_GraphNode* self, char* sep);
static char* btrc_Vector_GraphNode_joinToString(btrc_Vector_GraphNode* self, char* sep);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_filter(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred);
static int btrc_Vector_GraphNode_findIndex(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred);
static void btrc_Vector_GraphNode_forEach(btrc_Vector_GraphNode* self, __btrc_fn_void_GraphNode fn);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_map(btrc_Vector_GraphNode* self, __btrc_fn_GraphNode_GraphNode fn);
static bool btrc_Vector_GraphNode_any(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred);
static bool btrc_Vector_GraphNode_all(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred);
static GraphNode* btrc_Vector_GraphNode_reduce(btrc_Vector_GraphNode* self, GraphNode* init, __btrc_fn_GraphNode_GraphNode_GraphNode fn);
static btrc_Vector_GraphNode* btrc_Vector_GraphNode_copy(btrc_Vector_GraphNode* self);
static void btrc_Vector_GraphNode_removeAt(btrc_Vector_GraphNode* self, int idx);
static int btrc_Vector_GraphNode_iterLen(btrc_Vector_GraphNode* self);
static GraphNode* btrc_Vector_GraphNode_iterGet(btrc_Vector_GraphNode* self, int i);

static void btrc_Vector_int_init(btrc_Vector_int* self);
static btrc_Vector_int* btrc_Vector_int_new(void);
static void btrc_Vector_int_destroy(btrc_Vector_int* self);
static void btrc_Vector_int_push(btrc_Vector_int* self, int val);
static int btrc_Vector_int_pop(btrc_Vector_int* self);
static int btrc_Vector_int_get(btrc_Vector_int* self, int i);
static void btrc_Vector_int_set(btrc_Vector_int* self, int i, int val);
static void btrc_Vector_int_free(btrc_Vector_int* self);
static void btrc_Vector_int_remove(btrc_Vector_int* self, int idx);
static void btrc_Vector_int_reverse(btrc_Vector_int* self);
static btrc_Vector_int* btrc_Vector_int_reversed(btrc_Vector_int* self);
static void btrc_Vector_int_swap(btrc_Vector_int* self, int i, int j);
static void btrc_Vector_int_clear(btrc_Vector_int* self);
static void btrc_Vector_int_fill(btrc_Vector_int* self, int val);
static int btrc_Vector_int_size(btrc_Vector_int* self);
static bool btrc_Vector_int_isEmpty(btrc_Vector_int* self);
static int btrc_Vector_int_first(btrc_Vector_int* self);
static int btrc_Vector_int_last(btrc_Vector_int* self);
static btrc_Vector_int* btrc_Vector_int_slice(btrc_Vector_int* self, int start, int end);
static btrc_Vector_int* btrc_Vector_int_take(btrc_Vector_int* self, int n);
static btrc_Vector_int* btrc_Vector_int_drop(btrc_Vector_int* self, int n);
static void btrc_Vector_int_extend(btrc_Vector_int* self, btrc_Vector_int* other);
static void btrc_Vector_int_insert(btrc_Vector_int* self, int idx, int val);
static bool btrc_Vector_int_contains(btrc_Vector_int* self, int val);
static int btrc_Vector_int_indexOf(btrc_Vector_int* self, int val);
static int btrc_Vector_int_lastIndexOf(btrc_Vector_int* self, int val);
static int btrc_Vector_int_count(btrc_Vector_int* self, int val);
static void btrc_Vector_int_removeAll(btrc_Vector_int* self, int val);
static btrc_Vector_int* btrc_Vector_int_distinct(btrc_Vector_int* self);
static void btrc_Vector_int_sort(btrc_Vector_int* self);
static btrc_Vector_int* btrc_Vector_int_sorted(btrc_Vector_int* self);
static int btrc_Vector_int_min(btrc_Vector_int* self);
static int btrc_Vector_int_max(btrc_Vector_int* self);
static int btrc_Vector_int_sum(btrc_Vector_int* self);
static char* btrc_Vector_int_join(btrc_Vector_int* self, char* sep);
static char* btrc_Vector_int_joinToString(btrc_Vector_int* self, char* sep);
static btrc_Vector_int* btrc_Vector_int_filter(btrc_Vector_int* self, __btrc_fn_bool_int pred);
static int btrc_Vector_int_findIndex(btrc_Vector_int* self, __btrc_fn_bool_int pred);
static void btrc_Vector_int_forEach(btrc_Vector_int* self, __btrc_fn_void_int fn);
static btrc_Vector_int* btrc_Vector_int_map(btrc_Vector_int* self, __btrc_fn_int_int fn);
static bool btrc_Vector_int_any(btrc_Vector_int* self, __btrc_fn_bool_int pred);
static bool btrc_Vector_int_all(btrc_Vector_int* self, __btrc_fn_bool_int pred);
static int btrc_Vector_int_reduce(btrc_Vector_int* self, int init, __btrc_fn_int_int_int fn);
static btrc_Vector_int* btrc_Vector_int_copy(btrc_Vector_int* self);
static void btrc_Vector_int_removeAt(btrc_Vector_int* self, int idx);
static int btrc_Vector_int_iterLen(btrc_Vector_int* self);
static int btrc_Vector_int_iterGet(btrc_Vector_int* self, int i);

static void btrc_Vector_SemuE2eOperation_init(btrc_Vector_SemuE2eOperation* self);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_new(void);
static void btrc_Vector_SemuE2eOperation_destroy(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_push(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_pop(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_get(btrc_Vector_SemuE2eOperation* self, int i);
static void btrc_Vector_SemuE2eOperation_set(btrc_Vector_SemuE2eOperation* self, int i, SemuE2eOperation* val);
static void btrc_Vector_SemuE2eOperation_free(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_remove(btrc_Vector_SemuE2eOperation* self, int idx);
static void btrc_Vector_SemuE2eOperation_reverse(btrc_Vector_SemuE2eOperation* self);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_reversed(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_swap(btrc_Vector_SemuE2eOperation* self, int i, int j);
static void btrc_Vector_SemuE2eOperation_clear(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_fill(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static int btrc_Vector_SemuE2eOperation_size(btrc_Vector_SemuE2eOperation* self);
static bool btrc_Vector_SemuE2eOperation_isEmpty(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_first(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_last(btrc_Vector_SemuE2eOperation* self);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_slice(btrc_Vector_SemuE2eOperation* self, int start, int end);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_take(btrc_Vector_SemuE2eOperation* self, int n);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_drop(btrc_Vector_SemuE2eOperation* self, int n);
static void btrc_Vector_SemuE2eOperation_extend(btrc_Vector_SemuE2eOperation* self, btrc_Vector_SemuE2eOperation* other);
static void btrc_Vector_SemuE2eOperation_insert(btrc_Vector_SemuE2eOperation* self, int idx, SemuE2eOperation* val);
static bool btrc_Vector_SemuE2eOperation_contains(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static int btrc_Vector_SemuE2eOperation_indexOf(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static int btrc_Vector_SemuE2eOperation_lastIndexOf(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static int btrc_Vector_SemuE2eOperation_count(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static void btrc_Vector_SemuE2eOperation_removeAll(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_distinct(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_sort(btrc_Vector_SemuE2eOperation* self);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_sorted(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_min(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_max(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_sum(btrc_Vector_SemuE2eOperation* self);
static char* btrc_Vector_SemuE2eOperation_join(btrc_Vector_SemuE2eOperation* self, char* sep);
static char* btrc_Vector_SemuE2eOperation_joinToString(btrc_Vector_SemuE2eOperation* self, char* sep);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_filter(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred);
static int btrc_Vector_SemuE2eOperation_findIndex(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred);
static void btrc_Vector_SemuE2eOperation_forEach(btrc_Vector_SemuE2eOperation* self, __btrc_fn_void_SemuE2eOperation fn);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_map(btrc_Vector_SemuE2eOperation* self, __btrc_fn_SemuE2eOperation_SemuE2eOperation fn);
static bool btrc_Vector_SemuE2eOperation_any(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred);
static bool btrc_Vector_SemuE2eOperation_all(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_reduce(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* init, __btrc_fn_SemuE2eOperation_SemuE2eOperation_SemuE2eOperation fn);
static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_copy(btrc_Vector_SemuE2eOperation* self);
static void btrc_Vector_SemuE2eOperation_removeAt(btrc_Vector_SemuE2eOperation* self, int idx);
static int btrc_Vector_SemuE2eOperation_iterLen(btrc_Vector_SemuE2eOperation* self);
static SemuE2eOperation* btrc_Vector_SemuE2eOperation_iterGet(btrc_Vector_SemuE2eOperation* self, int i);

static void btrc_Map_string_string_init(btrc_Map_string_string* self);
static btrc_Map_string_string* btrc_Map_string_string_new(void);
static void btrc_Map_string_string_destroy(btrc_Map_string_string* self);
static void btrc_Map_string_string_resize(btrc_Map_string_string* self);
static void btrc_Map_string_string_put(btrc_Map_string_string* self, char* key, char* value);
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

static void btrc_Vector_GraphNode_init(btrc_Vector_GraphNode* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_new(void) {
    btrc_Vector_GraphNode* self = ((btrc_Vector_GraphNode*)malloc(sizeof(btrc_Vector_GraphNode)));
    memset(self, 0, sizeof(btrc_Vector_GraphNode));
    btrc_Vector_GraphNode_init(self);
    return self;
}

static void btrc_Vector_GraphNode_destroy(btrc_Vector_GraphNode* self) {
    free(self);
}

static void btrc_Vector_GraphNode_push(btrc_Vector_GraphNode* self, GraphNode* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((GraphNode**)__btrc_safe_realloc(self->data, (sizeof(GraphNode*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static GraphNode* btrc_Vector_GraphNode_pop(btrc_Vector_GraphNode* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static GraphNode* btrc_Vector_GraphNode_get(btrc_Vector_GraphNode* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_GraphNode_set(btrc_Vector_GraphNode* self, int i, GraphNode* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            GraphNode_destroy(self->data[i]);
        }
    }
    (val->__rc++);
    (self->data[i] = val);
}

static void btrc_Vector_GraphNode_free(btrc_Vector_GraphNode* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                GraphNode_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_GraphNode_remove(btrc_Vector_GraphNode* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            GraphNode_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_GraphNode_reverse(btrc_Vector_GraphNode* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        GraphNode* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_reversed(btrc_Vector_GraphNode* self) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_GraphNode_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_GraphNode_swap(btrc_Vector_GraphNode* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    GraphNode* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_GraphNode_clear(btrc_Vector_GraphNode* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                GraphNode_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_GraphNode_fill(btrc_Vector_GraphNode* self, GraphNode* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                GraphNode_destroy(self->data[i]);
            }
        }
        (val->__rc++);
        (self->data[i] = val);
    }
}

static int btrc_Vector_GraphNode_size(btrc_Vector_GraphNode* self) {
    return self->len;
}

static bool btrc_Vector_GraphNode_isEmpty(btrc_Vector_GraphNode* self) {
    return (self->len == 0);
}

static GraphNode* btrc_Vector_GraphNode_first(btrc_Vector_GraphNode* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static GraphNode* btrc_Vector_GraphNode_last(btrc_Vector_GraphNode* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_slice(btrc_Vector_GraphNode* self, int start, int end) {
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
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_GraphNode_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_take(btrc_Vector_GraphNode* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_GraphNode_slice(self, 0, n);
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_drop(btrc_Vector_GraphNode* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_GraphNode_slice(self, n, self->len);
}

static void btrc_Vector_GraphNode_extend(btrc_Vector_GraphNode* self, btrc_Vector_GraphNode* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_GraphNode_push(self, other->data[i]);
    }
}

static void btrc_Vector_GraphNode_insert(btrc_Vector_GraphNode* self, int idx, GraphNode* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((GraphNode**)__btrc_safe_realloc(self->data, (sizeof(GraphNode*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_GraphNode_contains(btrc_Vector_GraphNode* self, GraphNode* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_GraphNode_indexOf(btrc_Vector_GraphNode* self, GraphNode* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_GraphNode_lastIndexOf(btrc_Vector_GraphNode* self, GraphNode* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_GraphNode_count(btrc_Vector_GraphNode* self, GraphNode* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_GraphNode_removeAll(btrc_Vector_GraphNode* self, GraphNode* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                GraphNode_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_distinct(btrc_Vector_GraphNode* self) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_GraphNode_contains(result, self->data[i])) {
            btrc_Vector_GraphNode_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_GraphNode_sort(btrc_Vector_GraphNode* self) {
    for (int i = 1; (i < self->len); (i++)) {
        GraphNode* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_sorted(btrc_Vector_GraphNode* self) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_GraphNode_push(result, self->data[i]);
    }
    btrc_Vector_GraphNode_sort(result);
    return result;
}

static GraphNode* btrc_Vector_GraphNode_min(btrc_Vector_GraphNode* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    GraphNode* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static GraphNode* btrc_Vector_GraphNode_max(btrc_Vector_GraphNode* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    GraphNode* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_filter(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_GraphNode_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_GraphNode_findIndex(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_GraphNode_forEach(btrc_Vector_GraphNode* self, __btrc_fn_void_GraphNode fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_map(btrc_Vector_GraphNode* self, __btrc_fn_GraphNode_GraphNode fn) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_GraphNode_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_GraphNode_any(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_GraphNode_all(btrc_Vector_GraphNode* self, __btrc_fn_bool_GraphNode pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static GraphNode* btrc_Vector_GraphNode_reduce(btrc_Vector_GraphNode* self, GraphNode* init, __btrc_fn_GraphNode_GraphNode_GraphNode fn) {
    GraphNode* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_GraphNode* btrc_Vector_GraphNode_copy(btrc_Vector_GraphNode* self) {
    btrc_Vector_GraphNode* result = btrc_Vector_GraphNode_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_GraphNode_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_GraphNode_removeAt(btrc_Vector_GraphNode* self, int idx) {
    btrc_Vector_GraphNode_remove(self, idx);
}

static int btrc_Vector_GraphNode_iterLen(btrc_Vector_GraphNode* self) {
    return self->len;
}

static GraphNode* btrc_Vector_GraphNode_iterGet(btrc_Vector_GraphNode* self, int i) {
    return self->data[i];
}

static void btrc_Vector_int_init(btrc_Vector_int* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_int* btrc_Vector_int_new(void) {
    btrc_Vector_int* self = ((btrc_Vector_int*)malloc(sizeof(btrc_Vector_int)));
    memset(self, 0, sizeof(btrc_Vector_int));
    btrc_Vector_int_init(self);
    return self;
}

static void btrc_Vector_int_destroy(btrc_Vector_int* self) {
    free(self);
}

static void btrc_Vector_int_push(btrc_Vector_int* self, int val) {
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((int*)__btrc_safe_realloc(self->data, (sizeof(int) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static int btrc_Vector_int_pop(btrc_Vector_int* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static int btrc_Vector_int_get(btrc_Vector_int* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_int_set(btrc_Vector_int* self, int i, int val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    (self->data[i] = val);
}

static void btrc_Vector_int_free(btrc_Vector_int* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_int_remove(btrc_Vector_int* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_int_reverse(btrc_Vector_int* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        int tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_int* btrc_Vector_int_reversed(btrc_Vector_int* self) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_int_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_int_swap(btrc_Vector_int* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    int tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_int_clear(btrc_Vector_int* self) {
    for (int i = 0; (i < self->len); (i++)) {
    }
    (self->len = 0);
}

static void btrc_Vector_int_fill(btrc_Vector_int* self, int val) {
    for (int i = 0; (i < self->len); (i++)) {
        (self->data[i] = val);
    }
}

static int btrc_Vector_int_size(btrc_Vector_int* self) {
    return self->len;
}

static bool btrc_Vector_int_isEmpty(btrc_Vector_int* self) {
    return (self->len == 0);
}

static int btrc_Vector_int_first(btrc_Vector_int* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static int btrc_Vector_int_last(btrc_Vector_int* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_int* btrc_Vector_int_slice(btrc_Vector_int* self, int start, int end) {
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
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_int_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_int* btrc_Vector_int_take(btrc_Vector_int* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_int_slice(self, 0, n);
}

static btrc_Vector_int* btrc_Vector_int_drop(btrc_Vector_int* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_int_slice(self, n, self->len);
}

static void btrc_Vector_int_extend(btrc_Vector_int* self, btrc_Vector_int* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_int_push(self, other->data[i]);
    }
}

static void btrc_Vector_int_insert(btrc_Vector_int* self, int idx, int val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((int*)__btrc_safe_realloc(self->data, (sizeof(int) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_int_contains(btrc_Vector_int* self, int val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_int_indexOf(btrc_Vector_int* self, int val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_int_lastIndexOf(btrc_Vector_int* self, int val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_int_count(btrc_Vector_int* self, int val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_int_removeAll(btrc_Vector_int* self, int val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        }
    }
    (self->len = j);
}

static btrc_Vector_int* btrc_Vector_int_distinct(btrc_Vector_int* self) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_int_contains(result, self->data[i])) {
            btrc_Vector_int_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_int_sort(btrc_Vector_int* self) {
    for (int i = 1; (i < self->len); (i++)) {
        int key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_int* btrc_Vector_int_sorted(btrc_Vector_int* self) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_int_push(result, self->data[i]);
    }
    btrc_Vector_int_sort(result);
    return result;
}

static int btrc_Vector_int_min(btrc_Vector_int* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    int m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static int btrc_Vector_int_max(btrc_Vector_int* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    int m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static int btrc_Vector_int_sum(btrc_Vector_int* self) {
    int s = ((int)0);
    for (int i = 0; (i < self->len); (i++)) {
        (s = (s + self->data[i]));
    }
    return s;
}

static btrc_Vector_int* btrc_Vector_int_filter(btrc_Vector_int* self, __btrc_fn_bool_int pred) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_int_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_int_findIndex(btrc_Vector_int* self, __btrc_fn_bool_int pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_int_forEach(btrc_Vector_int* self, __btrc_fn_void_int fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_int* btrc_Vector_int_map(btrc_Vector_int* self, __btrc_fn_int_int fn) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_int_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_int_any(btrc_Vector_int* self, __btrc_fn_bool_int pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_int_all(btrc_Vector_int* self, __btrc_fn_bool_int pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static int btrc_Vector_int_reduce(btrc_Vector_int* self, int init, __btrc_fn_int_int_int fn) {
    int acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_int* btrc_Vector_int_copy(btrc_Vector_int* self) {
    btrc_Vector_int* result = btrc_Vector_int_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_int_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_int_removeAt(btrc_Vector_int* self, int idx) {
    btrc_Vector_int_remove(self, idx);
}

static int btrc_Vector_int_iterLen(btrc_Vector_int* self) {
    return self->len;
}

static int btrc_Vector_int_iterGet(btrc_Vector_int* self, int i) {
    return self->data[i];
}

static void btrc_Vector_SemuE2eOperation_init(btrc_Vector_SemuE2eOperation* self) {
    self->__rc = 1;
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_new(void) {
    btrc_Vector_SemuE2eOperation* self = ((btrc_Vector_SemuE2eOperation*)malloc(sizeof(btrc_Vector_SemuE2eOperation)));
    memset(self, 0, sizeof(btrc_Vector_SemuE2eOperation));
    btrc_Vector_SemuE2eOperation_init(self);
    return self;
}

static void btrc_Vector_SemuE2eOperation_destroy(btrc_Vector_SemuE2eOperation* self) {
    free(self);
}

static void btrc_Vector_SemuE2eOperation_push(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuE2eOperation**)__btrc_safe_realloc(self->data, (sizeof(SemuE2eOperation*) * self->cap))));
    }
    (self->data[self->len] = val);
    (self->len++);
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_pop(btrc_Vector_SemuE2eOperation* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector pop from empty list\n");
        exit(1);
    }
    (self->len--);
    return self->data[self->len];
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_get(btrc_Vector_SemuE2eOperation* self, int i) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    return self->data[i];
}

static void btrc_Vector_SemuE2eOperation_set(btrc_Vector_SemuE2eOperation* self, int i, SemuE2eOperation* val) {
    if ((i < 0) || (i >= self->len)) {
        fprintf(stderr, "Vector index out of bounds: %d (len=%d)\n", i, self->len);
        exit(1);
    }
    if (self->data[i]) {
        if ((--self->data[i]->__rc) <= 0) {
            SemuE2eOperation_destroy(self->data[i]);
        }
    }
    (val->__rc++);
    (self->data[i] = val);
}

static void btrc_Vector_SemuE2eOperation_free(btrc_Vector_SemuE2eOperation* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuE2eOperation_destroy(self->data[i]);
            }
        }
    }
    free(self->data);
    (self->data = NULL);
    (self->len = 0);
    (self->cap = 0);
}

static void btrc_Vector_SemuE2eOperation_remove(btrc_Vector_SemuE2eOperation* self, int idx) {
    if ((idx < 0) || (idx >= self->len)) {
        fprintf(stderr, "Vector remove index out of bounds: %d (len=%d)\n", idx, self->len);
        exit(1);
    }
    if (self->data[idx]) {
        if ((--self->data[idx]->__rc) <= 0) {
            SemuE2eOperation_destroy(self->data[idx]);
        }
    }
    for (int i = idx; (i < (self->len - 1)); (i++)) {
        (self->data[i] = self->data[(i + 1)]);
    }
    (self->len--);
}

static void btrc_Vector_SemuE2eOperation_reverse(btrc_Vector_SemuE2eOperation* self) {
    for (int i = 0; (i < (self->len / 2)); (i++)) {
        SemuE2eOperation* tmp = self->data[i];
        (self->data[i] = self->data[((self->len - 1) - i)]);
        (self->data[((self->len - 1) - i)] = tmp);
    }
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_reversed(btrc_Vector_SemuE2eOperation* self) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuE2eOperation_swap(btrc_Vector_SemuE2eOperation* self, int i, int j) {
    if ((((i < 0) || (i >= self->len)) || (j < 0)) || (j >= self->len)) {
        fprintf(stderr, "Vector swap index out of bounds\n");
        exit(1);
    }
    SemuE2eOperation* tmp = self->data[i];
    (self->data[i] = self->data[j]);
    (self->data[j] = tmp);
}

static void btrc_Vector_SemuE2eOperation_clear(btrc_Vector_SemuE2eOperation* self) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuE2eOperation_destroy(self->data[i]);
            }
        }
    }
    (self->len = 0);
}

static void btrc_Vector_SemuE2eOperation_fill(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuE2eOperation_destroy(self->data[i]);
            }
        }
        (val->__rc++);
        (self->data[i] = val);
    }
}

static int btrc_Vector_SemuE2eOperation_size(btrc_Vector_SemuE2eOperation* self) {
    return self->len;
}

static bool btrc_Vector_SemuE2eOperation_isEmpty(btrc_Vector_SemuE2eOperation* self) {
    return (self->len == 0);
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_first(btrc_Vector_SemuE2eOperation* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.first() called on empty list\n");
        exit(1);
    }
    return self->data[0];
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_last(btrc_Vector_SemuE2eOperation* self) {
    if (self->len == 0) {
        fprintf(stderr, "Vector.last() called on empty list\n");
        exit(1);
    }
    return self->data[(self->len - 1)];
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_slice(btrc_Vector_SemuE2eOperation* self, int start, int end) {
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
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = start; (i < end); (i++)) {
        btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
    }
    return result;
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_take(btrc_Vector_SemuE2eOperation* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuE2eOperation_slice(self, 0, n);
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_drop(btrc_Vector_SemuE2eOperation* self, int n) {
    if (n > self->len) {
        (n = self->len);
    }
    if (n < 0) {
        (n = 0);
    }
    return btrc_Vector_SemuE2eOperation_slice(self, n, self->len);
}

static void btrc_Vector_SemuE2eOperation_extend(btrc_Vector_SemuE2eOperation* self, btrc_Vector_SemuE2eOperation* other) {
    for (int i = 0; (i < other->len); (i++)) {
        btrc_Vector_SemuE2eOperation_push(self, other->data[i]);
    }
}

static void btrc_Vector_SemuE2eOperation_insert(btrc_Vector_SemuE2eOperation* self, int idx, SemuE2eOperation* val) {
    if ((idx < 0) || (idx > self->len)) {
        fprintf(stderr, "Vector insert index out of bounds: %d (size %d)\n", idx, self->len);
        exit(1);
    }
    (val->__rc++);
    if (self->len >= self->cap) {
        (self->cap = ((self->cap == 0) ? 4 : (self->cap * 2)));
        (self->data = ((SemuE2eOperation**)__btrc_safe_realloc(self->data, (sizeof(SemuE2eOperation*) * self->cap))));
    }
    for (int i = self->len; (i > idx); (i--)) {
        (self->data[i] = self->data[(i - 1)]);
    }
    (self->data[idx] = val);
    (self->len++);
}

static bool btrc_Vector_SemuE2eOperation_contains(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return true;
        }
    }
    return false;
}

static int btrc_Vector_SemuE2eOperation_indexOf(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuE2eOperation_lastIndexOf(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    for (int i = (self->len - 1); (i >= 0); (i--)) {
        if (__btrc_eq(self->data[i], val)) {
            return i;
        }
    }
    return (-1);
}

static int btrc_Vector_SemuE2eOperation_count(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    int c = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (__btrc_eq(self->data[i], val)) {
            (c++);
        }
    }
    return c;
}

static void btrc_Vector_SemuE2eOperation_removeAll(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* val) {
    int j = 0;
    for (int i = 0; (i < self->len); (i++)) {
        if (!__btrc_eq(self->data[i], val)) {
            (self->data[j] = self->data[i]);
            (j++);
        } else if (self->data[i]) {
            if ((--self->data[i]->__rc) <= 0) {
                SemuE2eOperation_destroy(self->data[i]);
            }
        }
    }
    (self->len = j);
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_distinct(btrc_Vector_SemuE2eOperation* self) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (!btrc_Vector_SemuE2eOperation_contains(result, self->data[i])) {
            btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
        }
    }
    return result;
}

static void btrc_Vector_SemuE2eOperation_sort(btrc_Vector_SemuE2eOperation* self) {
    for (int i = 1; (i < self->len); (i++)) {
        SemuE2eOperation* key = self->data[i];
        int j = (i - 1);
        while ((j >= 0) && __btrc_lt(key, self->data[j])) {
            (self->data[(j + 1)] = self->data[j]);
            (j = (j - 1));
        }
        (self->data[(j + 1)] = key);
    }
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_sorted(btrc_Vector_SemuE2eOperation* self) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
    }
    btrc_Vector_SemuE2eOperation_sort(result);
    return result;
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_min(btrc_Vector_SemuE2eOperation* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector min on empty list\n");
        exit(1);
    }
    SemuE2eOperation* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_lt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_max(btrc_Vector_SemuE2eOperation* self) {
    if (self->len <= 0) {
        fprintf(stderr, "Vector max on empty list\n");
        exit(1);
    }
    SemuE2eOperation* m = self->data[0];
    for (int i = 1; (i < self->len); (i++)) {
        if (__btrc_gt(self->data[i], m)) {
            (m = self->data[i]);
        }
    }
    return m;
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_filter(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
        }
    }
    return result;
}

static int btrc_Vector_SemuE2eOperation_findIndex(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return i;
        }
    }
    return (-1);
}

static void btrc_Vector_SemuE2eOperation_forEach(btrc_Vector_SemuE2eOperation* self, __btrc_fn_void_SemuE2eOperation fn) {
    for (int i = 0; (i < self->len); (i++)) {
        fn(self->data[i]);
    }
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_map(btrc_Vector_SemuE2eOperation* self, __btrc_fn_SemuE2eOperation_SemuE2eOperation fn) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuE2eOperation_push(result, fn(self->data[i]));
    }
    return result;
}

static bool btrc_Vector_SemuE2eOperation_any(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (pred(self->data[i])) {
            return true;
        }
    }
    return false;
}

static bool btrc_Vector_SemuE2eOperation_all(btrc_Vector_SemuE2eOperation* self, __btrc_fn_bool_SemuE2eOperation pred) {
    for (int i = 0; (i < self->len); (i++)) {
        if (!pred(self->data[i])) {
            return false;
        }
    }
    return true;
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_reduce(btrc_Vector_SemuE2eOperation* self, SemuE2eOperation* init, __btrc_fn_SemuE2eOperation_SemuE2eOperation_SemuE2eOperation fn) {
    SemuE2eOperation* acc = init;
    for (int i = 0; (i < self->len); (i++)) {
        (acc = fn(acc, self->data[i]));
    }
    return acc;
}

static btrc_Vector_SemuE2eOperation* btrc_Vector_SemuE2eOperation_copy(btrc_Vector_SemuE2eOperation* self) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    for (int i = 0; (i < self->len); (i++)) {
        btrc_Vector_SemuE2eOperation_push(result, self->data[i]);
    }
    return result;
}

static void btrc_Vector_SemuE2eOperation_removeAt(btrc_Vector_SemuE2eOperation* self, int idx) {
    btrc_Vector_SemuE2eOperation_remove(self, idx);
}

static int btrc_Vector_SemuE2eOperation_iterLen(btrc_Vector_SemuE2eOperation* self) {
    return self->len;
}

static SemuE2eOperation* btrc_Vector_SemuE2eOperation_iterGet(btrc_Vector_SemuE2eOperation* self, int i) {
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
            btrc_Map_string_string_put(self, old_keys[i], old_values[i]);
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
                btrc_Map_string_string_put(self, rk, rv);
                (j = ((j + 1) % self->cap));
            }
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
}

static void btrc_Map_string_string_clear(btrc_Map_string_string* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        (self->occupied[i] = false);
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
            btrc_Map_string_bool_put(self, old_keys[i], old_values[i]);
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
                btrc_Map_string_bool_put(self, rk, rv);
                (j = ((j + 1) % self->cap));
            }
            return;
        }
        (idx = ((idx + 1) % self->cap));
    }
}

static void btrc_Map_string_bool_clear(btrc_Map_string_bool* self) {
    for (int i = 0; (i < self->cap); (i++)) {
        (self->occupied[i] = false);
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
    int __fstr_1_len = snprintf(NULL, 0, "%s", s);
    char* __fstr_1_buf = __btrc_str_track(((char*)malloc((__fstr_1_len + 1))));
    snprintf(__fstr_1_buf, (__fstr_1_len + 1), "%s", s);
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

bool Strings_isDigit(char c) {
    return ((c >= '0') && (c <= '9'));
}

int Strings_toInt(char* s) {
    if (s == NULL) {
        return 0;
    }
    char* value = __btrc_str_track(__btrc_trim(s));
    if (__btrc_isEmpty(value)) {
        return 0;
    }
    int sign = 1;
    int i = 0;
    if (__btrc_startsWith(value, "-")) {
        (sign = (-1));
        (i = 1);
    } else if (__btrc_startsWith(value, "+")) {
        (i = 1);
    }
    int result = 0;
    while ((i < ((int)strlen(value))) && Strings_isDigit(value[i])) {
        (result = ((result * 10) + (value[i] - '0')));
        (i++);
    }
    return (result * sign);
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

char* Strings_removePrefix(char* s, char* prefix) {
    if (!__btrc_startsWith(s, prefix)) {
        return Strings_copy(s);
    }
    return __btrc_str_track(__btrc_substring(s, ((int)strlen(prefix)), (((int)strlen(s)) - ((int)strlen(prefix)))));
}

char* Strings_fromInt(int n) {
    char* buf = ((char*)malloc(32));
    snprintf(buf, 32, "%d", n);
    return buf;
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
    fseek(self->handle, 0, SEEK_END);
    long size = ftell(self->handle);
    fseek(self->handle, 0, SEEK_SET);
    char* buf = ((char*)malloc((size + 1)));
    long n = ((long)fread(buf, 1, size, self->handle));
    (buf[n] = '\0');
    return buf;
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
        char* __btrc_ret_2 = "";
        if (f != NULL) {
            if ((--f->__rc) <= 0) {
                File_destroy(f);
            }
        }
        return __btrc_ret_2;
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

int UnixPlatform_euid(void) {
    return ((int)geteuid());
}

int Platform_euid(void) {
    return UnixPlatform_euid();
}

char* Environment_get(char* name, char* fallback) {
    char* value = getenv(name);
    if ((value == NULL) || __btrc_isEmpty(value)) {
        return fallback;
    }
    return Strings_copy(value);
}

bool Environment_has(char* name) {
    char* value = getenv(name);
    return ((value != NULL) && (!__btrc_isEmpty(value)));
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
    if (self->raw > 255) {
        return __btrc_div_int(self->raw, 256);
    }
    return self->raw;
}

void UnixPipe_init(UnixPipe* self, char* command) {
    self->__rc = 1;
    (self->command = command);
    (self->handle = popen(command, "r"));
}

UnixPipe* UnixPipe_new(char* command) {
    UnixPipe* self = ((UnixPipe*)malloc(sizeof(UnixPipe)));
    memset(self, 0, sizeof(UnixPipe));
    UnixPipe_init(self, command);
    return self;
}

bool UnixPipe_ok(UnixPipe* self) {
    return (self->handle != NULL);
}

char* UnixPipe_readAll(UnixPipe* self) {
    if (!UnixPipe_ok(self)) {
        return "";
    }
    int cap = 4096;
    int len = 0;
    char* buffer = ((char*)malloc(cap));
    int ch = fgetc(self->handle);
    while (ch != EOF) {
        if ((len + 2) >= cap) {
            (cap = (cap * 2));
            (buffer = ((char*)realloc(buffer, cap)));
        }
        (buffer[len] = ((char)ch));
        (len++);
        (ch = fgetc(self->handle));
    }
    (buffer[len] = '\0');
    return buffer;
}

ProcessStatus* UnixPipe_close(UnixPipe* self) {
    if (!UnixPipe_ok(self)) {
        return ProcessStatus_new((-1));
    }
    int raw = pclose(self->handle);
    (self->handle = NULL);
    return ProcessStatus_new(raw);
}

ProcessStatus* UnixProcess_system(char* command) {
    return ProcessStatus_new(system(command));
}

UnixPipe* UnixProcess_pipe(char* command) {
    return UnixPipe_new(command);
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
    for (int __i_3 = 0; (raw[__i_3] != '\0'); (__i_3++)) {
        char ch = raw[__i_3];
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

bool ExecResult_ok(ExecResult* self) {
    return (self->code == 0);
}

char* ExecResult_stdout(ExecResult* self) {
    return self->out;
}

char* ExecResult_trimmed(ExecResult* self) {
    return __btrc_str_track(__btrc_trim(self->out));
}

void Command_init(Command* self, char* executable) {
    self->__rc = 1;
    (self->executable = executable);
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Vector_string_free(self->args);
        }
    }
    btrc_Vector_string* __list_5 = btrc_Vector_string_new();
    (self->args = __list_5);
    btrc_Vector_string* __list_4 = btrc_Vector_string_new();
    (__list_4->__rc++);
    if (self->env != NULL) {
        if ((--self->env->__rc) <= 0) {
            btrc_Vector_string_free(self->env);
        }
    }
    btrc_Vector_string* __list_7 = btrc_Vector_string_new();
    (self->env = __list_7);
    btrc_Vector_string* __list_6 = btrc_Vector_string_new();
    (__list_6->__rc++);
    (self->useSudo = false);
    (self->captureOutput = true);
    (self->checkStatus = true);
    (self->mergeStderr = true);
    (self->sensitive = "");
}

Command* Command_new(char* executable) {
    Command* self = ((Command*)malloc(sizeof(Command)));
    memset(self, 0, sizeof(Command));
    Command_init(self, executable);
    return self;
}

Command* Command_arg(Command* self, char* value) {
    btrc_Vector_string_push(self->args, value);
    return self;
}

Command* Command_flag(Command* self, char* name, char* value) {
    btrc_Vector_string_push(self->args, name);
    btrc_Vector_string_push(self->args, value);
    return self;
}

Command* Command_envVar(Command* self, char* name, char* value) {
    int __fstr_9_len = snprintf(NULL, 0, "%s=%s", name, value);
    char* __fstr_9_buf = __btrc_str_track(((char*)malloc((__fstr_9_len + 1))));
    snprintf(__fstr_9_buf, (__fstr_9_len + 1), "%s=%s", name, value);
    btrc_Vector_string_push(self->env, __fstr_9_buf);
    return self;
}

Command* Command_capture(Command* self, bool enabled) {
    (self->captureOutput = enabled);
    return self;
}

Command* Command_check(Command* self, bool enabled) {
    (self->checkStatus = enabled);
    return self;
}

char* Command_renderEnv(Command* self, char* item) {
    int split = Strings_find(item, "=", 0);
    if (split <= 0) {
        return ShellWords_quote(item);
    }
    char* name = __btrc_str_track(__btrc_substring(item, 0, split));
    char* value = __btrc_str_track(__btrc_substring(item, (split + 1), ((((int)strlen(item)) - split) - 1)));
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(name, "=")), ShellWords_quote(value)));
}

char* Command_render(Command* self) {
    btrc_Vector_string* parts = btrc_Vector_string_new();
    int __n_11 = btrc_Vector_string_iterLen(self->env);
    for (int __i_10 = 0; (__i_10 < __n_11); (__i_10++)) {
        char* item = btrc_Vector_string_iterGet(self->env, __i_10);
        btrc_Vector_string_push(parts, Command_renderEnv(self, item));
    }
    if (self->useSudo) {
        btrc_Vector_string_push(parts, "sudo");
    }
    btrc_Vector_string_push(parts, ShellWords_quote(self->executable));
    int __n_13 = btrc_Vector_string_iterLen(self->args);
    for (int __i_12 = 0; (__i_12 < __n_13); (__i_12++)) {
        char* item = btrc_Vector_string_iterGet(self->args, __i_12);
        btrc_Vector_string_push(parts, ShellWords_quote(item));
    }
    if (self->mergeStderr) {
        btrc_Vector_string_push(parts, "2>&1");
    }
    return btrc_Vector_string_join(parts, " ");
}

void UnixShell_init(UnixShell* self) {
    self->__rc = 1;
    (self->logCommands = false);
    (self->chrootPath = "");
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

ExecResult* UnixShell_run(UnixShell* self, char* command) {
    return UnixShell_runRaw(self, command, true, true, "");
}

ExecResult* UnixShell_runUnchecked(UnixShell* self, char* command) {
    return UnixShell_runRaw(self, command, true, false, "");
}

ExecResult* UnixShell_runCommand(UnixShell* self, Command* command) {
    return UnixShell_runRaw(self, Command_render(command), command->captureOutput, command->checkStatus, command->sensitive);
}

ExecResult* UnixShell_runRaw(UnixShell* self, char* command, bool captureOutput, bool checkStatus, char* sensitive) {
    char* rendered = command;
    if (((int)strlen(self->chrootPath)) > 0) {
        int __fstr_14_len = snprintf(NULL, 0, "nixos-enter --root %s --command %s", ShellWords_quote(self->chrootPath), ShellWords_quote(command));
        char* __fstr_14_buf = __btrc_str_track(((char*)malloc((__fstr_14_len + 1))));
        snprintf(__fstr_14_buf, (__fstr_14_len + 1), "nixos-enter --root %s --command %s", ShellWords_quote(self->chrootPath), ShellWords_quote(command));
        (rendered = __fstr_14_buf);
    }
    if (self->logCommands) {
        char* visible = UnixShell_redactText(rendered, sensitive);
        fprintf(stderr, "LOG: %s\n", visible);
    }
    if (!captureOutput) {
        ProcessStatus* status = UnixProcess_system(rendered);
        int code = ProcessStatus_code(status);
        if (checkStatus && (code != 0)) {
            fprintf(stderr, "Command failed (%d): %s\n", code, UnixShell_redactText(rendered, sensitive));
        }
        return ExecResult_new(code, "", "", rendered);
    }
    UnixPipe* pipe = UnixProcess_pipe(rendered);
    if (!UnixPipe_ok(pipe)) {
        return ExecResult_new(127, "", "popen failed", rendered);
    }
    char* output = UnixPipe_readAll(pipe);
    ProcessStatus* status = UnixPipe_close(pipe);
    int code = ProcessStatus_code(status);
    if (checkStatus && (code != 0)) {
        fprintf(stderr, "Command failed (%d): %s\n", code, UnixShell_redactText(rendered, sensitive));
    }
    return ExecResult_new(code, output, "", rendered);
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

bool FileStatus_isSymlink(FileStatus* self) {
    return (self->linkFound && S_ISLNK(self->linkMode));
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

int UnixFileSystem_statusCode(int raw) {
    if (raw == (-1)) {
        return 127;
    }
    if (raw > 255) {
        return __btrc_div_int(raw, 256);
    }
    return raw;
}

int UnixFileSystem_chmodPath(char* path, int mode) {
    return chmod(path, ((mode_t)mode));
}

int UnixFileSystem_mkdirPath(char* path, int mode) {
    return mkdir(path, ((mode_t)mode));
}

int UnixFileSystem_runShell(char* command) {
    return UnixFileSystem_statusCode(system(command));
}

int UnixFileSystem_mkdirp(char* path) {
    char* quoted = PathTools_shellQuote(path);
    int __fstr_15_len = snprintf(NULL, 0, "mkdir -p %s", quoted);
    char* __fstr_15_buf = __btrc_str_track(((char*)malloc((__fstr_15_len + 1))));
    snprintf(__fstr_15_buf, (__fstr_15_len + 1), "mkdir -p %s", quoted);
    return UnixFileSystem_runShell(__fstr_15_buf);
}

int UnixFileSystem_removeRecursive(char* path) {
    char* quoted = PathTools_shellQuote(path);
    int __fstr_16_len = snprintf(NULL, 0, "rm -rf %s", quoted);
    char* __fstr_16_buf = __btrc_str_track(((char*)malloc((__fstr_16_len + 1))));
    snprintf(__fstr_16_buf, (__fstr_16_len + 1), "rm -rf %s", quoted);
    return UnixFileSystem_runShell(__fstr_16_buf);
}

int UnixFileSystem_symlinkPath(char* target, char* linkPath) {
    return symlink(target, linkPath);
}

char* UnixFileSystem_readLink(char* path) {
    char buffer[4096];
    ssize_t length = readlink(path, buffer, 4095);
    if (length < 0) {
        return "";
    }
    (buffer[length] = '\0');
    return Strings_copy(buffer);
}

char* UnixFileSystem_tempDir(char* prefix) {
    char* base = Environment_get("TMPDIR", "/tmp");
    char* templatePath = PathTools_join(base, __btrc_str_track(__btrc_strcat(prefix, ".XXXXXX")));
    char* raw = Strings_copy(templatePath);
    char* result = mkdtemp(raw);
    if (result == NULL) {
        return "";
    }
    return Strings_copy(result);
}

char* PathTools_shellQuote(char* raw) {
    return ShellWords_quote(raw);
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
        int __fstr_17_len = snprintf(NULL, 0, "%s%s", left, right);
        char* __fstr_17_buf = __btrc_str_track(((char*)malloc((__fstr_17_len + 1))));
        snprintf(__fstr_17_buf, (__fstr_17_len + 1), "%s%s", left, right);
        return __fstr_17_buf;
    }
    int __fstr_18_len = snprintf(NULL, 0, "%s/%s", left, right);
    char* __fstr_18_buf = __btrc_str_track(((char*)malloc((__fstr_18_len + 1))));
    snprintf(__fstr_18_buf, (__fstr_18_len + 1), "%s/%s", left, right);
    return __fstr_18_buf;
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

bool FileSystem_isSymlink(char* path) {
    FileStatus* status = FileStatus_new(path);
    bool result = FileStatus_isSymlink(status);
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

int FileSystem_removeRecursive(char* path) {
    return UnixFileSystem_removeRecursive(path);
}

int FileSystem_symlink(char* target, char* linkPath) {
    return UnixFileSystem_symlinkPath(target, linkPath);
}

char* FileSystem_readLink(char* path) {
    return UnixFileSystem_readLink(path);
}

char* FileSystem_tempDir(char* prefix) {
    return UnixFileSystem_tempDir(prefix);
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
    return escaped;
}

char* JsonObject_unescape(char* text) {
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

void JsonObject_setString(JsonObject* self, char* key, char* value) {
    btrc_Map_string_string_put(self->values, key, value);
    btrc_Map_string_bool_put(self->quoted, key, true);
}

void JsonObject_setRaw(JsonObject* self, char* key, char* value) {
    btrc_Map_string_string_put(self->values, key, value);
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

int JsonObject_getInt(JsonObject* self, char* key, int fallback) {
    if (!btrc_Map_string_string_has(self->values, key)) {
        return fallback;
    }
    return Strings_toInt(btrc_Map_string_string_get(self->values, key));
}

char* JsonObject_stringify(JsonObject* self) {
    btrc_Vector_string* fields = btrc_Vector_string_new();
    int __n_20 = btrc_Map_string_string_iterLen(self->values);
    for (int __i_19 = 0; (__i_19 < __n_20); (__i_19++)) {
        char* key = btrc_Map_string_string_iterGet(self->values, __i_19);
        char* value = btrc_Map_string_string_iterValueAt(self->values, __i_19);
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

int JsonObject_skipSpaces(char* text, int i) {
    int len = ((int)strlen(text));
    while ((i < len) && ((((text[i] == ' ') || (text[i] == '\n')) || (text[i] == '\t')) || (text[i] == '\r'))) {
        (i++);
    }
    return i;
}

char* JsonObject_slice(char* text, int start, int end) {
    int len = (end - start);
    char* result = ((char*)malloc((len + 1)));
    memcpy(result, (text + start), len);
    (result[len] = '\0');
    return result;
}

int JsonObject_stringEnd(char* text, int start) {
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

JsonObject* JsonObject_parse(char* text) {
    JsonObject* obj = JsonObject_new();
    int len = ((int)strlen(text));
    int i = 0;
    while (i < len) {
        (i = JsonObject_skipSpaces(text, i));
        if (i >= len) {
            break;
        }
        if (text[i] != ((char)34)) {
            (i++);
            continue;
        }
        (i++);
        int keyStart = i;
        (i = JsonObject_stringEnd(text, keyStart));
        char* key = JsonObject_unescape(JsonObject_slice(text, keyStart, i));
        (i++);
        (i = JsonObject_skipSpaces(text, i));
        if ((i < len) && (text[i] == ':')) {
            (i++);
        }
        (i = JsonObject_skipSpaces(text, i));
        if (i >= len) {
            break;
        }
        if (text[i] == ((char)34)) {
            (i++);
            int valueStart = i;
            (i = JsonObject_stringEnd(text, valueStart));
            char* value = JsonObject_unescape(JsonObject_slice(text, valueStart, i));
            JsonObject_setString(obj, key, value);
            (i++);
        } else {
            int valueStart = i;
            while (((i < len) && (text[i] != ',')) && (text[i] != '}')) {
                (i++);
            }
            int valueEnd = i;
            while ((valueEnd > valueStart) && ((((text[(valueEnd - 1)] == ' ') || (text[(valueEnd - 1)] == '\n')) || (text[(valueEnd - 1)] == '\t')) || (text[(valueEnd - 1)] == '\r'))) {
                (valueEnd--);
            }
            char* value = JsonObject_slice(text, valueStart, valueEnd);
            JsonObject_setRaw(obj, key, value);
        }
    }
    return obj;
    if (obj != NULL) {
        if ((--obj->__rc) <= 0) {
            JsonObject_destroy(obj);
        }
    }
}

JsonObject* JsonObject_readFile(char* path) {
    return JsonObject_parse(Path_readAll(path));
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
                int keyEnd = JsonObject_stringEnd(text, keyStart);
                char* candidate = JsonObject_unescape(JsonObject_slice(text, keyStart, keyEnd));
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
    int keyEnd = JsonObject_stringEnd(text, (keyPos + 1));
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
    int end = JsonObject_stringEnd(text, start);
    return JsonObject_unescape(JsonObject_slice(text, start, end));
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
    char* raw = __btrc_str_track(__btrc_trim(JsonObject_slice(text, start, i)));
    if (__btrc_isEmpty(raw)) {
        return fallback;
    }
    return raw;
}

int JsonText_intField(char* text, char* key, int fallback) {
    char* raw = JsonText_field(text, key, "");
    if (__btrc_isEmpty(raw)) {
        return fallback;
    }
    return Strings_toInt(raw);
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
                return JsonObject_slice(text, i, (j + 1));
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
                    btrc_Vector_string_push(result, JsonObject_unescape(JsonObject_slice(text, start, j)));
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
                btrc_Vector_string_push(result, JsonObject_slice(text, start, (j + 1)));
                (start = (-1));
            }
        }
    }
    return result;
}

btrc_Map_string_string* JsonText_objectMap(char* objectText) {
    btrc_Map_string_string* result = btrc_Map_string_string_new();
    if (__btrc_isEmpty(objectText)) {
        return result;
    }
    JsonObject* parsed = JsonObject_parse(objectText);
    int __n_22 = btrc_Map_string_string_iterLen(parsed->values);
    for (int __i_21 = 0; (__i_21 < __n_22); (__i_21++)) {
        char* key = btrc_Map_string_string_iterGet(parsed->values, __i_21);
        char* value = btrc_Map_string_string_iterValueAt(parsed->values, __i_21);
        btrc_Map_string_string_put(result, key, value);
    }
    return result;
}

btrc_Map_string_string* JsonText_argsObject(char* text) {
    return JsonText_objectMap(JsonText_objectField(text, "args"));
}

char* JsonText_expand(char* text, btrc_Map_string_string* args) {
    char* result = Strings_copy(text);
    int __n_24 = btrc_Map_string_string_iterLen(args);
    for (int __i_23 = 0; (__i_23 < __n_24); (__i_23++)) {
        char* key = btrc_Map_string_string_iterGet(args, __i_23);
        char* value = btrc_Map_string_string_iterValueAt(args, __i_23);
        (result = Strings_replace(result, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{{", key)), "}}")), value));
    }
    int __n_26 = btrc_Map_string_string_iterLen(args);
    for (int __i_25 = 0; (__i_25 < __n_26); (__i_25++)) {
        char* key = btrc_Map_string_string_iterGet(args, __i_25);
        char* value = btrc_Map_string_string_iterValueAt(args, __i_25);
        (result = Strings_replace(result, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("${", key)), "}")), value));
    }
    return result;
}

void JsonText_putArgPair(btrc_Map_string_string* result, char* pair) {
    int pos = Strings_find(pair, "=", 0);
    if (pos <= 0) {
        fprintf(stderr, "error 0:0 Expected --arg key=value\n");
        exit(1);
    }
    btrc_Map_string_string_put(result, JsonObject_slice(pair, 0, pos), JsonObject_slice(pair, (pos + 1), ((int)strlen(pair))));
}

void CliArgs_init(CliArgs* self, int argc, char** argv) {
    self->__rc = 1;
    (self->program = ((argc > 0) ? Strings_copy(argv[0]) : ""));
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    btrc_Vector_string* __list_28 = btrc_Vector_string_new();
    (self->values = __list_28);
    btrc_Vector_string* __list_27 = btrc_Vector_string_new();
    (__list_27->__rc++);
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
    int __n_30 = btrc_Vector_string_iterLen(self->values);
    for (int __i_29 = 0; (__i_29 < __n_30); (__i_29++)) {
        char* value = btrc_Vector_string_iterGet(self->values, __i_29);
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

void Console_log(char* msg) {
    printf("%s\n", msg);
}

void Console_error(char* msg) {
    fprintf(stderr, "%s\n", msg);
}

void GraphNode_init(GraphNode* self) {
    self->__rc = 1;
    (self->id = "");
    (self->specPath = "");
    if (self->after != NULL) {
        if ((--self->after->__rc) <= 0) {
            btrc_Vector_string_free(self->after);
        }
    }
    btrc_Vector_string* __list_38 = btrc_Vector_string_new();
    (self->after = __list_38);
    btrc_Vector_string* __list_37 = btrc_Vector_string_new();
    (__list_37->__rc++);
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    (self->args = btrc_Map_string_string_new());
    (btrc_Map_string_string_new()->__rc++);
}

GraphNode* GraphNode_new(void) {
    GraphNode* self = ((GraphNode*)malloc(sizeof(GraphNode)));
    memset(self, 0, sizeof(GraphNode));
    GraphNode_init(self);
    return self;
}

void GraphNode_destroy(GraphNode* self) {
    if (self->after != NULL) {
        if ((--self->after->__rc) <= 0) {
            btrc_Vector_string_free(self->after);
        }
    }
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void ExecutionGraph_init(ExecutionGraph* self) {
    self->__rc = 1;
    (self->name = "graph");
    (self->path = "");
    (self->baseDir = ".");
    (self->workspaceRoot = ".");
    if (self->defaults != NULL) {
        if ((--self->defaults->__rc) <= 0) {
            btrc_Vector_string_free(self->defaults);
        }
    }
    btrc_Vector_string* __list_40 = btrc_Vector_string_new();
    (self->defaults = __list_40);
    btrc_Vector_string* __list_39 = btrc_Vector_string_new();
    (__list_39->__rc++);
    if (self->nodes != NULL) {
        if ((--self->nodes->__rc) <= 0) {
            btrc_Vector_GraphNode_free(self->nodes);
        }
    }
    btrc_Vector_GraphNode* __list_42 = btrc_Vector_GraphNode_new();
    (self->nodes = __list_42);
    btrc_Vector_GraphNode* __list_41 = btrc_Vector_GraphNode_new();
    (__list_41->__rc++);
}

ExecutionGraph* ExecutionGraph_new(void) {
    ExecutionGraph* self = ((ExecutionGraph*)malloc(sizeof(ExecutionGraph)));
    memset(self, 0, sizeof(ExecutionGraph));
    ExecutionGraph_init(self);
    return self;
}

void ExecutionGraph_destroy(ExecutionGraph* self) {
    if (self->defaults != NULL) {
        if ((--self->defaults->__rc) <= 0) {
            btrc_Vector_string_free(self->defaults);
        }
    }
    if (self->nodes != NULL) {
        if ((--self->nodes->__rc) <= 0) {
            btrc_Vector_GraphNode_free(self->nodes);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* ExecutionGraph_resolvePath(ExecutionGraph* self, char* base, char* value) {
    if (__btrc_startsWith(value, "/")) {
        return value;
    }
    return PathTools_join(base, value);
}

char* ExecutionGraph_resolvedWorkspaceRoot(ExecutionGraph* self) {
    return ExecutionGraph_resolvePath(self, self->baseDir, self->workspaceRoot);
}

char* ExecutionGraph_resolvedSpecPath(ExecutionGraph* self, GraphNode* node) {
    return ExecutionGraph_resolvePath(self, self->baseDir, node->specPath);
}

GraphNode* ExecutionGraph_node(ExecutionGraph* self, char* id) {
    int __n_44 = btrc_Vector_GraphNode_iterLen(self->nodes);
    for (int __i_43 = 0; (__i_43 < __n_44); (__i_43++)) {
        GraphNode* item = btrc_Vector_GraphNode_iterGet(self->nodes, __i_43);
        if (strcmp(item->id, id) == 0) {
            return item;
        }
    }
    Console_error(__btrc_str_track(__btrc_strcat("error 0:0 Unknown graph node: ", id)));
    exit(1);
    return GraphNode_new();
}

btrc_Vector_string* ExecutionGraph_defaultTargets(ExecutionGraph* self) {
    if (!btrc_Vector_string_isEmpty(self->defaults)) {
        return self->defaults;
    }
    btrc_Vector_string* result = btrc_Vector_string_new();
    int __n_46 = btrc_Vector_GraphNode_iterLen(self->nodes);
    for (int __i_45 = 0; (__i_45 < __n_46); (__i_45++)) {
        GraphNode* item = btrc_Vector_GraphNode_iterGet(self->nodes, __i_45);
        btrc_Vector_string_push(result, item->id);
    }
    return result;
}

GraphNode* GraphParser_node(char* objectText) {
    GraphNode* node = GraphNode_new();
    (node->id = JsonText_field(objectText, "id", ""));
    (node->specPath = JsonText_field(objectText, "spec", ""));
    if (node->after != NULL) {
        if ((--node->after->__rc) <= 0) {
            btrc_Vector_string_free(node->after);
        }
    }
    (node->after = JsonText_stringArray(objectText, "after"));
    (JsonText_stringArray(objectText, "after")->__rc++);
    if (node->args != NULL) {
        if ((--node->args->__rc) <= 0) {
            btrc_Map_string_string_free(node->args);
        }
    }
    (node->args = JsonText_argsObject(objectText));
    (JsonText_argsObject(objectText)->__rc++);
    if (__btrc_isEmpty(node->id)) {
        Console_error("error 0:0 Graph node is missing id");
        exit(1);
    }
    if (__btrc_isEmpty(node->specPath)) {
        Console_error(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("error 0:0 Graph node ", node->id)), " is missing spec")));
        exit(1);
    }
    return node;
    if (node != NULL) {
        if ((--node->__rc) <= 0) {
            GraphNode_destroy(node);
        }
    }
}

ExecutionGraph* GraphParser_readFile(char* path) {
    char* text = Path_readAll(path);
    ExecutionGraph* graph = ExecutionGraph_new();
    (graph->path = path);
    (graph->baseDir = PathTools_dirname(path));
    (graph->name = JsonText_field(text, "name", graph->name));
    (graph->workspaceRoot = JsonText_field(text, "workspaceRoot", graph->workspaceRoot));
    if (graph->defaults != NULL) {
        if ((--graph->defaults->__rc) <= 0) {
            btrc_Vector_string_free(graph->defaults);
        }
    }
    (graph->defaults = JsonText_stringArray(text, "default"));
    (JsonText_stringArray(text, "default")->__rc++);
    int __n_48 = btrc_Vector_string_iterLen(JsonText_objectArray(text, "nodes"));
    for (int __i_47 = 0; (__i_47 < __n_48); (__i_47++)) {
        char* objectText = btrc_Vector_string_iterGet(JsonText_objectArray(text, "nodes"), __i_47);
        btrc_Vector_GraphNode_push(graph->nodes, GraphParser_node(objectText));
    }
    if (btrc_Vector_GraphNode_isEmpty(graph->nodes)) {
        Console_error(__btrc_str_track(__btrc_strcat("error 0:0 Graph has no nodes: ", path)));
        exit(1);
    }
    return graph;
    if (graph != NULL) {
        if ((--graph->__rc) <= 0) {
            ExecutionGraph_destroy(graph);
        }
    }
}

btrc_Map_string_string* GraphCli_args(CliArgs* args, int startIndex) {
    btrc_Map_string_string* result = btrc_Map_string_string_new();
    for (int i = startIndex; (i < CliArgs_count(args)); (i++)) {
        char* value = CliArgs_get(args, i);
        if (strcmp(value, "--arg") == 0) {
            if ((i + 1) >= CliArgs_count(args)) {
                Console_error("error 0:0 Expected --arg key=value");
                exit(1);
            }
            JsonText_putArgPair(result, CliArgs_get(args, (i + 1)));
            (i++);
            continue;
        }
        if (__btrc_startsWith(value, "--arg=")) {
            JsonText_putArgPair(result, Strings_removePrefix(value, "--arg="));
        }
    }
    return result;
}

btrc_Vector_string* GraphCli_targets(CliArgs* args, int startIndex) {
    btrc_Vector_string* result = btrc_Vector_string_new();
    for (int i = startIndex; (i < CliArgs_count(args)); (i++)) {
        char* value = CliArgs_get(args, i);
        if (strcmp(value, "--arg") == 0) {
            (i++);
            continue;
        }
        if (__btrc_startsWith(value, "--arg=")) {
            continue;
        }
        btrc_Vector_string_push(result, value);
    }
    return result;
}

void GraphReport_list(ExecutionGraph* graph) {
    int __n_50 = btrc_Vector_GraphNode_iterLen(graph->nodes);
    for (int __i_49 = 0; (__i_49 < __n_50); (__i_49++)) {
        GraphNode* node = btrc_Vector_GraphNode_iterGet(graph->nodes, __i_49);
        char* parents = (btrc_Vector_string_isEmpty(node->after) ? "root" : btrc_Vector_string_join(node->after, ","));
        Console_log(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(node->id, " <- ")), parents)), " :: ")), ExecutionGraph_resolvedSpecPath(graph, node))));
    }
}

void GraphTraversal_init(GraphTraversal* self, ExecutionGraph* graph) {
    self->__rc = 1;
    if (self->graph != NULL) {
        if ((--self->graph->__rc) <= 0) {
            ExecutionGraph_destroy(self->graph);
        }
    }
    (self->graph = graph);
    (graph->__rc++);
    if (self->done != NULL) {
        if ((--self->done->__rc) <= 0) {
            btrc_Vector_string_free(self->done);
        }
    }
    btrc_Vector_string* __list_52 = btrc_Vector_string_new();
    (self->done = __list_52);
    btrc_Vector_string* __list_51 = btrc_Vector_string_new();
    (__list_51->__rc++);
    if (self->visiting != NULL) {
        if ((--self->visiting->__rc) <= 0) {
            btrc_Vector_string_free(self->visiting);
        }
    }
    btrc_Vector_string* __list_54 = btrc_Vector_string_new();
    (self->visiting = __list_54);
    btrc_Vector_string* __list_53 = btrc_Vector_string_new();
    (__list_53->__rc++);
    if (self->ordered != NULL) {
        if ((--self->ordered->__rc) <= 0) {
            btrc_Vector_string_free(self->ordered);
        }
    }
    btrc_Vector_string* __list_56 = btrc_Vector_string_new();
    (self->ordered = __list_56);
    btrc_Vector_string* __list_55 = btrc_Vector_string_new();
    (__list_55->__rc++);
    (self->error = "");
}

GraphTraversal* GraphTraversal_new(ExecutionGraph* graph) {
    GraphTraversal* self = ((GraphTraversal*)malloc(sizeof(GraphTraversal)));
    memset(self, 0, sizeof(GraphTraversal));
    GraphTraversal_init(self, graph);
    return self;
}

void GraphTraversal_destroy(GraphTraversal* self) {
    if (self->graph != NULL) {
        if ((--self->graph->__rc) <= 0) {
            ExecutionGraph_destroy(self->graph);
        }
    }
    if (self->done != NULL) {
        if ((--self->done->__rc) <= 0) {
            btrc_Vector_string_free(self->done);
        }
    }
    if (self->visiting != NULL) {
        if ((--self->visiting->__rc) <= 0) {
            btrc_Vector_string_free(self->visiting);
        }
    }
    if (self->ordered != NULL) {
        if ((--self->ordered->__rc) <= 0) {
            btrc_Vector_string_free(self->ordered);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

int GraphTraversal_visit(GraphTraversal* self, char* id) {
    if (btrc_Vector_string_contains(self->done, id)) {
        return 0;
    }
    if (btrc_Vector_string_contains(self->visiting, id)) {
        (self->error = __btrc_str_track(__btrc_strcat("Cycle in graph at ", id)));
        return 1;
    }
    btrc_Vector_string_push(self->visiting, id);
    GraphNode* node = ExecutionGraph_node(self->graph, id);
    int __n_58 = btrc_Vector_string_iterLen(node->after);
    for (int __i_57 = 0; (__i_57 < __n_58); (__i_57++)) {
        char* parent = btrc_Vector_string_iterGet(node->after, __i_57);
        int parentResult = GraphTraversal_visit(self, parent);
        if (parentResult != 0) {
            btrc_Vector_string_removeAll(self->visiting, id);
            return parentResult;
        }
    }
    btrc_Vector_string_push(self->done, id);
    btrc_Vector_string_push(self->ordered, id);
    btrc_Vector_string_removeAll(self->visiting, id);
    return 0;
}

btrc_Vector_string* GraphTraversal_order(GraphTraversal* self, btrc_Vector_string* targets) {
    if (self->done != NULL) {
        if ((--self->done->__rc) <= 0) {
            btrc_Vector_string_free(self->done);
        }
    }
    btrc_Vector_string* __list_60 = btrc_Vector_string_new();
    (self->done = __list_60);
    btrc_Vector_string* __list_59 = btrc_Vector_string_new();
    (__list_59->__rc++);
    if (self->visiting != NULL) {
        if ((--self->visiting->__rc) <= 0) {
            btrc_Vector_string_free(self->visiting);
        }
    }
    btrc_Vector_string* __list_62 = btrc_Vector_string_new();
    (self->visiting = __list_62);
    btrc_Vector_string* __list_61 = btrc_Vector_string_new();
    (__list_61->__rc++);
    if (self->ordered != NULL) {
        if ((--self->ordered->__rc) <= 0) {
            btrc_Vector_string_free(self->ordered);
        }
    }
    btrc_Vector_string* __list_64 = btrc_Vector_string_new();
    (self->ordered = __list_64);
    btrc_Vector_string* __list_63 = btrc_Vector_string_new();
    (__list_63->__rc++);
    (self->error = "");
    btrc_Vector_string* selected = (btrc_Vector_string_isEmpty(targets) ? ExecutionGraph_defaultTargets(self->graph) : targets);
    int __n_66 = btrc_Vector_string_iterLen(selected);
    for (int __i_65 = 0; (__i_65 < __n_66); (__i_65++)) {
        char* id = btrc_Vector_string_iterGet(selected, __i_65);
        int result = GraphTraversal_visit(self, id);
        if (result != 0) {
            return self->ordered;
        }
    }
    return self->ordered;
}

bool GraphTraversal_ok(GraphTraversal* self) {
    return __btrc_isEmpty(self->error);
}

char* jsonQ(char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"", JsonObject_escape(value))), "\""));
}

char* jsonField(char* key, char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(jsonQ(key), ": ")), value));
}

char* jsonStrField(char* key, char* value) {
    return jsonField(key, jsonQ(value));
}

char* jsonBoolField(char* key, bool value) {
    return jsonField(key, (value ? "true" : "false"));
}

char* jsonObject(btrc_Vector_string* fields) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("{", btrc_Vector_string_join(fields, ", "))), "}"));
}

char* jsonArray(btrc_Vector_string* values) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[", btrc_Vector_string_join(values, ", "))), "]"));
}

char* jsonStringArray(btrc_Vector_string* values) {
    btrc_Vector_string* quoted = btrc_Vector_string_new();
    int __n_68 = btrc_Vector_string_iterLen(values);
    for (int __i_67 = 0; (__i_67 < __n_68); (__i_67++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_67);
        btrc_Vector_string_push(quoted, jsonQ(value));
    }
    return jsonArray(quoted);
}

void writeJsonPrettyFile(char* path, char* text) {
    FileSystem_writeText(path, __btrc_str_track(__btrc_strcat(text, "\n")));
    char* tmp = __btrc_str_track(__btrc_strcat(path, ".tmp"));
    UnixShell* shell = UnixShell_new();
    if (commandExists("python3")) {
        ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("python3 -m json.tool ", ShellWords_quote(path))), " > ")), ShellWords_quote(tmp))), " && mv ")), ShellWords_quote(tmp))), " ")), ShellWords_quote(path))));
        if (ExecResult_ok(result)) {
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
            return;
        }
        FileSystem_removeRecursive(tmp);
    }
    if (commandExists("jq")) {
        ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("jq . ", ShellWords_quote(path))), " > ")), ShellWords_quote(tmp))), " && mv ")), ShellWords_quote(tmp))), " ")), ShellWords_quote(path))));
        if (ExecResult_ok(result)) {
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
            return;
        }
        FileSystem_removeRecursive(tmp);
    }
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

char* commandPath(char* name) {
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat("command -v ", ShellWords_quote(name))), true, false, "");
    if (!ExecResult_ok(result)) {
        char* __btrc_ret_69 = "";
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_69;
    }
    char* __btrc_ret_70 = ExecResult_trimmed(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_70;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool commandExists(char* name) {
    return (((int)strlen(commandPath(name))) > 0);
}

char* commandSpec(char* label, char* command) {
    btrc_Vector_string* __list_71 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_71, jsonStrField("label", label));
    btrc_Vector_string_push(__list_71, jsonStrField("command", command));
    return jsonObject(__list_71);
}

char* platformCommands(char* platform, btrc_Vector_string* commands) {
    return jsonField(platform, jsonArray(commands));
}

char* systemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile) {
    btrc_Vector_string* __list_72 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_72, platformCommands("linux", linuxCommands));
    btrc_Vector_string_push(__list_72, platformCommands("macos", macosCommands));
    btrc_Vector_string* commandPlatforms = __list_72;
    btrc_Vector_string* __list_73 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_73, jsonStrField("id", id));
    btrc_Vector_string_push(__list_73, jsonStrField("fullname", fullname));
    btrc_Vector_string_push(__list_73, jsonStrField("platform", platform));
    btrc_Vector_string_push(__list_73, jsonStrField("theme", theme));
    btrc_Vector_string_push(__list_73, jsonStrField("rom_dir", romDir));
    btrc_Vector_string_push(__list_73, jsonField("extensions", jsonStringArray(extensions)));
    btrc_Vector_string_push(__list_73, jsonField("commands", jsonObject(commandPlatforms)));
    btrc_Vector_string* fields = __list_73;
    if (bios->len > 0) {
        btrc_Vector_string_push(fields, jsonField("bios", jsonStringArray(bios)));
    }
    if (((int)strlen(controllerProfile)) > 0) {
        btrc_Vector_string_push(fields, jsonStrField("controller_profile", controllerProfile));
    }
    return jsonObject(fields);
}

char* biosSpec(char* id, char* system, bool required, btrc_Vector_string* files, char* target, char* match, char* note) {
    btrc_Vector_string* __list_74 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_74, jsonStrField("id", id));
    btrc_Vector_string_push(__list_74, jsonStrField("system", system));
    btrc_Vector_string_push(__list_74, jsonBoolField("required", required));
    btrc_Vector_string_push(__list_74, jsonField("files", jsonStringArray(files)));
    btrc_Vector_string_push(__list_74, jsonStrField("target", target));
    btrc_Vector_string_push(__list_74, jsonStrField("note", note));
    btrc_Vector_string* fields = __list_74;
    if (((int)strlen(match)) > 0) {
        btrc_Vector_string_push(fields, jsonStrField("match", match));
    }
    return jsonObject(fields);
}

char* steamInputTemplate(char* id, char* title, char* source, char* note) {
    btrc_Vector_string* __list_75 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_75, jsonStrField("id", id));
    btrc_Vector_string_push(__list_75, jsonStrField("title", title));
    btrc_Vector_string_push(__list_75, jsonStrField("source", source));
    btrc_Vector_string_push(__list_75, jsonBoolField("required", false));
    btrc_Vector_string_push(__list_75, jsonStrField("note", note));
    return jsonObject(__list_75);
}

char* xmlEscape(char* value) {
    char* result = Strings_replace(value, "&", "&amp;");
    (result = Strings_replace(result, "\"", "&quot;"));
    (result = Strings_replace(result, "<", "&lt;"));
    (result = Strings_replace(result, ">", "&gt;"));
    return result;
}

char* esExtensionList(btrc_Vector_string* extensions) {
    btrc_Vector_string* values = btrc_Vector_string_new();
    int __n_77 = btrc_Vector_string_iterLen(extensions);
    for (int __i_76 = 0; (__i_76 < __n_77); (__i_76++)) {
        char* extension = btrc_Vector_string_iterGet(extensions, __i_76);
        btrc_Vector_string_push(values, extension);
        btrc_Vector_string_push(values, __btrc_str_track(__btrc_toUpper(extension)));
    }
    return btrc_Vector_string_join(values, " ");
}

char* esCommandXml(char* commandJson) {
    JsonObject* command = JsonObject_parse(commandJson);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("    <command label=\"", xmlEscape(JsonObject_getString(command, "label", "")))), "\">")), xmlEscape(JsonObject_getString(command, "command", "")))), "</command>\n"));
}

char* esSystemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands) {
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("  <system>\n", "    <name>")), xmlEscape(id))), "</name>\n")), "    <fullname>")), xmlEscape(fullname))), "</fullname>\n")), "    <path>%ROMPATH%/")), xmlEscape(romDir))), "</path>\n")), "    <extension>")), xmlEscape(esExtensionList(extensions)))), "</extension>\n"));
    int __n_79 = btrc_Vector_string_iterLen(linuxCommands);
    for (int __i_78 = 0; (__i_78 < __n_79); (__i_78++)) {
        char* command = btrc_Vector_string_iterGet(linuxCommands, __i_78);
        (result = __btrc_str_track(__btrc_strcat(result, esCommandXml(command))));
    }
    (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "    <platform>")), xmlEscape(platform))), "</platform>\n")), "    <theme>")), xmlEscape(theme))), "</theme>\n")), "  </system>\n")));
    return result;
}

char* manifestPaths(void) {
    btrc_Vector_string* __list_80 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_80, jsonStrField("project_content_root", "${project}/ES-DE/ES-DE"));
    btrc_Vector_string_push(__list_80, jsonStrField("runtime_content_root", "${portable}"));
    btrc_Vector_string_push(__list_80, jsonStrField("roms", "${paths.runtime_content_root}/ROMs"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_roms", "${paths.project_content_root}/ROMs"));
    btrc_Vector_string_push(__list_80, jsonStrField("bios", "${paths.runtime_content_root}/bios"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_bios", "${paths.project_content_root}/bios"));
    btrc_Vector_string_push(__list_80, jsonStrField("saves", "${paths.runtime_content_root}/saves"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_saves", "${paths.project_content_root}/saves"));
    btrc_Vector_string_push(__list_80, jsonStrField("states", "${paths.runtime_content_root}/states"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_states", "${paths.project_content_root}/states"));
    btrc_Vector_string_push(__list_80, jsonStrField("screenshots", "${paths.runtime_content_root}/screenshots"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_screenshots", "${paths.project_content_root}/screenshots"));
    btrc_Vector_string_push(__list_80, jsonStrField("media", "${paths.runtime_content_root}/downloaded_media"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_media", "${paths.project_content_root}/downloaded_media"));
    btrc_Vector_string_push(__list_80, jsonStrField("gamelists", "${paths.runtime_content_root}/gamelists"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_gamelists", "${paths.project_content_root}/gamelists"));
    btrc_Vector_string_push(__list_80, jsonStrField("themes", "${paths.runtime_content_root}/themes"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_themes", "${paths.project_content_root}/themes"));
    btrc_Vector_string_push(__list_80, jsonStrField("custom_systems", "${paths.runtime_content_root}/custom_systems"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_custom_systems", "${project}/ES-DE/custom_systems"));
    btrc_Vector_string_push(__list_80, jsonStrField("settings", "${paths.runtime_content_root}/settings"));
    btrc_Vector_string_push(__list_80, jsonStrField("project_settings", "${project}/ES-DE"));
    btrc_Vector_string_push(__list_80, jsonStrField("keymaps", "${project}/input/keymaps"));
    btrc_Vector_string_push(__list_80, jsonStrField("steam_input", "${project}/input/steam-input"));
    btrc_Vector_string_push(__list_80, jsonStrField("emulator_profiles", "${project}/emulators/profiles"));
    btrc_Vector_string_push(__list_80, jsonStrField("es_de_profiles", "${project}/emulators/es-de"));
    btrc_Vector_string_push(__list_80, jsonStrField("linux_packaging", "${project}/packaging/linux"));
    btrc_Vector_string_push(__list_80, jsonStrField("retroarch_system", "${paths.bios}"));
    return jsonObject(__list_80);
}

char* esDe(void) {
    btrc_Vector_string* __list_81 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_81, jsonStrField("ROMDirectory", "~/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_81, jsonStrField("MediaDirectory", "~/ES-DE/downloaded_media"));
    btrc_Vector_string_push(__list_81, jsonStrField("UserThemeDirectory", "~/ES-DE/themes"));
    btrc_Vector_string_push(__list_81, jsonStrField("ApplicationUpdaterFrequency", "never"));
    btrc_Vector_string_push(__list_81, jsonStrField("SaveGamelistsMode", "always"));
    btrc_Vector_string_push(__list_81, jsonStrField("CreatePlaceholderSystemDirectories", "false"));
    btrc_Vector_string_push(__list_81, jsonStrField("InputControllerType", "xbox"));
    btrc_Vector_string_push(__list_81, jsonStrField("StartupSystem", "${env.SEMU_STARTUP_SYSTEM}"));
    btrc_Vector_string_push(__list_81, jsonStrField("StartupView", "system"));
    char* settings = jsonObject(__list_81);
    btrc_Vector_string* __list_83 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_83, jsonField("settings", settings));
    return jsonObject(__list_83);
}

char* controllerModel(char* id, char* label, char* vendor, char* layout, btrc_Vector_string* capabilities, btrc_Vector_string* defaultOutputs, char* preferredBackend, char* gyroPolicy, char* note) {
    btrc_Vector_string* __list_84 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_84, jsonStrField("id", id));
    btrc_Vector_string_push(__list_84, jsonStrField("label", label));
    btrc_Vector_string_push(__list_84, jsonStrField("vendor", vendor));
    btrc_Vector_string_push(__list_84, jsonStrField("layout", layout));
    btrc_Vector_string_push(__list_84, jsonField("capabilities", jsonStringArray(capabilities)));
    btrc_Vector_string_push(__list_84, jsonField("default_outputs", jsonStringArray(defaultOutputs)));
    btrc_Vector_string_push(__list_84, jsonStrField("preferred_backend", preferredBackend));
    btrc_Vector_string_push(__list_84, jsonStrField("gyro_policy", gyroPolicy));
    btrc_Vector_string_push(__list_84, jsonStrField("note", note));
    return jsonObject(__list_84);
}

char* emulationBackend(char* id, char* label, char* layer, btrc_Vector_string* emits, btrc_Vector_string* requires, bool automated, bool visual, char* note) {
    btrc_Vector_string* __list_85 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_85, jsonStrField("id", id));
    btrc_Vector_string_push(__list_85, jsonStrField("label", label));
    btrc_Vector_string_push(__list_85, jsonStrField("layer", layer));
    btrc_Vector_string_push(__list_85, jsonField("emits", jsonStringArray(emits)));
    btrc_Vector_string_push(__list_85, jsonField("requires", jsonStringArray(requires)));
    btrc_Vector_string_push(__list_85, jsonBoolField("automated", automated));
    btrc_Vector_string_push(__list_85, jsonBoolField("requires_visual", visual));
    btrc_Vector_string_push(__list_85, jsonStrField("note", note));
    return jsonObject(__list_85);
}

char* verificationProfile(char* id, char* label, char* backend, btrc_Vector_string* controllers, btrc_Vector_string* checks, bool automated, bool visual) {
    btrc_Vector_string* __list_86 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_86, jsonStrField("id", id));
    btrc_Vector_string_push(__list_86, jsonStrField("label", label));
    btrc_Vector_string_push(__list_86, jsonStrField("backend", backend));
    btrc_Vector_string_push(__list_86, jsonField("controllers", jsonStringArray(controllers)));
    btrc_Vector_string_push(__list_86, jsonField("checks", jsonStringArray(checks)));
    btrc_Vector_string_push(__list_86, jsonBoolField("automated", automated));
    btrc_Vector_string_push(__list_86, jsonBoolField("requires_visual", visual));
    return jsonObject(__list_86);
}

char* screenshotToolSpec(char* id, char* command, btrc_Vector_string* args, btrc_Vector_string* requires, char* note) {
    btrc_Vector_string* __list_87 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_87, jsonStrField("id", id));
    btrc_Vector_string_push(__list_87, jsonStrField("command", command));
    btrc_Vector_string_push(__list_87, jsonField("args", jsonStringArray(args)));
    btrc_Vector_string_push(__list_87, jsonField("requires", jsonStringArray(requires)));
    btrc_Vector_string_push(__list_87, jsonStrField("note", note));
    return jsonObject(__list_87);
}

char* screenshotHookSpec(char* id, char* lifecycle, char* output, char* enabledBy, int delayMs, btrc_Vector_string* emulators, char* note) {
    btrc_Vector_string* __list_88 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_88, jsonStrField("id", id));
    btrc_Vector_string_push(__list_88, jsonStrField("lifecycle", lifecycle));
    btrc_Vector_string_push(__list_88, jsonStrField("output", output));
    btrc_Vector_string_push(__list_88, jsonStrField("enabled_by", enabledBy));
    int __fstr_89_len = snprintf(NULL, 0, "%d", delayMs);
    char* __fstr_89_buf = __btrc_str_track(((char*)malloc((__fstr_89_len + 1))));
    snprintf(__fstr_89_buf, (__fstr_89_len + 1), "%d", delayMs);
    btrc_Vector_string_push(__list_88, jsonField("delay_ms", __fstr_89_buf));
    btrc_Vector_string_push(__list_88, jsonField("emulators", jsonStringArray(emulators)));
    btrc_Vector_string_push(__list_88, jsonStrField("note", note));
    return jsonObject(__list_88);
}

char* screenshotVerificationProfile(void) {
    btrc_Vector_string* emulators = lowercaseValues(linuxLauncherNames());
    char* output = "${paths.project_screenshots}/verification/${emulator}/${hook}.png";
    char* enabledBy = "SEMU_SCREENSHOT_HOOKS=1";
    btrc_Vector_string* __list_90 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_90, jsonBoolField("enabled_by_default", false));
    btrc_Vector_string_push(__list_90, jsonStrField("enable_env", enabledBy));
    btrc_Vector_string_push(__list_90, jsonStrField("delay_env", "SEMU_SCREENSHOT_DELAY_SECONDS"));
    btrc_Vector_string_push(__list_90, jsonStrField("output_root", "${paths.project_screenshots}/verification"));
    btrc_Vector_string* __list_91 = btrc_Vector_string_new();
    btrc_Vector_string* __list_92 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_92, "${output}");
    btrc_Vector_string* __list_93 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_93, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_91, screenshotToolSpec("grim", "grim", __list_92, __list_93, "SteamOS/KDE Wayland screenshot path."));
    btrc_Vector_string* __list_94 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_94, "-b");
    btrc_Vector_string_push(__list_94, "-n");
    btrc_Vector_string_push(__list_94, "-o");
    btrc_Vector_string_push(__list_94, "${output}");
    btrc_Vector_string* __list_95 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_95, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_91, screenshotToolSpec("spectacle", "spectacle", __list_94, __list_95, "KDE fallback when grim is not installed."));
    btrc_Vector_string* __list_96 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_96, "-f");
    btrc_Vector_string_push(__list_96, "${output}");
    btrc_Vector_string* __list_97 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_97, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_97, "DISPLAY");
    btrc_Vector_string_push(__list_91, screenshotToolSpec("gnome_screenshot", "gnome-screenshot", __list_96, __list_97, "Generic desktop fallback."));
    btrc_Vector_string* __list_98 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_98, "-window");
    btrc_Vector_string_push(__list_98, "root");
    btrc_Vector_string_push(__list_98, "${output}");
    btrc_Vector_string* __list_99 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_99, "DISPLAY");
    btrc_Vector_string_push(__list_91, screenshotToolSpec("imagemagick_import", "import", __list_98, __list_99, "X11 fallback for emulator windows that require XWayland."));
    btrc_Vector_string_push(__list_90, jsonField("tools", jsonArray(__list_91)));
    btrc_Vector_string* __list_100 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_100, screenshotHookSpec("before_launch", "launcher.before_exec", output, enabledBy, 0, emulators, "Captures the desktop state immediately before launching an emulator."));
    btrc_Vector_string_push(__list_100, screenshotHookSpec("after_spawn", "launcher.after_spawn", output, enabledBy, 2000, emulators, "Captures the emulator after the process has had time to present its first frame."));
    btrc_Vector_string_push(__list_100, screenshotHookSpec("after_exit", "launcher.after_exit", output, enabledBy, 0, emulators, "Captures the return path after the emulator exits, usually back to ES-DE."));
    btrc_Vector_string_push(__list_100, screenshotHookSpec("manual_visual_checkpoint", "operator.manual", output, "semu screenshot capture", 0, emulators, "CLI hook for VM/Deck verification scripts when a process is already running."));
    btrc_Vector_string_push(__list_90, jsonField("hooks", jsonArray(__list_100)));
    return jsonObject(__list_90);
}

char* inputStack(void) {
    btrc_Vector_string* __list_101 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_101, "steam_deck");
    btrc_Vector_string_push(__list_101, "steam_controller");
    btrc_Vector_string_push(__list_101, "xbox_xinput");
    btrc_Vector_string_push(__list_101, "dualshock4");
    btrc_Vector_string_push(__list_101, "dualsense");
    btrc_Vector_string_push(__list_101, "switch_pro");
    btrc_Vector_string* controllerIds = __list_101;
    btrc_Vector_string* __list_102 = btrc_Vector_string_new();
    btrc_Vector_string* __list_103 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_103, "controller_model");
    btrc_Vector_string_push(__list_103, "emulation_backend");
    btrc_Vector_string_push(__list_103, "emitted_input");
    btrc_Vector_string_push(__list_103, "emulator_keymap");
    btrc_Vector_string_push(__list_102, jsonField("layers", jsonStringArray(__list_103)));
    btrc_Vector_string* __list_104 = btrc_Vector_string_new();
    btrc_Vector_string* __list_105 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_105, "gamepad");
    btrc_Vector_string_push(__list_105, "left_trackpad");
    btrc_Vector_string_push(__list_105, "right_trackpad");
    btrc_Vector_string_push(__list_105, "grip_buttons");
    btrc_Vector_string_push(__list_105, "gyro");
    btrc_Vector_string_push(__list_105, "mouse_pointer");
    btrc_Vector_string_push(__list_105, "keyboard_hotkeys");
    btrc_Vector_string* __list_106 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_106, "gamepad");
    btrc_Vector_string_push(__list_106, "keyboard");
    btrc_Vector_string_push(__list_106, "mouse");
    btrc_Vector_string_push(__list_104, controllerModel("steam_deck", "Steam Deck Built-in Controller", "Valve", "xbox", __list_105, __list_106, "inputplumber", "opt_in", "Primary target: right trackpad mouse, left trackpad radial hotkeys, gyro opt-in."));
    btrc_Vector_string* __list_107 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_107, "gamepad");
    btrc_Vector_string_push(__list_107, "left_trackpad");
    btrc_Vector_string_push(__list_107, "right_trackpad");
    btrc_Vector_string_push(__list_107, "dual_stage_triggers");
    btrc_Vector_string_push(__list_107, "gyro");
    btrc_Vector_string_push(__list_107, "keyboard_hotkeys");
    btrc_Vector_string* __list_108 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_108, "gamepad");
    btrc_Vector_string_push(__list_108, "keyboard");
    btrc_Vector_string_push(__list_108, "mouse");
    btrc_Vector_string_push(__list_104, controllerModel("steam_controller", "Steam Controller", "Valve", "steam", __list_107, __list_108, "inputplumber", "opt_in", "Same abstraction as Deck controls, but with external Steam Controller hardware."));
    btrc_Vector_string* __list_109 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_109, "gamepad");
    btrc_Vector_string_push(__list_109, "analog_triggers");
    btrc_Vector_string_push(__list_109, "rumble");
    btrc_Vector_string* __list_110 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_110, "gamepad");
    btrc_Vector_string_push(__list_110, "keyboard");
    btrc_Vector_string_push(__list_104, controllerModel("xbox_xinput", "Xbox / XInput Controller", "Microsoft", "xbox", __list_109, __list_110, "uinput", "hardware_absent", "Baseline portable gamepad model for SDL/XInput-style controllers."));
    btrc_Vector_string* __list_111 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_111, "gamepad");
    btrc_Vector_string_push(__list_111, "touchpad");
    btrc_Vector_string_push(__list_111, "gyro");
    btrc_Vector_string_push(__list_111, "rumble");
    btrc_Vector_string* __list_112 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_112, "gamepad");
    btrc_Vector_string_push(__list_112, "keyboard");
    btrc_Vector_string_push(__list_112, "mouse");
    btrc_Vector_string_push(__list_104, controllerModel("dualshock4", "DualShock 4", "Sony", "playstation", __list_111, __list_112, "uinput", "opt_in", "PlayStation layout with touchpad available for mouse or menu bindings."));
    btrc_Vector_string* __list_113 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_113, "gamepad");
    btrc_Vector_string_push(__list_113, "touchpad");
    btrc_Vector_string_push(__list_113, "gyro");
    btrc_Vector_string_push(__list_113, "adaptive_triggers");
    btrc_Vector_string_push(__list_113, "rumble");
    btrc_Vector_string* __list_114 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_114, "gamepad");
    btrc_Vector_string_push(__list_114, "keyboard");
    btrc_Vector_string_push(__list_114, "mouse");
    btrc_Vector_string_push(__list_104, controllerModel("dualsense", "DualSense", "Sony", "playstation", __list_113, __list_114, "uinput", "opt_in", "DualShock-family mapping plus DualSense-specific trigger capability metadata."));
    btrc_Vector_string* __list_115 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_115, "gamepad");
    btrc_Vector_string_push(__list_115, "gyro");
    btrc_Vector_string_push(__list_115, "rumble");
    btrc_Vector_string* __list_116 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_116, "gamepad");
    btrc_Vector_string_push(__list_116, "keyboard");
    btrc_Vector_string_push(__list_104, controllerModel("switch_pro", "Nintendo Switch Pro Controller", "Nintendo", "nintendo", __list_115, __list_116, "uinput", "opt_in", "Nintendo layout model; face-button labeling can be handled above emulator keymaps."));
    btrc_Vector_string_push(__list_102, jsonField("controller_models", jsonArray(__list_104)));
    btrc_Vector_string* __list_117 = btrc_Vector_string_new();
    btrc_Vector_string* __list_118 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_118, "gamepad");
    btrc_Vector_string_push(__list_118, "keyboard");
    btrc_Vector_string_push(__list_118, "mouse");
    btrc_Vector_string* __list_119 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_119, "/dev/uinput");
    btrc_Vector_string_push(__list_117, emulationBackend("uinput", "Linux uinput", "kernel_virtual_input", __list_118, __list_119, true, false, "Fast automated Linux smoke tests for emitted key/button events."));
    btrc_Vector_string* __list_120 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_120, "recorded_evdev");
    btrc_Vector_string* __list_121 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_121, "evemu");
    btrc_Vector_string_push(__list_121, "/dev/input");
    btrc_Vector_string_push(__list_117, emulationBackend("evemu", "evemu record/replay", "evdev_replay", __list_120, __list_121, true, false, "Replay captured physical controller events into the same keymap pipeline."));
    btrc_Vector_string* __list_122 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_122, "hid_device");
    btrc_Vector_string* __list_123 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_123, "/dev/uhid");
    btrc_Vector_string_push(__list_117, emulationBackend("uhid", "Linux UHID", "userspace_hid", __list_122, __list_123, true, false, "Use when Steam needs a HID-shaped device instead of a generic uinput pad."));
    btrc_Vector_string* __list_124 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_124, "gamepad");
    btrc_Vector_string_push(__list_124, "keyboard");
    btrc_Vector_string_push(__list_124, "mouse");
    btrc_Vector_string* __list_125 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_125, "inputplumber");
    btrc_Vector_string_push(__list_125, "/dev/uinput");
    btrc_Vector_string_push(__list_117, emulationBackend("inputplumber", "InputPlumber", "routing_daemon", __list_124, __list_125, true, false, "Preferred route for Deck-style trackpad/radial/keyboard composition."));
    btrc_Vector_string* __list_126 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_126, "steam_virtual_gamepad");
    btrc_Vector_string_push(__list_126, "keyboard");
    btrc_Vector_string_push(__list_126, "mouse");
    btrc_Vector_string* __list_127 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_127, "steam");
    btrc_Vector_string_push(__list_127, "gamescope_or_game_mode");
    btrc_Vector_string_push(__list_117, emulationBackend("steam_input", "Steam Input / Game Mode", "steam_client", __list_126, __list_127, false, true, "Final integration layer; requires visual/live Steam verification."));
    btrc_Vector_string_push(__list_102, jsonField("emulation_backends", jsonArray(__list_117)));
    btrc_Vector_string* __list_128 = btrc_Vector_string_new();
    btrc_Vector_string* __list_129 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_129, "device_created");
    btrc_Vector_string_push(__list_129, "save_load_quit_events");
    btrc_Vector_string_push(__list_129, "mouse_motion_optional");
    btrc_Vector_string_push(__list_129, "no_gyro_events_when_disabled");
    btrc_Vector_string_push(__list_128, verificationProfile("linux_virtual_input", "Linux virtual input smoke test", "uinput", controllerIds, __list_129, true, false));
    btrc_Vector_string* __list_130 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_130, "descriptor_replay");
    btrc_Vector_string_push(__list_130, "button_event_order");
    btrc_Vector_string_push(__list_130, "axis_ranges");
    btrc_Vector_string_push(__list_128, verificationProfile("evdev_replay", "Recorded controller replay", "evemu", controllerIds, __list_130, true, false));
    btrc_Vector_string* __list_131 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_131, "steam_deck");
    btrc_Vector_string_push(__list_131, "steam_controller");
    btrc_Vector_string* __list_132 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_132, "steam_detects_device");
    btrc_Vector_string_push(__list_132, "hid_identity_stable");
    btrc_Vector_string_push(__list_128, verificationProfile("steam_hid_shape", "Steam HID shape test", "uhid", __list_131, __list_132, true, false));
    btrc_Vector_string* __list_133 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_133, "steam_deck");
    btrc_Vector_string_push(__list_133, "steam_controller");
    btrc_Vector_string* __list_134 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_134, "right_trackpad_mouse");
    btrc_Vector_string_push(__list_134, "left_radial_hotkeys");
    btrc_Vector_string_push(__list_134, "keyboard_chords");
    btrc_Vector_string_push(__list_128, verificationProfile("deck_route", "Deck-style route test", "inputplumber", __list_133, __list_134, true, false));
    btrc_Vector_string* __list_135 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_135, "steam_deck");
    btrc_Vector_string* __list_136 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_136, "template_visible");
    btrc_Vector_string_push(__list_136, "radial_menu_visual");
    btrc_Vector_string_push(__list_136, "hotkeys_work_in_emulators");
    btrc_Vector_string_push(__list_136, "quit_returns_to_es_de");
    btrc_Vector_string_push(__list_128, verificationProfile("steam_deck_game_mode", "Steam Deck Game Mode final pass", "steam_input", __list_135, __list_136, false, true));
    btrc_Vector_string_push(__list_102, jsonField("verification_profiles", jsonArray(__list_128)));
    return jsonObject(__list_102);
}

char* controllerProfiles(void) {
    btrc_Vector_string* __list_137 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_137, jsonStrField("templates_dir", "~/.steam/steam/controller_base/templates"));
    btrc_Vector_string* __list_138 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_138, steamInputTemplate("neptune_simple", "Semu: Steam Deck - Neptune SIMPLE", "${project}/input/steam-input/neptune-simple.vdf", "Generated Steam Input template; doctor validates it and steam-input install copies it."));
    btrc_Vector_string_push(__list_138, steamInputTemplate("neptune_full", "Semu: Steam Deck - Neptune FULL", "${project}/input/steam-input/neptune-full.vdf", "Optional full hotkey layout for users who want RetroDeck-style chords."));
    btrc_Vector_string_push(__list_137, jsonField("templates", jsonArray(__list_138)));
    char* steamInput = jsonObject(__list_137);
    btrc_Vector_string* __list_141 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_141, jsonStrField("controller_model", "steam_deck"));
    btrc_Vector_string* __list_142 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_142, "linux_virtual_input");
    btrc_Vector_string_push(__list_142, "deck_route");
    btrc_Vector_string_push(__list_142, "steam_deck_game_mode");
    btrc_Vector_string_push(__list_141, jsonField("verification_profiles", jsonStringArray(__list_142)));
    btrc_Vector_string_push(__list_141, jsonStrField("description", "Steam Deck built-in controls with gyro opt-in, right trackpad as mouse, and left trackpad radial hotkeys."));
    btrc_Vector_string_push(__list_141, jsonBoolField("gyro_enabled", false));
    btrc_Vector_string_push(__list_141, jsonStrField("hotkey_button", "View; L4/R4 optional in Steam Input templates"));
    btrc_Vector_string* __list_143 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_143, jsonStrField("left", "radial_hotkeys"));
    btrc_Vector_string_push(__list_143, jsonStrField("right", "mouse"));
    btrc_Vector_string_push(__list_141, jsonField("trackpads", jsonObject(__list_143)));
    btrc_Vector_string_push(__list_141, jsonField("hotkeys", steamDeckHotkeys()));
    btrc_Vector_string_push(__list_141, jsonField("steam_input", steamInput));
    btrc_Vector_string_push(__list_141, jsonField("profiles", jsonStringArray(controllerProfileFiles())));
    char* steamDeck = jsonObject(__list_141);
    btrc_Vector_string* __list_147 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_147, jsonField("steam_deck", steamDeck));
    return jsonObject(__list_147);
}

char* syncFolderSpec(char* id, char* label, char* path, bool enabled, bool watch, int rescanSeconds) {
    btrc_Vector_string* __list_148 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_148, jsonStrField("id", id));
    btrc_Vector_string_push(__list_148, jsonStrField("label", label));
    btrc_Vector_string_push(__list_148, jsonStrField("path", path));
    btrc_Vector_string_push(__list_148, jsonBoolField("enabled", enabled));
    btrc_Vector_string_push(__list_148, jsonBoolField("watch", watch));
    btrc_Vector_string_push(__list_148, jsonField("rescan_interval_s", Strings_fromInt(rescanSeconds)));
    return jsonObject(__list_148);
}

char* syncProfile(void) {
    btrc_Vector_string* __list_149 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_149, jsonBoolField("enabled", true));
    btrc_Vector_string_push(__list_149, jsonBoolField("start_at_boot", true));
    btrc_Vector_string_push(__list_149, jsonBoolField("tray", true));
    btrc_Vector_string_push(__list_149, jsonStrField("engine", "syncthing"));
    btrc_Vector_string_push(__list_149, jsonStrField("tray_app", "syncthingtray"));
    btrc_Vector_string_push(__list_149, jsonStrField("gui_address", "127.0.0.1:8384"));
    btrc_Vector_string* __list_150 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_150, syncFolderSpec("saves", "Saves", "${paths.project_saves}", true, true, 900));
    btrc_Vector_string_push(__list_150, syncFolderSpec("states", "States", "${paths.project_states}", true, true, 900));
    btrc_Vector_string_push(__list_150, syncFolderSpec("emulator_state", "Emulator State", "${paths.project}/.semu/appimage-state", true, true, 900));
    btrc_Vector_string_push(__list_150, syncFolderSpec("screenshots", "Screenshots", "${paths.project_screenshots}", true, true, 1800));
    btrc_Vector_string_push(__list_150, syncFolderSpec("gamelists", "Gamelists", "${paths.project_gamelists}", true, true, 1800));
    btrc_Vector_string_push(__list_150, syncFolderSpec("roms", "ROMs", "${paths.project_roms}", false, true, 3600));
    btrc_Vector_string_push(__list_150, syncFolderSpec("bios", "BIOS", "${paths.project_bios}", false, true, 3600));
    btrc_Vector_string_push(__list_149, jsonField("folders", jsonArray(__list_150)));
    return jsonObject(__list_149);
}

char* biosEntries(void) {
    btrc_Vector_string* __list_151 = btrc_Vector_string_new();
    btrc_Vector_string* __list_152 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_152, "scph5500.bin");
    btrc_Vector_string_push(__list_152, "scph5501.bin");
    btrc_Vector_string_push(__list_152, "scph5502.bin");
    btrc_Vector_string_push(__list_151, biosSpec("psx", "psx", true, __list_152, "${paths.bios}", "", "Beetle PSX expects user-supplied PlayStation BIOS dumps."));
    btrc_Vector_string* __list_153 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_153, "ps2-0230a-20080220.bin");
    btrc_Vector_string_push(__list_153, "ps2-0230e-20080220.bin");
    btrc_Vector_string_push(__list_153, "ps2-0230j-20080220.bin");
    btrc_Vector_string_push(__list_151, biosSpec("ps2", "ps2", true, __list_153, "${paths.bios}/ps2", "any", "PCSX2 needs at least one legally dumped PS2 BIOS image."));
    btrc_Vector_string* __list_154 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_154, "prod.keys");
    btrc_Vector_string_push(__list_154, "title.keys");
    btrc_Vector_string_push(__list_151, biosSpec("switch_keys", "switch", true, __list_154, "${paths.bios}/switch", "", "Ryujinx needs dumped Switch keys and firmware from the user's console."));
    btrc_Vector_string* __list_155 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_155, "keys.txt");
    btrc_Vector_string_push(__list_151, biosSpec("wiiu_keys", "wiiu", true, __list_155, "${project}/emulators/profiles/Cemu/data", "", "Cemu needs user-supplied Wii U keys for encrypted content."));
    btrc_Vector_string* __list_156 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_156, "dc_boot.bin");
    btrc_Vector_string_push(__list_156, "dc_flash.bin");
    btrc_Vector_string_push(__list_151, biosSpec("dreamcast", "dreamcast", false, __list_156, "${paths.bios}/dc", "", "Flycast can use HLE for many games, but original BIOS files improve compatibility."));
    return jsonArray(__list_151);
}

char* systemEntries(void) {
    SystemCatalog* catalog = systemCatalog();
    return SystemCatalog_systemsJson(catalog);
}

char* buildJson(void) {
    btrc_Vector_string* __list_157 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_157, jsonField("schema_version", "1"));
    btrc_Vector_string_push(__list_157, jsonField("paths", manifestPaths()));
    btrc_Vector_string_push(__list_157, jsonField("es_de", esDe()));
    btrc_Vector_string_push(__list_157, jsonField("input", inputStack()));
    btrc_Vector_string_push(__list_157, jsonField("controller_profiles", controllerProfiles()));
    btrc_Vector_string_push(__list_157, jsonField("keymaps", keymapsJson()));
    btrc_Vector_string_push(__list_157, jsonField("screenshot_verification", screenshotVerificationProfile()));
    btrc_Vector_string_push(__list_157, jsonField("sync", syncProfile()));
    btrc_Vector_string_push(__list_157, jsonField("bios", biosEntries()));
    btrc_Vector_string_push(__list_157, jsonField("launchers", launcherEntries()));
    btrc_Vector_string_push(__list_157, jsonField("systems", systemEntries()));
    return jsonObject(__list_157);
}

void SystemCatalog_init(SystemCatalog* self) {
    self->__rc = 1;
    if (self->ids != NULL) {
        if ((--self->ids->__rc) <= 0) {
            btrc_Vector_string_free(self->ids);
        }
    }
    btrc_Vector_string* __list_159 = btrc_Vector_string_new();
    (self->ids = __list_159);
    btrc_Vector_string* __list_158 = btrc_Vector_string_new();
    (__list_158->__rc++);
    if (self->romDirs != NULL) {
        if ((--self->romDirs->__rc) <= 0) {
            btrc_Vector_string_free(self->romDirs);
        }
    }
    btrc_Vector_string* __list_161 = btrc_Vector_string_new();
    (self->romDirs = __list_161);
    btrc_Vector_string* __list_160 = btrc_Vector_string_new();
    (__list_160->__rc++);
    if (self->jsonSpecs != NULL) {
        if ((--self->jsonSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->jsonSpecs);
        }
    }
    btrc_Vector_string* __list_163 = btrc_Vector_string_new();
    (self->jsonSpecs = __list_163);
    btrc_Vector_string* __list_162 = btrc_Vector_string_new();
    (__list_162->__rc++);
    if (self->esSystemSpecs != NULL) {
        if ((--self->esSystemSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->esSystemSpecs);
        }
    }
    btrc_Vector_string* __list_165 = btrc_Vector_string_new();
    (self->esSystemSpecs = __list_165);
    btrc_Vector_string* __list_164 = btrc_Vector_string_new();
    (__list_164->__rc++);
}

SystemCatalog* SystemCatalog_new(void) {
    SystemCatalog* self = ((SystemCatalog*)malloc(sizeof(SystemCatalog)));
    memset(self, 0, sizeof(SystemCatalog));
    SystemCatalog_init(self);
    return self;
}

void SystemCatalog_destroy(SystemCatalog* self) {
    if (self->ids != NULL) {
        if ((--self->ids->__rc) <= 0) {
            btrc_Vector_string_free(self->ids);
        }
    }
    if (self->romDirs != NULL) {
        if ((--self->romDirs->__rc) <= 0) {
            btrc_Vector_string_free(self->romDirs);
        }
    }
    if (self->jsonSpecs != NULL) {
        if ((--self->jsonSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->jsonSpecs);
        }
    }
    if (self->esSystemSpecs != NULL) {
        if ((--self->esSystemSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->esSystemSpecs);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void SystemCatalog_addSystem(SystemCatalog* self, char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile) {
    btrc_Vector_string_push(self->ids, id);
    btrc_Vector_string_push(self->romDirs, romDir);
    btrc_Vector_string_push(self->jsonSpecs, systemSpec(id, fullname, platform, theme, romDir, extensions, linuxCommands, macosCommands, bios, controllerProfile));
    btrc_Vector_string_push(self->esSystemSpecs, esSystemSpec(id, fullname, platform, theme, romDir, extensions, linuxCommands));
}

char* SystemCatalog_systemsJson(SystemCatalog* self) {
    return jsonArray(self->jsonSpecs);
}

char* SystemCatalog_esSystemsXml(SystemCatalog* self) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by semu.btrc from the system catalog -->\n")), "<systemList>\n")), btrc_Vector_string_join(self->esSystemSpecs, ""))), "</systemList>\n"));
}

SystemCatalog* systemCatalog(void) {
    SystemCatalog* catalog = SystemCatalog_new();
    btrc_Vector_string* noBios = btrc_Vector_string_new();
    btrc_Vector_string* __list_169 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_169, ".bin");
    btrc_Vector_string_push(__list_169, ".dmg");
    btrc_Vector_string_push(__list_169, ".gb");
    btrc_Vector_string_push(__list_169, ".gbs");
    btrc_Vector_string_push(__list_169, ".7z");
    btrc_Vector_string_push(__list_169, ".zip");
    btrc_Vector_string* __list_170 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_170, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%"));
    btrc_Vector_string* __list_171 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_171, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gb", "Nintendo Game Boy", "gb", "gb", "gb", __list_169, __list_170, __list_171, noBios, "");
    btrc_Vector_string* __list_175 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_175, ".bin");
    btrc_Vector_string_push(__list_175, ".cgb");
    btrc_Vector_string_push(__list_175, ".gb");
    btrc_Vector_string_push(__list_175, ".gbc");
    btrc_Vector_string_push(__list_175, ".7z");
    btrc_Vector_string_push(__list_175, ".zip");
    btrc_Vector_string* __list_176 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_176, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%"));
    btrc_Vector_string* __list_177 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_177, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gbc", "Nintendo Game Boy Color", "gbc", "gbc", "gbc", __list_175, __list_176, __list_177, noBios, "");
    btrc_Vector_string* __list_181 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_181, ".agb");
    btrc_Vector_string_push(__list_181, ".bin");
    btrc_Vector_string_push(__list_181, ".cgb");
    btrc_Vector_string_push(__list_181, ".gb");
    btrc_Vector_string_push(__list_181, ".gba");
    btrc_Vector_string_push(__list_181, ".gbc");
    btrc_Vector_string_push(__list_181, ".7z");
    btrc_Vector_string_push(__list_181, ".zip");
    btrc_Vector_string* __list_182 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_182, commandSpec("mGBA", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mgba_libretro.so %ROM%"));
    btrc_Vector_string* __list_183 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_183, commandSpec("mGBA", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mgba_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gba", "Nintendo Game Boy Advance", "gba", "gba", "gba", __list_181, __list_182, __list_183, noBios, "");
    btrc_Vector_string* __list_187 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_187, ".3dsen");
    btrc_Vector_string_push(__list_187, ".bin");
    btrc_Vector_string_push(__list_187, ".fds");
    btrc_Vector_string_push(__list_187, ".nes");
    btrc_Vector_string_push(__list_187, ".unf");
    btrc_Vector_string_push(__list_187, ".unif");
    btrc_Vector_string_push(__list_187, ".7z");
    btrc_Vector_string_push(__list_187, ".zip");
    btrc_Vector_string* __list_188 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_188, commandSpec("Mesen", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mesen_libretro.so %ROM%"));
    btrc_Vector_string* __list_189 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_189, commandSpec("Mesen", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mesen_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "nes", "Nintendo Entertainment System", "nes", "nes", "nes", __list_187, __list_188, __list_189, noBios, "");
    btrc_Vector_string* __list_193 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_193, ".bin");
    btrc_Vector_string_push(__list_193, ".bml");
    btrc_Vector_string_push(__list_193, ".bs");
    btrc_Vector_string_push(__list_193, ".bsx");
    btrc_Vector_string_push(__list_193, ".fig");
    btrc_Vector_string_push(__list_193, ".sfc");
    btrc_Vector_string_push(__list_193, ".smc");
    btrc_Vector_string_push(__list_193, ".swc");
    btrc_Vector_string_push(__list_193, ".7z");
    btrc_Vector_string_push(__list_193, ".zip");
    btrc_Vector_string* __list_194 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_194, commandSpec("Snes9x", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/snes9x_libretro.so %ROM%"));
    btrc_Vector_string* __list_195 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_195, commandSpec("Snes9x", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/snes9x_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "snes", "Super Nintendo", "snes", "snes", "snes", __list_193, __list_194, __list_195, noBios, "");
    btrc_Vector_string* __list_199 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_199, ".32x");
    btrc_Vector_string_push(__list_199, ".68k");
    btrc_Vector_string_push(__list_199, ".bin");
    btrc_Vector_string_push(__list_199, ".chd");
    btrc_Vector_string_push(__list_199, ".gen");
    btrc_Vector_string_push(__list_199, ".iso");
    btrc_Vector_string_push(__list_199, ".md");
    btrc_Vector_string_push(__list_199, ".mdx");
    btrc_Vector_string_push(__list_199, ".sg");
    btrc_Vector_string_push(__list_199, ".smd");
    btrc_Vector_string_push(__list_199, ".7z");
    btrc_Vector_string_push(__list_199, ".zip");
    btrc_Vector_string* __list_200 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_200, commandSpec("Genesis Plus GX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/genesis_plus_gx_libretro.so %ROM%"));
    btrc_Vector_string* __list_201 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_201, commandSpec("Genesis Plus GX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/genesis_plus_gx_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "genesis", "Sega Genesis", "genesis,megadrive", "genesis", "genesis", __list_199, __list_200, __list_201, noBios, "");
    btrc_Vector_string* __list_205 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_205, ".bin");
    btrc_Vector_string_push(__list_205, ".d64");
    btrc_Vector_string_push(__list_205, ".n64");
    btrc_Vector_string_push(__list_205, ".ndd");
    btrc_Vector_string_push(__list_205, ".u1");
    btrc_Vector_string_push(__list_205, ".v64");
    btrc_Vector_string_push(__list_205, ".z64");
    btrc_Vector_string_push(__list_205, ".7z");
    btrc_Vector_string_push(__list_205, ".zip");
    btrc_Vector_string* __list_206 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_206, commandSpec("Mupen64Plus-Next", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mupen64plus_next_libretro.so %ROM%"));
    btrc_Vector_string* __list_207 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_207, commandSpec("ares", "%EMULATOR_ARES% --fullscreen --system \"Nintendo 64\" %ROM%"));
    SystemCatalog_addSystem(catalog, "n64", "Nintendo 64", "n64", "n64", "n64", __list_205, __list_206, __list_207, noBios, "");
    btrc_Vector_string* __list_211 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_211, ".app");
    btrc_Vector_string_push(__list_211, ".bin");
    btrc_Vector_string_push(__list_211, ".nds");
    btrc_Vector_string_push(__list_211, ".7z");
    btrc_Vector_string_push(__list_211, ".zip");
    btrc_Vector_string* __list_212 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_212, commandSpec("melonDS", "%EMULATOR_MELONDS% %ROM%"));
    btrc_Vector_string_push(__list_212, commandSpec("DeSmuME", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/desmume_libretro.so %ROM%"));
    btrc_Vector_string* __list_213 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_213, commandSpec("DeSmuME", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/desmume_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "nds", "Nintendo DS", "nds", "nds", "nds", __list_211, __list_212, __list_213, noBios, "");
    btrc_Vector_string* __list_218 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_218, ".cdi");
    btrc_Vector_string_push(__list_218, ".chd");
    btrc_Vector_string_push(__list_218, ".cue");
    btrc_Vector_string_push(__list_218, ".elf");
    btrc_Vector_string_push(__list_218, ".gdi");
    btrc_Vector_string_push(__list_218, ".iso");
    btrc_Vector_string_push(__list_218, ".lst");
    btrc_Vector_string_push(__list_218, ".m3u");
    btrc_Vector_string_push(__list_218, ".7z");
    btrc_Vector_string_push(__list_218, ".zip");
    btrc_Vector_string* __list_219 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_219, commandSpec("Flycast", "%EMULATOR_FLYCAST% %ROM%"));
    btrc_Vector_string* __list_220 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_220, commandSpec("Flycast", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/flycast_libretro.dylib %ROM%"));
    btrc_Vector_string* __list_221 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_221, "dreamcast");
    SystemCatalog_addSystem(catalog, "dreamcast", "Sega Dreamcast", "dreamcast", "dreamcast", "dreamcast", __list_218, __list_219, __list_220, __list_221, "");
    btrc_Vector_string* __list_226 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_226, ".bin");
    btrc_Vector_string_push(__list_226, ".ccd");
    btrc_Vector_string_push(__list_226, ".chd");
    btrc_Vector_string_push(__list_226, ".cue");
    btrc_Vector_string_push(__list_226, ".ecm");
    btrc_Vector_string_push(__list_226, ".exe");
    btrc_Vector_string_push(__list_226, ".img");
    btrc_Vector_string_push(__list_226, ".iso");
    btrc_Vector_string_push(__list_226, ".m3u");
    btrc_Vector_string_push(__list_226, ".pbp");
    btrc_Vector_string_push(__list_226, ".psexe");
    btrc_Vector_string_push(__list_226, ".toc");
    btrc_Vector_string_push(__list_226, ".7z");
    btrc_Vector_string_push(__list_226, ".zip");
    btrc_Vector_string* __list_227 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_227, commandSpec("Beetle PSX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mednafen_psx_libretro.so %ROM%"));
    btrc_Vector_string* __list_228 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_228, commandSpec("Beetle PSX HW", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mednafen_psx_hw_libretro.dylib %ROM%"));
    btrc_Vector_string* __list_229 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_229, "psx");
    SystemCatalog_addSystem(catalog, "psx", "Sony PlayStation", "psx", "psx", "psx", __list_226, __list_227, __list_228, __list_229, "");
    btrc_Vector_string* __list_234 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_234, ".bin");
    btrc_Vector_string_push(__list_234, ".chd");
    btrc_Vector_string_push(__list_234, ".cso");
    btrc_Vector_string_push(__list_234, ".dump");
    btrc_Vector_string_push(__list_234, ".gz");
    btrc_Vector_string_push(__list_234, ".img");
    btrc_Vector_string_push(__list_234, ".iso");
    btrc_Vector_string_push(__list_234, ".mdf");
    btrc_Vector_string_push(__list_234, ".nrg");
    btrc_Vector_string_push(__list_234, ".7z");
    btrc_Vector_string_push(__list_234, ".zip");
    btrc_Vector_string* __list_235 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_235, commandSpec("PCSX2", "%EMULATOR_PCSX2% -batch -fullscreen %ROM%"));
    btrc_Vector_string* __list_236 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_236, commandSpec("PCSX2", "%EMULATOR_PCSX2% -batch -fullscreen %ROM%"));
    btrc_Vector_string* __list_237 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_237, "ps2");
    SystemCatalog_addSystem(catalog, "ps2", "Sony PlayStation 2", "ps2", "ps2", "ps2", __list_234, __list_235, __list_236, __list_237, "");
    btrc_Vector_string* __list_241 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_241, ".chd");
    btrc_Vector_string_push(__list_241, ".cso");
    btrc_Vector_string_push(__list_241, ".elf");
    btrc_Vector_string_push(__list_241, ".iso");
    btrc_Vector_string_push(__list_241, ".pbp");
    btrc_Vector_string_push(__list_241, ".prx");
    btrc_Vector_string_push(__list_241, ".7z");
    btrc_Vector_string_push(__list_241, ".zip");
    btrc_Vector_string* __list_242 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_242, commandSpec("PPSSPP", "%EMULATOR_PPSSPP% %ROM%"));
    btrc_Vector_string* __list_243 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_243, commandSpec("PPSSPP", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/ppsspp_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "psp", "Sony PlayStation Portable", "psp", "psp", "psp", __list_241, __list_242, __list_243, noBios, "");
    btrc_Vector_string* __list_247 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_247, ".3ds");
    btrc_Vector_string_push(__list_247, ".3dsx");
    btrc_Vector_string_push(__list_247, ".app");
    btrc_Vector_string_push(__list_247, ".axf");
    btrc_Vector_string_push(__list_247, ".cci");
    btrc_Vector_string_push(__list_247, ".cxi");
    btrc_Vector_string_push(__list_247, ".elf");
    btrc_Vector_string_push(__list_247, ".7z");
    btrc_Vector_string_push(__list_247, ".zip");
    btrc_Vector_string* __list_248 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_248, commandSpec("Azahar", "%EMULATOR_AZAHAR% %ROM%"));
    btrc_Vector_string* __list_249 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_249, commandSpec("Azahar", "%EMULATOR_AZAHAR% %ROM%"));
    SystemCatalog_addSystem(catalog, "n3ds", "Nintendo 3DS", "n3ds", "n3ds", "n3ds", __list_247, __list_248, __list_249, noBios, "");
    btrc_Vector_string* __list_253 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_253, ".ciso");
    btrc_Vector_string_push(__list_253, ".dol");
    btrc_Vector_string_push(__list_253, ".elf");
    btrc_Vector_string_push(__list_253, ".gcm");
    btrc_Vector_string_push(__list_253, ".gcz");
    btrc_Vector_string_push(__list_253, ".iso");
    btrc_Vector_string_push(__list_253, ".nkit.iso");
    btrc_Vector_string_push(__list_253, ".rvz");
    btrc_Vector_string_push(__list_253, ".tgc");
    btrc_Vector_string_push(__list_253, ".wad");
    btrc_Vector_string_push(__list_253, ".wbfs");
    btrc_Vector_string_push(__list_253, ".wia");
    btrc_Vector_string_push(__list_253, ".7z");
    btrc_Vector_string_push(__list_253, ".zip");
    btrc_Vector_string* __list_254 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_254, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% %ROM%"));
    btrc_Vector_string* __list_255 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_255, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% --config Dolphin.Display.Fullscreen=True -b -e %ROM%"));
    SystemCatalog_addSystem(catalog, "gc", "Nintendo GameCube", "gc", "gc", "gc", __list_253, __list_254, __list_255, noBios, "steam_deck");
    btrc_Vector_string* __list_259 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_259, ".ciso");
    btrc_Vector_string_push(__list_259, ".dol");
    btrc_Vector_string_push(__list_259, ".elf");
    btrc_Vector_string_push(__list_259, ".gcm");
    btrc_Vector_string_push(__list_259, ".gcz");
    btrc_Vector_string_push(__list_259, ".iso");
    btrc_Vector_string_push(__list_259, ".nkit.iso");
    btrc_Vector_string_push(__list_259, ".rvz");
    btrc_Vector_string_push(__list_259, ".tgc");
    btrc_Vector_string_push(__list_259, ".wad");
    btrc_Vector_string_push(__list_259, ".wbfs");
    btrc_Vector_string_push(__list_259, ".wia");
    btrc_Vector_string_push(__list_259, ".7z");
    btrc_Vector_string_push(__list_259, ".zip");
    btrc_Vector_string* __list_260 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_260, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% %ROM%"));
    btrc_Vector_string* __list_261 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_261, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% --config Dolphin.Display.Fullscreen=True -b -e %ROM%"));
    SystemCatalog_addSystem(catalog, "wii", "Nintendo Wii", "wii", "wii", "wii", __list_259, __list_260, __list_261, noBios, "steam_deck");
    btrc_Vector_string* __list_266 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_266, ".rpx");
    btrc_Vector_string_push(__list_266, ".wua");
    btrc_Vector_string_push(__list_266, ".wud");
    btrc_Vector_string_push(__list_266, ".wux");
    btrc_Vector_string* __list_267 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_267, commandSpec("Cemu", "%EMULATOR_CEMU% -f -g %ROM%"));
    btrc_Vector_string* __list_268 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_268, commandSpec("Cemu", "%EMULATOR_CEMU% -f -g %ROM%"));
    btrc_Vector_string* __list_269 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_269, "wiiu_keys");
    SystemCatalog_addSystem(catalog, "wiiu", "Nintendo Wii U", "wiiu", "wiiu", "wiiu", __list_266, __list_267, __list_268, __list_269, "steam_deck");
    btrc_Vector_string* __list_274 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_274, ".nca");
    btrc_Vector_string_push(__list_274, ".nro");
    btrc_Vector_string_push(__list_274, ".nso");
    btrc_Vector_string_push(__list_274, ".nsp");
    btrc_Vector_string_push(__list_274, ".xci");
    btrc_Vector_string_push(__list_274, ".7z");
    btrc_Vector_string_push(__list_274, ".zip");
    btrc_Vector_string* __list_275 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_275, commandSpec("Ryujinx", "%EMULATOR_RYUJINX% --fullscreen %ROM%"));
    btrc_Vector_string* __list_276 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_276, commandSpec("Ryujinx", "%EMULATOR_RYUJINX% --fullscreen %ROM%"));
    btrc_Vector_string* __list_277 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_277, "switch_keys");
    SystemCatalog_addSystem(catalog, "switch", "Nintendo Switch", "switch", "switch", "switch", __list_274, __list_275, __list_276, __list_277, "");
    return catalog;
    if (catalog != NULL) {
        if ((--catalog->__rc) <= 0) {
            SystemCatalog_destroy(catalog);
        }
    }
}

bool isIdentChar(char c) {
    if ((c >= 'a') && (c <= 'z')) {
        return true;
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return true;
    }
    if ((c >= '0') && (c <= '9')) {
        return true;
    }
    if ((((c == '_') || (c == '.')) || (c == '-')) || (c == '/')) {
        return true;
    }
    return false;
}

bool isSpaceChar(char c) {
    return (((c == ' ') || (c == '\t')) || (c == '\r'));
}

char* defaultKeymapSource(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("# Semu Steam Deck keymap\n", "action ui.open = Ctrl+O\n")), "action ui.pause = Ctrl+P\n")), "action ui.screenshot = Ctrl+X\n")), "action ui.fullscreen = Ctrl+Enter\n")), "action ui.menu = Ctrl+M\n")), "action app.quit = Ctrl+Q\n")), "action state.prev = Ctrl+J\n")), "action state.next = Ctrl+K\n")), "action state.load = Ctrl+A\n")), "action state.save = Ctrl+S\n")), "action speed.rewind = Ctrl+-\n")), "action speed.fast = Ctrl++\n")), "action screen.swap = Ctrl+Tab\n")), "action ui.escape = Esc\n")), "bind HKB + A -> ${ui.pause}\n")), "bind HKB + B -> ${ui.screenshot}\n")), "bind HKB + X -> ${ui.fullscreen}\n")), "bind HKB + Y -> ${ui.menu}\n")), "bind HKB + Start -> ${app.quit}\n")), "bind HKB + D-Pad Left -> ${state.prev}\n")), "bind HKB + D-Pad Right -> ${state.next}\n")), "bind HKB + L1 -> ${state.load}\n")), "bind HKB + R1 -> ${state.save}\n")), "bind HKB + L2 -> ${speed.rewind}\n")), "bind HKB + R2 -> ${speed.fast}\n")), "bind HKB + L3 -> ${screen.swap}\n")), "bind HKB + R3 -> ${ui.escape}\n"));
}

char* actionAlias(char* id) {
    if (strcmp(id, "ui.open") == 0) {
        return "open";
    }
    if (strcmp(id, "ui.pause") == 0) {
        return "pause";
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        return "screenshot";
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        return "fullscreen";
    }
    if (strcmp(id, "ui.menu") == 0) {
        return "menu";
    }
    if (strcmp(id, "app.quit") == 0) {
        return "quit";
    }
    if (strcmp(id, "state.prev") == 0) {
        return "previous_state_slot";
    }
    if (strcmp(id, "state.next") == 0) {
        return "next_state_slot";
    }
    if (strcmp(id, "state.load") == 0) {
        return "load_state";
    }
    if (strcmp(id, "state.save") == 0) {
        return "save_state";
    }
    if (strcmp(id, "speed.rewind") == 0) {
        return "rewind";
    }
    if (strcmp(id, "speed.fast") == 0) {
        return "fast_forward";
    }
    if (strcmp(id, "screen.swap") == 0) {
        return "swap_screens";
    }
    if (strcmp(id, "ui.escape") == 0) {
        return "escape";
    }
    return id;
}

char* actionLabel(char* id) {
    if (strcmp(id, "ui.open") == 0) {
        return "Open";
    }
    if (strcmp(id, "ui.pause") == 0) {
        return "Pause / Resume";
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        return "Take Screenshot";
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        return "Toggle Fullscreen";
    }
    if (strcmp(id, "ui.menu") == 0) {
        return "Open Menu";
    }
    if (strcmp(id, "app.quit") == 0) {
        return "Quit Emulator";
    }
    if (strcmp(id, "state.prev") == 0) {
        return "Previous State Slot";
    }
    if (strcmp(id, "state.next") == 0) {
        return "Next State Slot";
    }
    if (strcmp(id, "state.load") == 0) {
        return "Load State";
    }
    if (strcmp(id, "state.save") == 0) {
        return "Save State";
    }
    if (strcmp(id, "speed.rewind") == 0) {
        return "Rewind";
    }
    if (strcmp(id, "speed.fast") == 0) {
        return "Fast Forward";
    }
    if (strcmp(id, "screen.swap") == 0) {
        return "Swap Screens";
    }
    if (strcmp(id, "ui.escape") == 0) {
        return "Escape";
    }
    return id;
}

char* actionNote(char* id) {
    if (strcmp(id, "ui.open") == 0) {
        return "Open content or emulator dialog where supported.";
    }
    if (strcmp(id, "ui.pause") == 0) {
        return "Primary pause toggle for emulator cores and standalone emulators.";
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        return "Screenshot command for emulators that expose a keyboard hotkey.";
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        return "Standalone fullscreen toggle.";
    }
    if (strcmp(id, "ui.menu") == 0) {
        return "Open the emulator quick menu when supported.";
    }
    if (strcmp(id, "app.quit") == 0) {
        return "Unified exit command for Game Mode.";
    }
    if (strcmp(id, "state.prev") == 0) {
        return "Move to the previous save-state slot.";
    }
    if (strcmp(id, "state.next") == 0) {
        return "Move to the next save-state slot.";
    }
    if (strcmp(id, "state.load") == 0) {
        return "Load the current save-state slot.";
    }
    if (strcmp(id, "state.save") == 0) {
        return "Save to the current save-state slot.";
    }
    if (strcmp(id, "speed.rewind") == 0) {
        return "RetroArch rewind where the core supports rewind.";
    }
    if (strcmp(id, "speed.fast") == 0) {
        return "Fast-forward toggle.";
    }
    if (strcmp(id, "screen.swap") == 0) {
        return "Swap displays for dual-screen systems.";
    }
    if (strcmp(id, "ui.escape") == 0) {
        return "Plain escape for menus and desktop-oriented components.";
    }
    return "";
}

btrc_Vector_string* actionSystems(char* id) {
    if (strcmp(id, "ui.open") == 0) {
        btrc_Vector_string* __list_278 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_278, "dolphin");
        return __list_278;
    }
    if (strcmp(id, "ui.pause") == 0) {
        btrc_Vector_string* __list_279 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_279, "retroarch");
        btrc_Vector_string_push(__list_279, "dolphin");
        btrc_Vector_string_push(__list_279, "azahar");
        btrc_Vector_string_push(__list_279, "melonds");
        btrc_Vector_string_push(__list_279, "pcsx2");
        return __list_279;
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        btrc_Vector_string* __list_280 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_280, "retroarch");
        btrc_Vector_string_push(__list_280, "dolphin");
        btrc_Vector_string_push(__list_280, "azahar");
        btrc_Vector_string_push(__list_280, "pcsx2");
        return __list_280;
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        btrc_Vector_string* __list_281 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_281, "dolphin");
        btrc_Vector_string_push(__list_281, "azahar");
        btrc_Vector_string_push(__list_281, "melonds");
        btrc_Vector_string_push(__list_281, "pcsx2");
        return __list_281;
    }
    if (strcmp(id, "ui.menu") == 0) {
        btrc_Vector_string* __list_282 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_282, "retroarch");
        btrc_Vector_string_push(__list_282, "pcsx2");
        btrc_Vector_string_push(__list_282, "ppsspp");
        return __list_282;
    }
    if (strcmp(id, "app.quit") == 0) {
        btrc_Vector_string* __list_283 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_283, "retroarch");
        btrc_Vector_string_push(__list_283, "dolphin");
        btrc_Vector_string_push(__list_283, "azahar");
        btrc_Vector_string_push(__list_283, "pcsx2");
        btrc_Vector_string_push(__list_283, "ppsspp");
        btrc_Vector_string_push(__list_283, "flycast");
        btrc_Vector_string_push(__list_283, "gopher64");
        btrc_Vector_string_push(__list_283, "melonds");
        btrc_Vector_string_push(__list_283, "cemu");
        btrc_Vector_string_push(__list_283, "ryujinx");
        return __list_283;
    }
    if (strcmp(id, "state.prev") == 0) {
        btrc_Vector_string* __list_284 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_284, "retroarch");
        btrc_Vector_string_push(__list_284, "dolphin");
        btrc_Vector_string_push(__list_284, "pcsx2");
        btrc_Vector_string_push(__list_284, "ppsspp");
        return __list_284;
    }
    if (strcmp(id, "state.next") == 0) {
        btrc_Vector_string* __list_285 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_285, "retroarch");
        btrc_Vector_string_push(__list_285, "dolphin");
        btrc_Vector_string_push(__list_285, "pcsx2");
        btrc_Vector_string_push(__list_285, "ppsspp");
        return __list_285;
    }
    if (strcmp(id, "state.load") == 0) {
        btrc_Vector_string* __list_286 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_286, "retroarch");
        btrc_Vector_string_push(__list_286, "dolphin");
        btrc_Vector_string_push(__list_286, "azahar");
        btrc_Vector_string_push(__list_286, "pcsx2");
        btrc_Vector_string_push(__list_286, "ppsspp");
        return __list_286;
    }
    if (strcmp(id, "state.save") == 0) {
        btrc_Vector_string* __list_287 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_287, "retroarch");
        btrc_Vector_string_push(__list_287, "dolphin");
        btrc_Vector_string_push(__list_287, "azahar");
        btrc_Vector_string_push(__list_287, "pcsx2");
        btrc_Vector_string_push(__list_287, "ppsspp");
        return __list_287;
    }
    if (strcmp(id, "speed.rewind") == 0) {
        btrc_Vector_string* __list_288 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_288, "retroarch");
        return __list_288;
    }
    if (strcmp(id, "speed.fast") == 0) {
        btrc_Vector_string* __list_289 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_289, "retroarch");
        btrc_Vector_string_push(__list_289, "melonds");
        btrc_Vector_string_push(__list_289, "pcsx2");
        btrc_Vector_string_push(__list_289, "ppsspp");
        return __list_289;
    }
    if (strcmp(id, "screen.swap") == 0) {
        btrc_Vector_string* __list_290 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_290, "azahar");
        btrc_Vector_string_push(__list_290, "melonds");
        btrc_Vector_string_push(__list_290, "cemu");
        return __list_290;
    }
    if (strcmp(id, "ui.escape") == 0) {
        btrc_Vector_string* __list_291 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_291, "pc");
        return __list_291;
    }
    btrc_Vector_string* empty = btrc_Vector_string_new();
    return empty;
}

int lastPlusIndex(char* text) {
    int result = (-1);
    for (int i = 0; (i < ((int)strlen(text))); (i++)) {
        if (__btrc_charAt(text, i) == '+') {
            (result = i);
        }
    }
    return result;
}

char* keymapCommandKeyPart(char* command) {
    if (__btrc_endsWith(command, "++")) {
        return "+";
    }
    if (__btrc_endsWith(command, "+-")) {
        return "-";
    }
    int plus = lastPlusIndex(command);
    if (plus < 0) {
        return command;
    }
    return __btrc_str_track(__btrc_substring(command, (plus + 1), ((((int)strlen(command)) - plus) - 1)));
}

char* keymapCommandModifierPart(char* command) {
    if (__btrc_endsWith(command, "++") || __btrc_endsWith(command, "+-")) {
        if (((int)strlen(command)) <= 2) {
            return "";
        }
        return __btrc_str_track(__btrc_substring(command, 0, (((int)strlen(command)) - 2)));
    }
    int plus = lastPlusIndex(command);
    if (plus < 0) {
        return "";
    }
    return __btrc_str_track(__btrc_substring(command, 0, plus));
}

KeymapTokens* lexKeymap(char* source, KeymapErrors* errors) {
    KeymapTokens* tokens = KeymapTokens_new();
    int line = 1;
    int column = 1;
    int i = 0;
    while (i < ((int)strlen(source))) {
        char c = __btrc_charAt(source, i);
        if (c == '\n') {
            KeymapTokens_push(tokens, "newline", "\n", line, column);
            (i++);
            (line++);
            (column = 1);
        } else if (isSpaceChar(c)) {
            (i++);
            (column++);
        } else if (c == '#') {
            while ((i < ((int)strlen(source))) && (__btrc_charAt(source, i) != '\n')) {
                (i++);
                (column++);
            }
        } else if (c == '=') {
            KeymapTokens_push(tokens, "equals", "=", line, column);
            (i++);
            (column++);
        } else if (c == '+') {
            KeymapTokens_push(tokens, "plus", "+", line, column);
            (i++);
            (column++);
        } else if (c == '-') {
            if (((i + 1) < ((int)strlen(source))) && (__btrc_charAt(source, (i + 1)) == '>')) {
                KeymapTokens_push(tokens, "arrow", "->", line, column);
                (i = (i + 2));
                (column = (column + 2));
            } else {
                KeymapTokens_push(tokens, "ident", "-", line, column);
                (i++);
                (column++);
            }
        } else if (c == '$') {
            int startColumn = column;
            if (((i + 1) < ((int)strlen(source))) && (__btrc_charAt(source, (i + 1)) == '{')) {
                (i = (i + 2));
                (column = (column + 2));
                int start = i;
                while (((i < ((int)strlen(source))) && (__btrc_charAt(source, i) != '}')) && (__btrc_charAt(source, i) != '\n')) {
                    (i++);
                    (column++);
                }
                if ((i >= ((int)strlen(source))) || (__btrc_charAt(source, i) == '\n')) {
                    KeymapErrors_error(errors, line, startColumn, "unterminated action reference");
                } else {
                    KeymapTokens_push(tokens, "ref", __btrc_str_track(__btrc_substring(source, start, (i - start))), line, startColumn);
                    (i++);
                    (column++);
                }
            } else {
                KeymapErrors_error(errors, line, column, "expected ${action.id}");
                (i++);
                (column++);
            }
        } else if (c == '"') {
            int startColumn = column;
            (i++);
            (column++);
            int start = i;
            while ((i < ((int)strlen(source))) && (__btrc_charAt(source, i) != '"')) {
                if (__btrc_charAt(source, i) == '\n') {
                    KeymapErrors_error(errors, line, startColumn, "unterminated string");
                    break;
                }
                (i++);
                (column++);
            }
            if ((i < ((int)strlen(source))) && (__btrc_charAt(source, i) == '"')) {
                KeymapTokens_push(tokens, "string", __btrc_str_track(__btrc_substring(source, start, (i - start))), line, startColumn);
                (i++);
                (column++);
            }
        } else if (isIdentChar(c)) {
            int start = i;
            int startColumn = column;
            while ((i < ((int)strlen(source))) && isIdentChar(__btrc_charAt(source, i))) {
                (i++);
                (column++);
            }
            KeymapTokens_push(tokens, "ident", __btrc_str_track(__btrc_substring(source, start, (i - start))), line, startColumn);
        } else {
            int __fstr_293_len = snprintf(NULL, 0, "unexpected character '%c'", c);
            char* __fstr_293_buf = __btrc_str_track(((char*)malloc((__fstr_293_len + 1))));
            snprintf(__fstr_293_buf, (__fstr_293_len + 1), "unexpected character '%c'", c);
            KeymapErrors_error(errors, line, column, __fstr_293_buf);
            (i++);
            (column++);
        }
    }
    KeymapTokens_push(tokens, "eof", "", line, column);
    return tokens;
    if (tokens != NULL) {
        if ((--tokens->__rc) <= 0) {
            KeymapTokens_destroy(tokens);
        }
    }
}

bool keymapIrHasAction(KeymapIr* ir, char* id) {
    int __n_295 = btrc_Vector_string_iterLen(ir->actionIds);
    for (int __i_294 = 0; (__i_294 < __n_295); (__i_294++)) {
        char* action = btrc_Vector_string_iterGet(ir->actionIds, __i_294);
        if (strcmp(action, id) == 0) {
            return true;
        }
    }
    return false;
}

btrc_Vector_string* requiredKeymapActions(void) {
    btrc_Vector_string* __list_296 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_296, "ui.open");
    btrc_Vector_string_push(__list_296, "ui.pause");
    btrc_Vector_string_push(__list_296, "ui.screenshot");
    btrc_Vector_string_push(__list_296, "ui.fullscreen");
    btrc_Vector_string_push(__list_296, "ui.menu");
    btrc_Vector_string_push(__list_296, "app.quit");
    btrc_Vector_string_push(__list_296, "state.prev");
    btrc_Vector_string_push(__list_296, "state.next");
    btrc_Vector_string_push(__list_296, "state.load");
    btrc_Vector_string_push(__list_296, "state.save");
    btrc_Vector_string_push(__list_296, "speed.rewind");
    btrc_Vector_string_push(__list_296, "speed.fast");
    btrc_Vector_string_push(__list_296, "screen.swap");
    btrc_Vector_string_push(__list_296, "ui.escape");
    return __list_296;
}

bool allowedModifier(char* modifier) {
    return ((((strcmp(modifier, "Ctrl") == 0) || (strcmp(modifier, "Alt") == 0)) || (strcmp(modifier, "Shift") == 0)) || (strcmp(modifier, "Meta") == 0));
}

void validateKeymapIr(KeymapIr* ir, KeymapErrors* errors) {
    if (ir->actionIds->len == 0) {
        KeymapErrors_error(errors, 0, 0, "keymap has no actions");
    }
    if (ir->bindingCombos->len == 0) {
        KeymapErrors_error(errors, 0, 0, "keymap has no controller bindings");
    }
    int __n_298 = btrc_Vector_string_iterLen(requiredKeymapActions());
    for (int __i_297 = 0; (__i_297 < __n_298); (__i_297++)) {
        char* required = btrc_Vector_string_iterGet(requiredKeymapActions(), __i_297);
        if (!keymapIrHasAction(ir, required)) {
            int __fstr_300_len = snprintf(NULL, 0, "missing required action '%s'", required);
            char* __fstr_300_buf = __btrc_str_track(((char*)malloc((__fstr_300_len + 1))));
            snprintf(__fstr_300_buf, (__fstr_300_len + 1), "missing required action '%s'", required);
            KeymapErrors_error(errors, 0, 0, __fstr_300_buf);
        }
    }
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        char* id = btrc_Vector_string_get(ir->actionIds, i);
        char* command = btrc_Vector_string_get(ir->actionCommands, i);
        if (((int)strlen(command)) == 0) {
            int __fstr_302_len = snprintf(NULL, 0, "action '%s' has no key", id);
            char* __fstr_302_buf = __btrc_str_track(((char*)malloc((__fstr_302_len + 1))));
            snprintf(__fstr_302_buf, (__fstr_302_len + 1), "action '%s' has no key", id);
            KeymapErrors_error(errors, 0, 0, __fstr_302_buf);
        }
        if (allowedModifier(command)) {
            int __fstr_304_len = snprintf(NULL, 0, "action '%s' has no key after modifier '%s'", id, command);
            char* __fstr_304_buf = __btrc_str_track(((char*)malloc((__fstr_304_len + 1))));
            snprintf(__fstr_304_buf, (__fstr_304_len + 1), "action '%s' has no key after modifier '%s'", id, command);
            KeymapErrors_error(errors, 0, 0, __fstr_304_buf);
        }
        char* modifiers = keymapCommandModifierPart(command);
        if (((int)strlen(modifiers)) > 0) {
            btrc_Vector_string* parts = Strings_split(modifiers, "+");
            int __n_306 = btrc_Vector_string_iterLen(parts);
            for (int __i_305 = 0; (__i_305 < __n_306); (__i_305++)) {
                char* modifier = btrc_Vector_string_iterGet(parts, __i_305);
                if (!allowedModifier(modifier)) {
                    int __fstr_308_len = snprintf(NULL, 0, "action '%s' uses unsupported modifier '%s'", id, modifier);
                    char* __fstr_308_buf = __btrc_str_track(((char*)malloc((__fstr_308_len + 1))));
                    snprintf(__fstr_308_buf, (__fstr_308_len + 1), "action '%s' uses unsupported modifier '%s'", id, modifier);
                    KeymapErrors_error(errors, 0, 0, __fstr_308_buf);
                }
            }
        }
    }
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        for (int j = (i + 1); (j < ir->actionIds->len); (j++)) {
            if (strcmp(btrc_Vector_string_get(ir->actionIds, i), btrc_Vector_string_get(ir->actionIds, j)) == 0) {
                int __fstr_310_len = snprintf(NULL, 0, "duplicate action '%s'", btrc_Vector_string_get(ir->actionIds, i));
                char* __fstr_310_buf = __btrc_str_track(((char*)malloc((__fstr_310_len + 1))));
                snprintf(__fstr_310_buf, (__fstr_310_len + 1), "duplicate action '%s'", btrc_Vector_string_get(ir->actionIds, i));
                KeymapErrors_error(errors, 0, 0, __fstr_310_buf);
            }
        }
    }
    for (int i = 0; (i < ir->bindingActions->len); (i++)) {
        if (!keymapIrHasAction(ir, btrc_Vector_string_get(ir->bindingActions, i))) {
            int __fstr_312_len = snprintf(NULL, 0, "binding '%s' references unknown action '%s'", btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingActions, i));
            char* __fstr_312_buf = __btrc_str_track(((char*)malloc((__fstr_312_len + 1))));
            snprintf(__fstr_312_buf, (__fstr_312_len + 1), "binding '%s' references unknown action '%s'", btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingActions, i));
            KeymapErrors_error(errors, 0, 0, __fstr_312_buf);
        }
    }
    for (int i = 0; (i < ir->bindingCombos->len); (i++)) {
        for (int j = (i + 1); (j < ir->bindingCombos->len); (j++)) {
            if (strcmp(btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingCombos, j)) == 0) {
                int __fstr_314_len = snprintf(NULL, 0, "duplicate controller combo '%s'", btrc_Vector_string_get(ir->bindingCombos, i));
                char* __fstr_314_buf = __btrc_str_track(((char*)malloc((__fstr_314_len + 1))));
                snprintf(__fstr_314_buf, (__fstr_314_len + 1), "duplicate controller combo '%s'", btrc_Vector_string_get(ir->bindingCombos, i));
                KeymapErrors_error(errors, 0, 0, __fstr_314_buf);
            }
        }
    }
}

KeymapIr* compileKeymap(char* source, KeymapErrors* errors) {
    KeymapTokens* tokens = lexKeymap(source, errors);
    KeymapParser* parser = KeymapParser_new(tokens, errors);
    KeymapIr* ir = KeymapParser_parse(parser);
    validateKeymapIr(ir, errors);
    if (parser != NULL) {
        if ((--parser->__rc) <= 0) {
            KeymapParser_destroy(parser);
        }
    }
    return ir;
    if (parser != NULL) {
        if ((--parser->__rc) <= 0) {
            KeymapParser_destroy(parser);
        }
    }
}

KeymapIr* defaultKeymapIr(void) {
    KeymapErrors* errors = KeymapErrors_new();
    char* source = defaultKeymapSource();
    KeymapIr* ir = compileKeymap(source, errors);
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return ir;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

char* keymapIrActionJson(KeymapIr* ir, int index) {
    char* id = btrc_Vector_string_get(ir->actionIds, index);
    btrc_Vector_string* __list_315 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_315, jsonStrField("id", id));
    btrc_Vector_string_push(__list_315, jsonStrField("alias", actionAlias(id)));
    btrc_Vector_string_push(__list_315, jsonStrField("label", actionLabel(id)));
    btrc_Vector_string_push(__list_315, jsonStrField("command", btrc_Vector_string_get(ir->actionCommands, index)));
    btrc_Vector_string_push(__list_315, jsonField("systems", jsonStringArray(actionSystems(id))));
    btrc_Vector_string_push(__list_315, jsonStrField("note", actionNote(id)));
    return jsonObject(__list_315);
}

char* keymapIrBindingJson(KeymapIr* ir, int index) {
    char* action = btrc_Vector_string_get(ir->bindingActions, index);
    btrc_Vector_string* __list_316 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_316, jsonStrField("combo", btrc_Vector_string_get(ir->bindingCombos, index)));
    btrc_Vector_string_push(__list_316, jsonStrField("action", action));
    btrc_Vector_string_push(__list_316, jsonStrField("action_alias", actionAlias(action)));
    return jsonObject(__list_316);
}

char* keymapIrJson(KeymapIr* ir) {
    btrc_Vector_string* actions = btrc_Vector_string_new();
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        btrc_Vector_string_push(actions, keymapIrActionJson(ir, i));
    }
    btrc_Vector_string* bindings = btrc_Vector_string_new();
    for (int i = 0; (i < ir->bindingCombos->len); (i++)) {
        btrc_Vector_string_push(bindings, keymapIrBindingJson(ir, i));
    }
    btrc_Vector_string* __list_317 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_317, jsonStrField("source_language", "semu-keymap-v1"));
    btrc_Vector_string_push(__list_317, jsonStrField("source_path", "${paths.keymaps}/steam_deck.skm"));
    btrc_Vector_string* __list_318 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_318, "manifest");
    btrc_Vector_string_push(__list_318, "retroarch");
    btrc_Vector_string_push(__list_318, "dolphin");
    btrc_Vector_string_push(__list_318, "pcsx2");
    btrc_Vector_string_push(__list_318, "steam-input");
    btrc_Vector_string_push(__list_317, jsonField("render_targets", jsonStringArray(__list_318)));
    btrc_Vector_string_push(__list_317, jsonField("actions", jsonArray(actions)));
    btrc_Vector_string_push(__list_317, jsonField("bindings", jsonArray(bindings)));
    return jsonObject(__list_317);
}

char* irActionCommand(KeymapIr* ir, char* id) {
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        if (strcmp(btrc_Vector_string_get(ir->actionIds, i), id) == 0) {
            return btrc_Vector_string_get(ir->actionCommands, i);
        }
    }
    return "";
}

char* irBindingCombo(KeymapIr* ir, char* id) {
    for (int i = 0; (i < ir->bindingActions->len); (i++)) {
        if (strcmp(btrc_Vector_string_get(ir->bindingActions, i), id) == 0) {
            return btrc_Vector_string_get(ir->bindingCombos, i);
        }
    }
    return "";
}

char* keymapsJson(void) {
    btrc_Vector_string* __list_319 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_319, jsonField("steam_deck", keymapIrJson(defaultKeymapIr())));
    return jsonObject(__list_319);
}

char* hotkeyFromKeymapIr(KeymapIr* ir, char* actionId) {
    return hotkeySpec(actionAlias(actionId), actionLabel(actionId), irBindingCombo(ir, actionId), irActionCommand(ir, actionId), actionSystems(actionId), actionNote(actionId));
}

char* hotkeySpec(char* id, char* label, char* combo, char* command, btrc_Vector_string* systems, char* note) {
    btrc_Vector_string* __list_320 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_320, jsonStrField("id", id));
    btrc_Vector_string_push(__list_320, jsonStrField("label", label));
    btrc_Vector_string_push(__list_320, jsonStrField("combo", combo));
    btrc_Vector_string_push(__list_320, jsonStrField("command", command));
    btrc_Vector_string_push(__list_320, jsonField("systems", jsonStringArray(systems)));
    btrc_Vector_string_push(__list_320, jsonStrField("note", note));
    return jsonObject(__list_320);
}

char* steamDeckHotkeys(void) {
    KeymapIr* ir = defaultKeymapIr();
    btrc_Vector_string* __list_321 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "ui.pause"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "ui.screenshot"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "ui.fullscreen"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "ui.menu"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "app.quit"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "state.prev"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "state.next"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "state.load"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "state.save"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "speed.rewind"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "speed.fast"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "screen.swap"));
    btrc_Vector_string_push(__list_321, hotkeyFromKeymapIr(ir, "ui.escape"));
    return jsonArray(__list_321);
}

char* retroarchKeyName(char* key) {
    if (strcmp(key, "+") == 0) {
        return "add";
    }
    if (strcmp(key, "-") == 0) {
        return "subtract";
    }
    return __btrc_str_track(__btrc_toLower(key));
}

char* keymapCommandKey(KeymapIr* ir, char* actionId) {
    return retroarchKeyName(keymapCommandKeyPart(irActionCommand(ir, actionId)));
}

char* renderRetroArchKeymap(KeymapIr* ir) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("input_exit_emulator = \"escape\"\n", "input_quit_gamepad_combo = \"4\"\n")), "input_load_state = ")), jsonQ(keymapCommandKey(ir, "state.load")))), "\n")), "input_menu_toggle = ")), jsonQ(keymapCommandKey(ir, "ui.menu")))), "\n")), "input_save_state = ")), jsonQ(keymapCommandKey(ir, "state.save")))), "\n")), "input_screenshot = ")), jsonQ(keymapCommandKey(ir, "ui.screenshot")))), "\n")), "input_state_slot_decrease = ")), jsonQ(keymapCommandKey(ir, "state.prev")))), "\n")), "input_state_slot_increase = ")), jsonQ(keymapCommandKey(ir, "state.next")))), "\n")), "input_toggle_fast_forward = ")), jsonQ(keymapCommandKey(ir, "speed.fast")))), "\n")), "input_toggle_fullscreen = ")), jsonQ(keymapCommandKey(ir, "ui.fullscreen")))), "\n"));
}

char* dolphinChord(KeymapIr* ir, char* actionId) {
    char* command = irActionCommand(ir, actionId);
    if (strcmp(command, "Esc") == 0) {
        return "Escape";
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("@(", command)), ")"));
}

char* renderDolphinKeymap(KeymapIr* ir) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("General/Open = ", dolphinChord(ir, "ui.open"))), "\n")), "General/Toggle Pause = ")), dolphinChord(ir, "ui.pause"))), "\n")), "General/Stop = ")), dolphinChord(ir, "app.quit"))), "\n")), "General/Toggle Fullscreen = ")), dolphinChord(ir, "ui.fullscreen"))), "\n")), "General/Take Screenshot = ")), dolphinChord(ir, "ui.screenshot"))), "\n")), "Load State/Load State Slot 1 = ")), dolphinChord(ir, "state.load"))), "\n")), "Save State/Save State Slot 1 = ")), dolphinChord(ir, "state.save"))), "\n"));
}

char* pcsx2KeyName(char* key) {
    if (strcmp(key, "+") == 0) {
        return "Plus";
    }
    if (strcmp(key, "-") == 0) {
        return "Minus";
    }
    return key;
}

char* pcsx2Chord(KeymapIr* ir, char* actionId) {
    char* command = irActionCommand(ir, actionId);
    btrc_Vector_string* parts = btrc_Vector_string_new();
    char* modifiers = keymapCommandModifierPart(command);
    if (((int)strlen(modifiers)) > 0) {
        btrc_Vector_string* modifierParts = Strings_split(modifiers, "+");
        int __n_323 = btrc_Vector_string_iterLen(modifierParts);
        for (int __i_322 = 0; (__i_322 < __n_323); (__i_322++)) {
            char* modifier = btrc_Vector_string_iterGet(modifierParts, __i_322);
            if (strcmp(modifier, "Ctrl") == 0) {
                btrc_Vector_string_push(parts, "Keyboard/Control");
            } else {
                btrc_Vector_string_push(parts, __btrc_str_track(__btrc_strcat("Keyboard/", modifier)));
            }
        }
    }
    btrc_Vector_string_push(parts, __btrc_str_track(__btrc_strcat("Keyboard/", pcsx2KeyName(keymapCommandKeyPart(command)))));
    return btrc_Vector_string_join(parts, " & ");
}

char* renderPcsx2Keymap(KeymapIr* ir) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("OpenPauseMenu = ", pcsx2Chord(ir, "ui.menu"))), "\n")), "TogglePause = ")), pcsx2Chord(ir, "ui.pause"))), "\n")), "ToggleFullscreen = ")), pcsx2Chord(ir, "ui.fullscreen"))), "\n")), "PreviousSaveStateSlot = ")), pcsx2Chord(ir, "state.prev"))), "\n")), "NextSaveStateSlot = ")), pcsx2Chord(ir, "state.next"))), "\n")), "SaveStateToSlot = ")), pcsx2Chord(ir, "state.save"))), "\n")), "LoadStateFromSlot = ")), pcsx2Chord(ir, "state.load"))), "\n")), "Screenshot = ")), pcsx2Chord(ir, "ui.screenshot"))), "\n")), "ToggleTurbo = ")), pcsx2Chord(ir, "speed.fast"))), "\n")), "ToggleSlowMotion = ")), pcsx2Chord(ir, "speed.rewind"))), "\n"));
}

char* renderKeymap(KeymapIr* ir, char* target) {
    if (strcmp(target, "manifest") == 0) {
        return __btrc_str_track(__btrc_strcat(keymapIrJson(ir), "\n"));
    }
    if (strcmp(target, "retroarch") == 0) {
        return renderRetroArchKeymap(ir);
    }
    if (strcmp(target, "dolphin") == 0) {
        return renderDolphinKeymap(ir);
    }
    if (strcmp(target, "pcsx2") == 0) {
        return renderPcsx2Keymap(ir);
    }
    if (strcmp(target, "steam-input") == 0) {
        return steamInputTemplateVdf("Semu: Steam Deck - Neptune FULL", true, ir, ".");
    }
    int __fstr_324_len = snprintf(NULL, 0, "unknown keymap target: %s\n", target);
    char* __fstr_324_buf = __btrc_str_track(((char*)malloc((__fstr_324_len + 1))));
    snprintf(__fstr_324_buf, (__fstr_324_len + 1), "unknown keymap target: %s\n", target);
    return __fstr_324_buf;
}

bool isKeymapTarget(char* target) {
    return (((((strcmp(target, "manifest") == 0) || (strcmp(target, "retroarch") == 0)) || (strcmp(target, "dolphin") == 0)) || (strcmp(target, "pcsx2") == 0)) || (strcmp(target, "steam-input") == 0));
}

void KeymapErrors_init(KeymapErrors* self) {
    self->__rc = 1;
    if (self->levels != NULL) {
        if ((--self->levels->__rc) <= 0) {
            btrc_Vector_string_free(self->levels);
        }
    }
    btrc_Vector_string* __list_326 = btrc_Vector_string_new();
    (self->levels = __list_326);
    btrc_Vector_string* __list_325 = btrc_Vector_string_new();
    (__list_325->__rc++);
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    btrc_Vector_int* __list_328 = btrc_Vector_int_new();
    (self->lines = __list_328);
    btrc_Vector_int* __list_327 = btrc_Vector_int_new();
    (__list_327->__rc++);
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    btrc_Vector_int* __list_330 = btrc_Vector_int_new();
    (self->columns = __list_330);
    btrc_Vector_int* __list_329 = btrc_Vector_int_new();
    (__list_329->__rc++);
    if (self->messages != NULL) {
        if ((--self->messages->__rc) <= 0) {
            btrc_Vector_string_free(self->messages);
        }
    }
    btrc_Vector_string* __list_332 = btrc_Vector_string_new();
    (self->messages = __list_332);
    btrc_Vector_string* __list_331 = btrc_Vector_string_new();
    (__list_331->__rc++);
}

KeymapErrors* KeymapErrors_new(void) {
    KeymapErrors* self = ((KeymapErrors*)malloc(sizeof(KeymapErrors)));
    memset(self, 0, sizeof(KeymapErrors));
    KeymapErrors_init(self);
    return self;
}

void KeymapErrors_destroy(KeymapErrors* self) {
    if (self->levels != NULL) {
        if ((--self->levels->__rc) <= 0) {
            btrc_Vector_string_free(self->levels);
        }
    }
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    if (self->messages != NULL) {
        if ((--self->messages->__rc) <= 0) {
            btrc_Vector_string_free(self->messages);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void KeymapErrors_error(KeymapErrors* self, int line, int column, char* message) {
    btrc_Vector_string_push(self->levels, "error");
    btrc_Vector_int_push(self->lines, line);
    btrc_Vector_int_push(self->columns, column);
    btrc_Vector_string_push(self->messages, message);
}

int KeymapErrors_count(KeymapErrors* self) {
    return self->messages->len;
}

void KeymapTokens_init(KeymapTokens* self) {
    self->__rc = 1;
    if (self->kinds != NULL) {
        if ((--self->kinds->__rc) <= 0) {
            btrc_Vector_string_free(self->kinds);
        }
    }
    btrc_Vector_string* __list_334 = btrc_Vector_string_new();
    (self->kinds = __list_334);
    btrc_Vector_string* __list_333 = btrc_Vector_string_new();
    (__list_333->__rc++);
    if (self->texts != NULL) {
        if ((--self->texts->__rc) <= 0) {
            btrc_Vector_string_free(self->texts);
        }
    }
    btrc_Vector_string* __list_336 = btrc_Vector_string_new();
    (self->texts = __list_336);
    btrc_Vector_string* __list_335 = btrc_Vector_string_new();
    (__list_335->__rc++);
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    btrc_Vector_int* __list_338 = btrc_Vector_int_new();
    (self->lines = __list_338);
    btrc_Vector_int* __list_337 = btrc_Vector_int_new();
    (__list_337->__rc++);
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    btrc_Vector_int* __list_340 = btrc_Vector_int_new();
    (self->columns = __list_340);
    btrc_Vector_int* __list_339 = btrc_Vector_int_new();
    (__list_339->__rc++);
}

KeymapTokens* KeymapTokens_new(void) {
    KeymapTokens* self = ((KeymapTokens*)malloc(sizeof(KeymapTokens)));
    memset(self, 0, sizeof(KeymapTokens));
    KeymapTokens_init(self);
    return self;
}

void KeymapTokens_destroy(KeymapTokens* self) {
    if (self->kinds != NULL) {
        if ((--self->kinds->__rc) <= 0) {
            btrc_Vector_string_free(self->kinds);
        }
    }
    if (self->texts != NULL) {
        if ((--self->texts->__rc) <= 0) {
            btrc_Vector_string_free(self->texts);
        }
    }
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void KeymapTokens_push(KeymapTokens* self, char* kind, char* text, int line, int column) {
    btrc_Vector_string_push(self->kinds, kind);
    btrc_Vector_string_push(self->texts, text);
    btrc_Vector_int_push(self->lines, line);
    btrc_Vector_int_push(self->columns, column);
}

int KeymapTokens_count(KeymapTokens* self) {
    return self->kinds->len;
}

void KeymapIr_init(KeymapIr* self) {
    self->__rc = 1;
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    btrc_Vector_string* __list_342 = btrc_Vector_string_new();
    (self->actionIds = __list_342);
    btrc_Vector_string* __list_341 = btrc_Vector_string_new();
    (__list_341->__rc++);
    if (self->actionCommands != NULL) {
        if ((--self->actionCommands->__rc) <= 0) {
            btrc_Vector_string_free(self->actionCommands);
        }
    }
    btrc_Vector_string* __list_344 = btrc_Vector_string_new();
    (self->actionCommands = __list_344);
    btrc_Vector_string* __list_343 = btrc_Vector_string_new();
    (__list_343->__rc++);
    if (self->bindingCombos != NULL) {
        if ((--self->bindingCombos->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingCombos);
        }
    }
    btrc_Vector_string* __list_346 = btrc_Vector_string_new();
    (self->bindingCombos = __list_346);
    btrc_Vector_string* __list_345 = btrc_Vector_string_new();
    (__list_345->__rc++);
    if (self->bindingActions != NULL) {
        if ((--self->bindingActions->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingActions);
        }
    }
    btrc_Vector_string* __list_348 = btrc_Vector_string_new();
    (self->bindingActions = __list_348);
    btrc_Vector_string* __list_347 = btrc_Vector_string_new();
    (__list_347->__rc++);
}

KeymapIr* KeymapIr_new(void) {
    KeymapIr* self = ((KeymapIr*)malloc(sizeof(KeymapIr)));
    memset(self, 0, sizeof(KeymapIr));
    KeymapIr_init(self);
    return self;
}

void KeymapIr_destroy(KeymapIr* self) {
    if (self->actionIds != NULL) {
        if ((--self->actionIds->__rc) <= 0) {
            btrc_Vector_string_free(self->actionIds);
        }
    }
    if (self->actionCommands != NULL) {
        if ((--self->actionCommands->__rc) <= 0) {
            btrc_Vector_string_free(self->actionCommands);
        }
    }
    if (self->bindingCombos != NULL) {
        if ((--self->bindingCombos->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingCombos);
        }
    }
    if (self->bindingActions != NULL) {
        if ((--self->bindingActions->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingActions);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void KeymapIr_addAction(KeymapIr* self, char* id, char* command) {
    btrc_Vector_string_push(self->actionIds, id);
    btrc_Vector_string_push(self->actionCommands, command);
}

void KeymapIr_addBinding(KeymapIr* self, char* combo, char* action) {
    btrc_Vector_string_push(self->bindingCombos, combo);
    btrc_Vector_string_push(self->bindingActions, action);
}

void KeymapParser_init(KeymapParser* self, KeymapTokens* tokens, KeymapErrors* errors) {
    self->__rc = 1;
    if (self->tokens != NULL) {
        if ((--self->tokens->__rc) <= 0) {
            KeymapTokens_destroy(self->tokens);
        }
    }
    (self->tokens = tokens);
    (tokens->__rc++);
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            KeymapErrors_destroy(self->errors);
        }
    }
    (self->errors = errors);
    (errors->__rc++);
    (self->index = 0);
}

KeymapParser* KeymapParser_new(KeymapTokens* tokens, KeymapErrors* errors) {
    KeymapParser* self = ((KeymapParser*)malloc(sizeof(KeymapParser)));
    memset(self, 0, sizeof(KeymapParser));
    KeymapParser_init(self, tokens, errors);
    return self;
}

void KeymapParser_destroy(KeymapParser* self) {
    if (self->tokens != NULL) {
        if ((--self->tokens->__rc) <= 0) {
            KeymapTokens_destroy(self->tokens);
        }
    }
    if (self->errors != NULL) {
        if ((--self->errors->__rc) <= 0) {
            KeymapErrors_destroy(self->errors);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* KeymapParser_kind(KeymapParser* self) {
    return btrc_Vector_string_get(self->tokens->kinds, self->index);
}

char* KeymapParser_text(KeymapParser* self) {
    return btrc_Vector_string_get(self->tokens->texts, self->index);
}

void KeymapParser_errorHere(KeymapParser* self, char* message) {
    KeymapErrors_error(self->errors, btrc_Vector_int_get(self->tokens->lines, self->index), btrc_Vector_int_get(self->tokens->columns, self->index), message);
}

bool KeymapParser_accept(KeymapParser* self, char* kind) {
    if ((self->index < KeymapTokens_count(self->tokens)) && (strcmp(KeymapParser_kind(self), kind) == 0)) {
        (self->index++);
        return true;
    }
    return false;
}

void KeymapParser_skipLine(KeymapParser* self) {
    while (((self->index < KeymapTokens_count(self->tokens)) && (!(strcmp(KeymapParser_kind(self), "newline") == 0))) && (!(strcmp(KeymapParser_kind(self), "eof") == 0))) {
        (self->index++);
    }
    if ((self->index < KeymapTokens_count(self->tokens)) && (strcmp(KeymapParser_kind(self), "newline") == 0)) {
        (self->index++);
    }
}

char* KeymapParser_valueToken(KeymapParser* self, char* expected) {
    if (self->index >= KeymapTokens_count(self->tokens)) {
        int __fstr_350_len = snprintf(NULL, 0, "expected %s", expected);
        char* __fstr_350_buf = __btrc_str_track(((char*)malloc((__fstr_350_len + 1))));
        snprintf(__fstr_350_buf, (__fstr_350_len + 1), "expected %s", expected);
        KeymapErrors_error(self->errors, 0, 0, __fstr_350_buf);
        return "";
    }
    char* kind = KeymapParser_kind(self);
    if (((strcmp(kind, "ident") == 0) || (strcmp(kind, "string") == 0)) || (strcmp(kind, "ref") == 0)) {
        char* result = KeymapParser_text(self);
        (self->index++);
        return result;
    }
    int __fstr_352_len = snprintf(NULL, 0, "expected %s", expected);
    char* __fstr_352_buf = __btrc_str_track(((char*)malloc((__fstr_352_len + 1))));
    snprintf(__fstr_352_buf, (__fstr_352_len + 1), "expected %s", expected);
    KeymapParser_errorHere(self, __fstr_352_buf);
    (self->index++);
    return "";
}

char* KeymapParser_chordCommand(KeymapParser* self) {
    btrc_Vector_string* parts = btrc_Vector_string_new();
    bool done = false;
    while ((!done) && (self->index < KeymapTokens_count(self->tokens))) {
        char* kind = KeymapParser_kind(self);
        if ((strcmp(kind, "newline") == 0) || (strcmp(kind, "eof") == 0)) {
            (done = true);
        } else if (strcmp(kind, "ident") == 0) {
            char* part = KeymapParser_text(self);
            (self->index++);
            if (KeymapParser_accept(self, "plus")) {
                btrc_Vector_string_push(parts, part);
                if ((strcmp(KeymapParser_kind(self), "newline") == 0) || (strcmp(KeymapParser_kind(self), "eof") == 0)) {
                    KeymapParser_errorHere(self, "expected key after '+'");
                }
            } else {
                btrc_Vector_string_push(parts, part);
                (done = true);
            }
        } else if (strcmp(kind, "plus") == 0) {
            btrc_Vector_string_push(parts, "+");
            (self->index++);
            (done = true);
        } else {
            KeymapParser_errorHere(self, "expected key chord token");
            (self->index++);
            (done = true);
        }
    }
    return btrc_Vector_string_join(parts, "+");
}

KeymapIr* KeymapParser_parse(KeymapParser* self) {
    KeymapIr* ir = KeymapIr_new();
    while ((self->index < KeymapTokens_count(self->tokens)) && (!(strcmp(KeymapParser_kind(self), "eof") == 0))) {
        if (KeymapParser_accept(self, "newline")) {
            continue;
        }
        char* head = KeymapParser_valueToken(self, "statement");
        if (strcmp(head, "action") == 0) {
            char* id = KeymapParser_valueToken(self, "action id");
            if (!KeymapParser_accept(self, "equals")) {
                KeymapParser_errorHere(self, "expected '=' after action id");
                KeymapParser_skipLine(self);
            } else {
                KeymapIr_addAction(ir, id, KeymapParser_chordCommand(self));
                KeymapParser_skipLine(self);
            }
        } else if (strcmp(head, "bind") == 0) {
            btrc_Vector_string* comboParts = btrc_Vector_string_new();
            while ((((self->index < KeymapTokens_count(self->tokens)) && (!(strcmp(KeymapParser_kind(self), "arrow") == 0))) && (!(strcmp(KeymapParser_kind(self), "newline") == 0))) && (!(strcmp(KeymapParser_kind(self), "eof") == 0))) {
                char* kind = KeymapParser_kind(self);
                if ((strcmp(kind, "ident") == 0) || (strcmp(kind, "plus") == 0)) {
                    btrc_Vector_string_push(comboParts, KeymapParser_text(self));
                } else {
                    KeymapParser_errorHere(self, "expected controller combo token");
                }
                (self->index++);
            }
            if (!KeymapParser_accept(self, "arrow")) {
                KeymapParser_errorHere(self, "expected '->' in binding");
                KeymapParser_skipLine(self);
            } else {
                KeymapIr_addBinding(ir, btrc_Vector_string_join(comboParts, " "), KeymapParser_valueToken(self, "action reference"));
                KeymapParser_skipLine(self);
            }
        } else if (((int)strlen(head)) > 0) {
            int __fstr_354_len = snprintf(NULL, 0, "unknown statement '%s'", head);
            char* __fstr_354_buf = __btrc_str_track(((char*)malloc((__fstr_354_len + 1))));
            snprintf(__fstr_354_buf, (__fstr_354_len + 1), "unknown statement '%s'", head);
            KeymapParser_errorHere(self, __fstr_354_buf);
            KeymapParser_skipLine(self);
        }
    }
    return ir;
    if (ir != NULL) {
        if ((--ir->__rc) <= 0) {
            KeymapIr_destroy(ir);
        }
    }
}

char* joinPath(char* left, char* right) {
    return PathTools_join(left, right);
}

char* contentRoot(char* project) {
    return joinPath(project, "ES-DE/ES-DE");
}

char* romsRoot(char* project) {
    return joinPath(contentRoot(project), "ROMs");
}

char* syncRoot(char* project) {
    return joinPath(project, "sync");
}

char* syncConfigPath(char* project) {
    return joinPath(syncRoot(project), "sync.json");
}

char* syncthingHome(char* project) {
    return joinPath(syncRoot(project), "syncthing");
}

char* syncthingConfigDir(char* project) {
    return joinPath(syncthingHome(project), "config");
}

char* syncthingDataDir(char* project) {
    return joinPath(syncthingHome(project), "data");
}

char* syncScriptsDir(char* project) {
    return joinPath(syncRoot(project), "bin");
}

char* syncLogDir(char* project) {
    return joinPath(syncRoot(project), "logs");
}

char* biosRoot(char* project) {
    return joinPath(contentRoot(project), "bios");
}

char* customSystemsRoot(char* project) {
    return joinPath(project, "ES-DE/custom_systems");
}

char* esDeProfileCustomSystemsRoot(char* project) {
    return joinPath(project, "emulators/es-de/custom_systems");
}

char* emulatorProfilesRoot(char* project) {
    return joinPath(project, "emulators/profiles");
}

char* emulatorProfilePath(char* project, char* relative) {
    return joinPath(emulatorProfilesRoot(project), relative);
}

char* linuxPackagingRoot(char* project) {
    return joinPath(project, "packaging/linux");
}

char* linuxLauncherBin(char* project) {
    return joinPath(linuxPackagingRoot(project), "bin");
}

char* keymapsRoot(char* project) {
    return joinPath(project, "input/keymaps");
}

char* keymapSourcePath(char* project) {
    return joinPath(keymapsRoot(project), "steam_deck.skm");
}

char* homeDir(void) {
    return Environment_get("SEMU_HOME", Environment_get("HOME", "."));
}

char* systemdUserDir(void) {
    return joinPath(homeDir(), ".config/systemd/user");
}

char* applicationsDir(void) {
    return joinPath(homeDir(), ".local/share/applications");
}

char* expandProjectTemplate(char* project, char* value) {
    char* result = Strings_replace(value, "${project}", project);
    (result = Strings_replace(result, "${paths.project_roms}", romsRoot(project)));
    (result = Strings_replace(result, "${paths.project_bios}", biosRoot(project)));
    (result = Strings_replace(result, "${paths.project_saves}", joinPath(contentRoot(project), "saves")));
    (result = Strings_replace(result, "${paths.project_states}", joinPath(contentRoot(project), "states")));
    (result = Strings_replace(result, "${paths.project_screenshots}", joinPath(contentRoot(project), "screenshots")));
    (result = Strings_replace(result, "${paths.project_gamelists}", joinPath(contentRoot(project), "gamelists")));
    return result;
}

JsonObject* syncConfig(char* project) {
    char* path = syncConfigPath(project);
    if (!FileSystem_exists(path)) {
        return JsonObject_parse("{}");
    }
    return JsonObject_readFile(path);
}

char* syncString(char* project, char* key, char* fallback) {
    JsonObject* config = syncConfig(project);
    return expandProjectTemplate(project, JsonObject_getString(config, key, fallback));
}

bool syncBool(char* project, char* key, bool fallback) {
    JsonObject* config = syncConfig(project);
    return JsonObject_getBool(config, key, fallback);
}

int syncInt(char* project, char* key, int fallback) {
    JsonObject* config = syncConfig(project);
    return JsonObject_getInt(config, key, fallback);
}

char* configuredRomsRoot(char* project) {
    return syncString(project, "roms_dir", "${paths.project_roms}");
}

char* normalizeRomsRoot(char* romsDir) {
    if (((int)strlen(romsDir)) == 0) {
        return "";
    }
    btrc_Vector_string* __list_355 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_355, joinPath(romsDir, "Emulation/ES-DE/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_355, joinPath(romsDir, "ES-DE/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_355, joinPath(romsDir, "Emulation/ROMs"));
    btrc_Vector_string_push(__list_355, joinPath(romsDir, "ROMs"));
    btrc_Vector_string* candidates = __list_355;
    int __n_357 = btrc_Vector_string_iterLen(candidates);
    for (int __i_356 = 0; (__i_356 < __n_357); (__i_356++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_356);
        if (FileSystem_isDir(candidate)) {
            return candidate;
        }
    }
    return romsDir;
}

char* configuredEmulationRoot(char* project) {
    char* configured = Environment_get("SEMU_EMULATION_ROOT", syncString(project, "emulation_root", ""));
    if (((int)strlen(configured)) > 0) {
        return configured;
    }
    char* roms = configuredRomsRoot(project);
    if (__btrc_endsWith(roms, "/ES-DE/ES-DE/ROMs")) {
        return PathTools_dirname(PathTools_dirname(PathTools_dirname(roms)));
    }
    if (FileSystem_isDir("/run/media/deck/SD/Emulation")) {
        return "/run/media/deck/SD/Emulation";
    }
    if (FileSystem_isDir("/mnt/SD/Emulation")) {
        return "/mnt/SD/Emulation";
    }
    return "";
}

btrc_Vector_string* declaredSystemIds(void) {
    SystemCatalog* catalog = systemCatalog();
    return catalog->ids;
}

btrc_Vector_string* declaredRomDirs(void) {
    SystemCatalog* catalog = systemCatalog();
    return catalog->romDirs;
}

btrc_Vector_string* controllerProfileFiles(void) {
    btrc_Vector_string* __list_358 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_358, "emulators/profiles/RetroArch/retroarch.cfg.linux-backup");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Dolphin/config/Profiles/GCPad/Steam Deck.ini");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Dolphin/config/Profiles/Hotkeys/Steam Deck.ini");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Dolphin/config/Profiles/Wiimote/Wiimote (SD).ini");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Dolphin/config/Profiles/Wiimote/Wiimote + Classic Controller (SD).ini");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Cemu/config/controllerProfiles/SteamInput-P1.xml");
    btrc_Vector_string_push(__list_358, "emulators/profiles/PCSX2/config/inputprofiles/Steam Deck.ini");
    btrc_Vector_string_push(__list_358, "emulators/profiles/Ryujinx/config/profiles/controller/Steam Virtual Controller.json");
    return __list_358;
}

char* esSettingsXmlWithPaths(char* roms, char* media, char* themes) {
    char* startupSystem = Environment_get("SEMU_STARTUP_SYSTEM", "");
    char* startupView = Environment_get("SEMU_STARTUP_VIEW", "system");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<string name=\"ROMDirectory\" value=\"")), roms)), "\" />\n")), "<string name=\"MediaDirectory\" value=\"")), media)), "\" />\n")), "<string name=\"UserThemeDirectory\" value=\"")), themes)), "\" />\n")), "<string name=\"ApplicationUpdaterFrequency\" value=\"never\" />\n")), "<string name=\"SaveGamelistsMode\" value=\"always\" />\n")), "<string name=\"CreatePlaceholderSystemDirectories\" value=\"false\" />\n")), "<string name=\"InputControllerType\" value=\"xbox\" />\n")), "<string name=\"StartupSystem\" value=\"")), xmlEscape(startupSystem))), "\" />\n")), "<string name=\"StartupView\" value=\"")), xmlEscape(startupView))), "\" />\n")), "<string name=\"Theme\" value=\"slate-es-de\" />\n"));
}

char* esSettingsXmlForProject(char* project) {
    return esSettingsXmlWithPaths(configuredRomsRoot(project), joinPath(contentRoot(project), "downloaded_media"), joinPath(contentRoot(project), "themes"));
}

void ensureDir(char* path) {
    FileSystem_mkdirp(path);
}

char* launcherFlatpakId(char* emulator) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if (strcmp(key, "azahar") == 0) {
        return "org.azahar_emu.Azahar";
    }
    if (strcmp(key, "cemu") == 0) {
        return "info.cemu.Cemu";
    }
    if (strcmp(key, "dolphin") == 0) {
        return "org.DolphinEmu.dolphin-emu";
    }
    if (strcmp(key, "flycast") == 0) {
        return "org.flycast.Flycast";
    }
    if (strcmp(key, "gopher64") == 0) {
        return "io.github.gopher64.gopher64";
    }
    if (strcmp(key, "melonds") == 0) {
        return "net.kuribo64.melonDS";
    }
    if (strcmp(key, "pcsx2") == 0) {
        return "net.pcsx2.PCSX2";
    }
    if (strcmp(key, "ppsspp") == 0) {
        return "org.ppsspp.PPSSPP";
    }
    if (strcmp(key, "ryujinx") == 0) {
        return "org.ryujinx.Ryujinx";
    }
    return "";
}

bool launcherUsesX11(char* emulator) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    char* forceX11 = Environment_get("SEMU_FLATPAK_X11", Environment_get("SEMU_FLATPAK_X11", "0"));
    return (((strcmp(key, "azahar") == 0) || (strcmp(key, "dolphin") == 0)) || (strcmp(forceX11, "1") == 0));
}

btrc_Vector_string* launcherPresetArgs(char* emulator) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if (strcmp(key, "dolphin") == 0) {
        btrc_Vector_string* __list_359 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_359, "--config");
        btrc_Vector_string_push(__list_359, "Dolphin.Display.Fullscreen=True");
        btrc_Vector_string_push(__list_359, "-b");
        btrc_Vector_string_push(__list_359, "-e");
        return __list_359;
    }
    if (strcmp(key, "retroarch") == 0) {
        btrc_Vector_string* __list_360 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_360, "-f");
        return __list_360;
    }
    if (strcmp(key, "ppsspp") == 0) {
        btrc_Vector_string* __list_361 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_361, "--fullscreen");
        return __list_361;
    }
    if (strcmp(key, "flycast") == 0) {
        btrc_Vector_string* __list_362 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_362, "-config");
        btrc_Vector_string_push(__list_362, "window:fullscreen=yes");
        return __list_362;
    }
    if (strcmp(key, "gopher64") == 0) {
        btrc_Vector_string* __list_363 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_363, "--fullscreen");
        return __list_363;
    }
    if (strcmp(key, "melonds") == 0) {
        btrc_Vector_string* __list_364 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_364, "--fullscreen");
        return __list_364;
    }
    if (strcmp(key, "pcsx2") == 0) {
        btrc_Vector_string* __list_365 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_365, "-batch");
        btrc_Vector_string_push(__list_365, "-fullscreen");
        return __list_365;
    }
    if (strcmp(key, "cemu") == 0) {
        btrc_Vector_string* __list_366 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_366, "-f");
        btrc_Vector_string_push(__list_366, "-g");
        return __list_366;
    }
    if (strcmp(key, "azahar") == 0) {
        btrc_Vector_string* __list_367 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_367, "-f");
        return __list_367;
    }
    if (strcmp(key, "ryujinx") == 0) {
        btrc_Vector_string* __list_368 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_368, "--fullscreen");
        return __list_368;
    }
    btrc_Vector_string* empty = btrc_Vector_string_new();
    return empty;
}

bool launcherIsUtilityArg(char* arg) {
    return (((((((strcmp(arg, "--help") == 0) || (strcmp(arg, "-h") == 0)) || (strcmp(arg, "--version") == 0)) || (strcmp(arg, "-v") == 0)) || (strcmp(arg, "--features") == 0)) || (strcmp(arg, "--license") == 0)) || (strcmp(arg, "--author") == 0));
}

bool launcherIsUtilityInvocation(btrc_Vector_string* emulatorArgs) {
    int __n_370 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_369 = 0; (__i_369 < __n_370); (__i_369++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_369);
        if (launcherIsUtilityArg(arg)) {
            return true;
        }
    }
    return false;
}

bool launcherHasFlag(btrc_Vector_string* emulatorArgs, char* flag) {
    int __n_372 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_371 = 0; (__i_371 < __n_372); (__i_371++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_371);
        if (strcmp(arg, flag) == 0) {
            return true;
        }
        if (__btrc_startsWith(arg, __btrc_str_track(__btrc_strcat(flag, "=")))) {
            return true;
        }
    }
    return false;
}

bool launcherDefaultAlreadyPresent(char* emulator, char* arg, btrc_Vector_string* emulatorArgs) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if ((strcmp(arg, "-f") == 0) || (strcmp(arg, "--fullscreen") == 0)) {
        return (launcherHasFlag(emulatorArgs, "-f") || launcherHasFlag(emulatorArgs, "--fullscreen"));
    }
    if (strcmp(arg, "-batch") == 0) {
        return (launcherHasFlag(emulatorArgs, "-batch") || launcherHasFlag(emulatorArgs, "--batch"));
    }
    if (strcmp(arg, "-fullscreen") == 0) {
        return (launcherHasFlag(emulatorArgs, "-fullscreen") || launcherHasFlag(emulatorArgs, "--fullscreen"));
    }
    if ((strcmp(key, "flycast") == 0) && (strcmp(arg, "-config") == 0)) {
        return false;
    }
    if (strcmp(arg, "-config") == 0) {
        return (launcherHasFlag(emulatorArgs, "-config") || launcherHasFlag(emulatorArgs, "--config"));
    }
    if (strcmp(arg, "--config") == 0) {
        return false;
    }
    if ((strcmp(key, "dolphin") == 0) && (strcmp(arg, "-b") == 0)) {
        return (launcherHasFlag(emulatorArgs, "-b") || launcherHasFlag(emulatorArgs, "--batch"));
    }
    if ((strcmp(key, "dolphin") == 0) && (strcmp(arg, "-e") == 0)) {
        return (launcherHasFlag(emulatorArgs, "-e") || launcherHasFlag(emulatorArgs, "--exec"));
    }
    if ((strcmp(key, "cemu") == 0) && (strcmp(arg, "-g") == 0)) {
        return (launcherHasFlag(emulatorArgs, "-g") || launcherHasFlag(emulatorArgs, "--game"));
    }
    return false;
}

bool launcherDefaultTakesValue(char* arg) {
    return ((strcmp(arg, "--config") == 0) || (strcmp(arg, "-config") == 0));
}

btrc_Vector_string* launcherArgsWithDefaults(char* emulator, btrc_Vector_string* emulatorArgs) {
    if (launcherIsUtilityInvocation(emulatorArgs)) {
        return emulatorArgs;
    }
    btrc_Vector_string* out = btrc_Vector_string_new();
    btrc_Vector_string* defaults = launcherPresetArgs(emulator);
    for (int i = 0; (i < defaults->len); (i++)) {
        char* arg = btrc_Vector_string_get(defaults, i);
        if (!launcherDefaultAlreadyPresent(emulator, arg, emulatorArgs)) {
            btrc_Vector_string_push(out, arg);
            if (launcherDefaultTakesValue(arg) && ((i + 1) < defaults->len)) {
                (i++);
                btrc_Vector_string_push(out, btrc_Vector_string_get(defaults, i));
            }
        } else if (launcherDefaultTakesValue(arg)) {
            (i++);
        }
    }
    int __n_374 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_373 = 0; (__i_373 < __n_374); (__i_373++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_373);
        btrc_Vector_string_push(out, arg);
    }
    return out;
}

char* launcherFlatpakStateRoot(char* project, char* emulator) {
    return joinPath(joinPath(project, ".semu/flatpak-state"), __btrc_str_track(__btrc_toLower(emulator)));
}

char* launcherRoutedStateRoot(char* project, char* emulator) {
    return joinPath(joinPath(project, ".semu/appimage-state"), __btrc_str_track(__btrc_toLower(emulator)));
}

void launcherCopyDirContents(char* source, char* destination) {
    if (!FileSystem_isDir(source)) {
        return;
    }
    ensureDir(destination);
    UnixShell* shell = UnixShell_new();
    UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cp -a ", ShellWords_quote(joinPath(source, ".")))), " ")), ShellWords_quote(destination))), " 2>/dev/null || true")), false, false, "");
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void launcherCopyFile(char* source, char* destination) {
    if (!FileSystem_isFile(source)) {
        return;
    }
    ensureDir(PathTools_dirname(destination));
    FileSystem_writeText(destination, FileSystem_readText(source));
}

void launcherCopyProfileDir(char* project, char* relative, char* destination) {
    launcherCopyDirContents(emulatorProfilePath(project, relative), destination);
}

void launcherCopyProfileFile(char* project, char* relative, char* destination) {
    launcherCopyFile(emulatorProfilePath(project, relative), destination);
}

void launcherCopyStateDir(char* project, char* relative, char* destination) {
    launcherCopyProfileDir(project, relative, destination);
}

char* launcherRetroArchSystemDir(char* project) {
    char* externalRoot = configuredEmulationRoot(project);
    char* externalSystem = joinPath(externalRoot, "RetroArch/config/system");
    if ((((int)strlen(externalRoot)) > 0) && FileSystem_isDir(externalSystem)) {
        return externalSystem;
    }
    return biosRoot(project);
}

char* launcherRetroArchConfigValue(char* key, char* value) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(key, " = \"")), Strings_replace(value, "\"", "\\\""))), "\"\n"));
}

char* launcherRetroArchConfig(char* project) {
    char* content = contentRoot(project);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("", launcherRetroArchConfigValue("config_save_on_exit", "false"))), launcherRetroArchConfigValue("video_driver", "glcore"))), launcherRetroArchConfigValue("video_fullscreen", "true"))), launcherRetroArchConfigValue("video_windowed_fullscreen", "false"))), launcherRetroArchConfigValue("video_vsync", "true"))), launcherRetroArchConfigValue("audio_driver", "pulse"))), launcherRetroArchConfigValue("input_driver", "sdl2"))), launcherRetroArchConfigValue("input_joypad_driver", "sdl2"))), launcherRetroArchConfigValue("input_autodetect_enable", "true"))), launcherRetroArchConfigValue("input_remap_binds_enable", "true"))), launcherRetroArchConfigValue("system_directory", launcherRetroArchSystemDir(project)))), launcherRetroArchConfigValue("savestate_directory", joinPath(content, "states")))), launcherRetroArchConfigValue("savefile_directory", joinPath(content, "saves")))), launcherRetroArchConfigValue("screenshot_directory", joinPath(content, "screenshots")))), launcherRetroArchConfigValue("input_exit_emulator", "escape"))), launcherRetroArchConfigValue("input_quit_gamepad_combo", "4"))), launcherRetroArchConfigValue("input_load_state", "a"))), launcherRetroArchConfigValue("input_save_state", "s"))), launcherRetroArchConfigValue("input_menu_toggle", "m"))), launcherRetroArchConfigValue("input_screenshot", "x"))), launcherRetroArchConfigValue("input_state_slot_decrease", "j"))), launcherRetroArchConfigValue("input_state_slot_increase", "k"))), launcherRetroArchConfigValue("input_toggle_fast_forward", "add"))), launcherRetroArchConfigValue("input_toggle_fullscreen", "enter")));
}

bool launcherUsesNixGl(char* emulator) {
    if (strcmp(Environment_get("SEMU_DISABLE_NIXGL", "0"), "1") == 0) {
        return false;
    }
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    return ((((((((((strcmp(key, "retroarch") == 0) || (strcmp(key, "dolphin") == 0)) || (strcmp(key, "ppsspp") == 0)) || (strcmp(key, "flycast") == 0)) || (strcmp(key, "gopher64") == 0)) || (strcmp(key, "melonds") == 0)) || (strcmp(key, "pcsx2") == 0)) || (strcmp(key, "cemu") == 0)) || (strcmp(key, "azahar") == 0)) || (strcmp(key, "ryujinx") == 0));
}

char* launcherNixGlCandidate(char* path) {
    if ((((int)strlen(path)) > 0) && FileSystem_isFile(path)) {
        return path;
    }
    return "";
}

char* launcherNixGlWrapper(char* project) {
    char* configured = launcherNixGlCandidate(Environment_get("SEMU_NIXGL", ""));
    if (((int)strlen(configured)) > 0) {
        return configured;
    }
    char* path = commandPath("nixGLIntel");
    if (((int)strlen(path)) > 0) {
        return path;
    }
    (path = commandPath("nixGL"));
    if (((int)strlen(path)) > 0) {
        return path;
    }
    (path = launcherNixGlCandidate(joinPath(project, "result/bin/nixGLIntel")));
    if (((int)strlen(path)) > 0) {
        return path;
    }
    return launcherNixGlCandidate(joinPath(project, "result/bin/nixGL"));
}

char* launcherRoutedExecutableCommand(char* project, char* emulator, char* executable) {
    char* command = ShellWords_quote(executable);
    if (launcherUsesNixGl(emulator)) {
        char* wrapper = launcherNixGlWrapper(project);
        if (((int)strlen(wrapper)) > 0) {
            return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ShellWords_quote(wrapper), " ")), command));
        }
    }
    return command;
}

char* launcherRoutedEnvironment(char* project, char* emulator, char* home, char* configRoot, char* dataRoot, char* cacheRoot) {
    char* roms = launchRomsRoot(project);
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("SEMU_PROJECT_DIR=", ShellWords_quote(project))), " SEMU_ROMS_DIR=")), ShellWords_quote(roms))), " SEMU_EMULATOR=")), ShellWords_quote(emulator))), " HOME=")), ShellWords_quote(home))), " XDG_CONFIG_HOME=")), ShellWords_quote(configRoot))), " XDG_DATA_HOME=")), ShellWords_quote(dataRoot))), " XDG_CACHE_HOME=")), ShellWords_quote(cacheRoot)));
}

char* launcherRoutedDisplaySetup(void) {
    char* uid = currentUid();
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("if command -v pgrep >/dev/null 2>&1 && { [ -z \"${DISPLAY:-}\" ] || [ -z \"${XAUTHORITY:-}\" ]; }; then ", "p=$(pgrep -x kwin_wayland | head -1 || true); ")), "if [ -n \"$p\" ] && [ -r \"/proc/$p/environ\" ]; then ")), "eval \"$(tr '\\0' '\\n' < /proc/$p/environ | grep -E '^(DISPLAY|XAUTHORITY)=' | sed 's/^/export /')\"; ")), "fi; fi; ")), "if command -v pgrep >/dev/null 2>&1 && [ -z \"${XAUTHORITY:-}\" ]; then ")), "x=$(pgrep -a kwin_wayland | awk '{ for (i=1; i<NF; i++) if ($i == \"--xwayland-xauthority\") { print $(i+1); exit } }' || true); ")), "[ -n \"$x\" ] && export XAUTHORITY=\"$x\"; ")), "fi; ")), "if [ -z \"${XAUTHORITY:-}\" ]; then for f in /run/user/")), uid)), "/xauth_*; do [ -r \"$f\" ] && export XAUTHORITY=\"$f\" && break; done; fi; ")), "export DISPLAY=\"${DISPLAY:-:0}\"; "));
}

char* launcherQuitWatchCommand(char* childCommand) {
    if (strcmp(Environment_get("SEMU_DISABLE_QUIT_WATCH", "0"), "1") == 0) {
        return childCommand;
    }
    char* watcher = commandPath("semu-quit-watch");
    if (((int)strlen(watcher)) == 0) {
        return childCommand;
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ShellWords_quote(watcher), " -- sh -c ")), ShellWords_quote(childCommand)));
}

void launcherWriteRetroArchConfig(char* project, char* configRoot) {
    char* retroarchRoot = joinPath(configRoot, "retroarch");
    ensureDir(retroarchRoot);
    FileSystem_writeText(joinPath(retroarchRoot, "retroarch.cfg"), launcherRetroArchConfig(project));
}

void launcherSeedRoutedState(char* project, char* emulator, char* configRoot, char* dataRoot) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if (strcmp(key, "retroarch") == 0) {
        launcherCopyProfileDir(project, "RetroArch/config", joinPath(configRoot, "retroarch"));
        launcherCopyProfileFile(project, "RetroArch/retroarch.cfg", joinPath(configRoot, "retroarch/retroarch.cfg"));
        launcherWriteRetroArchConfig(project, configRoot);
        return;
    }
    if (strcmp(key, "dolphin") == 0) {
        launcherCopyStateDir(project, "Dolphin/config", joinPath(configRoot, "dolphin-emu"));
        launcherCopyStateDir(project, "Dolphin/data", joinPath(dataRoot, "dolphin-emu"));
        return;
    }
    if (strcmp(key, "pcsx2") == 0) {
        launcherCopyStateDir(project, "PCSX2/config", joinPath(configRoot, "PCSX2"));
        return;
    }
    if (strcmp(key, "cemu") == 0) {
        launcherCopyStateDir(project, "Cemu/config", joinPath(configRoot, "Cemu"));
        launcherCopyStateDir(project, "Cemu/data", joinPath(dataRoot, "Cemu"));
        return;
    }
    if (strcmp(key, "azahar") == 0) {
        launcherCopyStateDir(project, "Azahar/data", joinPath(dataRoot, "azahar-emu"));
        return;
    }
    if (strcmp(key, "ppsspp") == 0) {
        launcherCopyStateDir(project, "PPSSPP/config", joinPath(configRoot, "ppsspp"));
        launcherCopyStateDir(project, "PPSSPP/data", joinPath(dataRoot, "ppsspp"));
        return;
    }
    if (strcmp(key, "flycast") == 0) {
        launcherCopyStateDir(project, "Flycast/config", joinPath(configRoot, "flycast"));
        launcherCopyStateDir(project, "Flycast/data", joinPath(dataRoot, "flycast"));
        return;
    }
    if (strcmp(key, "gopher64") == 0) {
        launcherCopyStateDir(project, "Gopher64/config", joinPath(configRoot, "gopher64"));
        return;
    }
    if (strcmp(key, "melonds") == 0) {
        launcherCopyStateDir(project, "melonDS/config", joinPath(configRoot, "melonDS"));
        launcherCopyStateDir(project, "melonDS/data", joinPath(dataRoot, "melonDS"));
        return;
    }
    if (strcmp(key, "ryujinx") == 0) {
        launcherCopyStateDir(project, "Ryujinx/config", joinPath(configRoot, "Ryujinx"));
        return;
    }
}

int launcherRunRouted(char* project, char* emulator, char* executable, btrc_Vector_string* emulatorArgs) {
    if (!FileSystem_isDir(project)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("launcher: project dir '", project)), "' missing")));
        return 2;
    }
    char* stateRoot = launcherRoutedStateRoot(project, emulator);
    char* home = joinPath(stateRoot, "home");
    char* configRoot = joinPath(stateRoot, "config");
    char* dataRoot = joinPath(stateRoot, "data");
    char* cacheRoot = joinPath(stateRoot, "cache");
    ensureDir(home);
    ensureDir(configRoot);
    ensureDir(dataRoot);
    ensureDir(cacheRoot);
    launcherSeedRoutedState(project, emulator, configRoot, dataRoot);
    btrc_Vector_string* finalArgs = btrc_Vector_string_new();
    if (strcmp(__btrc_str_track(__btrc_toLower(emulator)), "retroarch") == 0) {
        char* retroarchConfig = joinPath(configRoot, "retroarch/retroarch.cfg");
        if (FileSystem_exists(retroarchConfig)) {
            btrc_Vector_string_push(finalArgs, "--config");
            btrc_Vector_string_push(finalArgs, retroarchConfig);
        }
    }
    int __n_376 = btrc_Vector_string_iterLen(launcherArgsWithDefaults(emulator, emulatorArgs));
    for (int __i_375 = 0; (__i_375 < __n_376); (__i_375++)) {
        char* arg = btrc_Vector_string_iterGet(launcherArgsWithDefaults(emulator, emulatorArgs), __i_375);
        btrc_Vector_string_push(finalArgs, arg);
    }
    char* childCommand = launcherRoutedExecutableCommand(project, emulator, executable);
    (childCommand = shellAppendAll(childCommand, finalArgs));
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(launcherRoutedDisplaySetup(), launcherRoutedEnvironment(project, emulator, home, configRoot, dataRoot, cacheRoot))), " ")), launcherQuitWatchCommand(childCommand)));
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_377 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_377;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int launcherRunFlatpak(char* project, char* emulator, char* flatpakId, btrc_Vector_string* emulatorArgs) {
    if (!FileSystem_isDir(project)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("launcher: project dir '", project)), "' missing")));
        return 2;
    }
    char* roms = launchRomsRoot(project);
    char* stateRoot = launcherFlatpakStateRoot(project, emulator);
    char* uid = currentUid();
    ensureDir(joinPath(stateRoot, "config"));
    ensureDir(joinPath(stateRoot, "data"));
    ensureDir(joinPath(stateRoot, "cache"));
    char* command = "unset VK_ICD_FILENAMES LIBGL_ALWAYS_SOFTWARE; ";
    if (launcherUsesX11(emulator)) {
        (command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, "unset MESA_LOADER_DRIVER_OVERRIDE MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE; ")), "if [ -z \"${DISPLAY:-}\" ] || [ -z \"${XAUTHORITY:-}\" ]; then ")), "p=$(pgrep -x plasmashell | head -1 || pgrep -x kwin_wayland | head -1 || true); ")), "if [ -n \"$p\" ] && [ -r \"/proc/$p/environ\" ]; then ")), "eval \"$(tr '\\0' '\\n' < /proc/$p/environ | grep -E '^(DISPLAY|XAUTHORITY)=' | sed 's/^/export /')\"; ")), "fi; fi; ")), "if [ -z \"${XAUTHORITY:-}\" ]; then for f in /run/user/")), uid)), "/xauth_*; do [ -r \"$f\" ] && export XAUTHORITY=\"$f\" && break; done; fi; ")), "export DISPLAY=\"${DISPLAY:-:1}\"; ")));
    }
    (command = __btrc_str_track(__btrc_strcat(command, "flatpak run")));
    btrc_Vector_string* __list_378 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("--filesystem=", project)));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("--filesystem=", roms)));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("--filesystem=", stateRoot)), ":create")));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("--env=XDG_CONFIG_HOME=", joinPath(stateRoot, "config"))));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("--env=XDG_DATA_HOME=", joinPath(stateRoot, "data"))));
    btrc_Vector_string_push(__list_378, __btrc_str_track(__btrc_strcat("--env=XDG_CACHE_HOME=", joinPath(stateRoot, "cache"))));
    btrc_Vector_string* flatpakArgs = __list_378;
    if (launcherUsesX11(emulator)) {
        btrc_Vector_string_push(flatpakArgs, "--socket=x11");
    } else {
        btrc_Vector_string_push(flatpakArgs, "--socket=wayland");
        btrc_Vector_string_push(flatpakArgs, "--env=QT_QPA_PLATFORM=wayland");
        btrc_Vector_string_push(flatpakArgs, "--env=SDL_VIDEODRIVER=wayland");
        btrc_Vector_string_push(flatpakArgs, "--env=GDK_BACKEND=wayland");
        btrc_Vector_string_push(flatpakArgs, "--env=XDG_SESSION_TYPE=wayland");
        btrc_Vector_string_push(flatpakArgs, __btrc_str_track(__btrc_strcat("--env=WAYLAND_DISPLAY=", Environment_get("WAYLAND_DISPLAY", "wayland-0"))));
        btrc_Vector_string_push(flatpakArgs, "--env=DISPLAY=");
        btrc_Vector_string_push(flatpakArgs, "--env=XAUTHORITY=");
    }
    btrc_Vector_string_push(flatpakArgs, flatpakId);
    int __n_380 = btrc_Vector_string_iterLen(launcherArgsWithDefaults(emulator, emulatorArgs));
    for (int __i_379 = 0; (__i_379 < __n_380); (__i_379++)) {
        char* arg = btrc_Vector_string_iterGet(launcherArgsWithDefaults(emulator, emulatorArgs), __i_379);
        btrc_Vector_string_push(flatpakArgs, arg);
    }
    (command = shellAppendAll(command, flatpakArgs));
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_381 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_381;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int launcherRunEmulator(char* project, char* emulator, btrc_Vector_string* emulatorArgs) {
    if (strcmp(emulator, "flatpak") == 0) {
        if (emulatorArgs->len == 0) {
            printf("%s\n", "error 0:0 launcher flatpak needs FLATPAK_ID [ARGS...]");
            return 1;
        }
        return launcherRunFlatpak(project, "flatpak", btrc_Vector_string_get(emulatorArgs, 0), vectorTail(emulatorArgs, 1));
    }
    if (strcmp(__btrc_str_track(__btrc_toLower(emulator)), "retroarch") == 0) {
        return sandboxLaunch(project, "retroarch", "/usr/bin/retroarch", emulatorArgs);
    }
    char* flatpakId = launcherFlatpakId(emulator);
    if (((int)strlen(flatpakId)) == 0) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("error 0:0 unknown launcher emulator '", emulator)), "'")));
        return 1;
    }
    return launcherRunFlatpak(project, emulator, flatpakId, emulatorArgs);
}

int launcherCommand(CliArgs* args, char* project) {
    if (CliArgs_count(args) < 2) {
        printf("%s\n", "error 0:0 launcher needs EMULATOR [ARGS...]");
        return 1;
    }
    char* emulator = CliArgs_get(args, 1);
    if (strcmp(emulator, "routed") == 0) {
        if (CliArgs_count(args) < 4) {
            printf("%s\n", "error 0:0 launcher routed needs EMULATOR EXECUTABLE [ARGS...]");
            return 1;
        }
        return launcherRunRouted(project, CliArgs_get(args, 2), CliArgs_get(args, 3), cliTail(args, 4));
    }
    return launcherRunEmulator(project, emulator, cliTail(args, 2));
}

btrc_Vector_string* linuxLauncherNames(void) {
    btrc_Vector_string* __list_382 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_382, "RETROARCH");
    btrc_Vector_string_push(__list_382, "DOLPHIN");
    btrc_Vector_string_push(__list_382, "PPSSPP");
    btrc_Vector_string_push(__list_382, "FLYCAST");
    btrc_Vector_string_push(__list_382, "AZAHAR");
    btrc_Vector_string_push(__list_382, "GOPHER64");
    btrc_Vector_string_push(__list_382, "MELONDS");
    btrc_Vector_string_push(__list_382, "PCSX2");
    btrc_Vector_string_push(__list_382, "CEMU");
    btrc_Vector_string_push(__list_382, "RYUJINX");
    return __list_382;
}

btrc_Vector_string* lowercaseValues(btrc_Vector_string* values) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    int __n_384 = btrc_Vector_string_iterLen(values);
    for (int __i_383 = 0; (__i_383 < __n_384); (__i_383++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_383);
        btrc_Vector_string_push(out, __btrc_str_track(__btrc_toLower(value)));
    }
    return out;
}

char* semuLauncherName(char* emulator) {
    return __btrc_str_track(__btrc_strcat("semu-", __btrc_str_track(__btrc_toLower(emulator))));
}

btrc_Vector_string* retroarchCoreSearchPaths(void) {
    btrc_Vector_string* __list_385 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_385, "/usr/lib/x86_64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_385, "/usr/lib/aarch64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_385, "/usr/lib/libretro");
    return __list_385;
}

char* launcherEntries(void) {
    btrc_Vector_string* linuxEmulatorFields = btrc_Vector_string_new();
    int __n_387 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_386 = 0; (__i_386 < __n_387); (__i_386++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_386);
        btrc_Vector_string_push(linuxEmulatorFields, jsonStrField(emulator, __btrc_str_track(__btrc_strcat("${project}/packaging/linux/bin/", semuLauncherName(emulator)))));
    }
    btrc_Vector_string* __list_388 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_388, jsonField("emulators", jsonObject(linuxEmulatorFields)));
    btrc_Vector_string* __list_389 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_389, jsonField("RETROARCH", jsonStringArray(retroarchCoreSearchPaths())));
    btrc_Vector_string_push(__list_388, jsonField("cores", jsonObject(__list_389)));
    btrc_Vector_string* __list_390 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_390, jsonStrField("org.DolphinEmu.dolphin-emu", "GameCube/Wii"));
    btrc_Vector_string_push(__list_390, jsonStrField("org.ppsspp.PPSSPP", "PSP"));
    btrc_Vector_string_push(__list_390, jsonStrField("org.flycast.Flycast", "Dreamcast"));
    btrc_Vector_string_push(__list_390, jsonStrField("org.azahar_emu.Azahar", "Nintendo 3DS"));
    btrc_Vector_string_push(__list_390, jsonStrField("io.github.gopher64.gopher64", "Nintendo 64"));
    btrc_Vector_string_push(__list_390, jsonStrField("net.kuribo64.melonDS", "Nintendo DS"));
    btrc_Vector_string_push(__list_390, jsonStrField("net.pcsx2.PCSX2", "PlayStation 2"));
    btrc_Vector_string_push(__list_390, jsonStrField("info.cemu.Cemu", "Wii U"));
    btrc_Vector_string_push(__list_390, jsonStrField("org.ryujinx.Ryujinx", "Nintendo Switch"));
    btrc_Vector_string_push(__list_388, jsonField("flatpaks", jsonObject(__list_390)));
    char* linux = jsonObject(__list_388);
    btrc_Vector_string* __list_394 = btrc_Vector_string_new();
    btrc_Vector_string* __list_395 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_395, jsonStrField("RETROARCH", "${project}/result/bin/retroarch"));
    btrc_Vector_string_push(__list_395, jsonStrField("AZAHAR", "${project}/result/Applications/azahar.app/Contents/MacOS/azahar"));
    btrc_Vector_string_push(__list_395, jsonStrField("DOLPHIN", "${project}/result/Applications/Dolphin.app/Contents/MacOS/Dolphin"));
    btrc_Vector_string_push(__list_395, jsonStrField("PCSX2", "${project}/result/Applications/PCSX2.app/Contents/MacOS/PCSX2"));
    btrc_Vector_string_push(__list_395, jsonStrField("CEMU", "${project}/result/Applications/Cemu.app/Contents/MacOS/Cemu"));
    btrc_Vector_string_push(__list_395, jsonStrField("RYUJINX", "~/.local/share/ryujinx-app/Ryujinx.app/Contents/MacOS/Ryujinx"));
    btrc_Vector_string_push(__list_395, jsonStrField("ARES", "${project}/result/Applications/ares.app/Contents/MacOS/ares"));
    btrc_Vector_string_push(__list_394, jsonField("emulators", jsonObject(__list_395)));
    btrc_Vector_string* __list_396 = btrc_Vector_string_new();
    btrc_Vector_string* __list_397 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_397, "${project}/result/cores");
    btrc_Vector_string_push(__list_396, jsonField("RETROARCH", jsonStringArray(__list_397)));
    btrc_Vector_string_push(__list_394, jsonField("cores", jsonObject(__list_396)));
    char* macos = jsonObject(__list_394);
    btrc_Vector_string* __list_402 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_402, jsonField("linux", linux));
    btrc_Vector_string_push(__list_402, jsonField("macos", macos));
    return jsonObject(__list_402);
}

char* stripTrailingSlashes(char* path) {
    char* result = path;
    while ((((int)strlen(result)) > 1) && (__btrc_charAt(result, (((int)strlen(result)) - 1)) == '/')) {
        (result = __btrc_str_track(__btrc_substring(result, 0, (((int)strlen(result)) - 1))));
    }
    return result;
}

char* sandboxResolveTarget(char* scratch, char* target) {
    char* resolved = Strings_replace(target, "${host}", "$HOME/.config");
    (resolved = Strings_replace(resolved, "${portable}", "$HOME/ES-DE"));
    if (__btrc_startsWith(resolved, "~/")) {
        (resolved = __btrc_str_track(__btrc_strcat("$HOME/", Strings_removePrefix(resolved, "~/"))));
    }
    (resolved = stripTrailingSlashes(resolved));
    if (__btrc_startsWith(resolved, "$HOME/")) {
        return joinPath(scratch, Strings_removePrefix(resolved, "$HOME/"));
    }
    return resolved;
}

char* sandboxProjectEmulatorDir(char* project, char* emulator) {
    char* wanted = __btrc_str_track(__btrc_toLower(emulator));
    char* profileRoot = emulatorProfilesRoot(project);
    int __n_404 = btrc_Vector_string_iterLen(FileSystem_listDir(profileRoot));
    for (int __i_403 = 0; (__i_403 < __n_404); (__i_403++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(profileRoot), __i_403);
        char* candidate = joinPath(profileRoot, name);
        if (FileSystem_isDir(candidate) && (strcmp(__btrc_str_track(__btrc_toLower(name)), wanted) == 0)) {
            return candidate;
        }
    }
    int __n_406 = btrc_Vector_string_iterLen(FileSystem_listDir(project));
    for (int __i_405 = 0; (__i_405 < __n_406); (__i_405++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(project), __i_405);
        char* candidate = joinPath(project, name);
        if (FileSystem_isDir(candidate) && (strcmp(__btrc_str_track(__btrc_toLower(name)), wanted) == 0)) {
            return candidate;
        }
    }
    return "";
}

void sandboxSymlink(char* linkPath, char* sourcePath) {
    ensureDir(PathTools_dirname(linkPath));
    FileSystem_removeRecursive(linkPath);
    FileSystem_symlink(sourcePath, linkPath);
}

void sandboxApplyLink(char* emuDir, char* scratch, char* entry, char* linuxTarget) {
    char* source = joinPath(emuDir, entry);
    if (!FileSystem_exists(source)) {
        return;
    }
    char* destDir = sandboxResolveTarget(scratch, linuxTarget);
    if (FileSystem_isDir(source)) {
        ensureDir(destDir);
        int __n_408 = btrc_Vector_string_iterLen(FileSystem_listDir(source));
        for (int __i_407 = 0; (__i_407 < __n_408); (__i_407++)) {
            char* child = btrc_Vector_string_iterGet(FileSystem_listDir(source), __i_407);
            sandboxSymlink(joinPath(destDir, child), joinPath(source, child));
        }
        return;
    }
    sandboxSymlink(joinPath(destDir, PathTools_basename(source)), source);
}

bool sandboxApplyKnownLinks(char* key, char* emuDir, char* scratch) {
    if (strcmp(key, "azahar") == 0) {
        sandboxApplyLink(emuDir, scratch, "data", "~/.local/share/azahar-emu/");
        return true;
    }
    if (strcmp(key, "cemu") == 0) {
        sandboxApplyLink(emuDir, scratch, "config", "~/.config/Cemu/");
        sandboxApplyLink(emuDir, scratch, "data", "~/.local/share/Cemu/");
        return true;
    }
    if (strcmp(key, "dolphin") == 0) {
        sandboxApplyLink(emuDir, scratch, "config", "~/.config/dolphin-emu/");
        sandboxApplyLink(emuDir, scratch, "data", "~/.local/share/dolphin-emu/");
        return true;
    }
    if (strcmp(key, "es-de") == 0) {
        sandboxApplyLink(emuDir, scratch, "ES-DE", "${portable}/");
        sandboxApplyLink(emuDir, scratch, "es_settings.xml", "${portable}/settings/");
        sandboxApplyLink(emuDir, scratch, "custom_systems", "${portable}/custom_systems/");
        return true;
    }
    if (strcmp(key, "lime3ds") == 0) {
        sandboxApplyLink(emuDir, scratch, "data", "${host}/lime3ds-emu/");
        return true;
    }
    if (strcmp(key, "pcsx2") == 0) {
        sandboxApplyLink(emuDir, scratch, "config", "${host}/PCSX2/");
        return true;
    }
    if (strcmp(key, "retroarch") == 0) {
        sandboxApplyLink(emuDir, scratch, "config", "${host}/retroarch/");
        sandboxApplyLink(emuDir, scratch, "retroarch.cfg", "${host}/retroarch/");
        return true;
    }
    if (strcmp(key, "ryujinx") == 0) {
        sandboxApplyLink(emuDir, scratch, "config", "~/.config/Ryujinx/");
        return true;
    }
    return false;
}

char* currentUid(void) {
    return Strings_fromInt(Platform_euid());
}

char* launchRomsRoot(char* project) {
    return Environment_get("SEMU_ROMS_DIR", configuredRomsRoot(project));
}

char* xdgRunDir(void) {
    return Environment_get("XDG_RUNTIME_DIR", __btrc_str_track(__btrc_strcat("/run/user/", currentUid())));
}

btrc_Vector_string* cliTail(CliArgs* args, int start) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    for (int i = start; (i < CliArgs_count(args)); (i++)) {
        char* value = CliArgs_get(args, i);
        if (strcmp(value, "--project") == 0) {
            (i++);
        } else if (!(strcmp(value, "--") == 0)) {
            btrc_Vector_string_push(out, value);
        }
    }
    return out;
}

btrc_Vector_string* vectorTail(btrc_Vector_string* values, int start) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    for (int i = start; (i < values->len); (i++)) {
        btrc_Vector_string_push(out, btrc_Vector_string_get(values, i));
    }
    return out;
}

btrc_Vector_string* launcherPassthroughArgs(CliArgs* args) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    bool skipNext = false;
    for (int i = 0; (i < CliArgs_count(args)); (i++)) {
        char* value = CliArgs_get(args, i);
        if (skipNext) {
            (skipNext = false);
        } else if (strcmp(value, "--project") == 0) {
            (skipNext = true);
        } else {
            btrc_Vector_string_push(out, value);
        }
    }
    return out;
}

char* shellAppend(char* command, char* arg) {
    if (((int)strlen(command)) == 0) {
        return ShellWords_quote(arg);
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(command, " ")), ShellWords_quote(arg)));
}

char* shellAppendAll(char* command, btrc_Vector_string* args) {
    char* out = command;
    int __n_410 = btrc_Vector_string_iterLen(args);
    for (int __i_409 = 0; (__i_409 < __n_410); (__i_409++)) {
        char* arg = btrc_Vector_string_iterGet(args, __i_409);
        (out = shellAppend(out, arg));
    }
    return out;
}

int sandboxLaunch(char* project, char* emulator, char* executable, btrc_Vector_string* emulatorArgs) {
    if ((((int)strlen(emulator)) == 0) || (((int)strlen(executable)) == 0)) {
        printf("%s\n", "error 0:0 sandbox launch needs EMULATOR EXECUTABLE [ARGS...]");
        return 1;
    }
    if (!FileSystem_isDir(project)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("sandbox: project dir '", project)), "' missing")));
        return 2;
    }
    char* scratch = joinPath(homeDir(), __btrc_str_track(__btrc_strcat(".local/share/semu/scratch/", emulator)));
    FileSystem_removeRecursive(scratch);
    ensureDir(scratch);
    char* emuDir = sandboxProjectEmulatorDir(project, emulator);
    if (((int)strlen(emuDir)) == 0) {
        int __fstr_413_len = snprintf(NULL, 0, "sandbox: no emulator directory for '%s' under %s", emulator, project);
        char* __fstr_413_buf = __btrc_str_track(((char*)malloc((__fstr_413_len + 1))));
        snprintf(__fstr_413_buf, (__fstr_413_len + 1), "sandbox: no emulator directory for '%s' under %s", emulator, project);
        printf("%s\n", __fstr_413_buf);
        return 3;
    }
    if (!sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch)) {
        int __fstr_416_len = snprintf(NULL, 0, "sandbox: no BTRC symlink spec for '%s'", emulator);
        char* __fstr_416_buf = __btrc_str_track(((char*)malloc((__fstr_416_len + 1))));
        snprintf(__fstr_416_buf, (__fstr_416_len + 1), "sandbox: no BTRC symlink spec for '%s'", emulator);
        printf("%s\n", __fstr_416_buf);
        return 3;
    }
    char* home = homeDir();
    char* runDir = xdgRunDir();
    char* shareNet = Environment_get("SEMU_SHARE_NET", "1");
    char* command = ShellWords_quote(Environment_get("SEMU_BWRAP", "bwrap"));
    btrc_Vector_string* __list_417 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_417, "--ro-bind");
    btrc_Vector_string_push(__list_417, "/usr");
    btrc_Vector_string_push(__list_417, "/usr");
    btrc_Vector_string_push(__list_417, "--ro-bind");
    btrc_Vector_string_push(__list_417, "/etc");
    btrc_Vector_string_push(__list_417, "/etc");
    btrc_Vector_string_push(__list_417, "--ro-bind");
    btrc_Vector_string_push(__list_417, "/opt");
    btrc_Vector_string_push(__list_417, "/opt");
    btrc_Vector_string_push(__list_417, "--ro-bind");
    btrc_Vector_string_push(__list_417, "/var");
    btrc_Vector_string_push(__list_417, "/var");
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, "/lib");
    btrc_Vector_string_push(__list_417, "/lib");
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, "/lib64");
    btrc_Vector_string_push(__list_417, "/lib64");
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, "/bin");
    btrc_Vector_string_push(__list_417, "/bin");
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, "/sbin");
    btrc_Vector_string_push(__list_417, "/sbin");
    btrc_Vector_string_push(__list_417, "--proc");
    btrc_Vector_string_push(__list_417, "/proc");
    btrc_Vector_string_push(__list_417, "--dev");
    btrc_Vector_string_push(__list_417, "/dev");
    btrc_Vector_string_push(__list_417, "--dev-bind");
    btrc_Vector_string_push(__list_417, "/dev/dri");
    btrc_Vector_string_push(__list_417, "/dev/dri");
    btrc_Vector_string_push(__list_417, "--dev-bind-try");
    btrc_Vector_string_push(__list_417, "/dev/snd");
    btrc_Vector_string_push(__list_417, "/dev/snd");
    btrc_Vector_string_push(__list_417, "--dev-bind-try");
    btrc_Vector_string_push(__list_417, "/dev/input");
    btrc_Vector_string_push(__list_417, "/dev/input");
    btrc_Vector_string_push(__list_417, "--dev-bind-try");
    btrc_Vector_string_push(__list_417, "/dev/uinput");
    btrc_Vector_string_push(__list_417, "/dev/uinput");
    btrc_Vector_string_push(__list_417, "--ro-bind");
    btrc_Vector_string_push(__list_417, "/sys");
    btrc_Vector_string_push(__list_417, "/sys");
    btrc_Vector_string_push(__list_417, "--bind");
    btrc_Vector_string_push(__list_417, scratch);
    btrc_Vector_string_push(__list_417, home);
    btrc_Vector_string_push(__list_417, "--tmpfs");
    btrc_Vector_string_push(__list_417, "/tmp");
    btrc_Vector_string_push(__list_417, "--bind");
    btrc_Vector_string_push(__list_417, project);
    btrc_Vector_string_push(__list_417, project);
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, launchRomsRoot(project));
    btrc_Vector_string_push(__list_417, launchRomsRoot(project));
    btrc_Vector_string_push(__list_417, "--bind");
    btrc_Vector_string_push(__list_417, runDir);
    btrc_Vector_string_push(__list_417, runDir);
    btrc_Vector_string_push(__list_417, "--ro-bind-try");
    btrc_Vector_string_push(__list_417, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_417, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_417, "--unshare-pid");
    btrc_Vector_string_push(__list_417, "--unshare-uts");
    btrc_Vector_string_push(__list_417, "--unshare-ipc");
    btrc_Vector_string_push(__list_417, "--die-with-parent");
    btrc_Vector_string_push(__list_417, "--new-session");
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "HOME");
    btrc_Vector_string_push(__list_417, home);
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "XDG_RUNTIME_DIR");
    btrc_Vector_string_push(__list_417, runDir);
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "XDG_DATA_HOME");
    btrc_Vector_string_push(__list_417, joinPath(home, ".local/share"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "XDG_CONFIG_HOME");
    btrc_Vector_string_push(__list_417, joinPath(home, ".config"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "XDG_CACHE_HOME");
    btrc_Vector_string_push(__list_417, joinPath(home, ".cache"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "XDG_SESSION_TYPE");
    btrc_Vector_string_push(__list_417, Environment_get("XDG_SESSION_TYPE", "wayland"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_417, Environment_get("WAYLAND_DISPLAY", "wayland-0"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "DBUS_SESSION_BUS_ADDRESS");
    btrc_Vector_string_push(__list_417, Environment_get("DBUS_SESSION_BUS_ADDRESS", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unix:path=", runDir)), "/bus"))));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "QT_QPA_PLATFORM");
    btrc_Vector_string_push(__list_417, Environment_get("QT_QPA_PLATFORM", "wayland;xcb"));
    btrc_Vector_string_push(__list_417, "--setenv");
    btrc_Vector_string_push(__list_417, "SDL_VIDEODRIVER");
    btrc_Vector_string_push(__list_417, Environment_get("SDL_VIDEODRIVER", "wayland"));
    btrc_Vector_string* bwrapArgs = __list_417;
    if (Environment_has("DISPLAY")) {
        btrc_Vector_string_push(bwrapArgs, "--setenv");
        btrc_Vector_string_push(bwrapArgs, "DISPLAY");
        btrc_Vector_string_push(bwrapArgs, Environment_get("DISPLAY", ""));
        if (Environment_has("XAUTHORITY")) {
            btrc_Vector_string_push(bwrapArgs, "--setenv");
            btrc_Vector_string_push(bwrapArgs, "XAUTHORITY");
            btrc_Vector_string_push(bwrapArgs, Environment_get("XAUTHORITY", ""));
        }
    } else {
        btrc_Vector_string_push(bwrapArgs, "--unsetenv");
        btrc_Vector_string_push(bwrapArgs, "DISPLAY");
        btrc_Vector_string_push(bwrapArgs, "--unsetenv");
        btrc_Vector_string_push(bwrapArgs, "XAUTHORITY");
    }
    if (strcmp(shareNet, "1") == 0) {
        btrc_Vector_string_push(bwrapArgs, "--share-net");
    } else {
        btrc_Vector_string_push(bwrapArgs, "--unshare-net");
    }
    btrc_Vector_string_push(bwrapArgs, "--unsetenv");
    btrc_Vector_string_push(bwrapArgs, "LIBGL_ALWAYS_SOFTWARE");
    btrc_Vector_string_push(bwrapArgs, "--unsetenv");
    btrc_Vector_string_push(bwrapArgs, "MESA_LOADER_DRIVER_OVERRIDE");
    btrc_Vector_string_push(bwrapArgs, "--unsetenv");
    btrc_Vector_string_push(bwrapArgs, "VK_ICD_FILENAMES");
    btrc_Vector_string_push(bwrapArgs, "--");
    btrc_Vector_string_push(bwrapArgs, executable);
    int __n_419 = btrc_Vector_string_iterLen(launcherArgsWithDefaults(emulator, emulatorArgs));
    for (int __i_418 = 0; (__i_418 < __n_419); (__i_418++)) {
        char* arg = btrc_Vector_string_iterGet(launcherArgsWithDefaults(emulator, emulatorArgs), __i_418);
        btrc_Vector_string_push(bwrapArgs, arg);
    }
    (command = shellAppendAll(command, bwrapArgs));
    if (strcmp(Environment_get("SEMU_DEBUG", "0"), "1") == 0) {
        printf("%s\n", command);
    }
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_420 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_420;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int sandboxPrepareCommand(CliArgs* args, char* project) {
    char* emulator = CliArgs_valueAfter(args, "--emulator", "");
    char* scratch = CliArgs_valueAfter(args, "--scratch", "");
    if ((((int)strlen(emulator)) == 0) || (((int)strlen(scratch)) == 0)) {
        printf("%s\n", "error 0:0 sandbox prepare needs --emulator NAME --scratch PATH");
        return 1;
    }
    char* emuDir = sandboxProjectEmulatorDir(project, emulator);
    if (((int)strlen(emuDir)) == 0) {
        int __fstr_423_len = snprintf(NULL, 0, "sandbox: no emulator directory for '%s' under %s", emulator, project);
        char* __fstr_423_buf = __btrc_str_track(((char*)malloc((__fstr_423_len + 1))));
        snprintf(__fstr_423_buf, (__fstr_423_len + 1), "sandbox: no emulator directory for '%s' under %s", emulator, project);
        printf("%s\n", __fstr_423_buf);
        return 3;
    }
    ensureDir(scratch);
    bool ok = sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch);
    if (!ok) {
        int __fstr_426_len = snprintf(NULL, 0, "sandbox: no BTRC symlink spec for '%s'", emulator);
        char* __fstr_426_buf = __btrc_str_track(((char*)malloc((__fstr_426_len + 1))));
        snprintf(__fstr_426_buf, (__fstr_426_len + 1), "sandbox: no BTRC symlink spec for '%s'", emulator);
        printf("%s\n", __fstr_426_buf);
        return 3;
    }
    return 0;
}

int sandboxCommand(CliArgs* args, char* project) {
    char* mode = "prepare";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "prepare") == 0) {
        return sandboxPrepareCommand(args, project);
    }
    if (strcmp(mode, "launch") == 0) {
        if (CliArgs_count(args) < 4) {
            printf("%s\n", "error 0:0 sandbox launch needs EMULATOR EXECUTABLE [ARGS...]");
            return 1;
        }
        return sandboxLaunch(project, CliArgs_get(args, 2), CliArgs_get(args, 3), cliTail(args, 4));
    }
    printUsage();
    return 1;
}

char* textLines(btrc_Vector_string* items) {
    return __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(items, "\n"), "\n"));
}

KeymapIr* projectKeymapIr(char* project) {
    char* source = (FileSystem_exists(keymapSourcePath(project)) ? FileSystem_readText(keymapSourcePath(project)) : defaultKeymapSource());
    KeymapErrors* errors = KeymapErrors_new();
    KeymapIr* ir = compileKeymap(source, errors);
    if (KeymapErrors_count(errors) == 0) {
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return ir;
    }
    KeymapErrors* fallbackErrors = KeymapErrors_new();
    KeymapIr* __btrc_ret_427 = compileKeymap(defaultKeymapSource(), fallbackErrors);
    if (fallbackErrors != NULL) {
        if ((--fallbackErrors->__rc) <= 0) {
            KeymapErrors_destroy(fallbackErrors);
        }
    }
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_427;
    if (fallbackErrors != NULL) {
        if ((--fallbackErrors->__rc) <= 0) {
            KeymapErrors_destroy(fallbackErrors);
        }
    }
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

char* retroArchProfileText(char* project, KeymapIr* ir) {
    btrc_Vector_string* __list_428 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_428, "config_save_on_exit = \"true\"");
    btrc_Vector_string_push(__list_428, "video_fullscreen = \"true\"");
    btrc_Vector_string_push(__list_428, "video_vsync = \"true\"");
    btrc_Vector_string_push(__list_428, "audio_driver = \"pulse\"");
    btrc_Vector_string_push(__list_428, "input_driver = \"sdl2\"");
    btrc_Vector_string_push(__list_428, "input_autodetect_enable = \"true\"");
    btrc_Vector_string_push(__list_428, "input_remap_binds_enable = \"true\"");
    btrc_Vector_string_push(__list_428, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("savestate_directory = \"", joinPath(contentRoot(project), "states"))), "\"")));
    btrc_Vector_string_push(__list_428, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("savefile_directory = \"", joinPath(contentRoot(project), "saves"))), "\"")));
    btrc_Vector_string_push(__list_428, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("screenshot_directory = \"", joinPath(contentRoot(project), "screenshots"))), "\"")));
    return __btrc_str_track(__btrc_strcat(textLines(__list_428), renderRetroArchKeymap(ir)));
}

char* dolphinGcpadProfileText(void) {
    btrc_Vector_string* __list_429 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_429, "[Profile]");
    btrc_Vector_string_push(__list_429, "Device = SDL/0/Steam Deck Controller");
    btrc_Vector_string_push(__list_429, "Buttons/A = `Button S`");
    btrc_Vector_string_push(__list_429, "Buttons/B = `Button W`");
    btrc_Vector_string_push(__list_429, "Buttons/X = `Button E`");
    btrc_Vector_string_push(__list_429, "Buttons/Y = `Button N`");
    btrc_Vector_string_push(__list_429, "Buttons/Z = `Shoulder R`|Back");
    btrc_Vector_string_push(__list_429, "Buttons/Start = Start");
    btrc_Vector_string_push(__list_429, "Main Stick/Up = `Axis 1-`");
    btrc_Vector_string_push(__list_429, "Main Stick/Down = `Axis 1+`");
    btrc_Vector_string_push(__list_429, "Main Stick/Left = `Axis 0-`");
    btrc_Vector_string_push(__list_429, "Main Stick/Right = `Axis 0+`");
    btrc_Vector_string_push(__list_429, "C-Stick/Up = `Axis 4-`");
    btrc_Vector_string_push(__list_429, "C-Stick/Down = `Axis 4+`");
    btrc_Vector_string_push(__list_429, "C-Stick/Left = `Axis 3-`");
    btrc_Vector_string_push(__list_429, "C-Stick/Right = `Axis 3+`");
    btrc_Vector_string_push(__list_429, "Triggers/L = `Trigger L`");
    btrc_Vector_string_push(__list_429, "Triggers/R = `Trigger R`");
    btrc_Vector_string_push(__list_429, "Triggers/L-Analog = `Trigger L`");
    btrc_Vector_string_push(__list_429, "Triggers/R-Analog = `Trigger R`");
    btrc_Vector_string_push(__list_429, "D-Pad/Up = `Pad N`");
    btrc_Vector_string_push(__list_429, "D-Pad/Down = `Pad S`");
    btrc_Vector_string_push(__list_429, "D-Pad/Left = `Pad W`");
    btrc_Vector_string_push(__list_429, "D-Pad/Right = `Pad E`");
    btrc_Vector_string_push(__list_429, "Rumble/Motor = Strong");
    btrc_Vector_string_push(__list_429, "Options/Always Connected = True");
    return textLines(__list_429);
}

char* dolphinHotkeysProfileText(KeymapIr* ir) {
    btrc_Vector_string* __list_430 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_430, "[Profile]");
    btrc_Vector_string_push(__list_430, "Device = XInput2/0/Virtual core pointer");
    btrc_Vector_string* __list_431 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_431, "Emulation Speed/Disable Emulation Speed Limit = Tab");
    btrc_Vector_string_push(__list_431, "Controller Profile 1/Next Profile = @(Alt+F5)");
    btrc_Vector_string_push(__list_431, "Other State Hotkeys/Undo Load State = F12");
    btrc_Vector_string_push(__list_431, "Other State Hotkeys/Undo Save State = @(Shift+F12)");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(textLines(__list_430), renderDolphinKeymap(ir))), textLines(__list_431)));
}

char* dolphinWiimoteProfileText(bool classic) {
    btrc_Vector_string* __list_432 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_432, "[Profile]");
    btrc_Vector_string_push(__list_432, "Device = SteamDeck/0/Steam Deck");
    btrc_Vector_string_push(__list_432, "Buttons/A = `XInput2/0/Virtual core pointer:Click 1`");
    btrc_Vector_string_push(__list_432, "Buttons/B = `R4`");
    btrc_Vector_string_push(__list_432, "Buttons/1 = `L4`");
    btrc_Vector_string_push(__list_432, "Buttons/2 = `L5`");
    btrc_Vector_string_push(__list_432, "Buttons/- = View");
    btrc_Vector_string_push(__list_432, "Buttons/+ = Menu");
    btrc_Vector_string_push(__list_432, "D-Pad/Up = `D-Pad Up`");
    btrc_Vector_string_push(__list_432, "D-Pad/Down = `D-Pad Down`");
    btrc_Vector_string_push(__list_432, "D-Pad/Left = `D-Pad Left`");
    btrc_Vector_string_push(__list_432, "D-Pad/Right = `D-Pad Right`");
    btrc_Vector_string_push(__list_432, "IR/Vertical Offset = 12.");
    btrc_Vector_string_push(__list_432, "IR/Total Yaw = 19.");
    btrc_Vector_string_push(__list_432, "IR/Total Pitch = 22.");
    btrc_Vector_string_push(__list_432, "IR/Auto-Hide = True");
    btrc_Vector_string_push(__list_432, "IR/Up = `XInput2/0/Virtual core pointer:Cursor Y-`");
    btrc_Vector_string_push(__list_432, "IR/Down = `XInput2/0/Virtual core pointer:Cursor Y+`");
    btrc_Vector_string_push(__list_432, "IR/Left = `XInput2/0/Virtual core pointer:Cursor X-`");
    btrc_Vector_string_push(__list_432, "IR/Right = `XInput2/0/Virtual core pointer:Cursor X+`");
    btrc_Vector_string_push(__list_432, "IR/Hide = `Thumb L`");
    btrc_Vector_string_push(__list_432, "IMUIR/Enabled = False");
    btrc_Vector_string* lines = __list_432;
    if (classic) {
        btrc_Vector_string_push(lines, "Extension = Classic");
        btrc_Vector_string_push(lines, "Classic/Buttons/A = B");
        btrc_Vector_string_push(lines, "Classic/Buttons/B = A");
        btrc_Vector_string_push(lines, "Classic/Buttons/X = Y");
        btrc_Vector_string_push(lines, "Classic/Buttons/Y = X");
        btrc_Vector_string_push(lines, "Classic/Buttons/ZL = `L1`");
        btrc_Vector_string_push(lines, "Classic/Buttons/ZR = `R1`");
        btrc_Vector_string_push(lines, "Classic/Buttons/- = View");
        btrc_Vector_string_push(lines, "Classic/Buttons/+ = Menu");
        btrc_Vector_string_push(lines, "Classic/Left Stick/Up = `Left Stick Y+`");
        btrc_Vector_string_push(lines, "Classic/Left Stick/Down = `Left Stick Y-`");
        btrc_Vector_string_push(lines, "Classic/Left Stick/Left = `Left Stick X-`");
        btrc_Vector_string_push(lines, "Classic/Left Stick/Right = `Left Stick X+`");
        btrc_Vector_string_push(lines, "Classic/Right Stick/Up = `Right Stick Y+`");
        btrc_Vector_string_push(lines, "Classic/Right Stick/Down = `Right Stick Y-`");
        btrc_Vector_string_push(lines, "Classic/Right Stick/Left = `Right Stick X-`");
        btrc_Vector_string_push(lines, "Classic/Right Stick/Right = `Right Stick X+`");
        btrc_Vector_string_push(lines, "Classic/Triggers/L = `L2`");
        btrc_Vector_string_push(lines, "Classic/Triggers/R = `R2`");
        btrc_Vector_string_push(lines, "Classic/D-Pad/Up = `D-Pad Up`");
        btrc_Vector_string_push(lines, "Classic/D-Pad/Down = `D-Pad Down`");
        btrc_Vector_string_push(lines, "Classic/D-Pad/Left = `D-Pad Left`");
        btrc_Vector_string_push(lines, "Classic/D-Pad/Right = `D-Pad Right`");
    } else {
        btrc_Vector_string_push(lines, "Nunchuk/Buttons/C = `Shoulder R`");
        btrc_Vector_string_push(lines, "Nunchuk/Buttons/Z = `Trigger R`");
        btrc_Vector_string_push(lines, "Nunchuk/Stick/Up = `Axis 1-`");
        btrc_Vector_string_push(lines, "Nunchuk/Stick/Down = `Axis 1+`");
        btrc_Vector_string_push(lines, "Nunchuk/Stick/Left = `Axis 0-`");
        btrc_Vector_string_push(lines, "Nunchuk/Stick/Right = `Axis 0+`");
    }
    btrc_Vector_string_push(lines, "Rumble/Motor = Strong");
    return textLines(lines);
}

char* pcsx2ProfileText(KeymapIr* ir) {
    btrc_Vector_string* __list_433 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_433, "[Pad]");
    btrc_Vector_string_push(__list_433, "UseProfileHotkeyBindings = true");
    btrc_Vector_string_push(__list_433, "MultitapPort1 = false");
    btrc_Vector_string_push(__list_433, "MultitapPort2 = false");
    btrc_Vector_string_push(__list_433, "");
    btrc_Vector_string_push(__list_433, "[InputSources]");
    btrc_Vector_string_push(__list_433, "Keyboard = true");
    btrc_Vector_string_push(__list_433, "Mouse = true");
    btrc_Vector_string_push(__list_433, "SDL = true");
    btrc_Vector_string_push(__list_433, "DInput = false");
    btrc_Vector_string_push(__list_433, "XInput = false");
    btrc_Vector_string_push(__list_433, "");
    btrc_Vector_string_push(__list_433, "[Pad1]");
    btrc_Vector_string_push(__list_433, "Type = DualShock2");
    btrc_Vector_string_push(__list_433, "Up = SDL-1/DPadUp");
    btrc_Vector_string_push(__list_433, "Right = SDL-1/DPadRight");
    btrc_Vector_string_push(__list_433, "Down = SDL-1/DPadDown");
    btrc_Vector_string_push(__list_433, "Left = SDL-1/DPadLeft");
    btrc_Vector_string_push(__list_433, "Triangle = SDL-1/Y");
    btrc_Vector_string_push(__list_433, "Circle = SDL-1/B");
    btrc_Vector_string_push(__list_433, "Cross = SDL-1/A");
    btrc_Vector_string_push(__list_433, "Square = SDL-1/X");
    btrc_Vector_string_push(__list_433, "Select = SDL-1/Back");
    btrc_Vector_string_push(__list_433, "Start = SDL-1/Start");
    btrc_Vector_string_push(__list_433, "L1 = SDL-1/LeftShoulder");
    btrc_Vector_string_push(__list_433, "L2 = SDL-1/+LeftTrigger");
    btrc_Vector_string_push(__list_433, "R1 = SDL-1/RightShoulder");
    btrc_Vector_string_push(__list_433, "R2 = SDL-1/+RightTrigger");
    btrc_Vector_string_push(__list_433, "L3 = SDL-1/LeftStick");
    btrc_Vector_string_push(__list_433, "R3 = SDL-1/RightStick");
    btrc_Vector_string_push(__list_433, "LUp = SDL-1/-LeftY");
    btrc_Vector_string_push(__list_433, "LRight = SDL-1/+LeftX");
    btrc_Vector_string_push(__list_433, "LDown = SDL-1/+LeftY");
    btrc_Vector_string_push(__list_433, "LLeft = SDL-1/-LeftX");
    btrc_Vector_string_push(__list_433, "RUp = SDL-1/-RightY");
    btrc_Vector_string_push(__list_433, "RRight = SDL-1/+RightX");
    btrc_Vector_string_push(__list_433, "RDown = SDL-1/+RightY");
    btrc_Vector_string_push(__list_433, "RLeft = SDL-1/-RightX");
    btrc_Vector_string_push(__list_433, "");
    btrc_Vector_string_push(__list_433, "[Pad2]");
    btrc_Vector_string_push(__list_433, "Type = None");
    btrc_Vector_string_push(__list_433, "");
    btrc_Vector_string_push(__list_433, "[Hotkeys]");
    return __btrc_str_track(__btrc_strcat(textLines(__list_433), renderPcsx2Keymap(ir)));
}

char* cemuProfileText(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", "<emulated_controller>\n")), "\t<type>Wii U GamePad</type>\n")), "\t<profile>SteamInput-P1</profile>\n")), "\t<controller>\n")), "\t\t<api>SDLController</api>\n")), "\t\t<display_name>Steam Virtual Gamepad</display_name>\n")), "\t\t<rumble>0</rumble>\n")), "\t</controller>\n")), "</emulated_controller>\n"));
}

char* ryujinxProfileText(void) {
    btrc_Vector_string* __list_434 = btrc_Vector_string_new();
    btrc_Vector_string* __list_435 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_435, jsonStrField("joystick", "Left"));
    btrc_Vector_string_push(__list_435, jsonBoolField("invert_stick_x", false));
    btrc_Vector_string_push(__list_435, jsonBoolField("invert_stick_y", false));
    btrc_Vector_string_push(__list_435, jsonStrField("stick_button", "LeftStick"));
    btrc_Vector_string_push(__list_434, jsonField("left_joycon_stick", jsonObject(__list_435)));
    btrc_Vector_string* __list_436 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_436, jsonStrField("joystick", "Right"));
    btrc_Vector_string_push(__list_436, jsonBoolField("invert_stick_x", false));
    btrc_Vector_string_push(__list_436, jsonBoolField("invert_stick_y", false));
    btrc_Vector_string_push(__list_436, jsonStrField("stick_button", "RightStick"));
    btrc_Vector_string_push(__list_434, jsonField("right_joycon_stick", jsonObject(__list_436)));
    btrc_Vector_string* __list_437 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_437, jsonField("slot", "0"));
    btrc_Vector_string_push(__list_437, jsonField("alt_slot", "0"));
    btrc_Vector_string_push(__list_437, jsonBoolField("mirror_input", false));
    btrc_Vector_string_push(__list_437, jsonStrField("motion_backend", "CemuHook"));
    btrc_Vector_string_push(__list_437, jsonField("sensitivity", "100"));
    btrc_Vector_string_push(__list_437, jsonField("gyro_deadzone", "1"));
    btrc_Vector_string_push(__list_437, jsonBoolField("enable_motion", false));
    btrc_Vector_string_push(__list_434, jsonField("motion", jsonObject(__list_437)));
    btrc_Vector_string* __list_438 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_438, jsonField("strong_rumble", "1"));
    btrc_Vector_string_push(__list_438, jsonField("weak_rumble", "1"));
    btrc_Vector_string_push(__list_438, jsonBoolField("enable_rumble", true));
    btrc_Vector_string_push(__list_434, jsonField("rumble", jsonObject(__list_438)));
    btrc_Vector_string* __list_439 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_439, jsonStrField("button_minus", "Back"));
    btrc_Vector_string_push(__list_439, jsonStrField("button_l", "LeftShoulder"));
    btrc_Vector_string_push(__list_439, jsonStrField("button_zl", "LeftTrigger"));
    btrc_Vector_string_push(__list_439, jsonStrField("dpad_up", "DpadUp"));
    btrc_Vector_string_push(__list_439, jsonStrField("dpad_down", "DpadDown"));
    btrc_Vector_string_push(__list_439, jsonStrField("dpad_left", "DpadLeft"));
    btrc_Vector_string_push(__list_439, jsonStrField("dpad_right", "DpadRight"));
    btrc_Vector_string_push(__list_434, jsonField("left_joycon", jsonObject(__list_439)));
    btrc_Vector_string* __list_440 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_440, jsonStrField("button_plus", "Start"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_r", "RightShoulder"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_zr", "RightTrigger"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_x", "Y"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_b", "A"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_y", "X"));
    btrc_Vector_string_push(__list_440, jsonStrField("button_a", "B"));
    btrc_Vector_string_push(__list_434, jsonField("right_joycon", jsonObject(__list_440)));
    btrc_Vector_string_push(__list_434, jsonField("version", "1"));
    btrc_Vector_string_push(__list_434, jsonStrField("backend", "GamepadSDL2"));
    btrc_Vector_string_push(__list_434, jsonStrField("id", "0-f7390003-28de-0000-ff11-000001000000"));
    btrc_Vector_string_push(__list_434, jsonStrField("controller_type", "ProController"));
    btrc_Vector_string_push(__list_434, jsonStrField("player_index", "Player1"));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_434), "\n"));
}

void writeGeneratedManifest(char* output) {
    writeJsonPrettyFile(output, buildJson());
}

char* bundledText(char* project, char* relative, char* fallback) {
    char* root = assetRoot(project);
    char* fromRoot = joinPath(root, relative);
    if (FileSystem_isFile(fromRoot)) {
        return FileSystem_readText(fromRoot);
    }
    char* fromProject = joinPath(project, relative);
    if (FileSystem_isFile(fromProject)) {
        return FileSystem_readText(fromProject);
    }
    if (FileSystem_isFile(relative)) {
        return FileSystem_readText(relative);
    }
    return fallback;
}

char* renderBundledTemplate(char* project, char* relative, btrc_Map_string_string* values, char* fallback) {
    return JsonText_expand(bundledText(project, relative, fallback), values);
}

char* assetRoot(char* project) {
    char* configured = Environment_get("SEMU_ASSET_ROOT", "");
    if ((((int)strlen(configured)) > 0) && FileSystem_exists(joinPath(configured, "packaging/linux/bin/semu-retroarch"))) {
        return configured;
    }
    if (FileSystem_exists("packaging/linux/bin/semu-retroarch")) {
        return ".";
    }
    if (FileSystem_exists(joinPath(project, "packaging/linux/bin/semu-retroarch"))) {
        return project;
    }
    return project;
}

void seedBundledFileFromRoot(char* root, char* project, char* relative, bool executable) {
    char* source = joinPath(root, relative);
    char* target = joinPath(project, relative);
    if (strcmp(source, target) == 0) {
        return;
    }
    if (!FileSystem_isFile(source)) {
        return;
    }
    ensureDir(PathTools_dirname(target));
    FileSystem_writeText(target, FileSystem_readText(source));
    if (executable) {
        FileSystem_chmod(target, 493);
    }
}

void seedBundledFile(char* project, char* relative, bool executable) {
    seedBundledFileFromRoot(assetRoot(project), project, relative, executable);
}

void seedBundledDirFiles(char* project, char* relative, bool executable) {
    char* root = assetRoot(project);
    char* sourceDir = joinPath(root, relative);
    if (!FileSystem_isDir(sourceDir)) {
        return;
    }
    ensureDir(joinPath(project, relative));
    int __n_442 = btrc_Vector_string_iterLen(FileSystem_listDir(sourceDir));
    for (int __i_441 = 0; (__i_441 < __n_442); (__i_441++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(sourceDir), __i_441);
        char* source = joinPath(sourceDir, name);
        if (FileSystem_isFile(source)) {
            seedBundledFileFromRoot(root, project, joinPath(relative, name), executable);
        }
    }
}

char* portableShellHeader(void) {
    return "#!/usr/bin/env sh\n";
}

void writeExecutableText(char* path, char* text) {
    ensureDir(PathTools_dirname(path));
    FileSystem_writeText(path, text);
    FileSystem_chmod(path, 493);
}

char* linuxSandboxShimText(void) {
    return __btrc_str_track(__btrc_strcat(portableShellHeader(), "exec \"$(dirname \"$0\")/bin/semu-btrc\" sandbox launch \"$@\"\n"));
}

char* linuxBtrcShimText(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(portableShellHeader(), "set -eu\n")), "\n")), "here=\"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd -P)\"\n")), "cli=\"${SEMU_BIN:-${SEMU_CLI:-}}\"\n")), "\n")), "if [ -z \"$cli\" ] && [ -x \"$here/semu\" ]; then\n")), "  cli=\"$here/semu\"\n")), "fi\n")), "if [ -z \"$cli\" ] && [ -x \"$here/../semu\" ]; then\n")), "  cli=\"$here/../semu\"\n")), "fi\n")), "if [ -z \"$cli\" ] && [ -x \"$here/../../../build/semu\" ]; then\n")), "  cli=\"$here/../../../build/semu\"\n")), "fi\n")), "if [ -z \"$cli\" ] && [ -n \"${APPDIR:-}\" ] && [ -x \"$APPDIR/usr/bin/semu\" ]; then\n")), "  cli=\"$APPDIR/usr/bin/semu\"\n")), "fi\n")), "if [ -z \"$cli\" ]; then\n")), "  cli=\"$(command -v semu || true)\"\n")), "fi\n")), "\n")), "if [ -z \"$cli\" ] || [ ! -x \"$cli\" ]; then\n")), "  echo \"semu-btrc: compiled BTRC CLI not found\" >&2\n")), "  exit 127\n")), "fi\n")), "\n")), "exec \"$cli\" \"$@\"\n"));
}

char* linuxFlatpakShimText(void) {
    return __btrc_str_track(__btrc_strcat(portableShellHeader(), "exec \"$(dirname \"$0\")/semu-btrc\" launcher flatpak \"$@\"\n"));
}

char* linuxLauncherShimText(char* emulator) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(portableShellHeader(), "exec \"$(dirname \"$0\")/semu-btrc\" launcher ")), __btrc_str_track(__btrc_toLower(emulator)))), " \"$@\"\n"));
}

void seedPortableLinuxShims(char* project) {
    writeExecutableText(joinPath(project, "packaging/linux/sandbox.sh"), linuxSandboxShimText());
    writeExecutableText(joinPath(project, "packaging/linux/bin/semu-btrc"), linuxBtrcShimText());
    writeExecutableText(joinPath(project, "packaging/linux/bin/semu-flatpak"), linuxFlatpakShimText());
    int __n_444 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_443 = 0; (__i_443 < __n_444); (__i_443++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_443);
        writeExecutableText(joinPath(project, __btrc_str_track(__btrc_strcat("packaging/linux/bin/", semuLauncherName(emulator)))), linuxLauncherShimText(emulator));
    }
}

void seedLinuxAssets(char* project) {
    seedBundledFile(project, "packaging/linux/AppRun", true);
    seedBundledFile(project, "packaging/linux/semu.desktop", false);
    seedBundledFile(project, "packaging/linux/ES-DE/es_systems_linux.xml", false);
    seedBundledFile(project, "packaging/linux/ES-DE/es_find_rules_linux.xml", false);
    seedPortableLinuxShims(project);
    seedBundledDirFiles(project, "src/semu/bootstrap/templates", false);
}

void writeProfile(char* project, char* relative, char* text) {
    char* target = emulatorProfilePath(project, relative);
    ensureDir(PathTools_dirname(target));
    FileSystem_writeText(target, text);
}

void seedEmulatorDefaults(char* project) {
    seedLinuxAssets(project);
    KeymapIr* ir = projectKeymapIr(project);
    writeProfile(project, "RetroArch/retroarch.cfg.linux-backup", retroArchProfileText(project, ir));
    writeProfile(project, "RetroArch/retroarch.cfg", retroArchProfileText(project, ir));
    writeProfile(project, "Dolphin/config/Profiles/GCPad/Steam Deck.ini", dolphinGcpadProfileText());
    writeProfile(project, "Dolphin/config/Profiles/Hotkeys/Steam Deck.ini", dolphinHotkeysProfileText(ir));
    writeProfile(project, "Dolphin/config/Profiles/Wiimote/Wiimote (SD).ini", dolphinWiimoteProfileText(false));
    writeProfile(project, "Dolphin/config/Profiles/Wiimote/Wiimote + Classic Controller (SD).ini", dolphinWiimoteProfileText(true));
    writeProfile(project, "Cemu/config/controllerProfiles/SteamInput-P1.xml", cemuProfileText());
    writeProfile(project, "PCSX2/config/inputprofiles/Steam Deck.ini", pcsx2ProfileText(ir));
    writeProfile(project, "Ryujinx/config/profiles/controller/Steam Virtual Controller.json", ryujinxProfileText());
}

void seedKeymapDefaults(char* project) {
    ensureDir(keymapsRoot(project));
    char* keymapPath = keymapSourcePath(project);
    if (!FileSystem_exists(keymapPath)) {
        FileSystem_writeText(keymapPath, defaultKeymapSource());
    }
}

char* syncDefaultConfigText(char* project, char* romsDir) {
    char* roms = ((((int)strlen(romsDir)) > 0) ? normalizeRomsRoot(romsDir) : "${paths.project_roms}");
    btrc_Vector_string* __list_445 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_445, jsonBoolField("enabled", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("start_at_boot", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("tray", true));
    btrc_Vector_string_push(__list_445, jsonStrField("gui_address", "127.0.0.1:8384"));
    btrc_Vector_string_push(__list_445, jsonStrField("roms_dir", roms));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_saves", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_states", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_emulator_state", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_screenshots", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_gamelists", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_roms", false));
    btrc_Vector_string_push(__list_445, jsonBoolField("sync_bios", false));
    btrc_Vector_string_push(__list_445, jsonField("rescan_saves_s", "900"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_states_s", "900"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_emulator_state_s", "900"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_screenshots_s", "1800"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_gamelists_s", "1800"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_roms_s", "3600"));
    btrc_Vector_string_push(__list_445, jsonField("rescan_bios_s", "3600"));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_saves", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_states", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_emulator_state", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_screenshots", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_gamelists", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_roms", true));
    btrc_Vector_string_push(__list_445, jsonBoolField("watch_bios", true));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_445), "\n"));
}

void writeSyncDefaults(char* project, char* romsDir) {
    ensureDir(syncRoot(project));
    ensureDir(syncScriptsDir(project));
    ensureDir(syncLogDir(project));
    ensureDir(syncthingConfigDir(project));
    ensureDir(syncthingDataDir(project));
    char* path = syncConfigPath(project);
    if (!FileSystem_exists(path)) {
        writeJsonPrettyFile(path, syncDefaultConfigText(project, romsDir));
        return;
    }
    if (((int)strlen(romsDir)) > 0) {
        JsonObject* config = JsonObject_readFile(path);
        JsonObject_setString(config, "roms_dir", normalizeRomsRoot(romsDir));
        writeJsonPrettyFile(path, JsonObject_stringify(config));
    }
}

void ensureRomDirsAt(char* root) {
    ensureDir(root);
    int __n_447 = btrc_Vector_string_iterLen(declaredRomDirs());
    for (int __i_446 = 0; (__i_446 < __n_447); (__i_446++)) {
        char* rom = btrc_Vector_string_iterGet(declaredRomDirs(), __i_446);
        ensureDir(joinPath(root, rom));
    }
}

void ensureProjectRomDirs(char* project) {
    char* configured = configuredRomsRoot(project);
    if (strcmp(configured, romsRoot(project)) == 0) {
        ensureRomDirsAt(configured);
        return;
    }
    int __fstr_450_len = snprintf(NULL, 0, "INFO external_roms: leaving %s untouched", configured);
    char* __fstr_450_buf = __btrc_str_track(((char*)malloc((__fstr_450_len + 1))));
    snprintf(__fstr_450_buf, (__fstr_450_len + 1), "INFO external_roms: leaving %s untouched", configured);
    printf("%s\n", __fstr_450_buf);
    ensureRomDirsAt(romsRoot(project));
}

void ensureProjectContentDirs(char* project) {
    ensureDir(contentRoot(project));
    ensureProjectRomDirs(project);
    ensureDir(biosRoot(project));
    ensureDir(joinPath(contentRoot(project), "saves"));
    ensureDir(joinPath(contentRoot(project), "states"));
    ensureDir(joinPath(contentRoot(project), "screenshots"));
    ensureDir(joinPath(contentRoot(project), "downloaded_media"));
    ensureDir(joinPath(contentRoot(project), "gamelists"));
    ensureDir(joinPath(contentRoot(project), "themes"));
    ensureDir(customSystemsRoot(project));
    btrc_Vector_string* ids = declaredSystemIds();
    int __n_452 = btrc_Vector_string_iterLen(ids);
    for (int __i_451 = 0; (__i_451 < __n_452); (__i_451++)) {
        char* id = btrc_Vector_string_iterGet(ids, __i_451);
        ensureDir(joinPath(joinPath(contentRoot(project), "downloaded_media"), id));
        ensureDir(joinPath(joinPath(contentRoot(project), "gamelists"), id));
    }
    ensureDir(joinPath(biosRoot(project), "ps2"));
    ensureDir(joinPath(biosRoot(project), "switch"));
    ensureDir(joinPath(biosRoot(project), "dc"));
    ensureDir(emulatorProfilePath(project, "Cemu/data"));
    ensureDir(joinPath(project, "input/steam-input"));
}

void bootstrapSteamDeck(char* project) {
    writeSyncDefaults(project, "");
    ensureProjectContentDirs(project);
    seedKeymapDefaults(project);
    seedEmulatorDefaults(project);
    writeSteamInputTemplates(project);
    writeScreenshotDefaults(project);
    writeGeneratedManifest(joinPath(project, "semu.json"));
    writeEsDeFiles(project);
    int __fstr_455_len = snprintf(NULL, 0, "Bootstrapped Steam Deck/Linux content at %s", contentRoot(project));
    char* __fstr_455_buf = __btrc_str_track(((char*)malloc((__fstr_455_len + 1))));
    snprintf(__fstr_455_buf, (__fstr_455_len + 1), "Bootstrapped Steam Deck/Linux content at %s", contentRoot(project));
    printf("%s\n", __fstr_455_buf);
    int __fstr_458_len = snprintf(NULL, 0, "  roms:    %s", romsRoot(project));
    char* __fstr_458_buf = __btrc_str_track(((char*)malloc((__fstr_458_len + 1))));
    snprintf(__fstr_458_buf, (__fstr_458_len + 1), "  roms:    %s", romsRoot(project));
    printf("%s\n", __fstr_458_buf);
    int __fstr_461_len = snprintf(NULL, 0, "  bios:    %s", biosRoot(project));
    char* __fstr_461_buf = __btrc_str_track(((char*)malloc((__fstr_461_len + 1))));
    snprintf(__fstr_461_buf, (__fstr_461_len + 1), "  bios:    %s", biosRoot(project));
    printf("%s\n", __fstr_461_buf);
    int __fstr_464_len = snprintf(NULL, 0, "  systems: %s", joinPath(customSystemsRoot(project), "es_systems.xml"));
    char* __fstr_464_buf = __btrc_str_track(((char*)malloc((__fstr_464_len + 1))));
    snprintf(__fstr_464_buf, (__fstr_464_len + 1), "  systems: %s", joinPath(customSystemsRoot(project), "es_systems.xml"));
    printf("%s\n", __fstr_464_buf);
}

char* esLauncherBin(char* project) {
    char* configured = Environment_get("SEMU_LAUNCHER_BIN", "");
    if (((int)strlen(configured)) > 0) {
        return configured;
    }
    char* resultBin = joinPath(project, "result/bin");
    if (FileSystem_exists(joinPath(resultBin, "semu-retroarch"))) {
        return resultBin;
    }
    return linuxLauncherBin(project);
}

char* appendRetroArchCoreRule(char* rules, char* corePath) {
    if (((int)strlen(corePath)) == 0) {
        return rules;
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(rules, "      <entry>")), xmlEscape(corePath))), "</entry>\n"));
}

char* esFindRulesXml(char* project, char* launcherBin) {
    char* launcherRules = "";
    int __n_466 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_465 = 0; (__i_465 < __n_466); (__i_465++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_465);
        (launcherRules = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(launcherRules, "  <emulator name=\"")), xmlEscape(emulator))), "\">\n")), "    <rule type=\"staticpath\">\n")), "      <entry>")), xmlEscape(joinPath(launcherBin, semuLauncherName(emulator))))), "</entry>\n")), "    </rule>\n")), "  </emulator>\n")));
    }
    char* retroarchCoreRules = "";
    char* envCorePath = Environment_get("SEMU_RETROARCH_CORE_DIR", "");
    (retroarchCoreRules = appendRetroArchCoreRule(retroarchCoreRules, envCorePath));
    char* projectCorePath = joinPath(project, "result/lib/retroarch/cores");
    if (FileSystem_isDir(projectCorePath)) {
        (retroarchCoreRules = appendRetroArchCoreRule(retroarchCoreRules, projectCorePath));
    }
    int __n_468 = btrc_Vector_string_iterLen(retroarchCoreSearchPaths());
    for (int __i_467 = 0; (__i_467 < __n_468); (__i_467++)) {
        char* corePath = btrc_Vector_string_iterGet(retroarchCoreSearchPaths(), __i_467);
        if ((!(strcmp(corePath, envCorePath) == 0)) && (!(strcmp(corePath, projectCorePath) == 0))) {
            (retroarchCoreRules = appendRetroArchCoreRule(retroarchCoreRules, corePath));
        }
    }
    btrc_Map_string_string* values = btrc_Map_string_string_new();
    btrc_Map_string_string_put(values, "LAUNCHER_RULES", launcherRules);
    btrc_Map_string_string_put(values, "RETROARCH_CORE_RULES", retroarchCoreRules);
    return renderBundledTemplate(project, "src/semu/bootstrap/templates/es_find_rules.xml", values, "");
}

void writeEsDeFiles(char* project) {
    ensureDir(customSystemsRoot(project));
    SystemCatalog* catalog = systemCatalog();
    char* systemsXml = SystemCatalog_esSystemsXml(catalog);
    FileSystem_writeText(joinPath(customSystemsRoot(project), "es_systems.xml"), systemsXml);
    ensureDir(esDeProfileCustomSystemsRoot(project));
    FileSystem_writeText(joinPath(esDeProfileCustomSystemsRoot(project), "es_systems.xml"), systemsXml);
    char* packagingSystems = joinPath(linuxPackagingRoot(project), "ES-DE");
    ensureDir(packagingSystems);
    FileSystem_writeText(joinPath(packagingSystems, "es_systems_linux.xml"), systemsXml);
    char* launcherBin = esLauncherBin(project);
    FileSystem_writeText(joinPath(customSystemsRoot(project), "es_find_rules.xml"), esFindRulesXml(project, launcherBin));
    FileSystem_writeText(joinPath(project, "ES-DE/es_settings.xml"), esSettingsXmlForProject(project));
}

char* esSettingsXmlForRuntime(char* project, char* romsDir) {
    char* roms = ((((int)strlen(romsDir)) > 0) ? romsDir : configuredRomsRoot(project));
    return esSettingsXmlWithPaths(roms, joinPath(contentRoot(project), "downloaded_media"), joinPath(contentRoot(project), "themes"));
}

void writeEsDeRuntimeFiles(char* project, char* userHome) {
    char* customRoot = joinPath(userHome, "ES-DE/custom_systems");
    char* settingsRoot = joinPath(userHome, "ES-DE/settings");
    ensureDir(customRoot);
    ensureDir(settingsRoot);
    SystemCatalog* catalog = systemCatalog();
    FileSystem_writeText(joinPath(customRoot, "es_systems.xml"), SystemCatalog_esSystemsXml(catalog));
    char* launcherBin = esLauncherBin(project);
    FileSystem_writeText(joinPath(customRoot, "es_find_rules.xml"), esFindRulesXml(project, launcherBin));
    char* roms = Environment_get("SEMU_ROMS_DIR", configuredRomsRoot(project));
    FileSystem_writeText(joinPath(settingsRoot, "es_settings.xml"), esSettingsXmlForRuntime(project, roms));
}

char* steamInputKeyName(char* key) {
    if (strcmp(key, "Ctrl") == 0) {
        return "LEFT_CONTROL";
    }
    if (strcmp(key, "Alt") == 0) {
        return "LEFT_ALT";
    }
    if (strcmp(key, "Shift") == 0) {
        return "LEFT_SHIFT";
    }
    if (strcmp(key, "Meta") == 0) {
        return "LEFT_GUI";
    }
    if (strcmp(key, "Enter") == 0) {
        return "RETURN";
    }
    if (strcmp(key, "Esc") == 0) {
        return "ESCAPE";
    }
    if (strcmp(key, "Tab") == 0) {
        return "TAB";
    }
    if (strcmp(key, "+") == 0) {
        return "KEYPAD_PLUS";
    }
    if (strcmp(key, "-") == 0) {
        return "KEYPAD_DASH";
    }
    return __btrc_str_track(__btrc_toUpper(key));
}

char* steamInputActionLabel(char* id) {
    if (strcmp(id, "ui.open") == 0) {
        return "Open";
    }
    if (strcmp(id, "ui.pause") == 0) {
        return "Pause / Resume";
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        return "Screenshot";
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        return "Fullscreen";
    }
    if (strcmp(id, "ui.menu") == 0) {
        return "Menu";
    }
    if (strcmp(id, "app.quit") == 0) {
        return "Quit";
    }
    if (strcmp(id, "state.prev") == 0) {
        return "Previous State Slot";
    }
    if (strcmp(id, "state.next") == 0) {
        return "Next State Slot";
    }
    if (strcmp(id, "state.load") == 0) {
        return "Load State";
    }
    if (strcmp(id, "state.save") == 0) {
        return "Save State";
    }
    if (strcmp(id, "speed.rewind") == 0) {
        return "Rewind";
    }
    if (strcmp(id, "speed.fast") == 0) {
        return "Fast Forward";
    }
    if (strcmp(id, "screen.swap") == 0) {
        return "Swap Screens";
    }
    if (strcmp(id, "ui.escape") == 0) {
        return "Escape";
    }
    return actionLabel(id);
}

char* steamInputKeyBindings(char* command, char* label) {
    btrc_Vector_string* keys = btrc_Vector_string_new();
    char* modifiers = keymapCommandModifierPart(command);
    if (((int)strlen(modifiers)) > 0) {
        int __n_470 = btrc_Vector_string_iterLen(Strings_split(modifiers, "+"));
        for (int __i_469 = 0; (__i_469 < __n_470); (__i_469++)) {
            char* modifier = btrc_Vector_string_iterGet(Strings_split(modifiers, "+"), __i_469);
            btrc_Vector_string_push(keys, steamInputKeyName(modifier));
        }
    }
    btrc_Vector_string_push(keys, steamInputKeyName(keymapCommandKeyPart(command)));
    btrc_Vector_string* bindings = btrc_Vector_string_new();
    int __n_472 = btrc_Vector_string_iterLen(keys);
    for (int __i_471 = 0; (__i_471 < __n_472); (__i_471++)) {
        char* key = btrc_Vector_string_iterGet(keys, __i_471);
        btrc_Vector_string_push(bindings, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"key_press ", key)), ", ")), label)), ", , \"")));
    }
    return btrc_Vector_string_join(bindings, " ");
}

char* steamInputXInputBinding(char* button, char* label) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"xinput_button ", button)), ", ")), label)), ", , \""));
}

char* steamInputActionBindings(KeymapIr* ir, char* actionId) {
    if (strcmp(actionId, "app.quit") == 0) {
        char* label = steamInputActionLabel(actionId);
        return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(steamInputXInputBinding("SELECT", label), " ")), steamInputXInputBinding("START", label))), " ")), steamInputKeyBindings("Esc", label))), " ")), steamInputKeyBindings(irActionCommand(ir, actionId), label))), " ")), steamInputKeyBindings("Alt+F4", label)));
    }
    return steamInputKeyBindings(irActionCommand(ir, actionId), steamInputActionLabel(actionId));
}

char* steamInputTemplateToken(char* actionId) {
    char* token = Strings_replace(__btrc_str_track(__btrc_toUpper(actionId)), ".", "_");
    (token = Strings_replace(token, "-", "_"));
    return __btrc_str_track(__btrc_strcat(token, "_BINDINGS"));
}

void steamInputPutAction(btrc_Map_string_string* values, KeymapIr* ir, char* actionId) {
    btrc_Map_string_string_put(values, steamInputTemplateToken(actionId), steamInputActionBindings(ir, actionId));
}

char* steamInputTemplateVdf(char* title, bool full, KeymapIr* ir, char* project) {
    btrc_Map_string_string* values = btrc_Map_string_string_new();
    btrc_Map_string_string_put(values, "TITLE", title);
    btrc_Map_string_string_put(values, "RADIAL_NAME", (full ? "Semu Full Radial" : "Semu Simple Radial"));
    steamInputPutAction(values, ir, "ui.pause");
    steamInputPutAction(values, ir, "ui.screenshot");
    steamInputPutAction(values, ir, "ui.fullscreen");
    steamInputPutAction(values, ir, "ui.menu");
    steamInputPutAction(values, ir, "speed.rewind");
    steamInputPutAction(values, ir, "speed.fast");
    steamInputPutAction(values, ir, "ui.open");
    steamInputPutAction(values, ir, "ui.escape");
    steamInputPutAction(values, ir, "state.next");
    steamInputPutAction(values, ir, "state.prev");
    steamInputPutAction(values, ir, "app.quit");
    steamInputPutAction(values, ir, "state.load");
    steamInputPutAction(values, ir, "state.save");
    return renderBundledTemplate(project, "src/semu/bootstrap/templates/steam_input_template.vdf", values, "");
}

void writeSteamInputTemplates(char* project) {
    ensureDir(joinPath(project, "input/steam-input"));
    KeymapIr* ir = projectKeymapIr(project);
    FileSystem_writeText(joinPath(project, "input/steam-input/neptune-simple.vdf"), steamInputTemplateVdf("Semu: Steam Deck - Neptune SIMPLE", false, ir, project));
    FileSystem_writeText(joinPath(project, "input/steam-input/neptune-full.vdf"), steamInputTemplateVdf("Semu: Steam Deck - Neptune FULL", true, ir, project));
}

bool allPresent(char* target, btrc_Vector_string* files) {
    int __n_474 = btrc_Vector_string_iterLen(files);
    for (int __i_473 = 0; (__i_473 < __n_474); (__i_473++)) {
        char* file = btrc_Vector_string_iterGet(files, __i_473);
        if (!FileSystem_exists(joinPath(target, file))) {
            return false;
        }
    }
    return true;
}

bool anyPresent(char* target, btrc_Vector_string* files) {
    int __n_476 = btrc_Vector_string_iterLen(files);
    for (int __i_475 = 0; (__i_475 < __n_476); (__i_475++)) {
        char* file = btrc_Vector_string_iterGet(files, __i_475);
        if (FileSystem_exists(joinPath(target, file))) {
            return true;
        }
    }
    return false;
}

void reportPath(char* label, char* path) {
    char* mark = (FileSystem_isDir(path) ? "OK" : "MISSING");
    int __fstr_479_len = snprintf(NULL, 0, "  %s %s: %s", mark, label, path);
    char* __fstr_479_buf = __btrc_str_track(((char*)malloc((__fstr_479_len + 1))));
    snprintf(__fstr_479_buf, (__fstr_479_len + 1), "  %s %s: %s", mark, label, path);
    printf("%s\n", __fstr_479_buf);
}

void reportBios(char* id, char* target, btrc_Vector_string* files, bool required, bool anyMatch) {
    bool ok = (anyMatch ? anyPresent(target, files) : allPresent(target, files));
    char* mark = (ok ? "OK" : (required ? "MISSING" : "optional"));
    char* requiredText = (required ? "required" : "optional");
    int __fstr_482_len = snprintf(NULL, 0, "  %s %s (%s) -> %s", mark, id, requiredText, target);
    char* __fstr_482_buf = __btrc_str_track(((char*)malloc((__fstr_482_len + 1))));
    snprintf(__fstr_482_buf, (__fstr_482_len + 1), "  %s %s (%s) -> %s", mark, id, requiredText, target);
    printf("%s\n", __fstr_482_buf);
    if ((!ok) && required) {
        int __fstr_485_len = snprintf(NULL, 0, "       expected: %s", btrc_Vector_string_join(files, ", "));
        char* __fstr_485_buf = __btrc_str_track(((char*)malloc((__fstr_485_len + 1))));
        snprintf(__fstr_485_buf, (__fstr_485_len + 1), "       expected: %s", btrc_Vector_string_join(files, ", "));
        printf("%s\n", __fstr_485_buf);
    }
}

bool biosOkInAnyTarget(btrc_Vector_string* targets, btrc_Vector_string* files, bool anyMatch) {
    int __n_487 = btrc_Vector_string_iterLen(targets);
    for (int __i_486 = 0; (__i_486 < __n_487); (__i_486++)) {
        char* target = btrc_Vector_string_iterGet(targets, __i_486);
        bool ok = (anyMatch ? anyPresent(target, files) : allPresent(target, files));
        if (ok) {
            return true;
        }
    }
    return false;
}

void reportBiosTargets(char* id, btrc_Vector_string* targets, btrc_Vector_string* files, bool required, bool anyMatch) {
    bool ok = biosOkInAnyTarget(targets, files, anyMatch);
    char* mark = (ok ? "OK" : (required ? "MISSING" : "optional"));
    char* requiredText = (required ? "required" : "optional");
    int __fstr_490_len = snprintf(NULL, 0, "  %s %s (%s)", mark, id, requiredText);
    char* __fstr_490_buf = __btrc_str_track(((char*)malloc((__fstr_490_len + 1))));
    snprintf(__fstr_490_buf, (__fstr_490_len + 1), "  %s %s (%s)", mark, id, requiredText);
    printf("%s\n", __fstr_490_buf);
    int __n_492 = btrc_Vector_string_iterLen(targets);
    for (int __i_491 = 0; (__i_491 < __n_492); (__i_491++)) {
        char* target = btrc_Vector_string_iterGet(targets, __i_491);
        int __fstr_495_len = snprintf(NULL, 0, "       path: %s", target);
        char* __fstr_495_buf = __btrc_str_track(((char*)malloc((__fstr_495_len + 1))));
        snprintf(__fstr_495_buf, (__fstr_495_len + 1), "       path: %s", target);
        printf("%s\n", __fstr_495_buf);
    }
    if ((!ok) && required) {
        int __fstr_498_len = snprintf(NULL, 0, "       expected: %s", btrc_Vector_string_join(files, ", "));
        char* __fstr_498_buf = __btrc_str_track(((char*)malloc((__fstr_498_len + 1))));
        snprintf(__fstr_498_buf, (__fstr_498_len + 1), "       expected: %s", btrc_Vector_string_join(files, ", "));
        printf("%s\n", __fstr_498_buf);
    }
}

void reportFile(char* label, char* path) {
    char* mark = (FileSystem_exists(path) ? "OK" : "MISSING");
    int __fstr_501_len = snprintf(NULL, 0, "  %s %s: %s", mark, label, path);
    char* __fstr_501_buf = __btrc_str_track(((char*)malloc((__fstr_501_len + 1))));
    snprintf(__fstr_501_buf, (__fstr_501_len + 1), "  %s %s: %s", mark, label, path);
    printf("%s\n", __fstr_501_buf);
}

int braceBalance(char* text) {
    int balance = 0;
    for (int i = 0; (i < ((int)strlen(text))); (i++)) {
        char c = __btrc_charAt(text, i);
        if (c == '{') {
            (balance++);
        } else if (c == '}') {
            (balance--);
        }
    }
    return balance;
}

void reportSteamInputTemplate(char* label, char* path) {
    if (!FileSystem_exists(path)) {
        int __fstr_504_len = snprintf(NULL, 0, "  MISSING %s: %s", label, path);
        char* __fstr_504_buf = __btrc_str_track(((char*)malloc((__fstr_504_len + 1))));
        snprintf(__fstr_504_buf, (__fstr_504_len + 1), "  MISSING %s: %s", label, path);
        printf("%s\n", __fstr_504_buf);
        return;
    }
    char* text = FileSystem_readText(path);
    btrc_Vector_string* missing = btrc_Vector_string_new();
    if (!__btrc_strContains(text, "\"controller_mappings\"")) {
        btrc_Vector_string_push(missing, "controller_mappings");
    }
    if (!__btrc_strContains(text, "\"controller_type\"\t\t\"controller_neptune\"")) {
        btrc_Vector_string_push(missing, "controller_neptune");
    }
    if (!__btrc_strContains(text, "left_trackpad active")) {
        btrc_Vector_string_push(missing, "left_trackpad");
    }
    if (!__btrc_strContains(text, "right_trackpad active")) {
        btrc_Vector_string_push(missing, "right_trackpad");
    }
    if (!__btrc_strContains(text, "key_press LEFT_CONTROL, Save State")) {
        btrc_Vector_string_push(missing, "save_state");
    }
    if (!__btrc_strContains(text, "key_press A, Load State")) {
        btrc_Vector_string_push(missing, "load_state");
    }
    if (!__btrc_strContains(text, "xinput_button SELECT, Quit")) {
        btrc_Vector_string_push(missing, "quit_select");
    }
    if (!__btrc_strContains(text, "xinput_button START, Quit")) {
        btrc_Vector_string_push(missing, "quit_start");
    }
    if (!__btrc_strContains(text, "key_press ESCAPE, Quit")) {
        btrc_Vector_string_push(missing, "quit_escape");
    }
    if (!__btrc_strContains(text, "key_press LEFT_ALT, Quit")) {
        btrc_Vector_string_push(missing, "quit_alt");
    }
    if (!__btrc_strContains(text, "key_press F4, Quit")) {
        btrc_Vector_string_push(missing, "quit_f4");
    }
    if (__btrc_strContains(text, "\"mode\" \"gyro") || __btrc_strContains(text, "gyro_")) {
        btrc_Vector_string_push(missing, "gyro_opt_in_policy");
    }
    int balance = braceBalance(text);
    if (balance != 0) {
        int __fstr_506_len = snprintf(NULL, 0, "brace_balance=%d", balance);
        char* __fstr_506_buf = __btrc_str_track(((char*)malloc((__fstr_506_len + 1))));
        snprintf(__fstr_506_buf, (__fstr_506_len + 1), "brace_balance=%d", balance);
        btrc_Vector_string_push(missing, __fstr_506_buf);
    }
    if (missing->len == 0) {
        int __fstr_509_len = snprintf(NULL, 0, "  OK %s: controller_neptune, trackpads, save/load/quit", label);
        char* __fstr_509_buf = __btrc_str_track(((char*)malloc((__fstr_509_len + 1))));
        snprintf(__fstr_509_buf, (__fstr_509_len + 1), "  OK %s: controller_neptune, trackpads, save/load/quit", label);
        printf("%s\n", __fstr_509_buf);
    } else {
        int __fstr_512_len = snprintf(NULL, 0, "  INVALID %s: %s", label, btrc_Vector_string_join(missing, ", "));
        char* __fstr_512_buf = __btrc_str_track(((char*)malloc((__fstr_512_len + 1))));
        snprintf(__fstr_512_buf, (__fstr_512_len + 1), "  INVALID %s: %s", label, btrc_Vector_string_join(missing, ", "));
        printf("%s\n", __fstr_512_buf);
    }
}

void doctorSteamDeck(char* project) {
    printf("%s\n", "Semu doctor (BTRC)");
    printf("%s\n", "");
    printf("%s\n", "Paths");
    reportPath("project_roms", romsRoot(project));
    reportPath("configured_roms", configuredRomsRoot(project));
    reportPath("project_bios", biosRoot(project));
    reportPath("project_saves", joinPath(contentRoot(project), "saves"));
    reportPath("project_states", joinPath(contentRoot(project), "states"));
    reportPath("project_media", joinPath(contentRoot(project), "downloaded_media"));
    reportPath("project_gamelists", joinPath(contentRoot(project), "gamelists"));
    printf("%s\n", "");
    printf("%s\n", "BIOS / firmware");
    char* externalRoot = configuredEmulationRoot(project);
    btrc_Vector_string* __list_515 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_515, biosRoot(project));
    btrc_Vector_string_push(__list_515, joinPath(externalRoot, "RetroArch/config/system"));
    btrc_Vector_string* __list_516 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_516, "scph5500.bin");
    btrc_Vector_string_push(__list_516, "scph5501.bin");
    btrc_Vector_string_push(__list_516, "scph5502.bin");
    reportBiosTargets("psx", __list_515, __list_516, true, false);
    btrc_Vector_string* __list_519 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_519, joinPath(biosRoot(project), "ps2"));
    btrc_Vector_string_push(__list_519, joinPath(externalRoot, "PCSX2/config/bios"));
    btrc_Vector_string* __list_520 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_520, "ps2-0230a-20080220.bin");
    btrc_Vector_string_push(__list_520, "ps2-0230e-20080220.bin");
    btrc_Vector_string_push(__list_520, "ps2-0230j-20080220.bin");
    btrc_Vector_string_push(__list_520, "SCPH-10000.bin");
    btrc_Vector_string_push(__list_520, "SCPH-30004R.bin");
    btrc_Vector_string_push(__list_520, "SCPH-39001.bin");
    btrc_Vector_string_push(__list_520, "SCPH-70012.bin");
    btrc_Vector_string_push(__list_520, "SCPH-90006.bin");
    reportBiosTargets("ps2", __list_519, __list_520, true, true);
    btrc_Vector_string* __list_523 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_523, joinPath(biosRoot(project), "switch"));
    btrc_Vector_string_push(__list_523, joinPath(externalRoot, "Ryujinx/config/system"));
    btrc_Vector_string* __list_524 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_524, "prod.keys");
    btrc_Vector_string_push(__list_524, "title.keys");
    reportBiosTargets("switch_keys", __list_523, __list_524, true, false);
    btrc_Vector_string* __list_527 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_527, emulatorProfilePath(project, "Cemu/data"));
    btrc_Vector_string_push(__list_527, joinPath(externalRoot, "Cemu/data"));
    btrc_Vector_string* __list_528 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_528, "keys.txt");
    reportBiosTargets("wiiu_keys", __list_527, __list_528, true, false);
    btrc_Vector_string* __list_531 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_531, joinPath(biosRoot(project), "dc"));
    btrc_Vector_string_push(__list_531, joinPath(externalRoot, "RetroArch/config/system"));
    btrc_Vector_string_push(__list_531, joinPath(externalRoot, "Flycast/data"));
    btrc_Vector_string* __list_532 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_532, "dc_boot.bin");
    btrc_Vector_string_push(__list_532, "dc_flash.bin");
    reportBiosTargets("dreamcast", __list_531, __list_532, false, false);
    printf("%s\n", "");
    printf("%s\n", "Controller profiles");
    bool controllersOk = true;
    int __n_534 = btrc_Vector_string_iterLen(controllerProfileFiles());
    for (int __i_533 = 0; (__i_533 < __n_534); (__i_533++)) {
        char* profile = btrc_Vector_string_iterGet(controllerProfileFiles(), __i_533);
        if (!FileSystem_exists(joinPath(project, profile))) {
            (controllersOk = false);
            int __fstr_537_len = snprintf(NULL, 0, "  MISSING %s", profile);
            char* __fstr_537_buf = __btrc_str_track(((char*)malloc((__fstr_537_len + 1))));
            snprintf(__fstr_537_buf, (__fstr_537_len + 1), "  MISSING %s", profile);
            printf("%s\n", __fstr_537_buf);
        }
    }
    if (controllersOk) {
        printf("%s\n", "  OK steam_deck");
    }
    printf("%s\n", "");
    printf("%s\n", "Steam Deck defaults");
    printf("%s\n", "  OK gyro: disabled");
    printf("%s\n", "  OK right_trackpad: mouse");
    printf("%s\n", "  OK left_trackpad: radial_hotkeys");
    printf("%s\n", "  OK hotkeys: HKB+L1 load, HKB+R1 save, HKB+Start quit");
    printf("%s\n", "");
    printf("%s\n", "Input abstraction");
    printf("%s\n", "  OK layers: controller_model -> emulation_backend -> emitted_input -> emulator_keymap");
    printf("%s\n", "  OK backends: uinput, evemu, uhid, inputplumber, steam_input");
    printf("%s\n", "  OK controller_models: steam_deck, steam_controller, xbox_xinput, dualshock4, dualsense, switch_pro");
    printf("%s\n", "  OK steam_deck verification: linux_virtual_input, deck_route, steam_deck_game_mode");
    char* keymapPath = keymapSourcePath(project);
    char* keymapSource = (FileSystem_exists(keymapPath) ? FileSystem_readText(keymapPath) : defaultKeymapSource());
    KeymapErrors* errors = KeymapErrors_new();
    compileKeymap(keymapSource, errors);
    printf("%s\n", "");
    printf("%s\n", "Keymap compiler");
    if (KeymapErrors_count(errors) == 0) {
        int __fstr_540_len = snprintf(NULL, 0, "  OK steam_deck: %s", keymapPath);
        char* __fstr_540_buf = __btrc_str_track(((char*)malloc((__fstr_540_len + 1))));
        snprintf(__fstr_540_buf, (__fstr_540_len + 1), "  OK steam_deck: %s", keymapPath);
        printf("%s\n", __fstr_540_buf);
    } else {
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_543_len = snprintf(NULL, 0, "  %s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_543_buf = __btrc_str_track(((char*)malloc((__fstr_543_len + 1))));
            snprintf(__fstr_543_buf, (__fstr_543_len + 1), "  %s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_543_buf);
        }
    }
    printf("%s\n", "");
    printf("%s\n", "Steam Input templates");
    reportSteamInputTemplate("neptune_simple", joinPath(project, "input/steam-input/neptune-simple.vdf"));
    reportSteamInputTemplate("neptune_full", joinPath(project, "input/steam-input/neptune-full.vdf"));
    printf("%s\n", "");
    printf("%s\n", "ROM preflight");
    reportN3dsRomPreflight(project);
    doctorScreenshotHooks(project);
    doctorSync(project);
    printf("%s\n", "");
    printf("%s\n", "linux launchers");
    int __n_545 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_544 = 0; (__i_544 < __n_545); (__i_544++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_544);
        reportFile(emulator, joinPath(linuxLauncherBin(project), semuLauncherName(emulator)));
    }
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

void BinaryEditor_init(BinaryEditor* self, char* path, char* mode) {
    self->__rc = 1;
    (self->handle = fopen(path, mode));
    (self->opened = (self->handle != NULL));
}

BinaryEditor* BinaryEditor_new(char* path, char* mode) {
    BinaryEditor* self = ((BinaryEditor*)malloc(sizeof(BinaryEditor)));
    memset(self, 0, sizeof(BinaryEditor));
    BinaryEditor_init(self, path, mode);
    return self;
}

void BinaryEditor_destroy(BinaryEditor* self) {
    BinaryEditor_close(self);
    free(self);
}

bool BinaryEditor_ok(BinaryEditor* self) {
    return self->opened;
}

bool BinaryEditor_seek(BinaryEditor* self, long offset) {
    if (!self->opened) {
        return false;
    }
    return (fseek(self->handle, offset, SEEK_SET) == 0);
}

int BinaryEditor_readU8(BinaryEditor* self, long offset) {
    if (!BinaryEditor_seek(self, offset)) {
        return (-1);
    }
    unsigned char buf[1];
    size_t n = fread(buf, 1, 1, self->handle);
    if (n != 1) {
        return (-1);
    }
    return ((int)buf[0]);
}

long BinaryEditor_readLe32(BinaryEditor* self, long offset) {
    if (!BinaryEditor_seek(self, offset)) {
        return (-1);
    }
    unsigned char buf[4];
    size_t n = fread(buf, 1, 4, self->handle);
    if (n != 4) {
        return (-1);
    }
    return (((((long)buf[0]) + (((long)buf[1]) * 256)) + (((long)buf[2]) * 65536)) + (((long)buf[3]) * 16777216));
}

char* BinaryEditor_readAscii(BinaryEditor* self, long offset, int count) {
    if (!BinaryEditor_seek(self, offset)) {
        return "";
    }
    char* buf = ((char*)malloc((count + 1)));
    size_t n = fread(buf, 1, count, self->handle);
    (buf[n] = '\0');
    return buf;
}

bool BinaryEditor_writeU8(BinaryEditor* self, long offset, int value) {
    if (!BinaryEditor_seek(self, offset)) {
        return false;
    }
    unsigned char buf[1];
    (buf[0] = ((unsigned char)value));
    size_t n = fwrite(buf, 1, 1, self->handle);
    return (n == 1);
}

bool BinaryEditor_writeLe32(BinaryEditor* self, long offset, long value) {
    if (!BinaryEditor_seek(self, offset)) {
        return false;
    }
    unsigned char buf[4];
    (buf[0] = ((unsigned char)__btrc_mod_int(value, 256)));
    (buf[1] = ((unsigned char)__btrc_mod_int(__btrc_div_int(value, 256), 256)));
    (buf[2] = ((unsigned char)__btrc_mod_int(__btrc_div_int(value, 65536), 256)));
    (buf[3] = ((unsigned char)__btrc_mod_int(__btrc_div_int(value, 16777216), 256)));
    size_t n = fwrite(buf, 1, 4, self->handle);
    return (n == 4);
}

bool BinaryEditor_writeAscii(BinaryEditor* self, long offset, char* text) {
    if (!BinaryEditor_seek(self, offset)) {
        return false;
    }
    size_t n = fwrite(text, 1, ((int)strlen(text)), self->handle);
    return (n == ((size_t)((int)strlen(text))));
}

void BinaryEditor_close(BinaryEditor* self) {
    if (self->opened) {
        fclose(self->handle);
        (self->opened = false);
    }
}

void BinaryReader_init(BinaryReader* self, char* path) {
    self->__rc = 1;
    (self->handle = fopen(path, "rb"));
    (self->opened = (self->handle != NULL));
}

BinaryReader* BinaryReader_new(char* path) {
    BinaryReader* self = ((BinaryReader*)malloc(sizeof(BinaryReader)));
    memset(self, 0, sizeof(BinaryReader));
    BinaryReader_init(self, path);
    return self;
}

void BinaryReader_destroy(BinaryReader* self) {
    BinaryReader_close(self);
    free(self);
}

bool BinaryReader_ok(BinaryReader* self) {
    return self->opened;
}

bool BinaryReader_seek(BinaryReader* self, long offset) {
    if (!self->opened) {
        return false;
    }
    return (fseek(self->handle, offset, SEEK_SET) == 0);
}

int BinaryReader_readU8(BinaryReader* self, long offset) {
    if (!BinaryReader_seek(self, offset)) {
        return (-1);
    }
    unsigned char buf[1];
    size_t n = fread(buf, 1, 1, self->handle);
    if (n != 1) {
        return (-1);
    }
    return ((int)buf[0]);
}

long BinaryReader_readLe32(BinaryReader* self, long offset) {
    if (!BinaryReader_seek(self, offset)) {
        return (-1);
    }
    unsigned char buf[4];
    size_t n = fread(buf, 1, 4, self->handle);
    if (n != 4) {
        return (-1);
    }
    return (((((long)buf[0]) + (((long)buf[1]) * 256)) + (((long)buf[2]) * 65536)) + (((long)buf[3]) * 16777216));
}

char* BinaryReader_readAscii(BinaryReader* self, long offset, int count) {
    if (!BinaryReader_seek(self, offset)) {
        return "";
    }
    char* buf = ((char*)malloc((count + 1)));
    size_t n = fread(buf, 1, count, self->handle);
    (buf[n] = '\0');
    return buf;
}

void BinaryReader_close(BinaryReader* self) {
    if (self->opened) {
        fclose(self->handle);
        (self->opened = false);
    }
}

void N3dsRomCheck_init(N3dsRomCheck* self) {
    self->__rc = 1;
    (self->status = "UNKNOWN");
    (self->note = "");
    (self->partitions = 0);
}

N3dsRomCheck* N3dsRomCheck_new(void) {
    N3dsRomCheck* self = ((N3dsRomCheck*)malloc(sizeof(N3dsRomCheck)));
    memset(self, 0, sizeof(N3dsRomCheck));
    N3dsRomCheck_init(self);
    return self;
}

void N3dsRomCheck_destroy(N3dsRomCheck* self) {
    free(self);
}

bool n3dsRomName(char* name) {
    char* lower = __btrc_str_track(__btrc_toLower(name));
    return (__btrc_endsWith(lower, ".3ds") || __btrc_endsWith(lower, ".cci"));
}

bool n3dsArchiveName(char* name) {
    char* lower = __btrc_str_track(__btrc_toLower(name));
    return (__btrc_endsWith(lower, ".zip") || __btrc_endsWith(lower, ".7z"));
}

bool exefsLooksDecrypted(char* text) {
    return ((__btrc_startsWith(text, ".code") || __btrc_startsWith(text, "icon")) || __btrc_startsWith(text, "banner"));
}

N3dsRomCheck* checkN3dsRom(char* path) {
    N3dsRomCheck* result = N3dsRomCheck_new();
    BinaryReader* reader = BinaryReader_new(path);
    if (!BinaryReader_ok(reader)) {
        (result->status = "INVALID");
        (result->note = "cannot open");
        if (reader != NULL) {
            if ((--reader->__rc) <= 0) {
                BinaryReader_destroy(reader);
            }
        }
        return result;
    }
    if (!(strcmp(BinaryReader_readAscii(reader, 0x100, 4), "NCSD") == 0)) {
        (result->status = "INVALID");
        (result->note = "missing NCSD header");
        if (reader != NULL) {
            if ((--reader->__rc) <= 0) {
                BinaryReader_destroy(reader);
            }
        }
        return result;
    }
    bool sawNoCrypto = false;
    bool sawNeedsFix = false;
    bool sawEncrypted = false;
    bool sawInvalidPartition = false;
    for (int i = 0; (i < 8); (i++)) {
        long entry = (0x120 + (i * 8));
        long offsetMu = BinaryReader_readLe32(reader, entry);
        long sizeMu = BinaryReader_readLe32(reader, (entry + 4));
        if ((offsetMu < 0) || (sizeMu < 0)) {
            (sawInvalidPartition = true);
        } else if (sizeMu > 0) {
            long partOffset = (offsetMu * 0x200);
            if (!(strcmp(BinaryReader_readAscii(reader, (partOffset + 0x100), 4), "NCCH") == 0)) {
                (sawInvalidPartition = true);
            } else {
                (result->partitions++);
                int flags = BinaryReader_readU8(reader, ((partOffset + 0x100) + 0x8f));
                bool noCrypto = (__btrc_mod_int(__btrc_div_int(flags, 4), 2) == 1);
                if (noCrypto) {
                    (sawNoCrypto = true);
                } else {
                    long exefsOffsetMu = BinaryReader_readLe32(reader, ((partOffset + 0x100) + 0xa0));
                    char* exefs = ((exefsOffsetMu > 0) ? BinaryReader_readAscii(reader, (partOffset + (exefsOffsetMu * 0x200)), 8) : "");
                    if (exefsLooksDecrypted(exefs)) {
                        (sawNeedsFix = true);
                    } else {
                        (sawEncrypted = true);
                    }
                }
            }
        }
    }
    if (result->partitions == 0) {
        (result->status = "INVALID");
        (result->note = (sawInvalidPartition ? "no valid NCCH partitions" : "no NCSD partitions"));
    } else if (sawEncrypted) {
        (result->status = "ENCRYPTED");
        (result->note = "Azahar will reject encrypted dumps; provide decrypted/NoCrypto content");
    } else if (sawNeedsFix) {
        (result->status = "NEEDS_FIX");
        (result->note = "decrypted content is missing NoCrypto flags");
    } else if (sawNoCrypto) {
        (result->status = "OK");
        int __fstr_546_len = snprintf(NULL, 0, "%d NoCrypto partition(s)", result->partitions);
        char* __fstr_546_buf = __btrc_str_track(((char*)malloc((__fstr_546_len + 1))));
        snprintf(__fstr_546_buf, (__fstr_546_len + 1), "%d NoCrypto partition(s)", result->partitions);
        (result->note = __fstr_546_buf);
    } else {
        (result->status = "UNKNOWN");
        (result->note = "could not classify NCCH encryption flags");
    }
    if (reader != NULL) {
        if ((--reader->__rc) <= 0) {
            BinaryReader_destroy(reader);
        }
    }
    return result;
    if (reader != NULL) {
        if ((--reader->__rc) <= 0) {
            BinaryReader_destroy(reader);
        }
    }
    if (result != NULL) {
        if ((--result->__rc) <= 0) {
            N3dsRomCheck_destroy(result);
        }
    }
}

btrc_Vector_string* n3dsInputFiles(char* input) {
    btrc_Vector_string* files = btrc_Vector_string_new();
    if (FileSystem_isDir(input)) {
        int __n_548 = btrc_Vector_string_iterLen(FileSystem_listDir(input));
        for (int __i_547 = 0; (__i_547 < __n_548); (__i_547++)) {
            char* name = btrc_Vector_string_iterGet(FileSystem_listDir(input), __i_547);
            if (n3dsRomName(name)) {
                btrc_Vector_string_push(files, joinPath(input, name));
            }
        }
        btrc_Vector_string_sort(files);
    } else if (FileSystem_isFile(input) && n3dsRomName(input)) {
        btrc_Vector_string_push(files, input);
    }
    return files;
}

bool copyFilePath(char* source, char* destination) {
    ensureDir(PathTools_dirname(destination));
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, Command_check(Command_arg(Command_arg(Command_arg(Command_new("cp"), "-p"), source), destination), false));
    bool __btrc_ret_549 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_549;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool n3dsNoCryptoFlag(int flags) {
    return (__btrc_mod_int(__btrc_div_int(flags, 4), 2) == 1);
}

int patchN3dsNoCryptoFlags(char* path) {
    BinaryEditor* editor = BinaryEditor_new(path, "r+b");
    if (!BinaryEditor_ok(editor)) {
        int __btrc_ret_550 = (-1);
        if (editor != NULL) {
            if ((--editor->__rc) <= 0) {
                BinaryEditor_destroy(editor);
            }
        }
        return __btrc_ret_550;
    }
    if (!(strcmp(BinaryEditor_readAscii(editor, 0x100, 4), "NCSD") == 0)) {
        BinaryEditor_close(editor);
        int __btrc_ret_551 = (-1);
        if (editor != NULL) {
            if ((--editor->__rc) <= 0) {
                BinaryEditor_destroy(editor);
            }
        }
        return __btrc_ret_551;
    }
    int patched = 0;
    for (int i = 0; (i < 8); (i++)) {
        long entry = (0x120 + (i * 8));
        long offsetMu = BinaryEditor_readLe32(editor, entry);
        long sizeMu = BinaryEditor_readLe32(editor, (entry + 4));
        if ((offsetMu < 0) || (sizeMu <= 0)) {
            continue;
        }
        long partOffset = (offsetMu * 0x200);
        if (!(strcmp(BinaryEditor_readAscii(editor, (partOffset + 0x100), 4), "NCCH") == 0)) {
            continue;
        }
        long flagsOffset = ((partOffset + 0x100) + 0x8f);
        int flags = BinaryEditor_readU8(editor, flagsOffset);
        if (flags < 0) {
            BinaryEditor_close(editor);
            int __btrc_ret_552 = (-1);
            if (editor != NULL) {
                if ((--editor->__rc) <= 0) {
                    BinaryEditor_destroy(editor);
                }
            }
            return __btrc_ret_552;
        }
        if (!n3dsNoCryptoFlag(flags)) {
            if (!BinaryEditor_writeU8(editor, flagsOffset, (flags + 4))) {
                BinaryEditor_close(editor);
                int __btrc_ret_553 = (-1);
                if (editor != NULL) {
                    if ((--editor->__rc) <= 0) {
                        BinaryEditor_destroy(editor);
                    }
                }
                return __btrc_ret_553;
            }
            if (!BinaryEditor_writeU8(editor, ((partOffset + 0x100) + 0x8b), 0)) {
                BinaryEditor_close(editor);
                int __btrc_ret_554 = (-1);
                if (editor != NULL) {
                    if ((--editor->__rc) <= 0) {
                        BinaryEditor_destroy(editor);
                    }
                }
                return __btrc_ret_554;
            }
            (patched++);
        }
    }
    BinaryEditor_close(editor);
    if (editor != NULL) {
        if ((--editor->__rc) <= 0) {
            BinaryEditor_destroy(editor);
        }
    }
    return patched;
    if (editor != NULL) {
        if ((--editor->__rc) <= 0) {
            BinaryEditor_destroy(editor);
        }
    }
}

bool fixN3dsNoCryptoFile(char* input, char* output) {
    if (!copyFilePath(input, output)) {
        return false;
    }
    int patched = patchN3dsNoCryptoFlags(output);
    if (patched < 0) {
        removePath(output);
        return false;
    }
    return true;
}

void reportN3dsRomPreflight(char* project) {
    char* n3dsDir = joinPath(romsRoot(project), "n3ds");
    if (!FileSystem_isDir(n3dsDir)) {
        int __fstr_557_len = snprintf(NULL, 0, "  MISSING n3ds rom dir: %s", n3dsDir);
        char* __fstr_557_buf = __btrc_str_track(((char*)malloc((__fstr_557_len + 1))));
        snprintf(__fstr_557_buf, (__fstr_557_len + 1), "  MISSING n3ds rom dir: %s", n3dsDir);
        printf("%s\n", __fstr_557_buf);
        return;
    }
    btrc_Vector_string* entries = FileSystem_listDir(n3dsDir);
    int checked = 0;
    int archives = 0;
    int __n_559 = btrc_Vector_string_iterLen(entries);
    for (int __i_558 = 0; (__i_558 < __n_559); (__i_558++)) {
        char* name = btrc_Vector_string_iterGet(entries, __i_558);
        char* path = joinPath(n3dsDir, name);
        if (FileSystem_isFile(path) && n3dsRomName(name)) {
            (checked++);
            N3dsRomCheck* result = checkN3dsRom(path);
            int __fstr_562_len = snprintf(NULL, 0, "  %s n3ds/%s: %s", result->status, name, result->note);
            char* __fstr_562_buf = __btrc_str_track(((char*)malloc((__fstr_562_len + 1))));
            snprintf(__fstr_562_buf, (__fstr_562_len + 1), "  %s n3ds/%s: %s", result->status, name, result->note);
            printf("%s\n", __fstr_562_buf);
        } else if (FileSystem_isFile(path) && n3dsArchiveName(name)) {
            (archives++);
        }
    }
    if (checked == 0) {
        printf("%s\n", "  OK n3ds: no top-level .3ds/.cci files to preflight");
    }
    if (archives > 0) {
        int __fstr_565_len = snprintf(NULL, 0, "  WARN n3ds archives unchecked: %d zip/7z file(s)", archives);
        char* __fstr_565_buf = __btrc_str_track(((char*)malloc((__fstr_565_len + 1))));
        snprintf(__fstr_565_buf, (__fstr_565_len + 1), "  WARN n3ds archives unchecked: %d zip/7z file(s)", archives);
        printf("%s\n", __fstr_565_buf);
    }
}

char* n3dsNoCryptoInputArg(CliArgs* args, int startIndex) {
    int i = startIndex;
    while (i < CliArgs_count(args)) {
        char* value = CliArgs_get(args, i);
        if ((strcmp(value, "-o") == 0) || (strcmp(value, "--output") == 0)) {
            (i = (i + 2));
        } else if (__btrc_startsWith(value, "--")) {
            (i = (i + 1));
        } else {
            return value;
        }
    }
    return "";
}

void printN3dsNoCryptoCheckSummary(int total, int ok, int needsFix, int encrypted, int invalid, int unknown) {
    printf("%s\n", "");
    int __fstr_568_len = snprintf(NULL, 0, "Summary: %d files", total);
    char* __fstr_568_buf = __btrc_str_track(((char*)malloc((__fstr_568_len + 1))));
    snprintf(__fstr_568_buf, (__fstr_568_len + 1), "Summary: %d files", total);
    printf("%s\n", __fstr_568_buf);
    int __fstr_571_len = snprintf(NULL, 0, "  Already OK (NoCrypto set):    %d", ok);
    char* __fstr_571_buf = __btrc_str_track(((char*)malloc((__fstr_571_len + 1))));
    snprintf(__fstr_571_buf, (__fstr_571_len + 1), "  Already OK (NoCrypto set):    %d", ok);
    printf("%s\n", __fstr_571_buf);
    int __fstr_574_len = snprintf(NULL, 0, "  Needs fix (flag missing):     %d", needsFix);
    char* __fstr_574_buf = __btrc_str_track(((char*)malloc((__fstr_574_len + 1))));
    snprintf(__fstr_574_buf, (__fstr_574_len + 1), "  Needs fix (flag missing):     %d", needsFix);
    printf("%s\n", __fstr_574_buf);
    int __fstr_577_len = snprintf(NULL, 0, "  Truly encrypted (need keys):  %d", encrypted);
    char* __fstr_577_buf = __btrc_str_track(((char*)malloc((__fstr_577_len + 1))));
    snprintf(__fstr_577_buf, (__fstr_577_len + 1), "  Truly encrypted (need keys):  %d", encrypted);
    printf("%s\n", __fstr_577_buf);
    int __fstr_580_len = snprintf(NULL, 0, "  Invalid/errors:               %d", invalid);
    char* __fstr_580_buf = __btrc_str_track(((char*)malloc((__fstr_580_len + 1))));
    snprintf(__fstr_580_buf, (__fstr_580_len + 1), "  Invalid/errors:               %d", invalid);
    printf("%s\n", __fstr_580_buf);
    if (unknown > 0) {
        int __fstr_583_len = snprintf(NULL, 0, "  Unknown:                      %d", unknown);
        char* __fstr_583_buf = __btrc_str_track(((char*)malloc((__fstr_583_len + 1))));
        snprintf(__fstr_583_buf, (__fstr_583_len + 1), "  Unknown:                      %d", unknown);
        printf("%s\n", __fstr_583_buf);
    }
}

int n3dsNoCryptoCommand(CliArgs* args, int startIndex) {
    char* input = n3dsNoCryptoInputArg(args, startIndex);
    if (((int)strlen(input)) == 0) {
        printf("%s\n", "error 0:0 n3ds-nocrypto needs an input .3ds/.cci file or directory");
        return 1;
    }
    btrc_Vector_string* files = n3dsInputFiles(input);
    if (files->len == 0) {
        printf("%s\n", "error 0:0 no .3ds/.cci files found");
        return 1;
    }
    if (CliArgs_has(args, "--check")) {
        int ok = 0;
        int needsFix = 0;
        int encrypted = 0;
        int invalid = 0;
        int unknown = 0;
        int __n_585 = btrc_Vector_string_iterLen(files);
        for (int __i_584 = 0; (__i_584 < __n_585); (__i_584++)) {
            char* file = btrc_Vector_string_iterGet(files, __i_584);
            N3dsRomCheck* result = checkN3dsRom(file);
            char* name = PathTools_basename(file);
            if (strcmp(result->status, "OK") == 0) {
                int __fstr_588_len = snprintf(NULL, 0, "  OK:        %s", name);
                char* __fstr_588_buf = __btrc_str_track(((char*)malloc((__fstr_588_len + 1))));
                snprintf(__fstr_588_buf, (__fstr_588_len + 1), "  OK:        %s", name);
                printf("%s\n", __fstr_588_buf);
                (ok++);
            } else if (strcmp(result->status, "NEEDS_FIX") == 0) {
                int __fstr_591_len = snprintf(NULL, 0, "  NEEDS FIX: %s", name);
                char* __fstr_591_buf = __btrc_str_track(((char*)malloc((__fstr_591_len + 1))));
                snprintf(__fstr_591_buf, (__fstr_591_len + 1), "  NEEDS FIX: %s", name);
                printf("%s\n", __fstr_591_buf);
                (needsFix++);
            } else if (strcmp(result->status, "ENCRYPTED") == 0) {
                int __fstr_594_len = snprintf(NULL, 0, "  ENCRYPTED: %s (truly encrypted, cannot fix with flag flip)", name);
                char* __fstr_594_buf = __btrc_str_track(((char*)malloc((__fstr_594_len + 1))));
                snprintf(__fstr_594_buf, (__fstr_594_len + 1), "  ENCRYPTED: %s (truly encrypted, cannot fix with flag flip)", name);
                printf("%s\n", __fstr_594_buf);
                (encrypted++);
            } else if (strcmp(result->status, "INVALID") == 0) {
                int __fstr_597_len = snprintf(NULL, 0, "  ERROR:     %s: %s", name, result->note);
                char* __fstr_597_buf = __btrc_str_track(((char*)malloc((__fstr_597_len + 1))));
                snprintf(__fstr_597_buf, (__fstr_597_len + 1), "  ERROR:     %s: %s", name, result->note);
                printf("%s\n", __fstr_597_buf);
                (invalid++);
            } else {
                int __fstr_600_len = snprintf(NULL, 0, "  UNKNOWN:   %s: %s", name, result->note);
                char* __fstr_600_buf = __btrc_str_track(((char*)malloc((__fstr_600_len + 1))));
                snprintf(__fstr_600_buf, (__fstr_600_len + 1), "  UNKNOWN:   %s: %s", name, result->note);
                printf("%s\n", __fstr_600_buf);
                (unknown++);
            }
        }
        printN3dsNoCryptoCheckSummary(files->len, ok, needsFix, encrypted, invalid, unknown);
        return ((invalid > 0) ? 1 : 0);
    }
    char* outputDir = CliArgs_valueAfter(args, "--output", CliArgs_valueAfter(args, "-o", ""));
    if (((int)strlen(outputDir)) == 0) {
        printf("%s\n", "error 0:0 n3ds-nocrypto fix mode needs -o/--output DIR");
        return 1;
    }
    ensureDir(outputDir);
    int fixed = 0;
    int copied = 0;
    int failed = 0;
    for (int i = 0; (i < files->len); (i++)) {
        char* file = btrc_Vector_string_get(files, i);
        char* name = PathTools_basename(file);
        char* output = joinPath(outputDir, name);
        int __fstr_603_len = snprintf(NULL, 0, "[%d/%d] %s", (i + 1), files->len, name);
        char* __fstr_603_buf = __btrc_str_track(((char*)malloc((__fstr_603_len + 1))));
        snprintf(__fstr_603_buf, (__fstr_603_len + 1), "[%d/%d] %s", (i + 1), files->len, name);
        printf("%s\n", __fstr_603_buf);
        N3dsRomCheck* result = checkN3dsRom(file);
        if (strcmp(result->status, "OK") == 0) {
            printf("%s\n", "  Already OK, copying as-is");
            if (copyFilePath(file, output)) {
                (copied++);
            } else {
                printf("%s\n", "  FAILED");
                (failed++);
            }
        } else if (strcmp(result->status, "NEEDS_FIX") == 0) {
            printf("%s\n", "  Fixing NoCrypto flag...");
            if (fixN3dsNoCryptoFile(file, output)) {
                printf("%s\n", "  Done");
                (fixed++);
            } else {
                printf("%s\n", "  FAILED");
                (failed++);
            }
        } else if (strcmp(result->status, "ENCRYPTED") == 0) {
            printf("%s\n", "  Truly encrypted, skipping (needs full decryption)");
            (failed++);
        } else if (strcmp(result->status, "INVALID") == 0) {
            int __fstr_606_len = snprintf(NULL, 0, "  ERROR: %s", result->note);
            char* __fstr_606_buf = __btrc_str_track(((char*)malloc((__fstr_606_len + 1))));
            snprintf(__fstr_606_buf, (__fstr_606_len + 1), "  ERROR: %s", result->note);
            printf("%s\n", __fstr_606_buf);
            (failed++);
        } else {
            printf("%s\n", "  Unknown state, copying as-is");
            if (copyFilePath(file, output)) {
                (copied++);
            } else {
                printf("%s\n", "  FAILED");
                (failed++);
            }
        }
    }
    printf("%s\n", "");
    int __fstr_609_len = snprintf(NULL, 0, "Done: %d fixed, %d copied as-is, %d failed", fixed, copied, failed);
    char* __fstr_609_buf = __btrc_str_track(((char*)malloc((__fstr_609_len + 1))));
    snprintf(__fstr_609_buf, (__fstr_609_len + 1), "Done: %d fixed, %d copied as-is, %d failed", fixed, copied, failed);
    printf("%s\n", __fstr_609_buf);
    return ((failed == 0) ? 0 : 1);
}

int utilitiesCommand(CliArgs* args) {
    char* mode = "";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if ((strcmp(mode, "n3ds-nocrypto") == 0) || (strcmp(mode, "3ds-nocrypto") == 0)) {
        return n3dsNoCryptoCommand(args, 2);
    }
    if (strcmp(mode, "decrypt3ds") == 0) {
        return decrypt3dsNoCryptoCommand(args, 2);
    }
    printUsage();
    return 1;
}

int decrypt3dsNoCryptoCommand(CliArgs* args, int startIndex) {
    return n3dsNoCryptoCommand(args, startIndex);
}

char* verificationRoot(char* project) {
    return joinPath(project, "verification");
}

char* screenshotConfigPath(char* project) {
    return joinPath(verificationRoot(project), "screenshots.json");
}

char* screenshotDefaultConfigText(void) {
    btrc_Vector_string* __list_610 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_610, jsonField("schema_version", "1"));
    btrc_Vector_string_push(__list_610, jsonBoolField("enabled", false));
    btrc_Vector_string_push(__list_610, jsonStrField("tool", "auto"));
    btrc_Vector_string_push(__list_610, jsonStrField("command", ""));
    btrc_Vector_string_push(__list_610, jsonField("delay_seconds", "2"));
    btrc_Vector_string_push(__list_610, jsonBoolField("capture_before_launch", true));
    btrc_Vector_string_push(__list_610, jsonBoolField("capture_after_spawn", true));
    btrc_Vector_string_push(__list_610, jsonBoolField("capture_after_exit", true));
    btrc_Vector_string_push(__list_610, jsonStrField("output_pattern", "${paths.project_screenshots}/verification/${emulator}/${hook}.png"));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_610), "\n"));
}

void writeScreenshotDefaults(char* project) {
    ensureDir(verificationRoot(project));
    ensureDir(joinPath(contentRoot(project), "screenshots/verification"));
    char* path = screenshotConfigPath(project);
    if (!FileSystem_exists(path)) {
        FileSystem_writeText(path, screenshotDefaultConfigText());
    }
}

JsonObject* screenshotConfig(char* project) {
    char* path = screenshotConfigPath(project);
    if (!FileSystem_exists(path)) {
        return JsonObject_parse("{}");
    }
    return JsonObject_readFile(path);
}

char* screenshotString(char* project, char* key, char* fallback) {
    JsonObject* config = screenshotConfig(project);
    return expandProjectTemplate(project, JsonObject_getString(config, key, fallback));
}

bool screenshotBool(char* project, char* key, bool fallback) {
    JsonObject* config = screenshotConfig(project);
    return JsonObject_getBool(config, key, fallback);
}

int screenshotInt(char* project, char* key, int fallback) {
    JsonObject* config = screenshotConfig(project);
    return JsonObject_getInt(config, key, fallback);
}

bool envEnabled(char* value) {
    char* normalized = __btrc_str_track(__btrc_toLower(value));
    return ((((strcmp(normalized, "1") == 0) || (strcmp(normalized, "true") == 0)) || (strcmp(normalized, "yes") == 0)) || (strcmp(normalized, "on") == 0));
}

bool screenshotHooksEnabled(char* project) {
    char* value = Environment_get("SEMU_SCREENSHOT_HOOKS", "");
    if (((int)strlen(value)) > 0) {
        return envEnabled(value);
    }
    return screenshotBool(project, "enabled", false);
}

bool screenshotHookEnabled(char* project, char* hook) {
    if (!screenshotHooksEnabled(project)) {
        return false;
    }
    if (strcmp(hook, "before_launch") == 0) {
        return screenshotBool(project, "capture_before_launch", true);
    }
    if (strcmp(hook, "after_spawn") == 0) {
        return screenshotBool(project, "capture_after_spawn", true);
    }
    if (strcmp(hook, "after_exit") == 0) {
        return screenshotBool(project, "capture_after_exit", true);
    }
    return true;
}

char* screenshotDelaySeconds(char* project) {
    int delay = screenshotInt(project, "delay_seconds", 2);
    int __fstr_611_len = snprintf(NULL, 0, "%d", delay);
    char* __fstr_611_buf = __btrc_str_track(((char*)malloc((__fstr_611_len + 1))));
    snprintf(__fstr_611_buf, (__fstr_611_len + 1), "%d", delay);
    return Environment_get("SEMU_SCREENSHOT_DELAY_SECONDS", __fstr_611_buf);
}

char* screenshotSafeName(char* value) {
    char* result = __btrc_str_track(__btrc_toLower(value));
    (result = Strings_replace(result, "/", "_"));
    (result = Strings_replace(result, " ", "_"));
    (result = Strings_replace(result, ":", "_"));
    if (((int)strlen(result)) == 0) {
        return "unknown";
    }
    return result;
}

char* screenshotExpandPathTemplate(char* project, char* value, char* emulator, char* hook) {
    char* result = expandProjectTemplate(project, value);
    (result = Strings_replace(result, "${emulator}", screenshotSafeName(emulator)));
    (result = Strings_replace(result, "${hook}", screenshotSafeName(hook)));
    return result;
}

char* screenshotExpandCommandTemplate(char* project, char* value, char* emulator, char* hook, char* output) {
    char* result = Strings_replace(value, "${project}", ShellWords_quote(project));
    (result = Strings_replace(result, "${paths.project_screenshots}", ShellWords_quote(joinPath(contentRoot(project), "screenshots"))));
    (result = Strings_replace(result, "${emulator}", ShellWords_quote(screenshotSafeName(emulator))));
    (result = Strings_replace(result, "${hook}", ShellWords_quote(screenshotSafeName(hook))));
    (result = Strings_replace(result, "${output}", ShellWords_quote(output)));
    return result;
}

char* screenshotCapturePath(char* project, char* emulator, char* hook) {
    char* configured = Environment_get("SEMU_SCREENSHOT_OUTPUT", screenshotString(project, "output_pattern", "${paths.project_screenshots}/verification/${emulator}/${hook}.png"));
    return screenshotExpandPathTemplate(project, configured, emulator, hook);
}

char* screenshotAutoTool(void) {
    btrc_Vector_string* __list_612 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_612, "grim");
    btrc_Vector_string_push(__list_612, "spectacle");
    btrc_Vector_string_push(__list_612, "gnome-screenshot");
    btrc_Vector_string_push(__list_612, "import");
    btrc_Vector_string* candidates = __list_612;
    int __n_614 = btrc_Vector_string_iterLen(candidates);
    for (int __i_613 = 0; (__i_613 < __n_614); (__i_613++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_613);
        if (commandExists(candidate)) {
            return candidate;
        }
    }
    return "";
}

char* screenshotConfiguredTool(char* project) {
    char* envTool = Environment_get("SEMU_SCREENSHOT_TOOL", "");
    if ((((int)strlen(envTool)) > 0) && (!(strcmp(envTool, "auto") == 0))) {
        return envTool;
    }
    char* configured = screenshotString(project, "tool", "auto");
    if ((((int)strlen(configured)) > 0) && (!(strcmp(configured, "auto") == 0))) {
        return configured;
    }
    return screenshotAutoTool();
}

char* screenshotCaptureCommand(char* project, char* emulator, char* hook, char* output) {
    char* template = Environment_get("SEMU_SCREENSHOT_CMD", screenshotString(project, "command", ""));
    if (((int)strlen(template)) > 0) {
        if (__btrc_strContains(template, "${output}")) {
            return screenshotExpandCommandTemplate(project, template, emulator, hook, output);
        }
        return shellAppend(ShellWords_quote(template), output);
    }
    char* tool = screenshotConfiguredTool(project);
    if (((int)strlen(tool)) == 0) {
        return "";
    }
    char* command = ShellWords_quote(tool);
    char* base = PathTools_basename(tool);
    if (strcmp(base, "spectacle") == 0) {
        btrc_Vector_string* __list_615 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_615, "-b");
        btrc_Vector_string_push(__list_615, "-n");
        btrc_Vector_string_push(__list_615, "-o");
        btrc_Vector_string_push(__list_615, output);
        return shellAppendAll(command, __list_615);
    }
    if (strcmp(base, "gnome-screenshot") == 0) {
        btrc_Vector_string* __list_616 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_616, "-f");
        btrc_Vector_string_push(__list_616, output);
        return shellAppendAll(command, __list_616);
    }
    if (strcmp(base, "import") == 0) {
        btrc_Vector_string* __list_617 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_617, "-window");
        btrc_Vector_string_push(__list_617, "root");
        btrc_Vector_string_push(__list_617, output);
        return shellAppendAll(command, __list_617);
    }
    return shellAppend(command, output);
}

bool screenshotCaptureTo(char* project, char* emulator, char* hook, char* output) {
    ensureDir(PathTools_dirname(output));
    char* command = screenshotCaptureCommand(project, emulator, hook, output);
    if (((int)strlen(command)) == 0) {
        printf("%s\n", "MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SEMU_SCREENSHOT_CMD");
        return false;
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    if (ExecResult_ok(result) && FileSystem_isFile(output)) {
        int __fstr_620_len = snprintf(NULL, 0, "OK screenshot %s:%s: %s", emulator, hook, output);
        char* __fstr_620_buf = __btrc_str_track(((char*)malloc((__fstr_620_len + 1))));
        snprintf(__fstr_620_buf, (__fstr_620_len + 1), "OK screenshot %s:%s: %s", emulator, hook, output);
        printf("%s\n", __fstr_620_buf);
        bool __btrc_ret_621 = true;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_621;
    }
    int __fstr_624_len = snprintf(NULL, 0, "MISSING screenshot %s:%s: %s", emulator, hook, output);
    char* __fstr_624_buf = __btrc_str_track(((char*)malloc((__fstr_624_len + 1))));
    snprintf(__fstr_624_buf, (__fstr_624_len + 1), "MISSING screenshot %s:%s: %s", emulator, hook, output);
    printf("%s\n", __fstr_624_buf);
    bool __btrc_ret_625 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_625;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool screenshotCapture(char* project, char* emulator, char* hook) {
    return screenshotCaptureTo(project, emulator, hook, screenshotCapturePath(project, emulator, hook));
}

void screenshotCaptureHook(char* project, char* emulator, char* hook) {
    if (screenshotHookEnabled(project, hook)) {
        screenshotCapture(project, emulator, hook);
    }
}

void screenshotScheduleHook(char* project, char* emulator, char* hook) {
    if (!screenshotHookEnabled(project, hook)) {
        return;
    }
    char* output = screenshotCapturePath(project, emulator, hook);
    ensureDir(PathTools_dirname(output));
    char* command = screenshotCaptureCommand(project, emulator, hook, output);
    if (((int)strlen(command)) == 0) {
        printf("%s\n", "MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SEMU_SCREENSHOT_CMD");
        return;
    }
    char* delay = screenshotDelaySeconds(project);
    UnixShell* shell = UnixShell_new();
    UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("(sleep ", ShellWords_quote(delay))), "; ")), command)), ") >/dev/null 2>&1 &")), false, false, "");
    int __fstr_628_len = snprintf(NULL, 0, "OK screenshot scheduled %s:%s: %s", emulator, hook, output);
    char* __fstr_628_buf = __btrc_str_track(((char*)malloc((__fstr_628_len + 1))));
    snprintf(__fstr_628_buf, (__fstr_628_len + 1), "OK screenshot scheduled %s:%s: %s", emulator, hook, output);
    printf("%s\n", __fstr_628_buf);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void doctorScreenshotHooks(char* project) {
    printf("%s\n", "");
    printf("%s\n", "Screenshot verification hooks");
    reportFile("screenshot_config", screenshotConfigPath(project));
    printf("%s\n", (screenshotHooksEnabled(project) ? "  OK launcher_hooks: enabled" : "  optional launcher_hooks: disabled (set SEMU_SCREENSHOT_HOOKS=1)"));
    printf("%s\n", "  OK hooks: before_launch, after_spawn, after_exit, manual_visual_checkpoint");
    char* tool = screenshotConfiguredTool(project);
    if (((int)strlen(tool)) > 0) {
        int __fstr_631_len = snprintf(NULL, 0, "  OK screenshot_tool: %s", tool);
        char* __fstr_631_buf = __btrc_str_track(((char*)malloc((__fstr_631_len + 1))));
        snprintf(__fstr_631_buf, (__fstr_631_len + 1), "  OK screenshot_tool: %s", tool);
        printf("%s\n", __fstr_631_buf);
    } else {
        printf("%s\n", "  MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SEMU_SCREENSHOT_CMD");
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("  output_pattern: ", screenshotString(project, "output_pattern", "${paths.project_screenshots}/verification/${emulator}/${hook}.png"))));
}

char* syncFolderPath(char* project, char* id) {
    if (strcmp(id, "saves") == 0) {
        return joinPath(contentRoot(project), "saves");
    }
    if (strcmp(id, "states") == 0) {
        return joinPath(contentRoot(project), "states");
    }
    if (strcmp(id, "emulator_state") == 0) {
        return joinPath(semuStateRoot(project), "appimage-state");
    }
    if (strcmp(id, "screenshots") == 0) {
        return joinPath(contentRoot(project), "screenshots");
    }
    if (strcmp(id, "gamelists") == 0) {
        return joinPath(contentRoot(project), "gamelists");
    }
    if (strcmp(id, "roms") == 0) {
        return configuredRomsRoot(project);
    }
    if (strcmp(id, "bios") == 0) {
        return biosRoot(project);
    }
    return project;
}

btrc_Vector_string* syncFolderIds(void) {
    btrc_Vector_string* __list_632 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_632, "saves");
    btrc_Vector_string_push(__list_632, "states");
    btrc_Vector_string_push(__list_632, "emulator_state");
    btrc_Vector_string_push(__list_632, "screenshots");
    btrc_Vector_string_push(__list_632, "gamelists");
    btrc_Vector_string_push(__list_632, "roms");
    btrc_Vector_string_push(__list_632, "bios");
    return __list_632;
}

char* syncFolderLabel(char* id) {
    if (strcmp(id, "saves") == 0) {
        return "Semu Saves";
    }
    if (strcmp(id, "states") == 0) {
        return "Semu States";
    }
    if (strcmp(id, "emulator_state") == 0) {
        return "Semu Emulator State";
    }
    if (strcmp(id, "screenshots") == 0) {
        return "Semu Screenshots";
    }
    if (strcmp(id, "gamelists") == 0) {
        return "Semu Gamelists";
    }
    if (strcmp(id, "roms") == 0) {
        return "Semu ROMs";
    }
    if (strcmp(id, "bios") == 0) {
        return "Semu BIOS";
    }
    return "Semu";
}

bool syncFolderEnabled(char* project, char* id) {
    return syncBool(project, __btrc_str_track(__btrc_strcat("sync_", id)), false);
}

bool syncFolderWatch(char* project, char* id) {
    return syncBool(project, __btrc_str_track(__btrc_strcat("watch_", id)), true);
}

int syncFolderRescan(char* project, char* id) {
    return syncInt(project, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("rescan_", id)), "_s")), 900);
}

char* syncGuiAddress(char* project) {
    return syncString(project, "gui_address", "127.0.0.1:8384");
}

char* serviceExecutable(void) {
    return Environment_get("SEMU_BIN", "semu");
}

char* syncSyncthingExecutable(void) {
    char* configured = Environment_get("SEMU_SYNCTHING_BIN", "");
    if ((((int)strlen(configured)) > 0) && FileSystem_exists(configured)) {
        return configured;
    }
    char* homeLocal = joinPath(Environment_get("HOME", ""), ".local/bin/syncthing");
    if ((((int)strlen(homeLocal)) > 0) && FileSystem_exists(homeLocal)) {
        return homeLocal;
    }
    if (FileSystem_exists("/home/linuxbrew/.linuxbrew/bin/syncthing")) {
        return "/home/linuxbrew/.linuxbrew/bin/syncthing";
    }
    if (FileSystem_exists("/var/home/linuxbrew/.linuxbrew/bin/syncthing")) {
        return "/var/home/linuxbrew/.linuxbrew/bin/syncthing";
    }
    char* path = commandPath("syncthing");
    if (((int)strlen(path)) > 0) {
        return path;
    }
    return "";
}

bool syncSyncthingAvailable(void) {
    return (((int)strlen(syncSyncthingExecutable())) > 0);
}

char* syncServeCommand(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ShellWords_quote(serviceExecutable()), " sync serve --project ")), ShellWords_quote(project)));
}

char* syncServiceText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Semu Syncthing\n")), "After=network-online.target\n\n")), "[Service]\n")), "Type=simple\n")), "ExecStart=")), syncServeCommand(project))), "\n")), "Restart=on-failure\n")), "RestartSec=5\n\n")), "[Install]\n")), "WantedBy=default.target\n"));
}

char* syncForceScriptText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env bash\n", "set -euo pipefail\n")), "exec ")), ShellWords_quote(serviceExecutable()))), " sync force all --project ")), ShellWords_quote(project))), "\n"));
}

char* syncForceServiceText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Semu force Syncthing scan\n\n")), "[Service]\n")), "Type=oneshot\n")), "ExecStart=")), ShellWords_quote(joinPath(syncScriptsDir(project), "sync-force.sh")))), "\n"));
}

char* syncTimerText(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Semu scheduled Syncthing scan\n\n")), "[Timer]\n")), "OnBootSec=5min\n")), "OnUnitActiveSec=15min\n")), "Persistent=true\n\n")), "[Install]\n")), "WantedBy=timers.target\n"));
}

void writeSyncSystemdUnits(char* project) {
    ensureDir(systemdUserDir());
    ensureDir(syncScriptsDir(project));
    char* script = joinPath(syncScriptsDir(project), "sync-force.sh");
    FileSystem_writeText(script, syncForceScriptText(project));
    FileSystem_chmod(script, 493);
    FileSystem_writeText(joinPath(systemdUserDir(), "semu-syncthing.service"), syncServiceText(project));
    FileSystem_writeText(joinPath(systemdUserDir(), "semu-sync-force.service"), syncForceServiceText(project));
    FileSystem_writeText(joinPath(systemdUserDir(), "semu-sync-force.timer"), syncTimerText());
}

char* deckDesktopText(char* project) {
    char* exe = serviceExecutable();
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Desktop Entry]\n", "Type=Application\n")), "Name=Semu\n")), "Comment=Steam Deck emulation frontend\n")), "Exec=")), exe)), " deck launch --project ")), ShellWords_quote(project))), "\n")), "Terminal=false\n")), "Categories=Game;Emulator;\n")), "Actions=ForceSync;SyncStatus;OpenSyncthing;OpenSyncTray;\n\n")), "[Desktop Action ForceSync]\n")), "Name=Force Sync\n")), "Exec=")), exe)), " sync force all --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action SyncStatus]\n")), "Name=Sync Status\n")), "Exec=")), exe)), " sync status --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action OpenSyncthing]\n")), "Name=Open Syncthing\n")), "Exec=")), exe)), " sync open --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action OpenSyncTray]\n")), "Name=Open Sync Tray\n")), "Exec=")), exe)), " sync tray --project ")), ShellWords_quote(project))), "\n"));
}

void writeDeckDesktopEntry(char* project) {
    ensureDir(applicationsDir());
    FileSystem_writeText(joinPath(applicationsDir(), "semu.desktop"), deckDesktopText(project));
}

char* syncConfigXmlPath(char* project) {
    return joinPath(syncthingHome(project), "config.xml");
}

char* findXmlValue(char* text, char* startTag, char* endTag) {
    int start = __btrc_indexOf(text, startTag);
    if (start < 0) {
        return "";
    }
    (start = (start + ((int)strlen(startTag))));
    int end = Strings_find(text, endTag, start);
    if (end < 0) {
        return "";
    }
    return __btrc_str_track(__btrc_substring(text, start, (end - start)));
}

char* syncApiKey(char* project) {
    char* path = syncConfigXmlPath(project);
    if (!FileSystem_exists(path)) {
        return "";
    }
    return findXmlValue(FileSystem_readText(path), "<apikey>", "</apikey>");
}

char* syncScanUrl(char* project, char* target) {
    char* url = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("http://", syncGuiAddress(project))), "/rest/db/scan"));
    if ((!(strcmp(target, "all") == 0)) && (((int)strlen(target)) > 0)) {
        (url = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(url, "?folder=semu-")), target)));
    }
    return url;
}

void syncGenerateIfNeeded(char* project) {
    if (FileSystem_exists(syncConfigXmlPath(project))) {
        return;
    }
    UnixShell* shell = UnixShell_new();
    UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ShellWords_quote(syncSyncthingExecutable()), " generate -H ")), ShellWords_quote(syncthingHome(project)))), " --no-port-probing >/dev/null 2>&1")));
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncSystemctl(char* verb, char* unit) {
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("systemctl --user ", verb)), " ")), unit)), " >/dev/null 2>&1")));
    bool __btrc_ret_633 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_633;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncWaitForApi(char* project) {
    UnixShell* shell = UnixShell_new();
    char* url = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("http://", syncGuiAddress(project))), "/rest/noauth/health"));
    for (int i = 0; (i < 20); (i++)) {
        ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("curl -fsS ", ShellWords_quote(url))), " >/dev/null 2>&1")));
        if (ExecResult_ok(result)) {
            bool __btrc_ret_634 = true;
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
            return __btrc_ret_634;
        }
        UnixShell_runUnchecked(shell, "sleep 1");
    }
    bool __btrc_ret_635 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_635;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncAddFolder(char* project, char* id) {
    if (!syncFolderEnabled(project, id)) {
        return true;
    }
    ensureDir(syncFolderPath(project, id));
    char* apiKey = syncApiKey(project);
    if (((int)strlen(apiKey)) == 0) {
        return false;
    }
    UnixShell* shell = UnixShell_new();
    char* folderId = __btrc_str_track(__btrc_strcat("semu-", id));
    char* syncthing = ShellWords_quote(syncSyncthingExecutable());
    char* cliPrefix = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(syncthing, " cli -H ")), ShellWords_quote(syncthingHome(project)))), " --gui-address ")), ShellWords_quote(syncGuiAddress(project)))), " --gui-apikey ")), ShellWords_quote(apiKey)));
    ExecResult* existing = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(cliPrefix, " config folders list | grep -Fx ")), ShellWords_quote(folderId))), " >/dev/null 2>&1")));
    if (ExecResult_ok(existing)) {
        bool __btrc_ret_636 = true;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_636;
    }
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(syncthing, " cli -H ")), ShellWords_quote(syncthingHome(project)))), " --gui-address ")), ShellWords_quote(syncGuiAddress(project)))), " --gui-apikey ")), ShellWords_quote(apiKey))), " config folders add")), " --id ")), ShellWords_quote(folderId))), " --label ")), ShellWords_quote(syncFolderLabel(id)))), " --path ")), ShellWords_quote(syncFolderPath(project, id)))), " --type sendreceive")), " --rescan-intervals ")), Strings_fromInt(syncFolderRescan(project, id))));
    if (syncFolderWatch(project, id)) {
        (command = __btrc_str_track(__btrc_strcat(command, " --fswatcher-enabled")));
    }
    ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(command, " >/dev/null 2>&1")));
    bool __btrc_ret_637 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_637;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncConfigureFolders(char* project) {
    bool ok = true;
    int __n_639 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_638 = 0; (__i_638 < __n_639); (__i_638++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_638);
        if (!syncAddFolder(project, id)) {
            (ok = false);
        }
    }
    return ok;
}

bool syncSetup(char* project) {
    writeSyncDefaults(project, "");
    writeSyncSystemdUnits(project);
    if (!syncSyncthingAvailable()) {
        printf("%s\n", "MISSING syncthing: install package or use bundled Semu");
        return false;
    }
    syncGenerateIfNeeded(project);
    bool ok = syncSystemctl("daemon-reload", "");
    if (syncBool(project, "start_at_boot", true)) {
        if (!syncSystemctl("enable", "semu-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("enable", "semu-sync-force.timer")) {
            (ok = false);
        }
    }
    if (syncBool(project, "enabled", true)) {
        if (!syncSystemctl("start", "semu-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("start", "semu-sync-force.timer")) {
            (ok = false);
        }
        if (!syncWaitForApi(project)) {
            (ok = false);
        }
        if (!syncConfigureFolders(project)) {
            (ok = false);
        }
    }
    if (ok) {
        int __fstr_642_len = snprintf(NULL, 0, "OK sync setup: %s", syncConfigPath(project));
        char* __fstr_642_buf = __btrc_str_track(((char*)malloc((__fstr_642_len + 1))));
        snprintf(__fstr_642_buf, (__fstr_642_len + 1), "OK sync setup: %s", syncConfigPath(project));
        printf("%s\n", __fstr_642_buf);
    } else {
        int __fstr_645_len = snprintf(NULL, 0, "MISSING sync setup incomplete: %s", syncConfigPath(project));
        char* __fstr_645_buf = __btrc_str_track(((char*)malloc((__fstr_645_len + 1))));
        snprintf(__fstr_645_buf, (__fstr_645_len + 1), "MISSING sync setup incomplete: %s", syncConfigPath(project));
        printf("%s\n", __fstr_645_buf);
    }
    return ok;
}

char* semuStateRoot(char* project) {
    return joinPath(project, ".semu");
}

char* lifecycleStatePath(char* project) {
    return joinPath(semuStateRoot(project), "lifecycle.json");
}

char* lifecycleBackupsRoot(char* project) {
    return joinPath(semuStateRoot(project), "backups");
}

char* upgradeBackupPath(char* project) {
    return joinPath(lifecycleBackupsRoot(project), "pre-upgrade-semu.json");
}

void removePath(char* path) {
    if (FileSystem_exists(path)) {
        FileSystem_removeRecursive(path);
    }
}

void writeLifecycleState(char* project, char* action) {
    ensureDir(semuStateRoot(project));
    btrc_Vector_string* __list_647 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_647, jsonStrField("schema_version", "1"));
    btrc_Vector_string_push(__list_647, jsonStrField("action", action));
    btrc_Vector_string_push(__list_647, jsonStrField("project", project));
    btrc_Vector_string_push(__list_647, jsonStrField("roms_dir", configuredRomsRoot(project)));
    btrc_Vector_string_push(__list_647, jsonStrField("source", "src/semu.btrc"));
    FileSystem_writeText(lifecycleStatePath(project), __btrc_str_track(__btrc_strcat(jsonObject(__list_647), "\n")));
}

void lifecycleReconfigure(char* project, char* romsDir) {
    writeSyncDefaults(project, romsDir);
    ensureProjectContentDirs(project);
    seedKeymapDefaults(project);
    seedEmulatorDefaults(project);
    writeSteamInputTemplates(project);
    writeScreenshotDefaults(project);
    writeGeneratedManifest(joinPath(project, "semu.json"));
    writeEsDeFiles(project);
    writeSyncSystemdUnits(project);
    writeDeckDesktopEntry(project);
    writeLifecycleState(project, "reconfigure");
}

void lifecycleInstall(char* project, char* romsDir) {
    lifecycleReconfigure(project, romsDir);
    if (syncBool(project, "enabled", true)) {
        syncSetup(project);
    }
    writeLifecycleState(project, "install");
    doctorSteamDeck(project);
}

void lifecycleUninstall(char* project, bool purgeGenerated, bool purgeState) {
    syncStop(project);
    syncAutostart(project, false);
    removePath(joinPath(systemdUserDir(), "semu-syncthing.service"));
    removePath(joinPath(systemdUserDir(), "semu-sync-force.service"));
    removePath(joinPath(systemdUserDir(), "semu-sync-force.timer"));
    removePath(joinPath(applicationsDir(), "semu.desktop"));
    removePath(joinPath(syncScriptsDir(project), "sync-force.sh"));
    removePath(joinPath(semuStateRoot(project), "appimage-state"));
    if (purgeGenerated) {
        removePath(joinPath(project, "ES-DE/custom_systems"));
        removePath(joinPath(project, "ES-DE/es_settings.xml"));
        removePath(joinPath(project, "semu.json"));
    }
    if (purgeState) {
        removePath(semuStateRoot(project));
    } else {
        writeLifecycleState(project, "uninstall");
    }
    printf("%s\n", "OK lifecycle uninstall");
}

bool lifecycleChangeKeymap(char* project, char* actionId, char* command) {
    seedKeymapDefaults(project);
    char* path = keymapSourcePath(project);
    char* source = FileSystem_readText(path);
    btrc_Vector_string* lines = Strings_split(source, "\n");
    btrc_Vector_string* out = btrc_Vector_string_new();
    bool changed = false;
    char* prefix = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("action ", actionId)), " = "));
    int __n_649 = btrc_Vector_string_iterLen(lines);
    for (int __i_648 = 0; (__i_648 < __n_649); (__i_648++)) {
        char* line = btrc_Vector_string_iterGet(lines, __i_648);
        if (__btrc_startsWith(line, prefix)) {
            btrc_Vector_string_push(out, __btrc_str_track(__btrc_strcat(prefix, command)));
            (changed = true);
        } else if (((int)strlen(line)) > 0) {
            btrc_Vector_string_push(out, line);
        }
    }
    if (!changed) {
        int __fstr_652_len = snprintf(NULL, 0, "error 0:0 unknown keymap action '%s'", actionId);
        char* __fstr_652_buf = __btrc_str_track(((char*)malloc((__fstr_652_len + 1))));
        snprintf(__fstr_652_buf, (__fstr_652_len + 1), "error 0:0 unknown keymap action '%s'", actionId);
        printf("%s\n", __fstr_652_buf);
        return false;
    }
    char* next = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(out, "\n"), "\n"));
    KeymapErrors* errors = KeymapErrors_new();
    compileKeymap(next, errors);
    if (KeymapErrors_count(errors) > 0) {
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_655_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_655_buf = __btrc_str_track(((char*)malloc((__fstr_655_len + 1))));
            snprintf(__fstr_655_buf, (__fstr_655_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_655_buf);
        }
        bool __btrc_ret_656 = false;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_656;
    }
    FileSystem_writeText(path, next);
    lifecycleReconfigure(project, "");
    writeLifecycleState(project, "change");
    int __fstr_659_len = snprintf(NULL, 0, "OK lifecycle change: %s=%s", actionId, command);
    char* __fstr_659_buf = __btrc_str_track(((char*)malloc((__fstr_659_len + 1))));
    snprintf(__fstr_659_buf, (__fstr_659_len + 1), "OK lifecycle change: %s=%s", actionId, command);
    printf("%s\n", __fstr_659_buf);
    bool __btrc_ret_660 = true;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_660;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

void lifecycleUpgrade(char* project) {
    ensureDir(lifecycleBackupsRoot(project));
    if (FileSystem_exists(joinPath(project, "semu.json"))) {
        FileSystem_writeText(upgradeBackupPath(project), FileSystem_readText(joinPath(project, "semu.json")));
    }
    lifecycleReconfigure(project, "");
    writeLifecycleState(project, "upgrade");
    int __fstr_663_len = snprintf(NULL, 0, "OK lifecycle upgrade: backup %s", upgradeBackupPath(project));
    char* __fstr_663_buf = __btrc_str_track(((char*)malloc((__fstr_663_len + 1))));
    snprintf(__fstr_663_buf, (__fstr_663_len + 1), "OK lifecycle upgrade: backup %s", upgradeBackupPath(project));
    printf("%s\n", __fstr_663_buf);
}

void lifecycleStatus(char* project) {
    printf("%s\n", "Semu lifecycle");
    reportFile("state", lifecycleStatePath(project));
    reportFile("manifest", joinPath(project, "semu.json"));
    reportFile("sync_config", syncConfigPath(project));
    reportFile("desktop_entry", joinPath(applicationsDir(), "semu.desktop"));
    reportFile("systemd_service", joinPath(systemdUserDir(), "semu-syncthing.service"));
    int __fstr_666_len = snprintf(NULL, 0, "  roms_dir: %s", configuredRomsRoot(project));
    char* __fstr_666_buf = __btrc_str_track(((char*)malloc((__fstr_666_len + 1))));
    snprintf(__fstr_666_buf, (__fstr_666_len + 1), "  roms_dir: %s", configuredRomsRoot(project));
    printf("%s\n", __fstr_666_buf);
}

bool syncStart(char* project) {
    bool ok = syncSetup(project);
    if (!syncSystemctl("start", "semu-syncthing.service")) {
        (ok = false);
    }
    printf("%s\n", (ok ? "OK sync start" : "MISSING sync start failed"));
    return ok;
}

bool syncServe(char* project) {
    char* syncthing = syncSyncthingExecutable();
    if (((int)strlen(syncthing)) == 0) {
        printf("%s\n", "MISSING syncthing: executable not found");
        return false;
    }
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ShellWords_quote(syncthing), " serve -H ")), ShellWords_quote(syncthingHome(project)))), " --no-browser --no-restart")), false, false, "");
    return ExecResult_ok(result);
}

bool syncStop(char* project) {
    bool ok = true;
    if (!syncSystemctl("stop", "semu-sync-force.timer")) {
        (ok = false);
    }
    if (!syncSystemctl("stop", "semu-syncthing.service")) {
        (ok = false);
    }
    printf("%s\n", (ok ? "OK sync stop" : "MISSING sync stop incomplete"));
    return ok;
}

bool syncAutostart(char* project, bool enabled) {
    writeSyncSystemdUnits(project);
    bool ok = syncSystemctl("daemon-reload", "");
    if (enabled) {
        if (!syncSystemctl("enable", "semu-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("enable", "semu-sync-force.timer")) {
            (ok = false);
        }
        printf("%s\n", (ok ? "OK sync autostart enabled" : "MISSING sync autostart enable incomplete"));
    } else {
        if (!syncSystemctl("disable", "semu-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("disable", "semu-sync-force.timer")) {
            (ok = false);
        }
        printf("%s\n", (ok ? "OK sync autostart disabled" : "MISSING sync autostart disable incomplete"));
    }
    return ok;
}

bool syncForce(char* project, char* target) {
    char* apiKey = syncApiKey(project);
    if (((int)strlen(apiKey)) == 0) {
        printf("%s\n", "MISSING syncthing API key; run sync setup/start first");
        return false;
    }
    UnixShell* shell = UnixShell_new();
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("curl -fsS -X POST -H ", ShellWords_quote(__btrc_str_track(__btrc_strcat("X-API-Key: ", apiKey))))), " ")), ShellWords_quote(syncScanUrl(project, target))));
    ExecResult* result = UnixShell_runUnchecked(shell, command);
    if (ExecResult_ok(result)) {
        int __fstr_669_len = snprintf(NULL, 0, "OK sync force: %s", target);
        char* __fstr_669_buf = __btrc_str_track(((char*)malloc((__fstr_669_len + 1))));
        snprintf(__fstr_669_buf, (__fstr_669_len + 1), "OK sync force: %s", target);
        printf("%s\n", __fstr_669_buf);
        bool __btrc_ret_670 = true;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_670;
    } else {
        int __fstr_673_len = snprintf(NULL, 0, "MISSING sync force failed: %s", target);
        char* __fstr_673_buf = __btrc_str_track(((char*)malloc((__fstr_673_len + 1))));
        snprintf(__fstr_673_buf, (__fstr_673_len + 1), "MISSING sync force failed: %s", target);
        printf("%s\n", __fstr_673_buf);
    }
    bool __btrc_ret_674 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_674;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void syncStatus(char* project) {
    printf("%s\n", "Semu sync");
    reportFile("sync_config", syncConfigPath(project));
    reportFile("syncthing_config", syncConfigXmlPath(project));
    reportFile("systemd_service", joinPath(systemdUserDir(), "semu-syncthing.service"));
    reportFile("systemd_timer", joinPath(systemdUserDir(), "semu-sync-force.timer"));
    printf("%s\n", (syncSyncthingAvailable() ? "  OK syncthing: executable found" : "  MISSING syncthing: executable not found"));
    printf("%s\n", (commandExists("syncthingtray") ? "  OK syncthingtray: executable found" : "  optional syncthingtray: executable not found"));
    printf("%s\n", (commandExists("curl") ? "  OK curl: executable found" : "  MISSING curl: executable not found"));
    int __n_676 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_675 = 0; (__i_675 < __n_676); (__i_675++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_675);
        char* mark = (syncFolderEnabled(project, id) ? "OK" : "optional");
        int __fstr_679_len = snprintf(NULL, 0, "  %s %s: %s", mark, id, syncFolderPath(project, id));
        char* __fstr_679_buf = __btrc_str_track(((char*)malloc((__fstr_679_len + 1))));
        snprintf(__fstr_679_buf, (__fstr_679_len + 1), "  %s %s: %s", mark, id, syncFolderPath(project, id));
        printf("%s\n", __fstr_679_buf);
    }
}

void syncTray(char* project) {
    UnixShell* shell = UnixShell_new();
    if (commandExists("syncthingtray")) {
        UnixShell_runUnchecked(shell, "syncthingtray >/dev/null 2>&1 &");
        printf("%s\n", "OK sync tray");
    } else if (commandExists("flatpak")) {
        UnixShell_runUnchecked(shell, "flatpak run io.github.martchus.syncthingtray >/dev/null 2>&1 &");
        printf("%s\n", "OK sync tray flatpak");
    } else {
        printf("%s\n", "MISSING syncthingtray");
    }
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void syncOpen(char* project) {
    UnixShell* shell = UnixShell_new();
    UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("xdg-open ", ShellWords_quote(__btrc_str_track(__btrc_strcat("http://", syncGuiAddress(project)))))), " >/dev/null 2>&1 &")));
    printf("%s\n", "OK sync open");
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void doctorSync(char* project) {
    printf("%s\n", "");
    printf("%s\n", "Sync");
    reportFile("sync_config", syncConfigPath(project));
    printf("%s\n", (syncSyncthingAvailable() ? "  OK syncthing: executable found" : "  MISSING syncthing: executable not found"));
    printf("%s\n", (commandExists("curl") ? "  OK curl: executable found" : "  MISSING curl: executable not found"));
    printf("%s\n", (syncBool(project, "start_at_boot", true) ? "  OK start_at_boot: enabled" : "  optional start_at_boot: disabled"));
    printf("%s\n", (syncBool(project, "tray", true) ? "  OK tray: enabled" : "  optional tray: disabled"));
    int __n_681 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_680 = 0; (__i_680 < __n_681); (__i_680++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_680);
        char* mark = (syncFolderEnabled(project, id) ? "OK" : "optional");
        char* watch = (syncFolderWatch(project, id) ? "watch" : "scan");
        int interval = syncFolderRescan(project, id);
        int __fstr_684_len = snprintf(NULL, 0, "  %s %s: %s, %ds, %s", mark, id, watch, interval, syncFolderPath(project, id));
        char* __fstr_684_buf = __btrc_str_track(((char*)malloc((__fstr_684_len + 1))));
        snprintf(__fstr_684_buf, (__fstr_684_len + 1), "  %s %s: %s, %ds, %s", mark, id, watch, interval, syncFolderPath(project, id));
        printf("%s\n", __fstr_684_buf);
    }
}

void e2eWriteExecutable(char* path, char* text) {
    FileSystem_writeText(path, text);
    FileSystem_chmod(path, 493);
}

char* e2eFakeEsdeAppImageText(void) {
    btrc_Vector_string* __list_685 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_685, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_685, "set -euo pipefail");
    btrc_Vector_string_push(__list_685, "if [ \"${1:-}\" != \"--appimage-extract\" ]; then");
    btrc_Vector_string_push(__list_685, "  echo \"fake ES-DE AppImage only supports --appimage-extract\" >&2");
    btrc_Vector_string_push(__list_685, "  exit 2");
    btrc_Vector_string_push(__list_685, "fi");
    btrc_Vector_string_push(__list_685, "mkdir -p squashfs-root/usr/bin squashfs-root/usr/share/applications squashfs-root/usr/lib");
    btrc_Vector_string_push(__list_685, "cat > squashfs-root/usr/bin/es-de <<'EOF'");
    btrc_Vector_string_push(__list_685, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_685, "echo fake es-de \"$@\"");
    btrc_Vector_string_push(__list_685, "EOF");
    btrc_Vector_string_push(__list_685, "chmod +x squashfs-root/usr/bin/es-de");
    btrc_Vector_string_push(__list_685, "printf 'fake icon\\n' > squashfs-root/semu.png");
    return textLines(__list_685);
}

char* e2eFakeAppImageToolText(void) {
    btrc_Vector_string* __list_686 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_686, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_686, "set -euo pipefail");
    btrc_Vector_string_push(__list_686, "if [ \"${1:-}\" = \"--no-appstream\" ]; then shift; fi");
    btrc_Vector_string_push(__list_686, "APPDIR=\"${1:?missing AppDir}\"");
    btrc_Vector_string_push(__list_686, "OUTPUT=\"${2:?missing output}\"");
    btrc_Vector_string_push(__list_686, "for required in AppRun usr/bin/es-de usr/bin/semu usr/bin/semu-quit-watch usr/bin/bwrap usr/bin/semu-retroarch usr/bin/semu-dolphin usr/bin/semu-ppsspp usr/bin/semu-flycast usr/bin/semu-gopher64 usr/bin/semu-melonds usr/bin/semu-pcsx2 usr/bin/semu-cemu usr/bin/semu-azahar usr/bin/semu-ryujinx usr/bin/semu-btrc usr/bin/semu-es-de usr/bin/syncthing usr/bin/syncthingtray usr/bin/curl usr/lib/retroarch/cores nix/store packaging/linux/AppRun packaging/linux/ES-DE/es_find_rules_linux.xml; do");
    btrc_Vector_string_push(__list_686, "  test -e \"$APPDIR/$required\" || { echo \"missing AppDir path: $required\" >&2; exit 3; }");
    btrc_Vector_string_push(__list_686, "done");
    btrc_Vector_string_push(__list_686, "test ! -e \"$APPDIR/packaging/linux/build-appimage.sh\" || { echo \"build script shipped inside AppDir\" >&2; exit 3; }");
    btrc_Vector_string_push(__list_686, "head -1 \"$APPDIR/AppRun\" | grep -F '#!/usr/bin/env bash' >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'SEMU_NIX_STORE_MOUNTED' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'HOST_PATH=' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'PATH=\"$HOST_PATH\" command -v bwrap' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F -- '--ro-bind \"$APPDIR/nix/store\" /nix/store' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'SEMU_LAUNCHER_BIN' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'SEMU_RETROARCH_CORE_DIR' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'SEMU_SYNCTHING_BIN' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "grep -F 'fake semu-btrc' \"$APPDIR/usr/bin/semu\" >/dev/null");
    btrc_Vector_string_push(__list_686, "! grep -F 'wrapped semu' \"$APPDIR/usr/bin/semu\" >/dev/null");
    btrc_Vector_string_push(__list_686, "! grep -F 'sync setup' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "! grep -F 'pkexec' \"$APPDIR/AppRun\" >/dev/null");
    btrc_Vector_string_push(__list_686, "find \"$APPDIR/usr/bin\" -maxdepth 1 -type f -perm -111 -print | sort > \"$OUTPUT\"");
    btrc_Vector_string_push(__list_686, "grep -o 'semu-[a-z0-9-]*' \"$APPDIR/packaging/linux/ES-DE/es_find_rules_linux.xml\" | sort -u | while IFS= read -r launcher; do");
    btrc_Vector_string_push(__list_686, "  test -x \"$APPDIR/usr/bin/$launcher\" || test -x \"$APPDIR/packaging/linux/bin/$launcher\" || { echo \"generated ES-DE launcher has no executable: $launcher\" >&2; exit 3; }");
    btrc_Vector_string_push(__list_686, "done");
    return textLines(__list_686);
}

char* e2eFakeNixText(void) {
    btrc_Vector_string* __list_687 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_687, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_687, "set -euo pipefail");
    btrc_Vector_string_push(__list_687, "if [ \"${1:-}\" != \"copy\" ]; then echo \"fake nix only supports copy\" >&2; exit 2; fi");
    btrc_Vector_string_push(__list_687, "ROOT=\"\"");
    btrc_Vector_string_push(__list_687, "while [ $# -gt 0 ]; do");
    btrc_Vector_string_push(__list_687, "  case \"$1\" in");
    btrc_Vector_string_push(__list_687, "    --to) ROOT=\"${2#local?root=}\"; shift 2 ;;");
    btrc_Vector_string_push(__list_687, "    *) shift ;;");
    btrc_Vector_string_push(__list_687, "  esac");
    btrc_Vector_string_push(__list_687, "done");
    btrc_Vector_string_push(__list_687, "test -n \"$ROOT\" || { echo \"fake nix copy missing local root\" >&2; exit 2; }");
    btrc_Vector_string_push(__list_687, "mkdir -p \"$ROOT/nix/store/fake-semu-closure\"");
    btrc_Vector_string_push(__list_687, "printf 'fake closure\\n' > \"$ROOT/nix/store/fake-semu-closure/marker\"");
    return textLines(__list_687);
}

btrc_Vector_string* appImageExpectedBins(void) {
    btrc_Vector_string* __list_688 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_688, "semu");
    btrc_Vector_string_push(__list_688, "semu-quit-watch");
    btrc_Vector_string_push(__list_688, "bwrap");
    btrc_Vector_string_push(__list_688, "semu-retroarch");
    btrc_Vector_string_push(__list_688, "semu-dolphin");
    btrc_Vector_string_push(__list_688, "semu-ppsspp");
    btrc_Vector_string_push(__list_688, "semu-flycast");
    btrc_Vector_string_push(__list_688, "semu-gopher64");
    btrc_Vector_string_push(__list_688, "semu-melonds");
    btrc_Vector_string_push(__list_688, "semu-pcsx2");
    btrc_Vector_string_push(__list_688, "semu-cemu");
    btrc_Vector_string_push(__list_688, "semu-azahar");
    btrc_Vector_string_push(__list_688, "semu-ryujinx");
    btrc_Vector_string_push(__list_688, "semu-es-de");
    btrc_Vector_string_push(__list_688, "syncthing");
    btrc_Vector_string_push(__list_688, "syncthingtray");
    btrc_Vector_string_push(__list_688, "curl");
    return __list_688;
}

void e2eWriteFakeNixPackage(char* root, bool includeBwrap) {
    ensureDir(joinPath(root, "bin"));
    ensureDir(joinPath(root, "lib/semu"));
    ensureDir(joinPath(root, "lib/retroarch/cores"));
    btrc_Vector_string* __list_690 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_690, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_690, "echo fake semu-btrc \"$@\"");
    e2eWriteExecutable(joinPath(root, "lib/semu/semu-btrc"), textLines(__list_690));
    FileSystem_writeText(joinPath(root, "lib/retroarch/cores/gambatte_libretro.so"), "fake core\n");
    int __n_692 = btrc_Vector_string_iterLen(appImageExpectedBins());
    for (int __i_691 = 0; (__i_691 < __n_692); (__i_691++)) {
        char* bin = btrc_Vector_string_iterGet(appImageExpectedBins(), __i_691);
        if (includeBwrap || (!(strcmp(bin, "bwrap") == 0))) {
            btrc_Vector_string* __list_694 = btrc_Vector_string_new();
            btrc_Vector_string_push(__list_694, "#!/usr/bin/env bash");
            btrc_Vector_string_push(__list_694, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("echo wrapped ", bin)), " \"$@\"")));
            e2eWriteExecutable(joinPath(root, __btrc_str_track(__btrc_strcat("bin/", bin))), textLines(__list_694));
        }
    }
}

bool e2eExpectStatus(int expected, char* command) {
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), command, true, false, "");
    if (result->code == expected) {
        return true;
    }
    int __fstr_697_len = snprintf(NULL, 0, "expected exit %d, got %d: %s", expected, result->code, command);
    char* __fstr_697_buf = __btrc_str_track(((char*)malloc((__fstr_697_len + 1))));
    snprintf(__fstr_697_buf, (__fstr_697_len + 1), "expected exit %d, got %d: %s", expected, result->code, command);
    printf("%s\n", __fstr_697_buf);
    printf("%s\n", ExecResult_stdout(result));
    return false;
}

bool e2eFileContains(char* path, char* expected, char* label) {
    if (!FileSystem_exists(path)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("missing ", label)), ": ")), path)));
        return false;
    }
    return e2eContains(FileSystem_readText(path), expected, label);
}

int e2eCountArgLines(char* text, char* expected) {
    int count = 0;
    btrc_Vector_string* lines = Strings_split(text, "\n");
    int __n_699 = btrc_Vector_string_iterLen(lines);
    for (int __i_698 = 0; (__i_698 < __n_699); (__i_698++)) {
        char* line = btrc_Vector_string_iterGet(lines, __i_698);
        if (strcmp(line, expected) == 0) {
            (count += 1);
        }
    }
    return count;
}

char* e2eBuildAppImageCommand(char* project, char* binDir, char* nixPackage, char* esde, char* output) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cd ", ShellWords_quote(project))), " && PATH=")), ShellWords_quote(binDir))), ":$PATH")), " APPIMAGETOOL=")), ShellWords_quote(joinPath(binDir, "appimagetool")))), " packaging/linux/build-appimage.sh")), " --nix-package ")), ShellWords_quote(nixPackage))), " --esde-appimage ")), ShellWords_quote(esde))), " --output ")), ShellWords_quote(output))), " --arch x86_64"));
}

int e2eAppImageSmoke(CliArgs* args) {
    char* project = CliArgs_valueAfter(args, "--project", Environment_get("SEMU_PROJECT_DIR", "."));
    char* tmp = e2eTempDir("semu-appimage-smoke");
    char* binDir = joinPath(tmp, "bin");
    ensureDir(binDir);
    char* fakeEsde = joinPath(tmp, "fake-esde.AppImage");
    e2eWriteExecutable(fakeEsde, e2eFakeEsdeAppImageText());
    e2eWriteExecutable(joinPath(binDir, "appimagetool"), e2eFakeAppImageToolText());
    e2eWriteExecutable(joinPath(binDir, "nix"), e2eFakeNixText());
    char* noBwrapPackage = joinPath(tmp, "no-bwrap-nix-package");
    e2eWriteFakeNixPackage(noBwrapPackage, false);
    if (!e2eExpectStatus(4, e2eBuildAppImageCommand(project, binDir, noBwrapPackage, fakeEsde, joinPath(tmp, "no-bwrap.AppImage")))) {
        return 1;
    }
    char* package = joinPath(tmp, "nix-package");
    e2eWriteFakeNixPackage(package, true);
    char* output = joinPath(tmp, "Semu-test.AppImage");
    if (!e2eExpectStatus(0, e2eBuildAppImageCommand(project, binDir, package, fakeEsde, output))) {
        return 1;
    }
    int __n_701 = btrc_Vector_string_iterLen(appImageExpectedBins());
    for (int __i_700 = 0; (__i_700 < __n_701); (__i_700++)) {
        char* bin = btrc_Vector_string_iterGet(appImageExpectedBins(), __i_700);
        if (!e2eFileContains(output, __btrc_str_track(__btrc_strcat("/usr/bin/", bin)), __btrc_str_track(__btrc_strcat("AppImage output ", bin)))) {
            return 1;
        }
    }
    if (!e2eFileContains(output, "/usr/bin/es-de", "AppImage output es-de")) {
        return 1;
    }
    char* appDir = joinPath(tmp, "AppRunProbe.AppDir");
    ensureDir(joinPath(appDir, "nix/store"));
    ensureDir(joinPath(appDir, "usr/bin"));
    copyFilePath(joinPath(project, "packaging/linux/AppRun"), joinPath(appDir, "AppRun"));
    FileSystem_chmod(joinPath(appDir, "AppRun"), 493);
    char* bwrapArgs = joinPath(tmp, "bwrap.args");
    btrc_Vector_string* __list_703 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_703, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_703, __btrc_str_track(__btrc_strcat("printf '%s\\n' \"$@\" > ", ShellWords_quote(bwrapArgs))));
    e2eWriteExecutable(joinPath(binDir, "bwrap"), textLines(__list_703));
    if (!e2eExpectStatus(0, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(appDir))), " SEMU_BWRAP=")), ShellWords_quote(joinPath(binDir, "bwrap")))), " ")), ShellWords_quote(joinPath(appDir, "AppRun")))), " --probe")))) {
        return 1;
    }
    if (!e2eFileContains(bwrapArgs, "--tmpfs", "bwrap args tmpfs")) {
        return 1;
    }
    if (!e2eFileContains(bwrapArgs, "/nix/store", "bwrap args nix store")) {
        return 1;
    }
    char* hostBwrapAppDir = joinPath(tmp, "AppRunHostBwrap.AppDir");
    ensureDir(joinPath(hostBwrapAppDir, "nix/store"));
    ensureDir(joinPath(hostBwrapAppDir, "usr/bin"));
    copyFilePath(joinPath(project, "packaging/linux/AppRun"), joinPath(hostBwrapAppDir, "AppRun"));
    FileSystem_chmod(joinPath(hostBwrapAppDir, "AppRun"), 493);
    btrc_Vector_string* __list_705 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_705, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_705, "echo bundled bwrap should not bootstrap the Nix store >&2");
    btrc_Vector_string_push(__list_705, "exit 88");
    e2eWriteExecutable(joinPath(hostBwrapAppDir, "usr/bin/bwrap"), textLines(__list_705));
    UnixShell_runRaw(UnixShell_new(), __btrc_str_track(__btrc_strcat("rm -f ", ShellWords_quote(bwrapArgs))), true, false, "");
    if (!e2eExpectStatus(0, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(hostBwrapAppDir))), " PATH=")), ShellWords_quote(binDir))), ":$PATH")), " ")), ShellWords_quote(joinPath(hostBwrapAppDir, "AppRun")))), " --probe")))) {
        return 1;
    }
    if (!e2eFileContains(bwrapArgs, "--tmpfs", "host bwrap args tmpfs")) {
        return 1;
    }
    if (!e2eFileContains(bwrapArgs, "/nix/store", "host bwrap args nix store")) {
        return 1;
    }
    char* noProject = joinPath(tmp, "AppRunNoProject.AppDir");
    ensureDir(joinPath(noProject, "usr/bin"));
    copyFilePath(joinPath(project, "packaging/linux/AppRun"), joinPath(noProject, "AppRun"));
    FileSystem_chmod(joinPath(noProject, "AppRun"), 493);
    if (!e2eExpectStatus(1, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(noProject))), " HOME=")), ShellWords_quote(joinPath(tmp, "no-project-home")))), " ")), ShellWords_quote(joinPath(noProject, "AppRun")))))) {
        return 1;
    }
    char* missingCli = joinPath(tmp, "AppRunMissingCli.AppDir");
    char* missingProject = joinPath(tmp, "missing-cli-project");
    ensureDir(joinPath(missingCli, "usr/bin"));
    ensureDir(missingProject);
    FileSystem_writeText(joinPath(missingProject, "semu.json"), "{\"schema_version\":1}\n");
    copyFilePath(joinPath(project, "packaging/linux/AppRun"), joinPath(missingCli, "AppRun"));
    FileSystem_chmod(joinPath(missingCli, "AppRun"), 493);
    if (!e2eExpectStatus(2, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(missingCli))), " SEMU_PROJECT_DIR=")), ShellWords_quote(missingProject))), " ")), ShellWords_quote(joinPath(missingCli, "AppRun")))))) {
        return 1;
    }
    char* missingPackage = joinPath(tmp, "missing-nix-package");
    if (!e2eExpectStatus(4, e2eBuildAppImageCommand(project, binDir, missingPackage, fakeEsde, joinPath(tmp, "missing-nix.AppImage")))) {
        return 1;
    }
    char* cliAppDir = joinPath(tmp, "AppRunCli.AppDir");
    char* cliProject = joinPath(tmp, "cli-project");
    char* cliArgs = joinPath(tmp, "cli.args");
    char* cliEnv = joinPath(tmp, "cli.env");
    char* cliAppImage = joinPath(tmp, "Semu-x86_64.AppImage");
    ensureDir(joinPath(cliAppDir, "usr/bin"));
    ensureDir(joinPath(cliAppDir, "packaging/linux/bin"));
    ensureDir(cliProject);
    FileSystem_writeText(cliAppImage, "fake appimage\n");
    FileSystem_writeText(joinPath(cliProject, "semu.json"), "{\"schema_version\":1}\n");
    copyFilePath(joinPath(project, "packaging/linux/AppRun"), joinPath(cliAppDir, "AppRun"));
    FileSystem_chmod(joinPath(cliAppDir, "AppRun"), 493);
    btrc_Vector_string* __list_707 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_707, "#!/usr/bin/env bash");
    btrc_Vector_string_push(__list_707, __btrc_str_track(__btrc_strcat("printf '%s\\n' \"$@\" > ", ShellWords_quote(cliArgs))));
    btrc_Vector_string_push(__list_707, __btrc_str_track(__btrc_strcat("printf '%s\\n' \"${SEMU_BIN:-}\" > ", ShellWords_quote(cliEnv))));
    e2eWriteExecutable(joinPath(cliAppDir, "usr/bin/semu"), textLines(__list_707));
    if (!e2eExpectStatus(0, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(cliAppDir))), " APPIMAGE=")), ShellWords_quote(cliAppImage))), " ")), ShellWords_quote(joinPath(cliAppDir, "AppRun")))), " manifest --output ")), ShellWords_quote(joinPath(tmp, "manifest.json")))), " --project ")), ShellWords_quote(cliProject))), " --roms=")), ShellWords_quote(joinPath(tmp, "explicit-roms")))))) {
        return 1;
    }
    if (!e2eFileContains(cliArgs, "manifest", "AppRun CLI passthrough")) {
        return 1;
    }
    if (!e2eFileContains(cliArgs, "--project", "AppRun project arg")) {
        return 1;
    }
    if (!e2eFileContains(cliArgs, "--roms", "AppRun roms arg")) {
        return 1;
    }
    char* manifestArgs = FileSystem_readText(cliArgs);
    if (e2eCountArgLines(manifestArgs, "--project") != 1) {
        printf("%s\n", "AppRun duplicated --project:");
        printf("%s\n", manifestArgs);
        return 1;
    }
    if (e2eCountArgLines(manifestArgs, "--roms") != 0) {
        printf("%s\n", "AppRun appended duplicate --roms after explicit --roms=...");
        printf("%s\n", manifestArgs);
        return 1;
    }
    if (!e2eFileContains(cliEnv, cliAppImage, "AppRun stable SEMU_BIN")) {
        return 1;
    }
    if (!e2eExpectStatus(0, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("APPDIR=", ShellWords_quote(cliAppDir))), " APPIMAGE=")), ShellWords_quote(cliAppImage))), " SEMU_PROJECT_DIR=")), ShellWords_quote(cliProject))), " SEMU_ROMS=")), ShellWords_quote(joinPath(tmp, "roms")))), " ")), ShellWords_quote(joinPath(cliAppDir, "AppRun")))), " launcher routed retroarch /bin/retroarch game.rom --sentinel")))) {
        return 1;
    }
    char* launcherArgs = FileSystem_readText(cliArgs);
    if (!e2eContains(launcherArgs, "launcher", "AppRun launcher passthrough")) {
        return 1;
    }
    if (!e2eContains(launcherArgs, "--sentinel", "AppRun launcher emulator arg")) {
        return 1;
    }
    if (__btrc_strContains(launcherArgs, "--project") || __btrc_strContains(launcherArgs, "--roms")) {
        printf("%s\n", "launcher AppRun passthrough leaked context args:");
        printf("%s\n", launcherArgs);
        return 1;
    }
    char* bootstrapProject = joinPath(tmp, "bootstrap-project");
    ensureDir(bootstrapProject);
    char* launcherBin = joinPath(appDir, "usr/bin");
    ExecResult* bootstrap = UnixShell_runRaw(UnixShell_new(), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("SEMU_LAUNCHER_BIN=", ShellWords_quote(launcherBin))), " ")), ShellWords_quote(args->program))), " bootstrap --project ")), ShellWords_quote(bootstrapProject))), true, false, "");
    if (!ExecResult_ok(bootstrap)) {
        printf("%s\n", ExecResult_stdout(bootstrap));
        return bootstrap->code;
    }
    if (!e2eFileContains(joinPath(bootstrapProject, "ES-DE/custom_systems/es_find_rules.xml"), joinPath(launcherBin, "semu-retroarch"), "bootstrap launcher bin")) {
        return 1;
    }
    FileSystem_removeRecursive(tmp);
    printf("%s\n", "OK AppImage assembly smoke");
    return 0;
}

void deckMaybeRun(char* command) {
    UnixShell_runUnchecked(UnixShell_new(), __btrc_str_track(__btrc_strcat(command, " >/dev/null 2>&1 || true")));
}

char* deckBrewExecutable(void) {
    if (commandExists("brew")) {
        return "brew";
    }
    if (FileSystem_exists("/home/linuxbrew/.linuxbrew/bin/brew")) {
        return "/home/linuxbrew/.linuxbrew/bin/brew";
    }
    if (FileSystem_exists("/var/home/linuxbrew/.linuxbrew/bin/brew")) {
        return "/var/home/linuxbrew/.linuxbrew/bin/brew";
    }
    return "";
}

void deckInstallHostPackages(void) {
    if (commandExists("pacman")) {
        deckMaybeRun("sudo pacman -Sy --needed --noconfirm bash curl jq rsync syncthing flatpak bubblewrap retroarch evtest gcc inputplumber");
    }
    if (commandExists("rpm-ostree")) {
        deckMaybeRun("sudo -n rpm-ostree install -y curl jq syncthing flatpak bubblewrap evtest gcc inputplumber");
    }
    char* brew = deckBrewExecutable();
    if (((int)strlen(brew)) > 0) {
        deckMaybeRun(__btrc_str_track(__btrc_strcat(ShellWords_quote(brew), " install syncthing jq")));
    }
    if (commandExists("flatpak")) {
        deckMaybeRun("flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo");
    }
}

void deckBuildGeneratedCli(char* project) {
    ensureDir(joinPath(project, "build"));
    char* generated = joinPath(project, "generated/semu.c");
    char* output = joinPath(project, "build/semu");
    if (commandExists("cc") && FileSystem_exists(generated)) {
        ExecResult* result = UnixShell_runRaw(UnixShell_new(), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cc ", ShellWords_quote(generated))), " -std=c11 -o ")), ShellWords_quote(output))), " -lm")), true, false, "");
        if (!ExecResult_ok(result)) {
            printf("%s\n", ExecResult_stdout(result));
        }
    }
}

int deckProvisionCommand(CliArgs* args, char* project) {
    char* roms = CliArgs_valueAfter(args, "--roms", Environment_get("SEMU_ROMS_DIR", joinPath(project, "ES-DE/ES-DE/ROMs")));
    deckInstallHostPackages();
    deckBuildGeneratedCli(project);
    lifecycleInstall(project, roms);
    syncSetup(project);
    printf("%s\n", "OK deck provision");
    return 0;
}

bool deckHasGraphicalSession(void) {
    return ((((int)strlen(Environment_get("WAYLAND_DISPLAY", ""))) > 0) || (((int)strlen(Environment_get("DISPLAY", ""))) > 0));
}

bool deckHasScreenshotTool(void) {
    return (((commandExists("grim") || commandExists("spectacle")) || commandExists("gnome-screenshot")) || commandExists("import"));
}

int deckVerifyEmulatorsCommand(CliArgs* args, char* project) {
    doctorSteamDeck(project);
    KeymapErrors* errors = KeymapErrors_new();
    compileKeymap((FileSystem_exists(keymapSourcePath(project)) ? FileSystem_readText(keymapSourcePath(project)) : defaultKeymapSource()), errors);
    if (KeymapErrors_count(errors) > 0) {
        int __btrc_ret_708 = 1;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_708;
    }
    KeymapIr* ir = compileKeymap((FileSystem_exists(keymapSourcePath(project)) ? FileSystem_readText(keymapSourcePath(project)) : defaultKeymapSource()), KeymapErrors_new());
    FileSystem_writeText("/tmp/semu-retroarch.cfg", renderRetroArchKeymap(ir));
    doctorScreenshotHooks(project);
    if (deckHasGraphicalSession() && deckHasScreenshotTool()) {
        screenshotCaptureTo(project, "deck_preflight", "manual_visual_checkpoint", screenshotCapturePath(project, "deck_preflight", "manual_visual_checkpoint"));
    } else {
        printf("%s\n", "INFO screenshot capture waits for a graphical session and screenshot tool");
    }
    int sandboxStatus = e2eSandboxSmoke();
    if (sandboxStatus != 0) {
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return sandboxStatus;
    }
    int launcherStatus = e2eLauncherSmoke(args);
    if (launcherStatus != 0) {
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return launcherStatus;
    }
    deckMaybeRun("retroarch --version | head -1");
    deckMaybeRun("flatpak list --app");
    printf("%s\n", "OK deck emulator preflight");
    int __btrc_ret_709 = 0;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_709;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

int deckVerifyInputCommand(char* project, bool strict) {
    doctorSteamDeck(project);
    bool ok = true;
    if (FileSystem_exists("/dev/uinput")) {
        printf("%s\n", "OK /dev/uinput");
    } else if (strict) {
        printf("%s\n", "FAIL /dev/uinput missing; virtual-input automation unavailable");
        (ok = false);
    } else {
        printf("%s\n", "INFO /dev/uinput unavailable in this environment");
    }
    if (commandExists("inputplumber")) {
        printf("%s\n", "OK inputplumber");
    } else if (strict) {
        printf("%s\n", "FAIL inputplumber missing; Steam Deck route cannot be verified");
        (ok = false);
    } else {
        printf("%s\n", "INFO inputplumber verification waits for the Deck environment");
    }
    printf("%s\n", (ok ? "OK deck input preflight" : "FAIL deck input preflight"));
    return (ok ? 0 : 2);
}

bool deckWaitForSyncHealth(char* project) {
    UnixShell* shell = UnixShell_new();
    for (int i = 0; (i < 30); (i++)) {
        ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("curl -fsS http://", syncGuiAddress(project))), "/rest/noauth/health >/dev/null 2>&1")));
        if (ExecResult_ok(result)) {
            bool __btrc_ret_710 = true;
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
            return __btrc_ret_710;
        }
        UnixShell_runUnchecked(shell, "sleep 1");
    }
    bool __btrc_ret_711 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_711;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int deckVerifySyncCommand(char* project) {
    syncStatus(project);
    if (!syncSetup(project)) {
        return 1;
    }
    if (!deckWaitForSyncHealth(project)) {
        printf("%s\n", "FAIL deck sync health");
        return 2;
    }
    if (!syncForce(project, "all")) {
        return 1;
    }
    if (!FileSystem_isDir(syncthingHome(project))) {
        printf("%s\n", "FAIL deck sync home missing");
        return 2;
    }
    printf("%s\n", "OK deck sync");
    return 0;
}

int e2eCommand(CliArgs* args) {
    char* mode = "all";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "graph") == 0) {
        return e2eGraphCommand(args);
    }
    if (strcmp(mode, "payload-audit") == 0) {
        return e2ePayloadAudit(CliArgs_valueAfter(args, "--project", "."));
    }
    if (strcmp(mode, "shell-syntax") == 0) {
        return e2eShellSyntaxSmoke(CliArgs_valueAfter(args, "--project", Environment_get("SEMU_PROJECT_DIR", ".")));
    }
    if (strcmp(mode, "appimage") == 0) {
        return e2eAppImageSmoke(args);
    }
    if (strcmp(mode, "sandbox") == 0) {
        return e2eSandboxSmoke();
    }
    if (strcmp(mode, "lifecycle") == 0) {
        return e2eLifecycleSmoke(args);
    }
    if (strcmp(mode, "launcher") == 0) {
        return e2eLauncherSmoke(args);
    }
    if (strcmp(mode, "sync") == 0) {
        return e2eSyncSmoke(args);
    }
    if ((strcmp(mode, "n3ds-nocrypto") == 0) || (strcmp(mode, "utils") == 0)) {
        return e2eN3dsNoCryptoSmoke(args);
    }
    if (strcmp(mode, "all") == 0) {
        int sandboxStatus = e2eSandboxSmoke();
        if (sandboxStatus != 0) {
            return sandboxStatus;
        }
        int launcherStatus = e2eLauncherSmoke(args);
        if (launcherStatus != 0) {
            return launcherStatus;
        }
        int syncStatus = e2eSyncSmoke(args);
        if (syncStatus != 0) {
            return syncStatus;
        }
        int n3dsStatus = e2eN3dsNoCryptoSmoke(args);
        if (n3dsStatus != 0) {
            return n3dsStatus;
        }
        int appImageStatus = e2eAppImageSmoke(args);
        if (appImageStatus != 0) {
            return appImageStatus;
        }
        int shellStatus = e2eShellSyntaxSmoke(CliArgs_valueAfter(args, "--project", Environment_get("SEMU_PROJECT_DIR", ".")));
        if (shellStatus != 0) {
            return shellStatus;
        }
        return e2eLifecycleSmoke(args);
    }
    printUsage();
    return 1;
}

void e2eFatal(char* message) {
    printf("%s\n", __btrc_str_track(__btrc_strcat("error 0:0 ", message)));
    exit(1);
}

char* e2eExpandArgs(char* text, btrc_Map_string_string* args) {
    return JsonText_expand(text, args);
}

void SemuE2eOperation_init(SemuE2eOperation* self) {
    self->__rc = 1;
    (self->kind = "");
    (self->command = "");
    (self->expect = "");
    (self->name = "");
    (self->path = "");
    (self->timeout = 300);
}

SemuE2eOperation* SemuE2eOperation_new(void) {
    SemuE2eOperation* self = ((SemuE2eOperation*)malloc(sizeof(SemuE2eOperation)));
    memset(self, 0, sizeof(SemuE2eOperation));
    SemuE2eOperation_init(self);
    return self;
}

void SemuE2eOperation_destroy(SemuE2eOperation* self) {
    free(self);
}

void SemuE2eOperation_expandArgs(SemuE2eOperation* self, btrc_Map_string_string* args) {
    (self->kind = e2eExpandArgs(self->kind, args));
    (self->command = e2eExpandArgs(self->command, args));
    (self->expect = e2eExpandArgs(self->expect, args));
    (self->name = e2eExpandArgs(self->name, args));
    (self->path = e2eExpandArgs(self->path, args));
}

void SemuE2eSpec_init(SemuE2eSpec* self) {
    self->__rc = 1;
    (self->name = "semu-e2e");
    (self->state = "semu-e2e");
    (self->parentState = "root");
    (self->stateRoot = "tests/vms/e2e-state");
    (self->stateMaterial = "");
    (self->parentHash = "root");
    (self->stateHash = "");
    (self->stateHashShort = "");
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    (self->args = btrc_Map_string_string_new());
    (btrc_Map_string_string_new()->__rc++);
    if (self->operations != NULL) {
        if ((--self->operations->__rc) <= 0) {
            btrc_Vector_SemuE2eOperation_free(self->operations);
        }
    }
    btrc_Vector_SemuE2eOperation* __list_713 = btrc_Vector_SemuE2eOperation_new();
    (self->operations = __list_713);
    btrc_Vector_SemuE2eOperation* __list_712 = btrc_Vector_SemuE2eOperation_new();
    (__list_712->__rc++);
}

SemuE2eSpec* SemuE2eSpec_new(void) {
    SemuE2eSpec* self = ((SemuE2eSpec*)malloc(sizeof(SemuE2eSpec)));
    memset(self, 0, sizeof(SemuE2eSpec));
    SemuE2eSpec_init(self);
    return self;
}

void SemuE2eSpec_destroy(SemuE2eSpec* self) {
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    if (self->operations != NULL) {
        if ((--self->operations->__rc) <= 0) {
            btrc_Vector_SemuE2eOperation_free(self->operations);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void SemuE2eSpec_setArg(SemuE2eSpec* self, char* key, char* value) {
    btrc_Map_string_string_put(self->args, key, value);
}

char* SemuE2eSpec_stateDir(SemuE2eSpec* self) {
    return joinPath(self->stateRoot, self->state);
}

char* SemuE2eSpec_stateHashFile(SemuE2eSpec* self) {
    return joinPath(SemuE2eSpec_stateDir(self), "hash");
}

char* SemuE2eSpec_parentHashFile(SemuE2eSpec* self) {
    return joinPath(joinPath(self->stateRoot, self->parentState), "hash");
}

char* SemuE2eSpec_resolveParentHash(SemuE2eSpec* self) {
    if ((strcmp(self->parentState, "root") == 0) || (((int)strlen(self->parentState)) == 0)) {
        return "root";
    }
    if (FileSystem_exists(SemuE2eSpec_parentHashFile(self))) {
        return __btrc_str_track(__btrc_trim(FileSystem_readText(SemuE2eSpec_parentHashFile(self))));
    }
    return __btrc_str_track(__btrc_strcat("missing:", self->parentState));
}

char* SemuE2eSpec_operationsMaterial(SemuE2eSpec* self) {
    btrc_Vector_string* lines = btrc_Vector_string_new();
    int __n_715 = btrc_Vector_SemuE2eOperation_iterLen(self->operations);
    for (int __i_714 = 0; (__i_714 < __n_715); (__i_714++)) {
        SemuE2eOperation* op = btrc_Vector_SemuE2eOperation_iterGet(self->operations, __i_714);
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("op=", op->kind)));
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("name=", op->name)));
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("command=", op->command)));
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("expect=", op->expect)));
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("path=", op->path)));
        btrc_Vector_string_push(lines, __btrc_str_track(__btrc_strcat("timeout=", Strings_fromInt(op->timeout))));
    }
    return btrc_Vector_string_join(lines, "\n");
}

char* SemuE2eSpec_hashMaterial(SemuE2eSpec* self) {
    char* material = self->stateMaterial;
    if (((int)strlen(material)) == 0) {
        (material = self->name);
    }
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("parent=", self->parentHash)), "\nstate=")), self->state)), "\nmaterial=")), material)), "\noperations=\n")), SemuE2eSpec_operationsMaterial(self))), "\n"));
}

void SemuE2eSpec_computeStateHash(SemuE2eSpec* self) {
    (self->parentHash = SemuE2eSpec_resolveParentHash(self));
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("printf %s ", ShellWords_quote(SemuE2eSpec_hashMaterial(self)))), " | (if command -v sha256sum >/dev/null 2>&1; then sha256sum; else shasum -a 256; fi)")), " | awk '{print $1}'"));
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), command, true, false, "");
    if (!ExecResult_ok(result)) {
        e2eFatal(__btrc_str_track(__btrc_strcat("Failed to compute E2E state hash for ", self->state)));
    }
    (self->stateHash = ExecResult_trimmed(result));
    (self->stateHashShort = __btrc_str_track(__btrc_substring(self->stateHash, 0, 12)));
}

void SemuE2eSpec_setDerivedArgs(SemuE2eSpec* self) {
    btrc_Map_string_string_putIfAbsent(self->args, "name", self->name);
    btrc_Map_string_string_putIfAbsent(self->args, "state", self->state);
    btrc_Map_string_string_putIfAbsent(self->args, "parentState", self->parentState);
    btrc_Map_string_string_putIfAbsent(self->args, "stateRoot", self->stateRoot);
    btrc_Map_string_string_putIfAbsent(self->args, "stateMaterial", self->stateMaterial);
    btrc_Map_string_string_putIfAbsent(self->args, "parentHash", self->parentHash);
    btrc_Map_string_string_putIfAbsent(self->args, "stateHash", self->stateHash);
    btrc_Map_string_string_putIfAbsent(self->args, "stateHashShort", self->stateHashShort);
}

void SemuE2eSpec_refreshDerivedArgs(SemuE2eSpec* self) {
    btrc_Map_string_string_put(self->args, "name", self->name);
    btrc_Map_string_string_put(self->args, "state", self->state);
    btrc_Map_string_string_put(self->args, "parentState", self->parentState);
    btrc_Map_string_string_put(self->args, "stateRoot", self->stateRoot);
    btrc_Map_string_string_put(self->args, "stateMaterial", self->stateMaterial);
    btrc_Map_string_string_put(self->args, "parentHash", self->parentHash);
    btrc_Map_string_string_put(self->args, "stateHash", self->stateHash);
    btrc_Map_string_string_put(self->args, "stateHashShort", self->stateHashShort);
}

void SemuE2eSpec_expandArgs(SemuE2eSpec* self) {
    SemuE2eSpec_setDerivedArgs(self);
    (self->name = e2eExpandArgs(self->name, self->args));
    (self->state = e2eExpandArgs(self->state, self->args));
    (self->parentState = e2eExpandArgs(self->parentState, self->args));
    (self->stateRoot = e2eExpandArgs(self->stateRoot, self->args));
    (self->stateMaterial = e2eExpandArgs(self->stateMaterial, self->args));
    SemuE2eSpec_refreshDerivedArgs(self);
    SemuE2eSpec_computeStateHash(self);
    SemuE2eSpec_refreshDerivedArgs(self);
    int __n_717 = btrc_Vector_SemuE2eOperation_iterLen(self->operations);
    for (int __i_716 = 0; (__i_716 < __n_717); (__i_716++)) {
        SemuE2eOperation* op = btrc_Vector_SemuE2eOperation_iterGet(self->operations, __i_716);
        SemuE2eOperation_expandArgs(op, self->args);
    }
}

void SemuE2eSpec_recordState(SemuE2eSpec* self) {
    ensureDir(SemuE2eSpec_stateDir(self));
    FileSystem_writeText(SemuE2eSpec_stateHashFile(self), __btrc_str_track(__btrc_strcat(self->stateHash, "\n")));
    btrc_Vector_string* __list_719 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_719, jsonStrField("state", self->state));
    btrc_Vector_string_push(__list_719, jsonStrField("parentState", self->parentState));
    btrc_Vector_string_push(__list_719, jsonStrField("parentHash", self->parentHash));
    btrc_Vector_string_push(__list_719, jsonStrField("hash", self->stateHash));
    btrc_Vector_string_push(__list_719, jsonStrField("hashShort", self->stateHashShort));
    FileSystem_writeText(joinPath(SemuE2eSpec_stateDir(self), "metadata.json"), __btrc_str_track(__btrc_strcat(jsonObject(__list_719), "\n")));
}

char* SemuE2eParser_field(char* text, char* key, char* fallback) {
    return JsonText_field(text, key, fallback);
}

int SemuE2eParser_intField(char* text, char* key, int fallback) {
    return JsonText_intField(text, key, fallback);
}

btrc_Vector_string* SemuE2eParser_objectArray(char* text, char* key) {
    return JsonText_objectArray(text, key);
}

btrc_Map_string_string* SemuE2eParser_argsObject(char* text) {
    return JsonText_argsObject(text);
}

SemuE2eOperation* SemuE2eParser_operation(char* objectText) {
    SemuE2eOperation* op = SemuE2eOperation_new();
    (op->kind = SemuE2eParser_field(objectText, "op", ""));
    if (((int)strlen(op->kind)) == 0) {
        (op->kind = SemuE2eParser_field(objectText, "kind", ""));
    }
    (op->command = SemuE2eParser_field(objectText, "command", ""));
    (op->expect = SemuE2eParser_field(objectText, "expect", ""));
    (op->name = SemuE2eParser_field(objectText, "name", ""));
    (op->path = SemuE2eParser_field(objectText, "path", ""));
    (op->timeout = SemuE2eParser_intField(objectText, "timeout", 300));
    return op;
    if (op != NULL) {
        if ((--op->__rc) <= 0) {
            SemuE2eOperation_destroy(op);
        }
    }
}

btrc_Vector_SemuE2eOperation* SemuE2eParser_operations(char* text) {
    btrc_Vector_SemuE2eOperation* result = btrc_Vector_SemuE2eOperation_new();
    int __n_721 = btrc_Vector_string_iterLen(SemuE2eParser_objectArray(text, "operations"));
    for (int __i_720 = 0; (__i_720 < __n_721); (__i_720++)) {
        char* objectText = btrc_Vector_string_iterGet(SemuE2eParser_objectArray(text, "operations"), __i_720);
        btrc_Vector_SemuE2eOperation_push(result, SemuE2eParser_operation(objectText));
    }
    return result;
}

SemuE2eSpec* SemuE2eParser_readSpecFile(char* path) {
    char* text = FileSystem_readText(path);
    SemuE2eSpec* spec = SemuE2eSpec_new();
    (spec->name = SemuE2eParser_field(text, "name", spec->name));
    (spec->state = SemuE2eParser_field(text, "state", spec->name));
    (spec->parentState = SemuE2eParser_field(text, "parentState", spec->parentState));
    (spec->stateRoot = SemuE2eParser_field(text, "stateRoot", spec->stateRoot));
    (spec->stateMaterial = SemuE2eParser_field(text, "stateMaterial", spec->stateMaterial));
    if (spec->args != NULL) {
        if ((--spec->args->__rc) <= 0) {
            btrc_Map_string_string_free(spec->args);
        }
    }
    (spec->args = SemuE2eParser_argsObject(text));
    (SemuE2eParser_argsObject(text)->__rc++);
    if (spec->operations != NULL) {
        if ((--spec->operations->__rc) <= 0) {
            btrc_Vector_SemuE2eOperation_free(spec->operations);
        }
    }
    (spec->operations = SemuE2eParser_operations(text));
    (SemuE2eParser_operations(text)->__rc++);
    return spec;
    if (spec != NULL) {
        if ((--spec->__rc) <= 0) {
            SemuE2eSpec_destroy(spec);
        }
    }
}

ExecutionGraph* SemuE2eParser_readGraphFile(char* path) {
    return GraphParser_readFile(path);
}

btrc_Vector_string* semuE2eOperationCatalog(void) {
    btrc_Vector_string* kinds = btrc_Vector_string_new();
    btrc_Vector_string_push(kinds, "require-command");
    btrc_Vector_string_push(kinds, "artifact");
    btrc_Vector_string_push(kinds, "host");
    btrc_Vector_string_push(kinds, "make");
    btrc_Vector_string_push(kinds, "payload-audit");
    return kinds;
}

int e2ePayloadAudit(char* project) {
    char* forbidden = "'^(Azahar|Cemu|Dolphin|ES-DE|Lime3DS|PCSX2|RetroArch|Ryujinx)(/|$)|(^|/)(ROMs|bios|BIOS|saves|states|nand|sdmc|sdcard|mlc01|originals)(/|$)|\\.(3ds|3dz|cia|cci|cxi|nds|nsp|xci|iso|chd|rvz|wad|wua|wud|ciso|gcz|wbfs|elf|dol|bin|cue|img|qcow2|fd|pkg|rap|z64|n64|v64|sfc|smc|gba|gbc|gb|nes|7z|zip|rar|appimage|AppImage|dmg|keys)$'";
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cd ", ShellWords_quote(project))), " && bad=$(git ls-files | LC_ALL=C grep -E ")), forbidden)), " || true)")), " && if [ -n \"$bad\" ]; then printf 'Forbidden tracked payloads:\\n%s\\n' \"$bad\"; exit 1; fi"));
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), command, true, false, "");
    if (!ExecResult_ok(result)) {
        printf("%s\n", ExecResult_stdout(result));
        return 1;
    }
    printf("%s\n", "OK payload audit");
    return 0;
}

void SemuE2eRunner_init(SemuE2eRunner* self, SemuE2eSpec* spec, char* workspaceRoot, char* program) {
    self->__rc = 1;
    if (self->spec != NULL) {
        if ((--self->spec->__rc) <= 0) {
            SemuE2eSpec_destroy(self->spec);
        }
    }
    (self->spec = spec);
    (spec->__rc++);
    (self->workspaceRoot = workspaceRoot);
    (self->program = program);
    (self->failures = 0);
}

SemuE2eRunner* SemuE2eRunner_new(SemuE2eSpec* spec, char* workspaceRoot, char* program) {
    SemuE2eRunner* self = ((SemuE2eRunner*)malloc(sizeof(SemuE2eRunner)));
    memset(self, 0, sizeof(SemuE2eRunner));
    SemuE2eRunner_init(self, spec, workspaceRoot, program);
    return self;
}

void SemuE2eRunner_destroy(SemuE2eRunner* self) {
    if (self->spec != NULL) {
        if ((--self->spec->__rc) <= 0) {
            SemuE2eSpec_destroy(self->spec);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

void SemuE2eRunner_fail(SemuE2eRunner* self, char* message) {
    (self->failures++);
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("E2E FAIL ", self->spec->name)), ": ")), message)));
}

bool SemuE2eRunner_outputMatches(SemuE2eRunner* self, ExecResult* result, char* expect) {
    if (((int)strlen(expect)) == 0) {
        return true;
    }
    return __btrc_strContains(ExecResult_stdout(result), expect);
}

void SemuE2eRunner_assertResult(SemuE2eRunner* self, char* label, ExecResult* result, char* expect) {
    if (!ExecResult_ok(result)) {
        printf("%s\n", ExecResult_stdout(result));
        SemuE2eRunner_fail(self, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(label, " failed with exit code ")), Strings_fromInt(result->code))));
        return;
    }
    if (!SemuE2eRunner_outputMatches(self, result, expect)) {
        printf("%s\n", ExecResult_stdout(result));
        SemuE2eRunner_fail(self, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(label, " output did not contain expected text: ")), expect)));
    }
}

ExecResult* SemuE2eRunner_runWorkspaceCommandWithTimeout(SemuE2eRunner* self, char* command, int timeout) {
    char* rendered = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cd ", ShellWords_quote(self->workspaceRoot))), " && ")), command)), " 2>&1"));
    if (timeout > 0) {
        char* wrapped = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("perl -e 'alarm shift; exec @ARGV' ", Strings_fromInt(timeout))), " /bin/bash -lc ")), ShellWords_quote(rendered)));
        return UnixShell_runRaw(UnixShell_new(), wrapped, true, false, "");
    }
    return UnixShell_runRaw(UnixShell_new(), rendered, true, false, "");
}

ExecResult* SemuE2eRunner_runWorkspaceCommand(SemuE2eRunner* self, char* command) {
    return SemuE2eRunner_runWorkspaceCommandWithTimeout(self, command, 0);
}

void SemuE2eRunner_runOperation(SemuE2eRunner* self, SemuE2eOperation* op) {
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("e2e ", self->spec->name)), ": ")), op->kind)));
    if (strcmp(op->kind, "require-command") == 0) {
        ExecResult* result = SemuE2eRunner_runWorkspaceCommandWithTimeout(self, __btrc_str_track(__btrc_strcat("command -v ", ShellWords_quote(op->command))), op->timeout);
        SemuE2eRunner_assertResult(self, __btrc_str_track(__btrc_strcat("require command ", op->command)), result, op->expect);
        return;
    }
    if (strcmp(op->kind, "artifact") == 0) {
        ExecResult* result = SemuE2eRunner_runWorkspaceCommandWithTimeout(self, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("test -e ", ShellWords_quote(op->path))), " && printf exists")), op->timeout);
        SemuE2eRunner_assertResult(self, __btrc_str_track(__btrc_strcat("artifact ", op->path)), result, ((((int)strlen(op->expect)) > 0) ? op->expect : "exists"));
        return;
    }
    if (strcmp(op->kind, "host") == 0) {
        ExecResult* result = SemuE2eRunner_runWorkspaceCommandWithTimeout(self, op->command, op->timeout);
        SemuE2eRunner_assertResult(self, "host command", result, op->expect);
        return;
    }
    if (strcmp(op->kind, "make") == 0) {
        ExecResult* result = SemuE2eRunner_runWorkspaceCommandWithTimeout(self, __btrc_str_track(__btrc_strcat("make ", op->command)), op->timeout);
        SemuE2eRunner_assertResult(self, __btrc_str_track(__btrc_strcat("make ", op->command)), result, op->expect);
        return;
    }
    if (strcmp(op->kind, "payload-audit") == 0) {
        int status = e2ePayloadAudit(self->workspaceRoot);
        if (status != 0) {
            SemuE2eRunner_fail(self, "payload audit failed");
        }
        return;
    }
    SemuE2eRunner_fail(self, __btrc_str_track(__btrc_strcat("Unknown E2E operation: ", op->kind)));
}

int SemuE2eRunner_run(SemuE2eRunner* self) {
    if (btrc_Vector_SemuE2eOperation_isEmpty(self->spec->operations)) {
        e2eFatal(__btrc_str_track(__btrc_strcat("E2E spec has no operations: ", self->spec->name)));
    }
    int __n_723 = btrc_Vector_SemuE2eOperation_iterLen(self->spec->operations);
    for (int __i_722 = 0; (__i_722 < __n_723); (__i_722++)) {
        SemuE2eOperation* op = btrc_Vector_SemuE2eOperation_iterGet(self->spec->operations, __i_722);
        if (self->failures > 0) {
            break;
        }
        SemuE2eRunner_runOperation(self, op);
    }
    if (self->failures > 0) {
        return 1;
    }
    SemuE2eSpec_recordState(self->spec);
    return 0;
}

void SemuE2eGraphRunner_init(SemuE2eGraphRunner* self, ExecutionGraph* graph, btrc_Map_string_string* args, char* program) {
    self->__rc = 1;
    if (self->graph != NULL) {
        if ((--self->graph->__rc) <= 0) {
            ExecutionGraph_destroy(self->graph);
        }
    }
    (self->graph = graph);
    (graph->__rc++);
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    (self->args = args);
    (args->__rc++);
    (self->sourceHashValue = "");
    (self->program = program);
}

SemuE2eGraphRunner* SemuE2eGraphRunner_new(ExecutionGraph* graph, btrc_Map_string_string* args, char* program) {
    SemuE2eGraphRunner* self = ((SemuE2eGraphRunner*)malloc(sizeof(SemuE2eGraphRunner)));
    memset(self, 0, sizeof(SemuE2eGraphRunner));
    SemuE2eGraphRunner_init(self, graph, args, program);
    return self;
}

void SemuE2eGraphRunner_destroy(SemuE2eGraphRunner* self) {
    if (self->graph != NULL) {
        if ((--self->graph->__rc) <= 0) {
            ExecutionGraph_destroy(self->graph);
        }
    }
    if (self->args != NULL) {
        if ((--self->args->__rc) <= 0) {
            btrc_Map_string_string_free(self->args);
        }
    }
    if (__btrc_tracking) {
        __btrc_mark_destroyed(self);
    }
    free(self);
}

char* SemuE2eGraphRunner_sourceHash(SemuE2eGraphRunner* self) {
    if (((int)strlen(self->sourceHashValue)) > 0) {
        return self->sourceHashValue;
    }
    char* root = ExecutionGraph_resolvedWorkspaceRoot(self->graph);
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cd ", ShellWords_quote(root))), " && find . -type f")), " ! -path './.git/*'")), " ! -path './.semu/*'")), " ! -path './.venv/*'")), " ! -path './.pytest_cache/*'")), " ! -path './.stfolder/*'")), " ! -path './tests/vms/*'")), " ! -path './build/*'")), " ! -path './backups/*'")), " ! -path './result/*'")), " ! -path './result-*/*'")), " ! -path './Azahar/*'")), " ! -path './Cemu/*'")), " ! -path './Dolphin/*'")), " ! -path './ES-DE/*'")), " ! -path './Lime3DS/*'")), " ! -path './PCSX2/*'")), " ! -path './RetroArch/*'")), " ! -path './Ryujinx/*'")), " ! -path './ES-DE/ES-DE/ROMs/*'")), " ! -path './ES-DE/ES-DE/bios/*'")), " ! -path './ES-DE/ES-DE/saves/*'")), " ! -path './ES-DE/ES-DE/states/*'")), " ! -path './ES-DE/ES-DE/downloaded_media/*'")), " ! -path './ES-DE/ES-DE/screenshots/*'")), " ! -path './sync/bin/*'")), " ! -path './sync/logs/*'")), " ! -path './sync/syncthing/*'")), " ! -name '.DS_Store'")), " ! -name '._*'")), " \\( -name '*.btrc' -o -name '*.nix' -o -name '*.json' -o -name '*.sh' -o -name '*.md' -o -name '*.xml' -o -name '*.ini' -o -name '*.cfg' -o -name '*.desktop' -o -name '*.vdf' -o -name '*.skm' -o -name '*.c' -o -name 'Makefile' -o -name 'Dockerfile' -o -name 'flake.lock' \\)")), " -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256 | shasum -a 256 | awk '{print $1}'"));
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), command, true, false, "");
    if (!ExecResult_ok(result)) {
        e2eFatal(__btrc_str_track(__btrc_strcat("Failed to compute workspace source hash for ", root)));
    }
    char* toolHash = "missing";
    if ((((int)strlen(self->program)) > 0) && FileSystem_exists(self->program)) {
        ExecResult* tool = UnixShell_runRaw(UnixShell_new(), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("shasum -a 256 ", ShellWords_quote(self->program))), " | awk '{print $1}'")), true, false, "");
        if (ExecResult_ok(tool)) {
            (toolHash = ExecResult_trimmed(tool));
        }
    }
    (self->sourceHashValue = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(ExecResult_trimmed(result), ":semu=")), toolHash)));
    return self->sourceHashValue;
}

void SemuE2eGraphRunner_applyStructuralOverrides(SemuE2eGraphRunner* self, SemuE2eSpec* spec, btrc_Map_string_string* overrides) {
    if (btrc_Map_string_string_has(overrides, "name")) {
        (spec->name = btrc_Map_string_string_get(overrides, "name"));
    }
    if (btrc_Map_string_string_has(overrides, "state")) {
        (spec->state = btrc_Map_string_string_get(overrides, "state"));
    }
    if (btrc_Map_string_string_has(overrides, "parentState")) {
        (spec->parentState = btrc_Map_string_string_get(overrides, "parentState"));
    }
    if (btrc_Map_string_string_has(overrides, "stateRoot")) {
        (spec->stateRoot = btrc_Map_string_string_get(overrides, "stateRoot"));
    }
    if (btrc_Map_string_string_has(overrides, "stateMaterial")) {
        (spec->stateMaterial = btrc_Map_string_string_get(overrides, "stateMaterial"));
    }
}

SemuE2eSpec* SemuE2eGraphRunner_specFor(SemuE2eGraphRunner* self, GraphNode* node) {
    SemuE2eSpec* spec = SemuE2eParser_readSpecFile(ExecutionGraph_resolvedSpecPath(self->graph, node));
    SemuE2eSpec_setArg(spec, "graphName", self->graph->name);
    SemuE2eSpec_setArg(spec, "nodeId", node->id);
    SemuE2eSpec_setArg(spec, "workspaceRoot", ExecutionGraph_resolvedWorkspaceRoot(self->graph));
    SemuE2eSpec_setArg(spec, "sourceHash", SemuE2eGraphRunner_sourceHash(self));
    SemuE2eSpec_setArg(spec, "program", self->program);
    SemuE2eGraphRunner_applyStructuralOverrides(self, spec, node->args);
    SemuE2eGraphRunner_applyStructuralOverrides(self, spec, self->args);
    int __n_725 = btrc_Map_string_string_iterLen(node->args);
    for (int __i_724 = 0; (__i_724 < __n_725); (__i_724++)) {
        char* key = btrc_Map_string_string_iterGet(node->args, __i_724);
        char* value = btrc_Map_string_string_iterValueAt(node->args, __i_724);
        SemuE2eSpec_setArg(spec, key, value);
    }
    int __n_727 = btrc_Map_string_string_iterLen(self->args);
    for (int __i_726 = 0; (__i_726 < __n_727); (__i_726++)) {
        char* key = btrc_Map_string_string_iterGet(self->args, __i_726);
        char* value = btrc_Map_string_string_iterValueAt(self->args, __i_726);
        SemuE2eSpec_setArg(spec, key, value);
    }
    SemuE2eSpec_expandArgs(spec);
    return spec;
}

bool SemuE2eGraphRunner_force(SemuE2eGraphRunner* self) {
    return (strcmp(btrc_Map_string_string_getOrDefault(self->args, "force", "false"), "true") == 0);
}

bool SemuE2eGraphRunner_ready(SemuE2eGraphRunner* self, SemuE2eSpec* spec) {
    if (!FileSystem_exists(SemuE2eSpec_stateHashFile(spec))) {
        return false;
    }
    return (strcmp(__btrc_str_track(__btrc_trim(FileSystem_readText(SemuE2eSpec_stateHashFile(spec)))), spec->stateHash) == 0);
}

void SemuE2eGraphRunner_list(SemuE2eGraphRunner* self) {
    GraphReport_list(self->graph);
}

void SemuE2eGraphRunner_status(SemuE2eGraphRunner* self) {
    int __n_729 = btrc_Vector_GraphNode_iterLen(self->graph->nodes);
    for (int __i_728 = 0; (__i_728 < __n_729); (__i_728++)) {
        GraphNode* node = btrc_Vector_GraphNode_iterGet(self->graph->nodes, __i_728);
        SemuE2eSpec* spec = SemuE2eGraphRunner_specFor(self, node);
        char* recorded = "missing";
        if (FileSystem_exists(SemuE2eSpec_stateHashFile(spec))) {
            char* saved = __btrc_str_track(__btrc_trim(FileSystem_readText(SemuE2eSpec_stateHashFile(spec))));
            (recorded = ((strcmp(saved, spec->stateHash) == 0) ? "ready" : "stale"));
        }
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(node->id, " ")), recorded)), " ")), spec->stateHashShort)), " ")), SemuE2eSpec_stateDir(spec))));
    }
}

int SemuE2eGraphRunner_operationCoverage(SemuE2eGraphRunner* self) {
    btrc_Vector_string* covered = btrc_Vector_string_new();
    int __n_731 = btrc_Vector_GraphNode_iterLen(self->graph->nodes);
    for (int __i_730 = 0; (__i_730 < __n_731); (__i_730++)) {
        GraphNode* node = btrc_Vector_GraphNode_iterGet(self->graph->nodes, __i_730);
        SemuE2eSpec* spec = SemuE2eParser_readSpecFile(ExecutionGraph_resolvedSpecPath(self->graph, node));
        int __n_733 = btrc_Vector_SemuE2eOperation_iterLen(spec->operations);
        for (int __i_732 = 0; (__i_732 < __n_733); (__i_732++)) {
            SemuE2eOperation* op = btrc_Vector_SemuE2eOperation_iterGet(spec->operations, __i_732);
            if ((((int)strlen(op->kind)) > 0) && (!btrc_Vector_string_contains(covered, op->kind))) {
                btrc_Vector_string_push(covered, op->kind);
            }
        }
    }
    btrc_Vector_string* missing = btrc_Vector_string_new();
    btrc_Vector_string* catalog = semuE2eOperationCatalog();
    int __n_735 = btrc_Vector_string_iterLen(catalog);
    for (int __i_734 = 0; (__i_734 < __n_735); (__i_734++)) {
        char* kind = btrc_Vector_string_iterGet(catalog, __i_734);
        if (!btrc_Vector_string_contains(covered, kind)) {
            btrc_Vector_string_push(missing, kind);
        }
    }
    if (!btrc_Vector_string_isEmpty(missing)) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("Missing e2e operation coverage: ", btrc_Vector_string_join(missing, ", "))));
        return 1;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("E2E operation coverage: ", Strings_fromInt(covered->len))), "/")), Strings_fromInt(catalog->len))));
    return 0;
}

int SemuE2eGraphRunner_run(SemuE2eGraphRunner* self, btrc_Vector_string* targets) {
    GraphTraversal* traversal = GraphTraversal_new(self->graph);
    btrc_Vector_string* order = GraphTraversal_order(traversal, targets);
    if (!GraphTraversal_ok(traversal)) {
        e2eFatal(traversal->error);
    }
    int __n_737 = btrc_Vector_string_iterLen(order);
    for (int __i_736 = 0; (__i_736 < __n_737); (__i_736++)) {
        char* id = btrc_Vector_string_iterGet(order, __i_736);
        GraphNode* node = ExecutionGraph_node(self->graph, id);
        SemuE2eSpec* spec = SemuE2eGraphRunner_specFor(self, node);
        if ((!SemuE2eGraphRunner_force(self)) && SemuE2eGraphRunner_ready(self, spec)) {
            printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("graph ", self->graph->name)), ": skip ")), node->id)), " -> ")), spec->state)), "@")), spec->stateHashShort)));
            continue;
        }
        printf("%s\n", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("graph ", self->graph->name)), ": run ")), node->id)), " -> ")), spec->state)), "@")), spec->stateHashShort)));
        SemuE2eRunner* runner = SemuE2eRunner_new(spec, ExecutionGraph_resolvedWorkspaceRoot(self->graph), self->program);
        int result = SemuE2eRunner_run(runner);
        if (result != 0) {
            if (runner != NULL) {
                if ((--runner->__rc) <= 0) {
                    SemuE2eRunner_destroy(runner);
                }
            }
            if (traversal != NULL) {
                if ((--traversal->__rc) <= 0) {
                    GraphTraversal_destroy(traversal);
                }
            }
            return result;
        }
        if (runner != NULL) {
            if ((--runner->__rc) <= 0) {
                SemuE2eRunner_destroy(runner);
            }
        }
    }
    int __btrc_ret_738 = 0;
    if (traversal != NULL) {
        if ((--traversal->__rc) <= 0) {
            GraphTraversal_destroy(traversal);
        }
    }
    return __btrc_ret_738;
    if (traversal != NULL) {
        if ((--traversal->__rc) <= 0) {
            GraphTraversal_destroy(traversal);
        }
    }
}

btrc_Map_string_string* e2eGraphArgs(CliArgs* args, int startIndex) {
    return GraphCli_args(args, startIndex);
}

btrc_Vector_string* e2eGraphTargets(CliArgs* args, int startIndex) {
    return GraphCli_targets(args, startIndex);
}

int e2eGraphCommand(CliArgs* args) {
    if (CliArgs_count(args) < 4) {
        printf("%s\n", "Usage: semu e2e graph <graph.json> <list|status|coverage|run> [node ...] [--arg key=value]");
        return 1;
    }
    ExecutionGraph* graph = SemuE2eParser_readGraphFile(CliArgs_get(args, 2));
    char* action = CliArgs_get(args, 3);
    btrc_Map_string_string* overrides = e2eGraphArgs(args, 4);
    SemuE2eGraphRunner* runner = SemuE2eGraphRunner_new(graph, overrides, args->program);
    if (strcmp(action, "list") == 0) {
        SemuE2eGraphRunner_list(runner);
        int __btrc_ret_739 = 0;
        if (runner != NULL) {
            if ((--runner->__rc) <= 0) {
                SemuE2eGraphRunner_destroy(runner);
            }
        }
        return __btrc_ret_739;
    }
    if (strcmp(action, "status") == 0) {
        SemuE2eGraphRunner_status(runner);
        int __btrc_ret_740 = 0;
        if (runner != NULL) {
            if ((--runner->__rc) <= 0) {
                SemuE2eGraphRunner_destroy(runner);
            }
        }
        return __btrc_ret_740;
    }
    if (strcmp(action, "coverage") == 0) {
        int __btrc_ret_741 = SemuE2eGraphRunner_operationCoverage(runner);
        if (runner != NULL) {
            if ((--runner->__rc) <= 0) {
                SemuE2eGraphRunner_destroy(runner);
            }
        }
        return __btrc_ret_741;
    }
    if (strcmp(action, "run") == 0) {
        int __btrc_ret_742 = SemuE2eGraphRunner_run(runner, e2eGraphTargets(args, 4));
        if (runner != NULL) {
            if ((--runner->__rc) <= 0) {
                SemuE2eGraphRunner_destroy(runner);
            }
        }
        return __btrc_ret_742;
    }
    printf("%s\n", "Usage: semu e2e graph <graph.json> <list|status|coverage|run> [node ...] [--arg key=value]");
    int __btrc_ret_743 = 1;
    if (runner != NULL) {
        if ((--runner->__rc) <= 0) {
            SemuE2eGraphRunner_destroy(runner);
        }
    }
    return __btrc_ret_743;
    if (runner != NULL) {
        if ((--runner->__rc) <= 0) {
            SemuE2eGraphRunner_destroy(runner);
        }
    }
}

bool e2eOk(bool condition, char* message) {
    if (condition) {
        return true;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("E2E FAIL ", message)));
    return false;
}

bool e2eContains(char* text, char* expected, char* message) {
    return e2eOk(__btrc_strContains(text, expected), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(message, ": missing ")), expected)));
}

char* e2eTempDir(char* label) {
    char* tmp = FileSystem_tempDir(label);
    if (((int)strlen(tmp)) > 0) {
        return tmp;
    }
    char* fallback = joinPath(Environment_get("TMPDIR", "/tmp"), __btrc_str_track(__btrc_strcat(label, "-fallback")));
    FileSystem_removeRecursive(fallback);
    ensureDir(fallback);
    return fallback;
}

void e2eSeedFile(char* path) {
    ensureDir(PathTools_dirname(path));
    FileSystem_writeText(path, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("seed ", path)), "\n")));
}

bool e2eAssertLink(char* linkPath, char* targetPath) {
    if (!e2eOk(FileSystem_isSymlink(linkPath), __btrc_str_track(__btrc_strcat("expected symlink ", linkPath)))) {
        return false;
    }
    char* target = FileSystem_readLink(linkPath);
    if (!e2eOk((((int)strlen(target)) > 0), __btrc_str_track(__btrc_strcat("readlink failed ", linkPath)))) {
        return false;
    }
    return e2eOk((strcmp(target, targetPath) == 0), __btrc_str_track(__btrc_strcat("bad symlink target for ", linkPath)));
}

ExecResult* e2eRunLifecycle(char* exe, char* home, char* mode, char* project, char* romsDir) {
    Command* command = Command_check(Command_capture(Command_flag(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), mode), "--project", project), true), false);
    if (((int)strlen(romsDir)) > 0) {
        Command_flag(command, "--roms", romsDir);
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunLifecycleChange(char* exe, char* home, char* project, char* actionId, char* commandText) {
    Command* command = Command_check(Command_capture(Command_flag(Command_flag(Command_flag(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), "change"), "--project", project), "--action", actionId), "--command", commandText), true), false);
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunLifecycleArgs(char* exe, char* home, char* project, btrc_Vector_string* lifecycleArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), true), false);
    int __n_745 = btrc_Vector_string_iterLen(lifecycleArgs);
    for (int __i_744 = 0; (__i_744 < __n_745); (__i_744++)) {
        char* arg = btrc_Vector_string_iterGet(lifecycleArgs, __i_744);
        Command_arg(command, arg);
    }
    Command_flag(command, "--project", project);
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunSync(char* exe, char* home, char* binDir, char* project, btrc_Vector_string* syncArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "PATH", __btrc_str_track(__btrc_strcat(binDir, ":/usr/bin:/bin"))), "SEMU_CAPTURE", joinPath(PathTools_dirname(binDir), "capture")), "sync"), true), false);
    int __n_747 = btrc_Vector_string_iterLen(syncArgs);
    for (int __i_746 = 0; (__i_746 < __n_747); (__i_746++)) {
        char* arg = btrc_Vector_string_iterGet(syncArgs, __i_746);
        Command_arg(command, arg);
    }
    Command_flag(command, "--project", project);
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunSteamInput(char* exe, char* home, char* project, btrc_Vector_string* steamInputArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "steam-input"), true), false);
    int __n_749 = btrc_Vector_string_iterLen(steamInputArgs);
    for (int __i_748 = 0; (__i_748 < __n_749); (__i_748++)) {
        char* arg = btrc_Vector_string_iterGet(steamInputArgs, __i_748);
        Command_arg(command, arg);
    }
    Command_flag(command, "--project", project);
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunLauncher(char* exe, char* home, char* project, char* roms, char* binDir, char* captureDir, char* bwrapPath, char* emulator, btrc_Vector_string* emulatorArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "SEMU_PROJECT_DIR", project), "SEMU_ROMS_DIR", roms), "SEMU_FLATPAK_CAPTURE", captureDir), "SEMU_BWRAP", bwrapPath), "SEMU_SCREENSHOT_HOOKS", "1"), "SEMU_SCREENSHOT_DELAY_SECONDS", "0"), "PATH", __btrc_str_track(__btrc_strcat(binDir, ":/usr/bin:/bin"))), "WAYLAND_DISPLAY", "wayland-test"), "launcher"), emulator), true), false);
    int __n_751 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_750 = 0; (__i_750 < __n_751); (__i_750++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_750);
        Command_arg(command, arg);
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunRoutedLauncher(char* exe, char* home, char* project, char* roms, char* binDir, char* captureDir, char* emulator, char* routedExe, btrc_Vector_string* emulatorArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_arg(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SEMU_HOME", home), "SEMU_BIN", exe), "SEMU_PROJECT_DIR", project), "SEMU_ROMS_DIR", roms), "SEMU_FLATPAK_CAPTURE", captureDir), "SEMU_SCREENSHOT_HOOKS", "0"), "PATH", __btrc_str_track(__btrc_strcat(binDir, ":/usr/bin:/bin"))), "launcher"), "routed"), emulator), routedExe), true), false);
    int __n_753 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_752 = 0; (__i_752 < __n_753); (__i_752++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_752);
        Command_arg(command, arg);
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool e2eRunOk(ExecResult* result, char* label) {
    if (ExecResult_ok(result)) {
        return true;
    }
    printf("%s\n", __btrc_str_track(__btrc_strcat("E2E FAIL ", label)));
    printf("%s\n", ExecResult_stdout(result));
    return false;
}

bool e2eWaitForFile(char* path, char* label) {
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("for i in 1 2 3 4 5; do [ -f ", ShellWords_quote(path))), " ] && exit 0; sleep 0.1; done; exit 1")), false, false, "");
    bool __btrc_ret_754 = e2eOk(ExecResult_ok(result), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(label, ": missing ")), path)));
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_754;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool e2eCatalogConsistency(char* project, char* launcherBin) {
    char* systems = FileSystem_readText(joinPath(customSystemsRoot(project), "es_systems.xml"));
    char* rules = FileSystem_readText(joinPath(customSystemsRoot(project), "es_find_rules.xml"));
    int __n_756 = btrc_Vector_string_iterLen(declaredSystemIds());
    for (int __i_755 = 0; (__i_755 < __n_756); (__i_755++)) {
        char* id = btrc_Vector_string_iterGet(declaredSystemIds(), __i_755);
        if (!e2eContains(systems, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<name>", id)), "</name>")), __btrc_str_track(__btrc_strcat("ES-DE system catalog ", id)))) {
            return false;
        }
    }
    int __n_758 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_757 = 0; (__i_757 < __n_758); (__i_757++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_757);
        char* launcher = joinPath(launcherBin, semuLauncherName(emulator));
        if (!e2eContains(rules, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<emulator name=\"", emulator)), "\">")), __btrc_str_track(__btrc_strcat("ES-DE find rule ", emulator)))) {
            return false;
        }
        if (!e2eContains(rules, launcher, __btrc_str_track(__btrc_strcat("ES-DE launcher path ", emulator)))) {
            return false;
        }
        if (!e2eOk(FileSystem_exists(launcher), __btrc_str_track(__btrc_strcat("launcher missing ", launcher)))) {
            return false;
        }
    }
    return true;
}

int e2eLifecycleSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("semu-lifecycle-smoke");
    char* project = joinPath(tmp, "project");
    char* home = joinPath(tmp, "home");
    char* romsOne = joinPath(tmp, "roms-one");
    char* romsTwo = joinPath(tmp, "roms-two");
    char* exe = Environment_get("SEMU_BIN", args->program);
    ensureDir(project);
    ensureDir(home);
    ensureDir(joinPath(romsOne, "existing"));
    ensureDir(joinPath(romsTwo, "existing"));
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "install", project, romsOne), "install")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "install", project, romsOne), "install idempotent")) {
        return 1;
    }
    if (!e2eCatalogConsistency(project, linuxLauncherBin(project))) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(romsOne, "gba"))), "install mutated external ROM dirs")) {
        return 1;
    }
    if (!e2eOk(FileSystem_isDir(joinPath(romsRoot(project), "gba")), "install did not create project fallback ROM dirs")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(syncConfigPath(project)), romsOne, "sync config after install")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(screenshotConfigPath(project)), "output_pattern", "screenshot config after install")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(home, ".local/share/applications/semu.desktop")), "desktop entry missing after install")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(home, ".config/systemd/user/semu-syncthing.service")), "systemd service missing after install")) {
        return 1;
    }
    char* steamTemplates = joinPath(home, "steam-templates");
    btrc_Vector_string* __list_759 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_759, "install");
    btrc_Vector_string_push(__list_759, "--dest");
    btrc_Vector_string_push(__list_759, steamTemplates);
    if (!e2eRunOk(e2eRunSteamInput(exe, home, project, __list_759), "steam-input install")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(steamTemplates, "neptune-simple.vdf")), "\"controller_type\"\t\t\"controller_neptune\"", "installed Steam Input simple template")) {
        return 1;
    }
    e2eSeedFile(joinPath(contentRoot(project), "saves/gba/game.sav"));
    e2eSeedFile(joinPath(biosRoot(project), "ps2/ps2-0230a-20080220.bin"));
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "reconfigure", project, romsTwo), "reconfigure")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(syncConfigPath(project)), romsTwo, "sync config after reconfigure")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "ES-DE/es_settings.xml")), romsTwo, "ES-DE settings after reconfigure")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(romsOne, "existing")), "original ROM root sentinel missing")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(romsTwo, "gba"))), "reconfigure mutated external ROM dirs")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(contentRoot(project), "saves/gba/game.sav")), "save should survive reconfigure")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(biosRoot(project), "ps2/ps2-0230a-20080220.bin")), "BIOS should survive reconfigure")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycleChange(exe, home, project, "state.save", "Ctrl+V"), "change keymap")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(keymapSourcePath(project)), "action state.save = Ctrl+V", "keymap source after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(emulatorProfilePath(project, "RetroArch/retroarch.cfg")), "input_save_state = \"v\"", "RetroArch keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(emulatorProfilePath(project, "Dolphin/config/Profiles/Hotkeys/Steam Deck.ini")), "Save State/Save State Slot 1 = @(Ctrl+V)", "Dolphin keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(emulatorProfilePath(project, "PCSX2/config/inputprofiles/Steam Deck.ini")), "SaveStateToSlot = Keyboard/Control & Keyboard/V", "PCSX2 keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "input/steam-input/neptune-full.vdf")), "key_press V, Save State", "Steam Input keymap after change")) {
        return 1;
    }
    char* keymapBeforeFailure = FileSystem_readText(keymapSourcePath(project));
    char* retroarchBeforeFailure = FileSystem_readText(emulatorProfilePath(project, "RetroArch/retroarch.cfg"));
    btrc_Vector_string* __list_760 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_760, "change");
    btrc_Vector_string_push(__list_760, "--action");
    btrc_Vector_string_push(__list_760, "state.save");
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleArgs(exe, home, project, __list_760))), "lifecycle change without command unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleChange(exe, home, project, "missing.action", "Ctrl+B"))), "unknown keymap action unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleChange(exe, home, project, "state.save", "Ctrl"))), "invalid keymap command unexpectedly succeeded")) {
        return 1;
    }
    btrc_Vector_string* __list_761 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_761, "definitely-not-a-mode");
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleArgs(exe, home, project, __list_761))), "unknown lifecycle mode unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((strcmp(FileSystem_readText(keymapSourcePath(project)), keymapBeforeFailure) == 0), "failed keymap change mutated source")) {
        return 1;
    }
    if (!e2eOk((strcmp(FileSystem_readText(emulatorProfilePath(project, "RetroArch/retroarch.cfg")), retroarchBeforeFailure) == 0), "failed keymap change mutated RetroArch config")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "uninstall", project, ""), "uninstall")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(home, ".local/share/applications/semu.desktop"))), "uninstall clears desktop entry")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(home, ".config/systemd/user/semu-syncthing.service"))), "uninstall clears systemd service")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(syncConfigPath(project)), "sync config should survive uninstall")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(contentRoot(project), "saves/gba/game.sav")), "save should survive uninstall")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(biosRoot(project), "ps2/ps2-0230a-20080220.bin")), "BIOS should survive uninstall")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "reinstall", project, ""), "reinstall")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(home, ".local/share/applications/semu.desktop")), "desktop entry missing after reinstall")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(syncConfigPath(project)), romsTwo, "sync config after reinstall")) {
        return 1;
    }
    FileSystem_writeText(joinPath(project, "semu.json"), "{\"previous_manifest\": true}\n");
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "upgrade", project, ""), "upgrade")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(upgradeBackupPath(project)), "\"previous_manifest\": true", "upgrade backup")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "semu.json")), "\"schema_version\": 1", "regenerated manifest")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(keymapSourcePath(project)), "action state.save = Ctrl+V", "keymap should survive upgrade")) {
        return 1;
    }
    btrc_Vector_string* __list_762 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_762, "uninstall");
    btrc_Vector_string_push(__list_762, "--purge-generated");
    btrc_Vector_string_push(__list_762, "--purge-state");
    if (!e2eRunOk(e2eRunLifecycleArgs(exe, home, project, __list_762), "purge uninstall")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(project, "ES-DE/custom_systems"))), "custom systems should be purged")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(lifecycleStatePath(project))), "lifecycle state should be purged")) {
        return 1;
    }
    printf("%s\n", "OK BTRC lifecycle smoke");
    return 0;
}

bool e2ePrepareSandbox(char* project, char* scratchRoot, char* emulator) {
    char* scratch = joinPath(scratchRoot, emulator);
    FileSystem_removeRecursive(scratch);
    char* emuDir = sandboxProjectEmulatorDir(project, emulator);
    if (!e2eOk((((int)strlen(emuDir)) > 0), __btrc_str_track(__btrc_strcat("missing emulator dir ", emulator)))) {
        return false;
    }
    ensureDir(scratch);
    if (!e2eOk(sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch), __btrc_str_track(__btrc_strcat("missing sandbox spec ", emulator)))) {
        return false;
    }
    if (!e2eOk(sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch), __btrc_str_track(__btrc_strcat("sandbox idempotency failed ", emulator)))) {
        return false;
    }
    return true;
}

int e2eSandboxSmoke(void) {
    char* tmp = e2eTempDir("semu-sandbox-smoke");
    char* project = joinPath(tmp, "project");
    char* scratchRoot = joinPath(tmp, "scratch");
    ensureDir(project);
    ensureDir(scratchRoot);
    e2eSeedFile(emulatorProfilePath(project, "Azahar/data/qt-config.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "azahar")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "azahar/.local/share/azahar-emu/qt-config.ini"), emulatorProfilePath(project, "Azahar/data/qt-config.ini"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "Cemu/config/settings.xml"));
    e2eSeedFile(emulatorProfilePath(project, "Cemu/data/keys.txt"));
    if (!e2ePrepareSandbox(project, scratchRoot, "cemu")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "cemu/.config/Cemu/settings.xml"), emulatorProfilePath(project, "Cemu/config/settings.xml"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "cemu/.local/share/Cemu/keys.txt"), emulatorProfilePath(project, "Cemu/data/keys.txt"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "Dolphin/config/Dolphin.ini"));
    e2eSeedFile(emulatorProfilePath(project, "Dolphin/data/Wii/title.dat"));
    if (!e2ePrepareSandbox(project, scratchRoot, "dolphin")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "dolphin/.config/dolphin-emu/Dolphin.ini"), emulatorProfilePath(project, "Dolphin/config/Dolphin.ini"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "dolphin/.local/share/dolphin-emu/Wii"), emulatorProfilePath(project, "Dolphin/data/Wii"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "ES-DE/ES-DE/ROMs/.keep"));
    e2eSeedFile(joinPath(project, "ES-DE/es_settings.xml"));
    e2eSeedFile(joinPath(project, "ES-DE/custom_systems/es_find_rules.xml"));
    if (!e2ePrepareSandbox(project, scratchRoot, "es-de")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "es-de/ES-DE/ROMs"), joinPath(project, "ES-DE/ES-DE/ROMs"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "es-de/ES-DE/settings/es_settings.xml"), joinPath(project, "ES-DE/es_settings.xml"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "es-de/ES-DE/custom_systems/es_find_rules.xml"), joinPath(project, "ES-DE/custom_systems/es_find_rules.xml"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "Lime3DS/data/qt-config.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "lime3ds")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "lime3ds/.config/lime3ds-emu/qt-config.ini"), emulatorProfilePath(project, "Lime3DS/data/qt-config.ini"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "PCSX2/config/PCSX2.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "pcsx2")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "pcsx2/.config/PCSX2/PCSX2.ini"), emulatorProfilePath(project, "PCSX2/config/PCSX2.ini"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "RetroArch/config/input.cfg"));
    e2eSeedFile(emulatorProfilePath(project, "RetroArch/retroarch.cfg"));
    if (!e2ePrepareSandbox(project, scratchRoot, "retroarch")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "retroarch/.config/retroarch/input.cfg"), emulatorProfilePath(project, "RetroArch/config/input.cfg"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "retroarch/.config/retroarch/retroarch.cfg"), emulatorProfilePath(project, "RetroArch/retroarch.cfg"))) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "Ryujinx/config/Config.json"));
    if (!e2ePrepareSandbox(project, scratchRoot, "ryujinx")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "ryujinx/.config/Ryujinx/Config.json"), emulatorProfilePath(project, "Ryujinx/config/Config.json"))) {
        return 1;
    }
    if (!e2eOk((((int)strlen(sandboxProjectEmulatorDir(project, "missing"))) == 0), "missing emulator unexpectedly resolved")) {
        return 1;
    }
    ensureDir(joinPath(project, "Foo"));
    if (!e2eOk((!sandboxApplyKnownLinks("foo", joinPath(project, "Foo"), joinPath(scratchRoot, "foo"))), "unknown emulator spec unexpectedly succeeded")) {
        return 1;
    }
    printf("%s\n", "OK BTRC sandbox smoke");
    return 0;
}

void e2eWriteSyncFakes(char* binDir) {
    char* fakeSystemctl = joinPath(binDir, "systemctl");
    FileSystem_writeText(fakeSystemctl, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SEMU_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SEMU_CAPTURE/systemctl.log\"\n")));
    FileSystem_chmod(fakeSystemctl, 493);
    char* fakeCurl = joinPath(binDir, "curl");
    FileSystem_writeText(fakeCurl, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SEMU_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SEMU_CAPTURE/curl.log\"\n")));
    FileSystem_chmod(fakeCurl, 493);
    char* fakeSyncthing = joinPath(binDir, "syncthing");
    FileSystem_writeText(fakeSyncthing, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SEMU_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SEMU_CAPTURE/syncthing.log\"\n")), "if [ \"${1:-}\" = generate ]; then\n")), "  home=\"\"\n")), "  while [ $# -gt 0 ]; do\n")), "    if [ \"$1\" = -H ]; then home=\"$2\"; shift 2; else shift; fi\n")), "  done\n")), "  mkdir -p \"$home\"\n")), "  printf '<configuration><gui><apikey>test-key</apikey></gui></configuration>\\n' > \"$home/config.xml\"\n")), "  exit 0\n")), "fi\n")), "exit 0\n")));
    FileSystem_chmod(fakeSyncthing, 493);
}

int e2eSyncSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("semu-sync-smoke");
    char* project = joinPath(tmp, "project");
    char* missingProject = joinPath(tmp, "missing-project");
    char* home = joinPath(tmp, "home");
    char* binDir = joinPath(tmp, "bin");
    char* capture = joinPath(tmp, "capture");
    char* exe = Environment_get("SEMU_BIN", args->program);
    ensureDir(project);
    ensureDir(missingProject);
    ensureDir(home);
    ensureDir(binDir);
    ensureDir(capture);
    e2eWriteSyncFakes(binDir);
    btrc_Vector_string* __list_763 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_763, "setup");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_763), "sync setup")) {
        return 1;
    }
    char* systemctlLog = FileSystem_readText(joinPath(capture, "systemctl.log"));
    char* syncService = FileSystem_readText(joinPath(home, ".config/systemd/user/semu-syncthing.service"));
    char* syncForceScript = FileSystem_readText(joinPath(project, "sync/bin/sync-force.sh"));
    if (!e2eContains(syncService, " sync serve --project ", "sync service stable semu entrypoint")) {
        return 1;
    }
    if (!e2eContains(syncForceScript, " sync force all --project ", "sync force stable semu entrypoint")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user daemon-reload", "sync daemon-reload")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable semu-syncthing.service", "sync enable service")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable semu-sync-force.timer", "sync enable timer")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user start semu-syncthing.service", "sync start service")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "syncthing.log")), "semu-emulator_state", "sync emulator state folder")) {
        return 1;
    }
    btrc_Vector_string* __list_764 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_764, "force");
    btrc_Vector_string_push(__list_764, "all");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_764), "sync force all")) {
        return 1;
    }
    btrc_Vector_string* __list_765 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_765, "force");
    btrc_Vector_string_push(__list_765, "saves");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_765), "sync force saves")) {
        return 1;
    }
    char* curlLog = FileSystem_readText(joinPath(capture, "curl.log"));
    if (!e2eContains(curlLog, "/rest/db/scan", "sync force all url")) {
        return 1;
    }
    if (!e2eContains(curlLog, "?folder=semu-saves", "sync force saves url")) {
        return 1;
    }
    if (!e2eContains(curlLog, "X-API-Key: test-key", "sync force api key")) {
        return 1;
    }
    btrc_Vector_string* __list_766 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_766, "autostart");
    btrc_Vector_string_push(__list_766, "disable");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_766), "sync autostart disable")) {
        return 1;
    }
    btrc_Vector_string* __list_767 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_767, "autostart");
    btrc_Vector_string_push(__list_767, "enable");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_767), "sync autostart enable")) {
        return 1;
    }
    (systemctlLog = FileSystem_readText(joinPath(capture, "systemctl.log")));
    if (!e2eContains(systemctlLog, "--user disable semu-syncthing.service", "sync disable service")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable semu-syncthing.service", "sync re-enable service")) {
        return 1;
    }
    btrc_Vector_string* __list_768 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_768, "force");
    btrc_Vector_string_push(__list_768, "all");
    if (!e2eOk((!ExecResult_ok(e2eRunSync(exe, home, binDir, missingProject, __list_768))), "sync force without API key unexpectedly succeeded")) {
        return 1;
    }
    printf("%s\n", "OK BTRC sync smoke");
    return 0;
}

int e2eLauncherSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("semu-launcher-smoke");
    char* project = joinPath(tmp, "project");
    char* roms = joinPath(tmp, "roms");
    char* home = joinPath(tmp, "home");
    char* capture = joinPath(tmp, "capture");
    char* binDir = joinPath(tmp, "bin");
    char* exe = Environment_get("SEMU_BIN", args->program);
    ensureDir(project);
    ensureDir(roms);
    ensureDir(home);
    ensureDir(capture);
    ensureDir(binDir);
    char* fakeFlatpak = joinPath(binDir, "flatpak");
    FileSystem_writeText(fakeFlatpak, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "capture=\"${SEMU_FLATPAK_CAPTURE:?}\"\n")), "mkdir -p \"$capture\"\n")), "idx=\"$(find \"$capture\" -type f -name 'flatpak-*.args' | wc -l | tr -d ' ')\"\n")), "idx=\"$((idx + 1))\"\n")), "printf '%s\\n' \"$@\" > \"$capture/flatpak-$idx.args\"\n")));
    FileSystem_chmod(fakeFlatpak, 493);
    char* fakeBwrap = joinPath(binDir, "bwrap");
    FileSystem_writeText(fakeBwrap, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "printf '%s\\n' \"$@\" > \"${SEMU_FLATPAK_CAPTURE:?}/retroarch.args\"\n")));
    FileSystem_chmod(fakeBwrap, 493);
    char* fakeGrim = joinPath(binDir, "grim");
    FileSystem_writeText(fakeGrim, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "out=\"${1:?}\"\n")), "mkdir -p \"$(dirname \"$out\")\"\n")), "printf 'fake screenshot %s\\n' \"$out\" > \"$out\"\n")), "printf '%s\\n' \"$out\" >> \"${SEMU_FLATPAK_CAPTURE:?}/grim.log\"\n")));
    FileSystem_chmod(fakeGrim, 493);
    btrc_Vector_string* __list_769 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_769, "game.wua");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "cemu", __list_769), "launcher cemu")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "run", "cemu flatpak args")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), __btrc_str_track(__btrc_strcat("--filesystem=", project)), "cemu project grant")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), __btrc_str_track(__btrc_strcat("--filesystem=", roms)), "cemu rom grant")) {
        return 1;
    }
    char* cemuState = launcherFlatpakStateRoot(project, "cemu");
    if (!e2eOk(FileSystem_isDir(joinPath(cemuState, "config")), "cemu flatpak config state missing")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("--filesystem=", cemuState)), ":create")), "cemu state grant")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), __btrc_str_track(__btrc_strcat("--env=XDG_CONFIG_HOME=", joinPath(cemuState, "config"))), "cemu config env")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "--socket=wayland", "cemu wayland")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "info.cemu.Cemu", "cemu flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "-f", "cemu fullscreen flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "-g", "cemu game flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-1.args")), "game.wua", "cemu rom arg")) {
        return 1;
    }
    char* cemuScreens = joinPath(contentRoot(project), "screenshots/verification/cemu");
    if (!e2eOk(FileSystem_exists(joinPath(cemuScreens, "before_launch.png")), "cemu before_launch screenshot missing")) {
        return 1;
    }
    if (!e2eWaitForFile(joinPath(cemuScreens, "after_spawn.png"), "cemu after_spawn screenshot")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(cemuScreens, "after_exit.png")), "cemu after_exit screenshot missing")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(cemuScreens, "after_exit.png")), "fake screenshot", "cemu screenshot content")) {
        return 1;
    }
    btrc_Vector_string* __list_770 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_770, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "dolphin", __list_770), "launcher dolphin")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "--socket=x11", "dolphin x11")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "org.DolphinEmu.dolphin-emu", "dolphin flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "Dolphin.Display.Fullscreen=True", "dolphin fullscreen config")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "-b", "dolphin boot flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "-e", "dolphin execute flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "game.iso", "dolphin rom arg")) {
        return 1;
    }
    btrc_Vector_string* __list_771 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_771, "game.3ds");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "azahar", __list_771), "launcher azahar")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-3.args")), "--socket=x11", "azahar x11")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-3.args")), "org.azahar_emu.Azahar", "azahar flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-3.args")), "-f", "azahar fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_772 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_772, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "pcsx2", __list_772), "launcher pcsx2")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "--socket=wayland", "pcsx2 wayland")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "net.pcsx2.PCSX2", "pcsx2 flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "-batch", "pcsx2 batch flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "-fullscreen", "pcsx2 fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_773 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_773, "game.nsp");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "ryujinx", __list_773), "launcher ryujinx")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-5.args")), "org.ryujinx.Ryujinx", "ryujinx flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-5.args")), "--fullscreen", "ryujinx fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_774 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_774, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "ppsspp", __list_774), "launcher ppsspp")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-6.args")), "org.ppsspp.PPSSPP", "ppsspp flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-6.args")), "--fullscreen", "ppsspp fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_775 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_775, "game.chd");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "flycast", __list_775), "launcher flycast")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-7.args")), "org.flycast.Flycast", "flycast flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-7.args")), "-config", "flycast config flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-7.args")), "window:fullscreen=yes", "flycast fullscreen config")) {
        return 1;
    }
    btrc_Vector_string* __list_776 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_776, "game.z64");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "gopher64", __list_776), "launcher gopher64")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-8.args")), "io.github.gopher64.gopher64", "gopher64 flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-8.args")), "--fullscreen", "gopher64 fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_777 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_777, "game.nds");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "melonds", __list_777), "launcher melonds")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-9.args")), "net.kuribo64.melonDS", "melonds flatpak id")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-9.args")), "--fullscreen", "melonds fullscreen flag")) {
        return 1;
    }
    btrc_Vector_string* __list_778 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_778, "--help");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "dolphin", __list_778), "launcher dolphin help")) {
        return 1;
    }
    if (!e2eOk((!__btrc_strContains(FileSystem_readText(joinPath(capture, "flatpak-10.args")), "Dolphin.Display.Fullscreen=True")), "dolphin help should not receive fullscreen defaults")) {
        return 1;
    }
    e2eSeedFile(emulatorProfilePath(project, "RetroArch/config/input.cfg"));
    e2eSeedFile(emulatorProfilePath(project, "RetroArch/retroarch.cfg"));
    btrc_Vector_string* __list_779 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_779, "-L");
    btrc_Vector_string_push(__list_779, "core.so");
    btrc_Vector_string_push(__list_779, "game.gba");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "retroarch", __list_779), "launcher retroarch")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "/usr/bin/retroarch", "retroarch executable")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "-f", "retroarch fullscreen flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "-L", "retroarch core flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "core.so", "retroarch core arg")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "game.gba", "retroarch rom arg")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "--ro-bind-try", "retroarch rom bind")) {
        return 1;
    }
    char* retroScreens = joinPath(contentRoot(project), "screenshots/verification/retroarch");
    if (!e2eOk(FileSystem_exists(joinPath(retroScreens, "before_launch.png")), "retroarch before_launch screenshot missing")) {
        return 1;
    }
    if (!e2eWaitForFile(joinPath(retroScreens, "after_spawn.png"), "retroarch after_spawn screenshot")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(retroScreens, "after_exit.png")), "retroarch after_exit screenshot missing")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "grim.log")), "screenshots/verification/retroarch/after_exit.png", "retroarch screenshot hook log")) {
        return 1;
    }
    char* fakeRouted = joinPath(binDir, "fake-emulator");
    FileSystem_writeText(fakeRouted, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "printf '%s\\n' \"$@\" > \"${SEMU_FLATPAK_CAPTURE:?}/routed-dolphin.args\"\n")));
    FileSystem_chmod(fakeRouted, 493);
    btrc_Vector_string* __list_780 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_780, "game.iso");
    if (!e2eRunOk(e2eRunRoutedLauncher(exe, home, project, roms, binDir, capture, "dolphin", fakeRouted, __list_780), "routed dolphin")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "routed-dolphin.args")), "Dolphin.Display.Fullscreen=True", "routed dolphin fullscreen config")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "routed-dolphin.args")), "-b", "routed dolphin batch flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "routed-dolphin.args")), "-e", "routed dolphin exec flag")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "routed-dolphin.args")), "game.iso", "routed dolphin rom arg")) {
        return 1;
    }
    printf("%s\n", "OK BTRC launcher smoke");
    return 0;
}

bool e2eWriteFakeN3dsRom(char* path, bool noCrypto, bool decrypted) {
    ensureDir(PathTools_dirname(path));
    BinaryEditor* editor = BinaryEditor_new(path, "w+b");
    if (!BinaryEditor_ok(editor)) {
        bool __btrc_ret_781 = false;
        if (editor != NULL) {
            if ((--editor->__rc) <= 0) {
                BinaryEditor_destroy(editor);
            }
        }
        return __btrc_ret_781;
    }
    bool ok = true;
    long partOffset = 0x200;
    (ok = (ok && BinaryEditor_writeAscii(editor, 0x100, "NCSD")));
    (ok = (ok && BinaryEditor_writeLe32(editor, 0x120, 1)));
    (ok = (ok && BinaryEditor_writeLe32(editor, 0x124, 4)));
    (ok = (ok && BinaryEditor_writeAscii(editor, (partOffset + 0x100), "NCCH")));
    (ok = (ok && BinaryEditor_writeU8(editor, ((partOffset + 0x100) + 0x8b), (noCrypto ? 0 : 1))));
    (ok = (ok && BinaryEditor_writeU8(editor, ((partOffset + 0x100) + 0x8f), (noCrypto ? 4 : 0))));
    (ok = (ok && BinaryEditor_writeLe32(editor, ((partOffset + 0x100) + 0xa0), 1)));
    (ok = (ok && BinaryEditor_writeAscii(editor, (partOffset + 0x200), (decrypted ? ".code" : "cipher"))));
    BinaryEditor_close(editor);
    if (editor != NULL) {
        if ((--editor->__rc) <= 0) {
            BinaryEditor_destroy(editor);
        }
    }
    return ok;
    if (editor != NULL) {
        if ((--editor->__rc) <= 0) {
            BinaryEditor_destroy(editor);
        }
    }
}

ExecResult* e2eRunN3dsNoCrypto(char* exe, char* input, char* outputDir, btrc_Vector_string* extraArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_new(exe), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "utils"), "n3ds-nocrypto"), input), true), false);
    if (((int)strlen(outputDir)) > 0) {
        Command_flag(command, "-o", outputDir);
    }
    int __n_783 = btrc_Vector_string_iterLen(extraArgs);
    for (int __i_782 = 0; (__i_782 < __n_783); (__i_782++)) {
        char* arg = btrc_Vector_string_iterGet(extraArgs, __i_782);
        Command_arg(command, arg);
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

ExecResult* e2eRunDecrypt3dsNoCrypto(char* exe, char* input, btrc_Vector_string* extraArgs) {
    Command* command = Command_check(Command_capture(Command_arg(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_new(exe), "SEMU_BIN", exe), "PATH", "/usr/bin:/bin"), "utils"), "decrypt3ds"), input), true), false);
    int __n_785 = btrc_Vector_string_iterLen(extraArgs);
    for (int __i_784 = 0; (__i_784 < __n_785); (__i_784++)) {
        char* arg = btrc_Vector_string_iterGet(extraArgs, __i_784);
        Command_arg(command, arg);
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runCommand(shell, command);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return result;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int e2eN3dsNoCryptoSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("semu-n3ds-nocrypto-smoke");
    char* input = joinPath(tmp, "input");
    char* output = joinPath(tmp, "output");
    char* exe = Environment_get("SEMU_BIN", args->program);
    ensureDir(input);
    ensureDir(output);
    char* needsPath = joinPath(input, "needs.3ds");
    char* okPath = joinPath(input, "already-ok.3ds");
    if (!e2eOk(e2eWriteFakeN3dsRom(needsPath, false, true), "fake needs-fix 3DS ROM")) {
        return 1;
    }
    if (!e2eOk(e2eWriteFakeN3dsRom(okPath, true, true), "fake OK 3DS ROM")) {
        return 1;
    }
    btrc_Vector_string* __list_786 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_786, "--check");
    ExecResult* check = e2eRunN3dsNoCrypto(exe, input, "", __list_786);
    if (!e2eRunOk(check, "n3ds-nocrypto check")) {
        return 1;
    }
    if (!e2eContains(ExecResult_stdout(check), "NEEDS FIX: needs.3ds", "n3ds-nocrypto check output")) {
        return 1;
    }
    if (!e2eContains(ExecResult_stdout(check), "OK:        already-ok.3ds", "n3ds-nocrypto OK output")) {
        return 1;
    }
    btrc_Vector_string* __list_788 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_788, "--check");
    if (!e2eRunOk(e2eRunDecrypt3dsNoCrypto(exe, input, __list_788), "decrypt3ds compatibility check")) {
        return 1;
    }
    btrc_Vector_string* noArgs = btrc_Vector_string_new();
    if (!e2eRunOk(e2eRunN3dsNoCrypto(exe, input, output, noArgs), "n3ds-nocrypto fix")) {
        return 1;
    }
    char* fixedPath = joinPath(output, "needs.3ds");
    N3dsRomCheck* fixed = checkN3dsRom(fixedPath);
    if (!e2eOk((strcmp(fixed->status, "OK") == 0), "fixed fake ROM should be OK")) {
        return 1;
    }
    BinaryReader* reader = BinaryReader_new(fixedPath);
    int flags = BinaryReader_readU8(reader, ((0x200 + 0x100) + 0x8f));
    int cryptoMethod = BinaryReader_readU8(reader, ((0x200 + 0x100) + 0x8b));
    BinaryReader_close(reader);
    if (!e2eOk(n3dsNoCryptoFlag(flags), "fixed fake ROM NoCrypto flag missing")) {
        int __btrc_ret_789 = 1;
        if (reader != NULL) {
            if ((--reader->__rc) <= 0) {
                BinaryReader_destroy(reader);
            }
        }
        return __btrc_ret_789;
    }
    if (!e2eOk((cryptoMethod == 0), "fixed fake ROM crypto method not cleared")) {
        int __btrc_ret_790 = 1;
        if (reader != NULL) {
            if ((--reader->__rc) <= 0) {
                BinaryReader_destroy(reader);
            }
        }
        return __btrc_ret_790;
    }
    if (!e2eOk((strcmp(checkN3dsRom(joinPath(output, "already-ok.3ds"))->status, "OK") == 0), "already OK fake ROM copy")) {
        int __btrc_ret_791 = 1;
        if (reader != NULL) {
            if ((--reader->__rc) <= 0) {
                BinaryReader_destroy(reader);
            }
        }
        return __btrc_ret_791;
    }
    printf("%s\n", "OK BTRC n3ds-nocrypto smoke");
    int __btrc_ret_792 = 0;
    if (reader != NULL) {
        if ((--reader->__rc) <= 0) {
            BinaryReader_destroy(reader);
        }
    }
    return __btrc_ret_792;
    if (reader != NULL) {
        if ((--reader->__rc) <= 0) {
            BinaryReader_destroy(reader);
        }
    }
}

int e2eShellSyntaxSmoke(char* project) {
    char* root = project;
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("cd ", ShellWords_quote(root))), " && bash -n")), " packaging/linux/AppRun")), " packaging/linux/sandbox.sh")), " packaging/linux/build-appimage.sh")), " packaging/linux/bin/semu-*")), " utils/steam-deck-bootstrap.sh"));
    ExecResult* result = UnixShell_runRaw(UnixShell_new(), command, true, false, "");
    if (!ExecResult_ok(result)) {
        printf("%s\n", ExecResult_stdout(result));
        return result->code;
    }
    printf("%s\n", "OK shell artifact syntax");
    return 0;
}

void printUsage(void) {
    printf("%s\n", "semu [manifest|bootstrap|doctor|deck|lifecycle|sync|config|apprun|steam-input|keymap|screenshot|sandbox|launcher|utils|e2e] [graph|payload-audit|n3ds-nocrypto|validate|render|install|setup|reconfigure|change|uninstall|reinstall|upgrade|status|force|serve|capture|prepare|launch] [--project PATH] [--roms PATH] [--source PATH] [--output PATH] [--dest PATH] [--target manifest|retroarch|dolphin|pcsx2|steam-input] [--emulator NAME] [--hook HOOK] [--scratch PATH] [--action ID] [--command KEYS]");
}

int keymapCommand(CliArgs* args) {
    char* mode = "validate";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    char* sourcePath = CliArgs_valueAfter(args, "--source", "");
    char* project = CliArgs_valueAfter(args, "--project", "");
    char* source = defaultKeymapSource();
    if (((int)strlen(sourcePath)) > 0) {
        if (!FileSystem_exists(sourcePath)) {
            int __fstr_795_len = snprintf(NULL, 0, "error 0:0 keymap source not found: %s", sourcePath);
            char* __fstr_795_buf = __btrc_str_track(((char*)malloc((__fstr_795_len + 1))));
            snprintf(__fstr_795_buf, (__fstr_795_len + 1), "error 0:0 keymap source not found: %s", sourcePath);
            printf("%s\n", __fstr_795_buf);
            return 1;
        }
        (source = FileSystem_readText(sourcePath));
    } else if ((((int)strlen(project)) > 0) && FileSystem_exists(keymapSourcePath(project))) {
        (source = FileSystem_readText(keymapSourcePath(project)));
    }
    KeymapErrors* errors = KeymapErrors_new();
    KeymapIr* ir = compileKeymap(source, errors);
    if (strcmp(mode, "validate") == 0) {
        if (KeymapErrors_count(errors) == 0) {
            printf("%s\n", "OK keymap steam_deck");
            int __btrc_ret_796 = 0;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_796;
        }
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_799_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_799_buf = __btrc_str_track(((char*)malloc((__fstr_799_len + 1))));
            snprintf(__fstr_799_buf, (__fstr_799_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_799_buf);
        }
        int __btrc_ret_800 = 1;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_800;
    }
    if (strcmp(mode, "render") == 0) {
        if (KeymapErrors_count(errors) > 0) {
            for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
                int __fstr_803_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
                char* __fstr_803_buf = __btrc_str_track(((char*)malloc((__fstr_803_len + 1))));
                snprintf(__fstr_803_buf, (__fstr_803_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
                printf("%s\n", __fstr_803_buf);
            }
            int __btrc_ret_804 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_804;
        }
        char* target = CliArgs_valueAfter(args, "--target", "manifest");
        if (!isKeymapTarget(target)) {
            int __fstr_807_len = snprintf(NULL, 0, "error 0:0 unknown keymap target '%s'", target);
            char* __fstr_807_buf = __btrc_str_track(((char*)malloc((__fstr_807_len + 1))));
            snprintf(__fstr_807_buf, (__fstr_807_len + 1), "error 0:0 unknown keymap target '%s'", target);
            printf("%s\n", __fstr_807_buf);
            int __btrc_ret_808 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_808;
        }
        char* rendered = renderKeymap(ir, target);
        char* output = CliArgs_valueAfter(args, "--output", "");
        if (((int)strlen(output)) > 0) {
            FileSystem_writeText(output, rendered);
        } else {
            printf("%s\n", rendered);
        }
        int __btrc_ret_809 = 0;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_809;
    }
    printUsage();
    int __btrc_ret_810 = 1;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_810;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

int screenshotCommand(CliArgs* args, char* project) {
    char* mode = "status";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if ((strcmp(mode, "setup") == 0) || (strcmp(mode, "defaults") == 0)) {
        writeScreenshotDefaults(project);
        int __fstr_813_len = snprintf(NULL, 0, "OK screenshot defaults: %s", screenshotConfigPath(project));
        char* __fstr_813_buf = __btrc_str_track(((char*)malloc((__fstr_813_len + 1))));
        snprintf(__fstr_813_buf, (__fstr_813_len + 1), "OK screenshot defaults: %s", screenshotConfigPath(project));
        printf("%s\n", __fstr_813_buf);
        return 0;
    }
    if (strcmp(mode, "status") == 0) {
        doctorScreenshotHooks(project);
        return 0;
    }
    if (strcmp(mode, "validate") == 0) {
        doctorScreenshotHooks(project);
        return ((((int)strlen(screenshotConfiguredTool(project))) > 0) ? 0 : 1);
    }
    if (strcmp(mode, "capture") == 0) {
        char* emulator = CliArgs_valueAfter(args, "--emulator", "");
        if (((((int)strlen(emulator)) == 0) && (CliArgs_count(args) > 2)) && (!__btrc_startsWith(CliArgs_get(args, 2), "--"))) {
            (emulator = CliArgs_get(args, 2));
        }
        if (((int)strlen(emulator)) == 0) {
            printf("%s\n", "error 0:0 screenshot capture needs --emulator NAME");
            return 1;
        }
        char* hook = CliArgs_valueAfter(args, "--hook", "manual_visual_checkpoint");
        char* output = CliArgs_valueAfter(args, "--output", screenshotCapturePath(project, emulator, hook));
        return (screenshotCaptureTo(project, emulator, hook, output) ? 0 : 1);
    }
    printUsage();
    return 1;
}

int syncCommand(CliArgs* args, char* project) {
    char* mode = "status";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "setup") == 0) {
        return (syncSetup(project) ? 0 : 1);
    }
    if (strcmp(mode, "start") == 0) {
        return (syncStart(project) ? 0 : 1);
    }
    if (strcmp(mode, "serve") == 0) {
        return (syncServe(project) ? 0 : 1);
    }
    if (strcmp(mode, "stop") == 0) {
        return (syncStop(project) ? 0 : 1);
    }
    if (strcmp(mode, "status") == 0) {
        syncStatus(project);
        return 0;
    }
    if ((strcmp(mode, "force") == 0) || (strcmp(mode, "now") == 0)) {
        char* target = "all";
        if ((CliArgs_count(args) > 2) && (!__btrc_startsWith(CliArgs_get(args, 2), "--"))) {
            (target = CliArgs_get(args, 2));
        }
        return (syncForce(project, target) ? 0 : 1);
    }
    if (strcmp(mode, "tray") == 0) {
        syncTray(project);
        return 0;
    }
    if (strcmp(mode, "open") == 0) {
        syncOpen(project);
        return 0;
    }
    if (strcmp(mode, "autostart") == 0) {
        char* action = ((CliArgs_count(args) > 2) ? CliArgs_get(args, 2) : "enable");
        return (syncAutostart(project, (!(strcmp(action, "disable") == 0))) ? 0 : 1);
    }
    printUsage();
    return 1;
}

int lifecycleCommand(CliArgs* args, char* project) {
    char* mode = "status";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if ((strcmp(mode, "install") == 0) || (strcmp(mode, "setup") == 0)) {
        lifecycleInstall(project, CliArgs_valueAfter(args, "--roms", ""));
        return 0;
    }
    if (strcmp(mode, "reconfigure") == 0) {
        lifecycleReconfigure(project, CliArgs_valueAfter(args, "--roms", ""));
        writeLifecycleState(project, "reconfigure");
        printf("%s\n", "OK lifecycle reconfigure");
        return 0;
    }
    if (strcmp(mode, "change") == 0) {
        char* actionId = CliArgs_valueAfter(args, "--action", "");
        char* command = CliArgs_valueAfter(args, "--command", "");
        if ((((int)strlen(actionId)) == 0) || (((int)strlen(command)) == 0)) {
            printf("%s\n", "error 0:0 lifecycle change needs --action ID --command KEYS");
            return 1;
        }
        return (lifecycleChangeKeymap(project, actionId, command) ? 0 : 1);
    }
    if (strcmp(mode, "uninstall") == 0) {
        lifecycleUninstall(project, CliArgs_has(args, "--purge-generated"), CliArgs_has(args, "--purge-state"));
        return 0;
    }
    if (strcmp(mode, "reinstall") == 0) {
        lifecycleUninstall(project, false, false);
        lifecycleInstall(project, CliArgs_valueAfter(args, "--roms", ""));
        writeLifecycleState(project, "reinstall");
        return 0;
    }
    if (strcmp(mode, "upgrade") == 0) {
        lifecycleUpgrade(project);
        return 0;
    }
    if (strcmp(mode, "status") == 0) {
        lifecycleStatus(project);
        return 0;
    }
    printUsage();
    return 1;
}

int deckLaunch(char* project) {
    if (!FileSystem_exists(syncConfigPath(project))) {
        printf("%s\n", __btrc_str_track(__btrc_strcat("MISSING setup: run lifecycle install --project ", ShellWords_quote(project))));
        return 2;
    }
    writeEsDeRuntimeFiles(project, homeDir());
    UnixShell* shell = UnixShell_new();
    char* env = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("SEMU_PROJECT_DIR=", ShellWords_quote(project))), " SEMU_ROMS_DIR=")), ShellWords_quote(configuredRomsRoot(project))));
    if (commandExists("es-de")) {
        UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(env, " es-de")), false, false, "");
        int __btrc_ret_814 = 0;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_814;
    } else {
        printf("%s\n", "MISSING es-de: use the bundled AppImage or install ES-DE");
    }
    int __btrc_ret_815 = 127;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_815;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

int deckCommand(CliArgs* args, char* project) {
    char* mode = "install";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "install") == 0) {
        char* roms = CliArgs_valueAfter(args, "--roms", "");
        lifecycleInstall(project, roms);
        return 0;
    }
    if (strcmp(mode, "provision") == 0) {
        return deckProvisionCommand(args, project);
    }
    if (strcmp(mode, "verify-emulators") == 0) {
        return deckVerifyEmulatorsCommand(args, project);
    }
    if (strcmp(mode, "verify-sync") == 0) {
        return deckVerifySyncCommand(project);
    }
    if (strcmp(mode, "verify-input") == 0) {
        return deckVerifyInputCommand(project, (CliArgs_has(args, "--strict") || (strcmp(Environment_get("SEMU_STRICT_INPUT", "0"), "1") == 0)));
    }
    if (((((((strcmp(mode, "setup") == 0) || (strcmp(mode, "reconfigure") == 0)) || (strcmp(mode, "change") == 0)) || (strcmp(mode, "uninstall") == 0)) || (strcmp(mode, "reinstall") == 0)) || (strcmp(mode, "upgrade") == 0)) || (strcmp(mode, "status") == 0)) {
        return lifecycleCommand(args, project);
    }
    if (strcmp(mode, "verify") == 0) {
        doctorSteamDeck(project);
        KeymapErrors* errors = KeymapErrors_new();
        compileKeymap((FileSystem_exists(keymapSourcePath(project)) ? FileSystem_readText(keymapSourcePath(project)) : defaultKeymapSource()), errors);
        if (KeymapErrors_count(errors) > 0) {
            int __btrc_ret_816 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_816;
        }
        int __btrc_ret_817 = 0;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_817;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
    }
    if (strcmp(mode, "launch") == 0) {
        return deckLaunch(project);
    }
    printUsage();
    return 1;
}

int appRunCommand(CliArgs* args, char* project) {
    char* mode = "prepare";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "prepare") == 0) {
        writeEsDeRuntimeFiles(project, homeDir());
        printf("%s\n", "OK apprun prepare");
        return 0;
    }
    printUsage();
    return 1;
}

char* steamInputTemplateDir(CliArgs* args) {
    return CliArgs_valueAfter(args, "--dest", Environment_get("SEMU_STEAM_INPUT_DIR", joinPath(homeDir(), ".steam/steam/controller_base/templates")));
}

void copySteamInputTemplate(char* project, char* destination, char* name) {
    char* source = joinPath(project, __btrc_str_track(__btrc_strcat("input/steam-input/", name)));
    FileSystem_writeText(joinPath(destination, name), FileSystem_readText(source));
}

int steamInputCommand(CliArgs* args, char* project) {
    char* mode = "status";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if ((strcmp(mode, "install") == 0) || (strcmp(mode, "copy") == 0)) {
        char* destination = steamInputTemplateDir(args);
        writeSteamInputTemplates(project);
        ensureDir(destination);
        copySteamInputTemplate(project, destination, "neptune-simple.vdf");
        copySteamInputTemplate(project, destination, "neptune-full.vdf");
        int __fstr_820_len = snprintf(NULL, 0, "OK steam-input templates: %s", destination);
        char* __fstr_820_buf = __btrc_str_track(((char*)malloc((__fstr_820_len + 1))));
        snprintf(__fstr_820_buf, (__fstr_820_len + 1), "OK steam-input templates: %s", destination);
        printf("%s\n", __fstr_820_buf);
        return 0;
    }
    if ((strcmp(mode, "status") == 0) || (strcmp(mode, "validate") == 0)) {
        writeSteamInputTemplates(project);
        reportSteamInputTemplate("neptune_simple", joinPath(project, "input/steam-input/neptune-simple.vdf"));
        reportSteamInputTemplate("neptune_full", joinPath(project, "input/steam-input/neptune-full.vdf"));
        return 0;
    }
    printUsage();
    return 1;
}

int configCommand(CliArgs* args, char* project) {
    char* mode = "show";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
    }
    if (strcmp(mode, "env") == 0) {
        char* roms = CliArgs_valueAfter(args, "--roms", configuredRomsRoot(project));
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SEMU_PROJECT_DIR=", ShellWords_quote(project))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SEMU_ROMS_DIR=", ShellWords_quote(normalizeRomsRoot(roms)))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SEMU_BIN=", ShellWords_quote(serviceExecutable()))));
        return 0;
    }
    if (strcmp(mode, "set-roms") == 0) {
        char* roms = CliArgs_valueAfter(args, "--roms", "");
        if ((((int)strlen(roms)) == 0) && (CliArgs_count(args) > 2)) {
            (roms = CliArgs_get(args, 2));
        }
        if (((int)strlen(roms)) == 0) {
            printf("%s\n", "error 0:0 set-roms needs --roms PATH");
            return 1;
        }
        writeSyncDefaults(project, roms);
        ensureProjectContentDirs(project);
        writeEsDeFiles(project);
        int __fstr_823_len = snprintf(NULL, 0, "OK roms_dir: %s", roms);
        char* __fstr_823_buf = __btrc_str_track(((char*)malloc((__fstr_823_len + 1))));
        snprintf(__fstr_823_buf, (__fstr_823_len + 1), "OK roms_dir: %s", roms);
        printf("%s\n", __fstr_823_buf);
        return 0;
    }
    if (strcmp(mode, "show") == 0) {
        reportFile("sync_config", syncConfigPath(project));
        int __fstr_826_len = snprintf(NULL, 0, "  roms_dir: %s", configuredRomsRoot(project));
        char* __fstr_826_buf = __btrc_str_track(((char*)malloc((__fstr_826_len + 1))));
        snprintf(__fstr_826_buf, (__fstr_826_len + 1), "  roms_dir: %s", configuredRomsRoot(project));
        printf("%s\n", __fstr_826_buf);
        return 0;
    }
    printUsage();
    return 1;
}

char* launcherNameFromProgram(char* program) {
    char* base = PathTools_basename(program);
    if (!__btrc_startsWith(base, "semu-")) {
        return "";
    }
    if (((strcmp(base, "semu-btrc") == 0) || (strcmp(base, "semu-flatpak") == 0)) || (strcmp(base, "semu") == 0)) {
        return "";
    }
    return Strings_removePrefix(base, "semu-");
}

int main(int argc, char** argv) {
    CliArgs* args = CliArgs_new(argc, argv);
    char* command = CliArgs_command(args);
    char* project = CliArgs_valueAfter(args, "--project", Environment_get("SEMU_PROJECT_DIR", "."));
    char* programLauncher = launcherNameFromProgram(args->program);
    if (((int)strlen(programLauncher)) > 0) {
        int __btrc_ret_827 = launcherRunEmulator(project, programLauncher, launcherPassthroughArgs(args));
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_827;
    }
    if ((strcmp(command, "") == 0) || (strcmp(command, "manifest") == 0)) {
        char* output = CliArgs_valueAfter(args, "--output", "semu.json");
        writeGeneratedManifest(output);
        int __btrc_ret_828 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_828;
    }
    if (strcmp(command, "bootstrap") == 0) {
        bootstrapSteamDeck(project);
        int __btrc_ret_829 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_829;
    }
    if (strcmp(command, "doctor") == 0) {
        doctorSteamDeck(project);
        int __btrc_ret_830 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_830;
    }
    if (strcmp(command, "deck") == 0) {
        int status = deckCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "lifecycle") == 0) {
        int status = lifecycleCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "sync") == 0) {
        int status = syncCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "config") == 0) {
        int status = configCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "apprun") == 0) {
        int status = appRunCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "steam-input") == 0) {
        int status = steamInputCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "keymap") == 0) {
        int status = keymapCommand(args);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "screenshot") == 0) {
        int status = screenshotCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "sandbox") == 0) {
        int status = sandboxCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "launcher") == 0) {
        int status = launcherCommand(args, project);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if ((strcmp(command, "utils") == 0) || (strcmp(command, "util") == 0)) {
        int status = utilitiesCommand(args);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if ((strcmp(command, "n3ds-nocrypto") == 0) || (strcmp(command, "3ds-nocrypto") == 0)) {
        int status = n3dsNoCryptoCommand(args, 1);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    if (strcmp(command, "e2e") == 0) {
        int status = e2eCommand(args);
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return status;
    }
    printUsage();
    int __btrc_ret_831 = 1;
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
    return __btrc_ret_831;
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
}
