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
typedef struct CliArgs CliArgs;
void CliArgs_destroy(CliArgs* self);
typedef struct CliCommand CliCommand;
typedef struct KeymapErrors KeymapErrors;
void KeymapErrors_destroy(KeymapErrors* self);
typedef struct KeymapTokens KeymapTokens;
void KeymapTokens_destroy(KeymapTokens* self);
typedef struct KeymapIr KeymapIr;
void KeymapIr_destroy(KeymapIr* self);
typedef struct KeymapParser KeymapParser;
void KeymapParser_destroy(KeymapParser* self);
typedef struct SystemCatalog SystemCatalog;
void SystemCatalog_destroy(SystemCatalog* self);
typedef struct BinaryReader BinaryReader;
void BinaryReader_destroy(BinaryReader* self);
typedef struct N3dsRomCheck N3dsRomCheck;
void N3dsRomCheck_destroy(N3dsRomCheck* self);
typedef struct btrc_Vector_string btrc_Vector_string;
typedef struct btrc_Vector_bool btrc_Vector_bool;
typedef struct btrc_Vector_int btrc_Vector_int;
typedef struct btrc_Map_string_string btrc_Map_string_string;
typedef struct btrc_Map_string_bool btrc_Map_string_bool;
char* jsonQ(char* value);
char* jsonField(char* key, char* value);
char* jsonStrField(char* key, char* value);
char* jsonBoolField(char* key, bool value);
char* jsonObject(btrc_Vector_string* fields);
char* jsonArray(btrc_Vector_string* values);
char* jsonStringArray(btrc_Vector_string* values);
char* commandSpec(char* label, char* command);
char* platformCommands(char* platform, btrc_Vector_string* commands);
char* systemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile);
char* biosSpec(char* id, char* system, bool required, btrc_Vector_string* files, char* target, char* match, char* note);
char* steamInputTemplate(char* id, char* title, char* source, char* note);
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
btrc_Vector_string* linuxLauncherNames(void);
btrc_Vector_string* lowercaseValues(btrc_Vector_string* values);
char* schemLauncherName(char* emulator);
btrc_Vector_string* retroarchCoreSearchPaths(void);
char* launcherEntries(void);
char* xmlEscape(char* value);
char* esExtensionList(btrc_Vector_string* extensions);
char* esCommandXml(char* commandJson);
char* esSystemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands);
SystemCatalog* systemCatalog(void);
char* systemEntries(void);
char* buildJson(void);
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
btrc_Vector_string* declaredSystemIds(void);
btrc_Vector_string* declaredRomDirs(void);
btrc_Vector_string* controllerProfileFiles(void);
char* esSettingsXmlWithPaths(char* roms, char* media, char* themes);
char* esSettingsXmlForProject(char* project);
void ensureDir(char* path);
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
char* launcherFlatpakId(char* emulator);
bool launcherUsesX11(char* emulator);
btrc_Vector_string* launcherPresetArgs(char* emulator);
char* launcherFlatpakStateRoot(char* project, char* emulator);
char* launcherRoutedStateRoot(char* project, char* emulator);
void launcherCopyDirContents(char* source, char* destination);
void launcherCopyFile(char* source, char* destination);
void launcherSeedRoutedState(char* project, char* emulator, char* configRoot, char* dataRoot);
int launcherRunRouted(char* project, char* emulator, char* executable, btrc_Vector_string* emulatorArgs);
int launcherRunFlatpak(char* project, char* emulator, char* flatpakId, btrc_Vector_string* emulatorArgs);
int launcherRunEmulator(char* project, char* emulator, btrc_Vector_string* emulatorArgs);
int launcherCommand(CliArgs* args, char* project);
void writeGeneratedManifest(char* output);
char* assetRoot(char* project);
void seedBundledFileFromRoot(char* root, char* project, char* relative, bool executable);
void seedBundledFile(char* project, char* relative, bool executable);
void seedBundledDirFiles(char* project, char* relative, bool executable);
void seedLinuxAssets(char* project);
char* textLines(btrc_Vector_string* items);
KeymapIr* projectKeymapIr(char* project);
char* retroArchProfileText(char* project, KeymapIr* ir);
char* dolphinGcpadProfileText(void);
char* dolphinHotkeysProfileText(KeymapIr* ir);
char* dolphinWiimoteProfileText(bool classic);
char* pcsx2ProfileText(KeymapIr* ir);
char* cemuProfileText(void);
char* ryujinxProfileText(void);
void writeProfile(char* project, char* relative, char* text);
void seedEmulatorDefaults(char* project);
void seedKeymapDefaults(char* project);
char* syncDefaultConfigText(char* project, char* romsDir);
void writeSyncDefaults(char* project, char* romsDir);
void ensureRomDirsAt(char* root);
char* steamInputKeyName(char* key);
char* steamInputActionLabel(char* id);
char* steamInputKeyBindings(char* command, char* label);
char* steamInputActionBindings(KeymapIr* ir, char* actionId);
char* steamInputTemplateVdf(char* title, bool full, KeymapIr* ir);
void writeSteamInputTemplates(char* project);
char* esFindRulesXml(char* launcherBin);
void writeEsDeFiles(char* project);
char* esSettingsXmlForRuntime(char* project, char* romsDir);
void writeEsDeRuntimeFiles(char* project, char* userHome);
void bootstrapSteamDeck(char* project);
bool allPresent(char* target, btrc_Vector_string* files);
bool anyPresent(char* target, btrc_Vector_string* files);
void reportPath(char* label, char* path);
void reportBios(char* id, char* target, btrc_Vector_string* files, bool required, bool anyMatch);
void reportFile(char* label, char* path);
int braceBalance(char* text);
void reportSteamInputTemplate(char* label, char* path);
bool n3dsRomName(char* name);
bool n3dsArchiveName(char* name);
bool exefsLooksDecrypted(char* text);
N3dsRomCheck* checkN3dsRom(char* path);
void reportN3dsRomPreflight(char* project);
bool commandExists(char* name);
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
char* syncServiceText(char* project);
char* syncForceScriptText(char* project);
char* syncForceServiceText(char* project);
char* syncTimerText(void);
void writeSyncSystemdUnits(char* project);
char* deckDesktopText(char* project);
void writeDeckDesktopEntry(char* project);
char* schemulatorStateRoot(char* project);
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
bool syncStart(char* project);
bool syncStop(char* project);
bool syncAutostart(char* project, bool enabled);
bool syncForce(char* project, char* target);
void syncStatus(char* project);
void syncTray(char* project);
void syncOpen(char* project);
void doctorSync(char* project);
void doctorSteamDeck(char* project);
bool e2eOk(bool condition, char* message);
bool e2eContains(char* text, char* expected, char* message);
char* e2eTempDir(char* label);
void e2eSeedFile(char* path);
bool e2eAssertLink(char* linkPath, char* targetPath);
bool e2ePrepareSandbox(char* project, char* scratchRoot, char* emulator);
int e2eSandboxSmoke(void);
ExecResult* e2eRunLifecycle(char* exe, char* home, char* mode, char* project, char* romsDir);
ExecResult* e2eRunLifecycleChange(char* exe, char* home, char* project, char* actionId, char* commandText);
ExecResult* e2eRunLifecycleArgs(char* exe, char* home, char* project, btrc_Vector_string* lifecycleArgs);
ExecResult* e2eRunSync(char* exe, char* home, char* binDir, char* project, btrc_Vector_string* syncArgs);
ExecResult* e2eRunSteamInput(char* exe, char* home, char* project, btrc_Vector_string* steamInputArgs);
ExecResult* e2eRunLauncher(char* exe, char* home, char* project, char* roms, char* binDir, char* captureDir, char* bwrapPath, char* emulator, btrc_Vector_string* emulatorArgs);
bool e2eRunOk(ExecResult* result, char* label);
bool e2eWaitForFile(char* path, char* label);
bool e2eCatalogConsistency(char* project, char* launcherBin);
int e2eLifecycleSmoke(CliArgs* args);
int e2eLauncherSmoke(CliArgs* args);
void e2eWriteSyncFakes(char* binDir);
int e2eSyncSmoke(CliArgs* args);
int e2eCommand(CliArgs* args);
void printUsage(void);
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
int keymapCommand(CliArgs* args);
int screenshotCommand(CliArgs* args, char* project);
int syncCommand(CliArgs* args, char* project);
int appRunCommand(CliArgs* args, char* project);
char* steamInputTemplateDir(CliArgs* args);
void copySteamInputTemplate(char* project, char* destination, char* name);
int steamInputCommand(CliArgs* args, char* project);
int configCommand(CliArgs* args, char* project);
int lifecycleCommand(CliArgs* args, char* project);
int deckLaunch(char* project);
int deckCommand(CliArgs* args, char* project);
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
void JsonObject_writeFile(JsonObject* self, char* path);
void CliArgs_init(CliArgs* self, int argc, char** argv);
CliArgs* CliArgs_new(int argc, char** argv);
int CliArgs_count(CliArgs* self);
char* CliArgs_get(CliArgs* self, int index);
char* CliArgs_command(CliArgs* self);
bool CliArgs_has(CliArgs* self, char* flag);
char* CliArgs_valueAfter(CliArgs* self, char* flag, char* fallback);
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
void SystemCatalog_init(SystemCatalog* self);
SystemCatalog* SystemCatalog_new(void);
void SystemCatalog_addSystem(SystemCatalog* self, char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile);
char* SystemCatalog_systemsJson(SystemCatalog* self);
char* SystemCatalog_esSystemsXml(SystemCatalog* self);
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
typedef bool (*__btrc_fn_bool_string)(char*);
typedef void (*__btrc_fn_void_string)(char*);
typedef char* (*__btrc_fn_string_string)(char*);
typedef char* (*__btrc_fn_string_string_string)(char*, char*);
typedef bool (*__btrc_fn_bool_bool)(bool);
typedef void (*__btrc_fn_void_bool)(bool);
typedef bool (*__btrc_fn_bool_bool_bool)(bool, bool);
typedef bool (*__btrc_fn_bool_int)(int);
typedef void (*__btrc_fn_void_int)(int);
typedef int (*__btrc_fn_int_int)(int);
typedef int (*__btrc_fn_int_int_int)(int, int);

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

struct btrc_Vector_int {
    int __rc;
    int* data;
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

struct SystemCatalog {
    int __rc;
    btrc_Vector_string* ids;
    btrc_Vector_string* romDirs;
    btrc_Vector_string* jsonSpecs;
    btrc_Vector_string* esSystemSpecs;
};

struct BinaryReader {
    int __rc;
    FILE* handle;
    bool opened;
};

struct N3dsRomCheck {
    int __rc;
    char* status;
    char* note;
    int partitions;
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

void JsonObject_writeFile(JsonObject* self, char* path) {
    Path_writeAll(path, JsonObject_stringify(self));
}

void CliArgs_init(CliArgs* self, int argc, char** argv) {
    self->__rc = 1;
    (self->program = ((argc > 0) ? Strings_copy(argv[0]) : ""));
    if (self->values != NULL) {
        if ((--self->values->__rc) <= 0) {
            btrc_Vector_string_free(self->values);
        }
    }
    btrc_Vector_string* __list_22 = btrc_Vector_string_new();
    (self->values = __list_22);
    btrc_Vector_string* __list_21 = btrc_Vector_string_new();
    (__list_21->__rc++);
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
    int __n_24 = btrc_Vector_string_iterLen(self->values);
    for (int __i_23 = 0; (__i_23 < __n_24); (__i_23++)) {
        char* value = btrc_Vector_string_iterGet(self->values, __i_23);
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
    int __n_32 = btrc_Vector_string_iterLen(values);
    for (int __i_31 = 0; (__i_31 < __n_32); (__i_31++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_31);
        btrc_Vector_string_push(quoted, jsonQ(value));
    }
    return jsonArray(quoted);
}

char* commandSpec(char* label, char* command) {
    btrc_Vector_string* __list_33 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_33, jsonStrField("label", label));
    btrc_Vector_string_push(__list_33, jsonStrField("command", command));
    return jsonObject(__list_33);
}

char* platformCommands(char* platform, btrc_Vector_string* commands) {
    return jsonField(platform, jsonArray(commands));
}

char* systemSpec(char* id, char* fullname, char* platform, char* theme, char* romDir, btrc_Vector_string* extensions, btrc_Vector_string* linuxCommands, btrc_Vector_string* macosCommands, btrc_Vector_string* bios, char* controllerProfile) {
    btrc_Vector_string* __list_34 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_34, platformCommands("linux", linuxCommands));
    btrc_Vector_string_push(__list_34, platformCommands("macos", macosCommands));
    btrc_Vector_string* commandPlatforms = __list_34;
    btrc_Vector_string* __list_35 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_35, jsonStrField("id", id));
    btrc_Vector_string_push(__list_35, jsonStrField("fullname", fullname));
    btrc_Vector_string_push(__list_35, jsonStrField("platform", platform));
    btrc_Vector_string_push(__list_35, jsonStrField("theme", theme));
    btrc_Vector_string_push(__list_35, jsonStrField("rom_dir", romDir));
    btrc_Vector_string_push(__list_35, jsonField("extensions", jsonStringArray(extensions)));
    btrc_Vector_string_push(__list_35, jsonField("commands", jsonObject(commandPlatforms)));
    btrc_Vector_string* fields = __list_35;
    if (bios->len > 0) {
        btrc_Vector_string_push(fields, jsonField("bios", jsonStringArray(bios)));
    }
    if (((int)strlen(controllerProfile)) > 0) {
        btrc_Vector_string_push(fields, jsonStrField("controller_profile", controllerProfile));
    }
    return jsonObject(fields);
}

char* biosSpec(char* id, char* system, bool required, btrc_Vector_string* files, char* target, char* match, char* note) {
    btrc_Vector_string* __list_36 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_36, jsonStrField("id", id));
    btrc_Vector_string_push(__list_36, jsonStrField("system", system));
    btrc_Vector_string_push(__list_36, jsonBoolField("required", required));
    btrc_Vector_string_push(__list_36, jsonField("files", jsonStringArray(files)));
    btrc_Vector_string_push(__list_36, jsonStrField("target", target));
    btrc_Vector_string_push(__list_36, jsonStrField("note", note));
    btrc_Vector_string* fields = __list_36;
    if (((int)strlen(match)) > 0) {
        btrc_Vector_string_push(fields, jsonStrField("match", match));
    }
    return jsonObject(fields);
}

char* steamInputTemplate(char* id, char* title, char* source, char* note) {
    btrc_Vector_string* __list_37 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_37, jsonStrField("id", id));
    btrc_Vector_string_push(__list_37, jsonStrField("title", title));
    btrc_Vector_string_push(__list_37, jsonStrField("source", source));
    btrc_Vector_string_push(__list_37, jsonBoolField("required", false));
    btrc_Vector_string_push(__list_37, jsonStrField("note", note));
    return jsonObject(__list_37);
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
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("# Schemulator Steam Deck keymap\n", "action ui.open = Ctrl+O\n")), "action ui.pause = Ctrl+P\n")), "action ui.screenshot = Ctrl+X\n")), "action ui.fullscreen = Ctrl+Enter\n")), "action ui.menu = Ctrl+M\n")), "action app.quit = Ctrl+Q\n")), "action state.prev = Ctrl+J\n")), "action state.next = Ctrl+K\n")), "action state.load = Ctrl+A\n")), "action state.save = Ctrl+S\n")), "action speed.rewind = Ctrl+-\n")), "action speed.fast = Ctrl++\n")), "action screen.swap = Ctrl+Tab\n")), "action ui.escape = Esc\n")), "bind HKB + A -> ${ui.pause}\n")), "bind HKB + B -> ${ui.screenshot}\n")), "bind HKB + X -> ${ui.fullscreen}\n")), "bind HKB + Y -> ${ui.menu}\n")), "bind HKB + Start -> ${app.quit}\n")), "bind HKB + D-Pad Left -> ${state.prev}\n")), "bind HKB + D-Pad Right -> ${state.next}\n")), "bind HKB + L1 -> ${state.load}\n")), "bind HKB + R1 -> ${state.save}\n")), "bind HKB + L2 -> ${speed.rewind}\n")), "bind HKB + R2 -> ${speed.fast}\n")), "bind HKB + L3 -> ${screen.swap}\n")), "bind HKB + R3 -> ${ui.escape}\n"));
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
        btrc_Vector_string* __list_38 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_38, "dolphin");
        return __list_38;
    }
    if (strcmp(id, "ui.pause") == 0) {
        btrc_Vector_string* __list_39 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_39, "retroarch");
        btrc_Vector_string_push(__list_39, "dolphin");
        btrc_Vector_string_push(__list_39, "azahar");
        btrc_Vector_string_push(__list_39, "melonds");
        btrc_Vector_string_push(__list_39, "pcsx2");
        return __list_39;
    }
    if (strcmp(id, "ui.screenshot") == 0) {
        btrc_Vector_string* __list_40 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_40, "retroarch");
        btrc_Vector_string_push(__list_40, "dolphin");
        btrc_Vector_string_push(__list_40, "azahar");
        btrc_Vector_string_push(__list_40, "pcsx2");
        return __list_40;
    }
    if (strcmp(id, "ui.fullscreen") == 0) {
        btrc_Vector_string* __list_41 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_41, "dolphin");
        btrc_Vector_string_push(__list_41, "azahar");
        btrc_Vector_string_push(__list_41, "melonds");
        btrc_Vector_string_push(__list_41, "pcsx2");
        return __list_41;
    }
    if (strcmp(id, "ui.menu") == 0) {
        btrc_Vector_string* __list_42 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_42, "retroarch");
        btrc_Vector_string_push(__list_42, "pcsx2");
        btrc_Vector_string_push(__list_42, "ppsspp");
        return __list_42;
    }
    if (strcmp(id, "app.quit") == 0) {
        btrc_Vector_string* __list_43 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_43, "retroarch");
        btrc_Vector_string_push(__list_43, "dolphin");
        btrc_Vector_string_push(__list_43, "azahar");
        btrc_Vector_string_push(__list_43, "pcsx2");
        btrc_Vector_string_push(__list_43, "ppsspp");
        return __list_43;
    }
    if (strcmp(id, "state.prev") == 0) {
        btrc_Vector_string* __list_44 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_44, "retroarch");
        btrc_Vector_string_push(__list_44, "dolphin");
        btrc_Vector_string_push(__list_44, "pcsx2");
        btrc_Vector_string_push(__list_44, "ppsspp");
        return __list_44;
    }
    if (strcmp(id, "state.next") == 0) {
        btrc_Vector_string* __list_45 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_45, "retroarch");
        btrc_Vector_string_push(__list_45, "dolphin");
        btrc_Vector_string_push(__list_45, "pcsx2");
        btrc_Vector_string_push(__list_45, "ppsspp");
        return __list_45;
    }
    if (strcmp(id, "state.load") == 0) {
        btrc_Vector_string* __list_46 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_46, "retroarch");
        btrc_Vector_string_push(__list_46, "dolphin");
        btrc_Vector_string_push(__list_46, "azahar");
        btrc_Vector_string_push(__list_46, "pcsx2");
        btrc_Vector_string_push(__list_46, "ppsspp");
        return __list_46;
    }
    if (strcmp(id, "state.save") == 0) {
        btrc_Vector_string* __list_47 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_47, "retroarch");
        btrc_Vector_string_push(__list_47, "dolphin");
        btrc_Vector_string_push(__list_47, "azahar");
        btrc_Vector_string_push(__list_47, "pcsx2");
        btrc_Vector_string_push(__list_47, "ppsspp");
        return __list_47;
    }
    if (strcmp(id, "speed.rewind") == 0) {
        btrc_Vector_string* __list_48 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_48, "retroarch");
        return __list_48;
    }
    if (strcmp(id, "speed.fast") == 0) {
        btrc_Vector_string* __list_49 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_49, "retroarch");
        btrc_Vector_string_push(__list_49, "melonds");
        btrc_Vector_string_push(__list_49, "pcsx2");
        btrc_Vector_string_push(__list_49, "ppsspp");
        return __list_49;
    }
    if (strcmp(id, "screen.swap") == 0) {
        btrc_Vector_string* __list_50 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_50, "azahar");
        btrc_Vector_string_push(__list_50, "melonds");
        btrc_Vector_string_push(__list_50, "cemu");
        return __list_50;
    }
    if (strcmp(id, "ui.escape") == 0) {
        btrc_Vector_string* __list_51 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_51, "pc");
        return __list_51;
    }
    btrc_Vector_string* empty = btrc_Vector_string_new();
    return empty;
}

void KeymapErrors_init(KeymapErrors* self) {
    self->__rc = 1;
    if (self->levels != NULL) {
        if ((--self->levels->__rc) <= 0) {
            btrc_Vector_string_free(self->levels);
        }
    }
    btrc_Vector_string* __list_53 = btrc_Vector_string_new();
    (self->levels = __list_53);
    btrc_Vector_string* __list_52 = btrc_Vector_string_new();
    (__list_52->__rc++);
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    btrc_Vector_int* __list_55 = btrc_Vector_int_new();
    (self->lines = __list_55);
    btrc_Vector_int* __list_54 = btrc_Vector_int_new();
    (__list_54->__rc++);
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    btrc_Vector_int* __list_57 = btrc_Vector_int_new();
    (self->columns = __list_57);
    btrc_Vector_int* __list_56 = btrc_Vector_int_new();
    (__list_56->__rc++);
    if (self->messages != NULL) {
        if ((--self->messages->__rc) <= 0) {
            btrc_Vector_string_free(self->messages);
        }
    }
    btrc_Vector_string* __list_59 = btrc_Vector_string_new();
    (self->messages = __list_59);
    btrc_Vector_string* __list_58 = btrc_Vector_string_new();
    (__list_58->__rc++);
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
    btrc_Vector_string* __list_61 = btrc_Vector_string_new();
    (self->kinds = __list_61);
    btrc_Vector_string* __list_60 = btrc_Vector_string_new();
    (__list_60->__rc++);
    if (self->texts != NULL) {
        if ((--self->texts->__rc) <= 0) {
            btrc_Vector_string_free(self->texts);
        }
    }
    btrc_Vector_string* __list_63 = btrc_Vector_string_new();
    (self->texts = __list_63);
    btrc_Vector_string* __list_62 = btrc_Vector_string_new();
    (__list_62->__rc++);
    if (self->lines != NULL) {
        if ((--self->lines->__rc) <= 0) {
            btrc_Vector_int_free(self->lines);
        }
    }
    btrc_Vector_int* __list_65 = btrc_Vector_int_new();
    (self->lines = __list_65);
    btrc_Vector_int* __list_64 = btrc_Vector_int_new();
    (__list_64->__rc++);
    if (self->columns != NULL) {
        if ((--self->columns->__rc) <= 0) {
            btrc_Vector_int_free(self->columns);
        }
    }
    btrc_Vector_int* __list_67 = btrc_Vector_int_new();
    (self->columns = __list_67);
    btrc_Vector_int* __list_66 = btrc_Vector_int_new();
    (__list_66->__rc++);
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
    btrc_Vector_string* __list_69 = btrc_Vector_string_new();
    (self->actionIds = __list_69);
    btrc_Vector_string* __list_68 = btrc_Vector_string_new();
    (__list_68->__rc++);
    if (self->actionCommands != NULL) {
        if ((--self->actionCommands->__rc) <= 0) {
            btrc_Vector_string_free(self->actionCommands);
        }
    }
    btrc_Vector_string* __list_71 = btrc_Vector_string_new();
    (self->actionCommands = __list_71);
    btrc_Vector_string* __list_70 = btrc_Vector_string_new();
    (__list_70->__rc++);
    if (self->bindingCombos != NULL) {
        if ((--self->bindingCombos->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingCombos);
        }
    }
    btrc_Vector_string* __list_73 = btrc_Vector_string_new();
    (self->bindingCombos = __list_73);
    btrc_Vector_string* __list_72 = btrc_Vector_string_new();
    (__list_72->__rc++);
    if (self->bindingActions != NULL) {
        if ((--self->bindingActions->__rc) <= 0) {
            btrc_Vector_string_free(self->bindingActions);
        }
    }
    btrc_Vector_string* __list_75 = btrc_Vector_string_new();
    (self->bindingActions = __list_75);
    btrc_Vector_string* __list_74 = btrc_Vector_string_new();
    (__list_74->__rc++);
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
            int __fstr_77_len = snprintf(NULL, 0, "unexpected character '%c'", c);
            char* __fstr_77_buf = __btrc_str_track(((char*)malloc((__fstr_77_len + 1))));
            snprintf(__fstr_77_buf, (__fstr_77_len + 1), "unexpected character '%c'", c);
            KeymapErrors_error(errors, line, column, __fstr_77_buf);
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
        int __fstr_79_len = snprintf(NULL, 0, "expected %s", expected);
        char* __fstr_79_buf = __btrc_str_track(((char*)malloc((__fstr_79_len + 1))));
        snprintf(__fstr_79_buf, (__fstr_79_len + 1), "expected %s", expected);
        KeymapErrors_error(self->errors, 0, 0, __fstr_79_buf);
        return "";
    }
    char* kind = KeymapParser_kind(self);
    if (((strcmp(kind, "ident") == 0) || (strcmp(kind, "string") == 0)) || (strcmp(kind, "ref") == 0)) {
        char* result = KeymapParser_text(self);
        (self->index++);
        return result;
    }
    int __fstr_81_len = snprintf(NULL, 0, "expected %s", expected);
    char* __fstr_81_buf = __btrc_str_track(((char*)malloc((__fstr_81_len + 1))));
    snprintf(__fstr_81_buf, (__fstr_81_len + 1), "expected %s", expected);
    KeymapParser_errorHere(self, __fstr_81_buf);
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
            int __fstr_83_len = snprintf(NULL, 0, "unknown statement '%s'", head);
            char* __fstr_83_buf = __btrc_str_track(((char*)malloc((__fstr_83_len + 1))));
            snprintf(__fstr_83_buf, (__fstr_83_len + 1), "unknown statement '%s'", head);
            KeymapParser_errorHere(self, __fstr_83_buf);
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

bool keymapIrHasAction(KeymapIr* ir, char* id) {
    int __n_85 = btrc_Vector_string_iterLen(ir->actionIds);
    for (int __i_84 = 0; (__i_84 < __n_85); (__i_84++)) {
        char* action = btrc_Vector_string_iterGet(ir->actionIds, __i_84);
        if (strcmp(action, id) == 0) {
            return true;
        }
    }
    return false;
}

btrc_Vector_string* requiredKeymapActions(void) {
    btrc_Vector_string* __list_86 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_86, "ui.open");
    btrc_Vector_string_push(__list_86, "ui.pause");
    btrc_Vector_string_push(__list_86, "ui.screenshot");
    btrc_Vector_string_push(__list_86, "ui.fullscreen");
    btrc_Vector_string_push(__list_86, "ui.menu");
    btrc_Vector_string_push(__list_86, "app.quit");
    btrc_Vector_string_push(__list_86, "state.prev");
    btrc_Vector_string_push(__list_86, "state.next");
    btrc_Vector_string_push(__list_86, "state.load");
    btrc_Vector_string_push(__list_86, "state.save");
    btrc_Vector_string_push(__list_86, "speed.rewind");
    btrc_Vector_string_push(__list_86, "speed.fast");
    btrc_Vector_string_push(__list_86, "screen.swap");
    btrc_Vector_string_push(__list_86, "ui.escape");
    return __list_86;
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
    int __n_88 = btrc_Vector_string_iterLen(requiredKeymapActions());
    for (int __i_87 = 0; (__i_87 < __n_88); (__i_87++)) {
        char* required = btrc_Vector_string_iterGet(requiredKeymapActions(), __i_87);
        if (!keymapIrHasAction(ir, required)) {
            int __fstr_90_len = snprintf(NULL, 0, "missing required action '%s'", required);
            char* __fstr_90_buf = __btrc_str_track(((char*)malloc((__fstr_90_len + 1))));
            snprintf(__fstr_90_buf, (__fstr_90_len + 1), "missing required action '%s'", required);
            KeymapErrors_error(errors, 0, 0, __fstr_90_buf);
        }
    }
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        char* id = btrc_Vector_string_get(ir->actionIds, i);
        char* command = btrc_Vector_string_get(ir->actionCommands, i);
        if (((int)strlen(command)) == 0) {
            int __fstr_92_len = snprintf(NULL, 0, "action '%s' has no key", id);
            char* __fstr_92_buf = __btrc_str_track(((char*)malloc((__fstr_92_len + 1))));
            snprintf(__fstr_92_buf, (__fstr_92_len + 1), "action '%s' has no key", id);
            KeymapErrors_error(errors, 0, 0, __fstr_92_buf);
        }
        if (allowedModifier(command)) {
            int __fstr_94_len = snprintf(NULL, 0, "action '%s' has no key after modifier '%s'", id, command);
            char* __fstr_94_buf = __btrc_str_track(((char*)malloc((__fstr_94_len + 1))));
            snprintf(__fstr_94_buf, (__fstr_94_len + 1), "action '%s' has no key after modifier '%s'", id, command);
            KeymapErrors_error(errors, 0, 0, __fstr_94_buf);
        }
        char* modifiers = keymapCommandModifierPart(command);
        if (((int)strlen(modifiers)) > 0) {
            btrc_Vector_string* parts = Strings_split(modifiers, "+");
            int __n_96 = btrc_Vector_string_iterLen(parts);
            for (int __i_95 = 0; (__i_95 < __n_96); (__i_95++)) {
                char* modifier = btrc_Vector_string_iterGet(parts, __i_95);
                if (!allowedModifier(modifier)) {
                    int __fstr_98_len = snprintf(NULL, 0, "action '%s' uses unsupported modifier '%s'", id, modifier);
                    char* __fstr_98_buf = __btrc_str_track(((char*)malloc((__fstr_98_len + 1))));
                    snprintf(__fstr_98_buf, (__fstr_98_len + 1), "action '%s' uses unsupported modifier '%s'", id, modifier);
                    KeymapErrors_error(errors, 0, 0, __fstr_98_buf);
                }
            }
        }
    }
    for (int i = 0; (i < ir->actionIds->len); (i++)) {
        for (int j = (i + 1); (j < ir->actionIds->len); (j++)) {
            if (strcmp(btrc_Vector_string_get(ir->actionIds, i), btrc_Vector_string_get(ir->actionIds, j)) == 0) {
                int __fstr_100_len = snprintf(NULL, 0, "duplicate action '%s'", btrc_Vector_string_get(ir->actionIds, i));
                char* __fstr_100_buf = __btrc_str_track(((char*)malloc((__fstr_100_len + 1))));
                snprintf(__fstr_100_buf, (__fstr_100_len + 1), "duplicate action '%s'", btrc_Vector_string_get(ir->actionIds, i));
                KeymapErrors_error(errors, 0, 0, __fstr_100_buf);
            }
        }
    }
    for (int i = 0; (i < ir->bindingActions->len); (i++)) {
        if (!keymapIrHasAction(ir, btrc_Vector_string_get(ir->bindingActions, i))) {
            int __fstr_102_len = snprintf(NULL, 0, "binding '%s' references unknown action '%s'", btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingActions, i));
            char* __fstr_102_buf = __btrc_str_track(((char*)malloc((__fstr_102_len + 1))));
            snprintf(__fstr_102_buf, (__fstr_102_len + 1), "binding '%s' references unknown action '%s'", btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingActions, i));
            KeymapErrors_error(errors, 0, 0, __fstr_102_buf);
        }
    }
    for (int i = 0; (i < ir->bindingCombos->len); (i++)) {
        for (int j = (i + 1); (j < ir->bindingCombos->len); (j++)) {
            if (strcmp(btrc_Vector_string_get(ir->bindingCombos, i), btrc_Vector_string_get(ir->bindingCombos, j)) == 0) {
                int __fstr_104_len = snprintf(NULL, 0, "duplicate controller combo '%s'", btrc_Vector_string_get(ir->bindingCombos, i));
                char* __fstr_104_buf = __btrc_str_track(((char*)malloc((__fstr_104_len + 1))));
                snprintf(__fstr_104_buf, (__fstr_104_len + 1), "duplicate controller combo '%s'", btrc_Vector_string_get(ir->bindingCombos, i));
                KeymapErrors_error(errors, 0, 0, __fstr_104_buf);
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
    btrc_Vector_string* __list_105 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_105, jsonStrField("id", id));
    btrc_Vector_string_push(__list_105, jsonStrField("alias", actionAlias(id)));
    btrc_Vector_string_push(__list_105, jsonStrField("label", actionLabel(id)));
    btrc_Vector_string_push(__list_105, jsonStrField("command", btrc_Vector_string_get(ir->actionCommands, index)));
    btrc_Vector_string_push(__list_105, jsonField("systems", jsonStringArray(actionSystems(id))));
    btrc_Vector_string_push(__list_105, jsonStrField("note", actionNote(id)));
    return jsonObject(__list_105);
}

char* keymapIrBindingJson(KeymapIr* ir, int index) {
    char* action = btrc_Vector_string_get(ir->bindingActions, index);
    btrc_Vector_string* __list_106 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_106, jsonStrField("combo", btrc_Vector_string_get(ir->bindingCombos, index)));
    btrc_Vector_string_push(__list_106, jsonStrField("action", action));
    btrc_Vector_string_push(__list_106, jsonStrField("action_alias", actionAlias(action)));
    return jsonObject(__list_106);
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
    btrc_Vector_string* __list_107 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_107, jsonStrField("source_language", "schemulator-keymap-v1"));
    btrc_Vector_string_push(__list_107, jsonStrField("source_path", "${paths.keymaps}/steam_deck.skm"));
    btrc_Vector_string* __list_108 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_108, "manifest");
    btrc_Vector_string_push(__list_108, "retroarch");
    btrc_Vector_string_push(__list_108, "dolphin");
    btrc_Vector_string_push(__list_108, "pcsx2");
    btrc_Vector_string_push(__list_108, "steam-input");
    btrc_Vector_string_push(__list_107, jsonField("render_targets", jsonStringArray(__list_108)));
    btrc_Vector_string_push(__list_107, jsonField("actions", jsonArray(actions)));
    btrc_Vector_string_push(__list_107, jsonField("bindings", jsonArray(bindings)));
    return jsonObject(__list_107);
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
    btrc_Vector_string* __list_109 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_109, jsonField("steam_deck", keymapIrJson(defaultKeymapIr())));
    return jsonObject(__list_109);
}

char* hotkeyFromKeymapIr(KeymapIr* ir, char* actionId) {
    return hotkeySpec(actionAlias(actionId), actionLabel(actionId), irBindingCombo(ir, actionId), irActionCommand(ir, actionId), actionSystems(actionId), actionNote(actionId));
}

char* hotkeySpec(char* id, char* label, char* combo, char* command, btrc_Vector_string* systems, char* note) {
    btrc_Vector_string* __list_110 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_110, jsonStrField("id", id));
    btrc_Vector_string_push(__list_110, jsonStrField("label", label));
    btrc_Vector_string_push(__list_110, jsonStrField("combo", combo));
    btrc_Vector_string_push(__list_110, jsonStrField("command", command));
    btrc_Vector_string_push(__list_110, jsonField("systems", jsonStringArray(systems)));
    btrc_Vector_string_push(__list_110, jsonStrField("note", note));
    return jsonObject(__list_110);
}

char* steamDeckHotkeys(void) {
    KeymapIr* ir = defaultKeymapIr();
    btrc_Vector_string* __list_111 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "ui.pause"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "ui.screenshot"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "ui.fullscreen"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "ui.menu"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "app.quit"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "state.prev"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "state.next"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "state.load"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "state.save"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "speed.rewind"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "speed.fast"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "screen.swap"));
    btrc_Vector_string_push(__list_111, hotkeyFromKeymapIr(ir, "ui.escape"));
    return jsonArray(__list_111);
}

char* manifestPaths(void) {
    btrc_Vector_string* __list_112 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_112, jsonStrField("project_content_root", "${project}/ES-DE/ES-DE"));
    btrc_Vector_string_push(__list_112, jsonStrField("runtime_content_root", "${portable}"));
    btrc_Vector_string_push(__list_112, jsonStrField("roms", "${paths.runtime_content_root}/ROMs"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_roms", "${paths.project_content_root}/ROMs"));
    btrc_Vector_string_push(__list_112, jsonStrField("bios", "${paths.runtime_content_root}/bios"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_bios", "${paths.project_content_root}/bios"));
    btrc_Vector_string_push(__list_112, jsonStrField("saves", "${paths.runtime_content_root}/saves"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_saves", "${paths.project_content_root}/saves"));
    btrc_Vector_string_push(__list_112, jsonStrField("states", "${paths.runtime_content_root}/states"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_states", "${paths.project_content_root}/states"));
    btrc_Vector_string_push(__list_112, jsonStrField("screenshots", "${paths.runtime_content_root}/screenshots"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_screenshots", "${paths.project_content_root}/screenshots"));
    btrc_Vector_string_push(__list_112, jsonStrField("media", "${paths.runtime_content_root}/downloaded_media"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_media", "${paths.project_content_root}/downloaded_media"));
    btrc_Vector_string_push(__list_112, jsonStrField("gamelists", "${paths.runtime_content_root}/gamelists"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_gamelists", "${paths.project_content_root}/gamelists"));
    btrc_Vector_string_push(__list_112, jsonStrField("themes", "${paths.runtime_content_root}/themes"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_themes", "${paths.project_content_root}/themes"));
    btrc_Vector_string_push(__list_112, jsonStrField("custom_systems", "${paths.runtime_content_root}/custom_systems"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_custom_systems", "${project}/ES-DE/custom_systems"));
    btrc_Vector_string_push(__list_112, jsonStrField("settings", "${paths.runtime_content_root}/settings"));
    btrc_Vector_string_push(__list_112, jsonStrField("project_settings", "${project}/ES-DE"));
    btrc_Vector_string_push(__list_112, jsonStrField("keymaps", "${project}/keymaps"));
    btrc_Vector_string_push(__list_112, jsonStrField("retroarch_system", "${paths.bios}"));
    return jsonObject(__list_112);
}

char* esDe(void) {
    btrc_Vector_string* __list_113 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_113, jsonStrField("ROMDirectory", "~/ES-DE/ROMs"));
    btrc_Vector_string_push(__list_113, jsonStrField("MediaDirectory", "~/ES-DE/downloaded_media"));
    btrc_Vector_string_push(__list_113, jsonStrField("UserThemeDirectory", "~/ES-DE/themes"));
    btrc_Vector_string_push(__list_113, jsonStrField("SaveGamelistsMode", "always"));
    btrc_Vector_string_push(__list_113, jsonStrField("CreatePlaceholderSystemDirectories", "false"));
    btrc_Vector_string_push(__list_113, jsonStrField("InputControllerType", "xbox"));
    char* settings = jsonObject(__list_113);
    btrc_Vector_string* __list_115 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_115, jsonField("settings", settings));
    return jsonObject(__list_115);
}

char* controllerModel(char* id, char* label, char* vendor, char* layout, btrc_Vector_string* capabilities, btrc_Vector_string* defaultOutputs, char* preferredBackend, char* gyroPolicy, char* note) {
    btrc_Vector_string* __list_116 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_116, jsonStrField("id", id));
    btrc_Vector_string_push(__list_116, jsonStrField("label", label));
    btrc_Vector_string_push(__list_116, jsonStrField("vendor", vendor));
    btrc_Vector_string_push(__list_116, jsonStrField("layout", layout));
    btrc_Vector_string_push(__list_116, jsonField("capabilities", jsonStringArray(capabilities)));
    btrc_Vector_string_push(__list_116, jsonField("default_outputs", jsonStringArray(defaultOutputs)));
    btrc_Vector_string_push(__list_116, jsonStrField("preferred_backend", preferredBackend));
    btrc_Vector_string_push(__list_116, jsonStrField("gyro_policy", gyroPolicy));
    btrc_Vector_string_push(__list_116, jsonStrField("note", note));
    return jsonObject(__list_116);
}

char* emulationBackend(char* id, char* label, char* layer, btrc_Vector_string* emits, btrc_Vector_string* requires, bool automated, bool visual, char* note) {
    btrc_Vector_string* __list_117 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_117, jsonStrField("id", id));
    btrc_Vector_string_push(__list_117, jsonStrField("label", label));
    btrc_Vector_string_push(__list_117, jsonStrField("layer", layer));
    btrc_Vector_string_push(__list_117, jsonField("emits", jsonStringArray(emits)));
    btrc_Vector_string_push(__list_117, jsonField("requires", jsonStringArray(requires)));
    btrc_Vector_string_push(__list_117, jsonBoolField("automated", automated));
    btrc_Vector_string_push(__list_117, jsonBoolField("requires_visual", visual));
    btrc_Vector_string_push(__list_117, jsonStrField("note", note));
    return jsonObject(__list_117);
}

char* verificationProfile(char* id, char* label, char* backend, btrc_Vector_string* controllers, btrc_Vector_string* checks, bool automated, bool visual) {
    btrc_Vector_string* __list_118 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_118, jsonStrField("id", id));
    btrc_Vector_string_push(__list_118, jsonStrField("label", label));
    btrc_Vector_string_push(__list_118, jsonStrField("backend", backend));
    btrc_Vector_string_push(__list_118, jsonField("controllers", jsonStringArray(controllers)));
    btrc_Vector_string_push(__list_118, jsonField("checks", jsonStringArray(checks)));
    btrc_Vector_string_push(__list_118, jsonBoolField("automated", automated));
    btrc_Vector_string_push(__list_118, jsonBoolField("requires_visual", visual));
    return jsonObject(__list_118);
}

char* screenshotToolSpec(char* id, char* command, btrc_Vector_string* args, btrc_Vector_string* requires, char* note) {
    btrc_Vector_string* __list_119 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_119, jsonStrField("id", id));
    btrc_Vector_string_push(__list_119, jsonStrField("command", command));
    btrc_Vector_string_push(__list_119, jsonField("args", jsonStringArray(args)));
    btrc_Vector_string_push(__list_119, jsonField("requires", jsonStringArray(requires)));
    btrc_Vector_string_push(__list_119, jsonStrField("note", note));
    return jsonObject(__list_119);
}

char* screenshotHookSpec(char* id, char* lifecycle, char* output, char* enabledBy, int delayMs, btrc_Vector_string* emulators, char* note) {
    btrc_Vector_string* __list_120 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_120, jsonStrField("id", id));
    btrc_Vector_string_push(__list_120, jsonStrField("lifecycle", lifecycle));
    btrc_Vector_string_push(__list_120, jsonStrField("output", output));
    btrc_Vector_string_push(__list_120, jsonStrField("enabled_by", enabledBy));
    int __fstr_121_len = snprintf(NULL, 0, "%d", delayMs);
    char* __fstr_121_buf = __btrc_str_track(((char*)malloc((__fstr_121_len + 1))));
    snprintf(__fstr_121_buf, (__fstr_121_len + 1), "%d", delayMs);
    btrc_Vector_string_push(__list_120, jsonField("delay_ms", __fstr_121_buf));
    btrc_Vector_string_push(__list_120, jsonField("emulators", jsonStringArray(emulators)));
    btrc_Vector_string_push(__list_120, jsonStrField("note", note));
    return jsonObject(__list_120);
}

char* screenshotVerificationProfile(void) {
    btrc_Vector_string* emulators = lowercaseValues(linuxLauncherNames());
    char* output = "${paths.project_screenshots}/verification/${emulator}/${hook}.png";
    char* enabledBy = "SCHEMULATOR_SCREENSHOT_HOOKS=1";
    btrc_Vector_string* __list_122 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_122, jsonBoolField("enabled_by_default", false));
    btrc_Vector_string_push(__list_122, jsonStrField("enable_env", enabledBy));
    btrc_Vector_string_push(__list_122, jsonStrField("delay_env", "SCHEMULATOR_SCREENSHOT_DELAY_SECONDS"));
    btrc_Vector_string_push(__list_122, jsonStrField("output_root", "${paths.project_screenshots}/verification"));
    btrc_Vector_string* __list_123 = btrc_Vector_string_new();
    btrc_Vector_string* __list_124 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_124, "${output}");
    btrc_Vector_string* __list_125 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_125, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_123, screenshotToolSpec("grim", "grim", __list_124, __list_125, "SteamOS/KDE Wayland screenshot path."));
    btrc_Vector_string* __list_126 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_126, "-b");
    btrc_Vector_string_push(__list_126, "-n");
    btrc_Vector_string_push(__list_126, "-o");
    btrc_Vector_string_push(__list_126, "${output}");
    btrc_Vector_string* __list_127 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_127, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_123, screenshotToolSpec("spectacle", "spectacle", __list_126, __list_127, "KDE fallback when grim is not installed."));
    btrc_Vector_string* __list_128 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_128, "-f");
    btrc_Vector_string_push(__list_128, "${output}");
    btrc_Vector_string* __list_129 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_129, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_129, "DISPLAY");
    btrc_Vector_string_push(__list_123, screenshotToolSpec("gnome_screenshot", "gnome-screenshot", __list_128, __list_129, "Generic desktop fallback."));
    btrc_Vector_string* __list_130 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_130, "-window");
    btrc_Vector_string_push(__list_130, "root");
    btrc_Vector_string_push(__list_130, "${output}");
    btrc_Vector_string* __list_131 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_131, "DISPLAY");
    btrc_Vector_string_push(__list_123, screenshotToolSpec("imagemagick_import", "import", __list_130, __list_131, "X11 fallback for emulator windows that require XWayland."));
    btrc_Vector_string_push(__list_122, jsonField("tools", jsonArray(__list_123)));
    btrc_Vector_string* __list_132 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_132, screenshotHookSpec("before_launch", "launcher.before_exec", output, enabledBy, 0, emulators, "Captures the desktop state immediately before launching an emulator."));
    btrc_Vector_string_push(__list_132, screenshotHookSpec("after_spawn", "launcher.after_spawn", output, enabledBy, 2000, emulators, "Captures the emulator after the process has had time to present its first frame."));
    btrc_Vector_string_push(__list_132, screenshotHookSpec("after_exit", "launcher.after_exit", output, enabledBy, 0, emulators, "Captures the return path after the emulator exits, usually back to ES-DE."));
    btrc_Vector_string_push(__list_132, screenshotHookSpec("manual_visual_checkpoint", "operator.manual", output, "schemulator screenshot capture", 0, emulators, "CLI hook for VM/Deck verification scripts when a process is already running."));
    btrc_Vector_string_push(__list_122, jsonField("hooks", jsonArray(__list_132)));
    return jsonObject(__list_122);
}

char* inputStack(void) {
    btrc_Vector_string* __list_133 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_133, "steam_deck");
    btrc_Vector_string_push(__list_133, "steam_controller");
    btrc_Vector_string_push(__list_133, "xbox_xinput");
    btrc_Vector_string_push(__list_133, "dualshock4");
    btrc_Vector_string_push(__list_133, "dualsense");
    btrc_Vector_string_push(__list_133, "switch_pro");
    btrc_Vector_string* controllerIds = __list_133;
    btrc_Vector_string* __list_134 = btrc_Vector_string_new();
    btrc_Vector_string* __list_135 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_135, "controller_model");
    btrc_Vector_string_push(__list_135, "emulation_backend");
    btrc_Vector_string_push(__list_135, "emitted_input");
    btrc_Vector_string_push(__list_135, "emulator_keymap");
    btrc_Vector_string_push(__list_134, jsonField("layers", jsonStringArray(__list_135)));
    btrc_Vector_string* __list_136 = btrc_Vector_string_new();
    btrc_Vector_string* __list_137 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_137, "gamepad");
    btrc_Vector_string_push(__list_137, "left_trackpad");
    btrc_Vector_string_push(__list_137, "right_trackpad");
    btrc_Vector_string_push(__list_137, "grip_buttons");
    btrc_Vector_string_push(__list_137, "gyro");
    btrc_Vector_string_push(__list_137, "mouse_pointer");
    btrc_Vector_string_push(__list_137, "keyboard_hotkeys");
    btrc_Vector_string* __list_138 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_138, "gamepad");
    btrc_Vector_string_push(__list_138, "keyboard");
    btrc_Vector_string_push(__list_138, "mouse");
    btrc_Vector_string_push(__list_136, controllerModel("steam_deck", "Steam Deck Built-in Controller", "Valve", "xbox", __list_137, __list_138, "inputplumber", "disabled_by_default", "Primary target: right trackpad mouse, left trackpad radial hotkeys, no gyro."));
    btrc_Vector_string* __list_139 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_139, "gamepad");
    btrc_Vector_string_push(__list_139, "left_trackpad");
    btrc_Vector_string_push(__list_139, "right_trackpad");
    btrc_Vector_string_push(__list_139, "dual_stage_triggers");
    btrc_Vector_string_push(__list_139, "gyro");
    btrc_Vector_string_push(__list_139, "keyboard_hotkeys");
    btrc_Vector_string* __list_140 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_140, "gamepad");
    btrc_Vector_string_push(__list_140, "keyboard");
    btrc_Vector_string_push(__list_140, "mouse");
    btrc_Vector_string_push(__list_136, controllerModel("steam_controller", "Steam Controller", "Valve", "steam", __list_139, __list_140, "inputplumber", "disabled_by_default", "Same abstraction as Deck controls, but with external Steam Controller hardware."));
    btrc_Vector_string* __list_141 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_141, "gamepad");
    btrc_Vector_string_push(__list_141, "analog_triggers");
    btrc_Vector_string_push(__list_141, "rumble");
    btrc_Vector_string* __list_142 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_142, "gamepad");
    btrc_Vector_string_push(__list_142, "keyboard");
    btrc_Vector_string_push(__list_136, controllerModel("xbox_xinput", "Xbox / XInput Controller", "Microsoft", "xbox", __list_141, __list_142, "uinput", "not_available", "Baseline portable gamepad model for SDL/XInput-style controllers."));
    btrc_Vector_string* __list_143 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_143, "gamepad");
    btrc_Vector_string_push(__list_143, "touchpad");
    btrc_Vector_string_push(__list_143, "gyro");
    btrc_Vector_string_push(__list_143, "rumble");
    btrc_Vector_string* __list_144 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_144, "gamepad");
    btrc_Vector_string_push(__list_144, "keyboard");
    btrc_Vector_string_push(__list_144, "mouse");
    btrc_Vector_string_push(__list_136, controllerModel("dualshock4", "DualShock 4", "Sony", "playstation", __list_143, __list_144, "uinput", "disabled_by_default", "PlayStation layout with touchpad available for mouse or menu bindings."));
    btrc_Vector_string* __list_145 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_145, "gamepad");
    btrc_Vector_string_push(__list_145, "touchpad");
    btrc_Vector_string_push(__list_145, "gyro");
    btrc_Vector_string_push(__list_145, "adaptive_triggers");
    btrc_Vector_string_push(__list_145, "rumble");
    btrc_Vector_string* __list_146 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_146, "gamepad");
    btrc_Vector_string_push(__list_146, "keyboard");
    btrc_Vector_string_push(__list_146, "mouse");
    btrc_Vector_string_push(__list_136, controllerModel("dualsense", "DualSense", "Sony", "playstation", __list_145, __list_146, "uinput", "disabled_by_default", "DualShock-family mapping plus DualSense-specific trigger capability metadata."));
    btrc_Vector_string* __list_147 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_147, "gamepad");
    btrc_Vector_string_push(__list_147, "gyro");
    btrc_Vector_string_push(__list_147, "rumble");
    btrc_Vector_string* __list_148 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_148, "gamepad");
    btrc_Vector_string_push(__list_148, "keyboard");
    btrc_Vector_string_push(__list_136, controllerModel("switch_pro", "Nintendo Switch Pro Controller", "Nintendo", "nintendo", __list_147, __list_148, "uinput", "disabled_by_default", "Nintendo layout model; face-button labeling can be handled above emulator keymaps."));
    btrc_Vector_string_push(__list_134, jsonField("controller_models", jsonArray(__list_136)));
    btrc_Vector_string* __list_149 = btrc_Vector_string_new();
    btrc_Vector_string* __list_150 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_150, "gamepad");
    btrc_Vector_string_push(__list_150, "keyboard");
    btrc_Vector_string_push(__list_150, "mouse");
    btrc_Vector_string* __list_151 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_151, "/dev/uinput");
    btrc_Vector_string_push(__list_149, emulationBackend("uinput", "Linux uinput", "kernel_virtual_input", __list_150, __list_151, true, false, "Fast automated Linux smoke tests for emitted key/button events."));
    btrc_Vector_string* __list_152 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_152, "recorded_evdev");
    btrc_Vector_string* __list_153 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_153, "evemu");
    btrc_Vector_string_push(__list_153, "/dev/input");
    btrc_Vector_string_push(__list_149, emulationBackend("evemu", "evemu record/replay", "evdev_replay", __list_152, __list_153, true, false, "Replay captured physical controller events into the same keymap pipeline."));
    btrc_Vector_string* __list_154 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_154, "hid_device");
    btrc_Vector_string* __list_155 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_155, "/dev/uhid");
    btrc_Vector_string_push(__list_149, emulationBackend("uhid", "Linux UHID", "userspace_hid", __list_154, __list_155, true, false, "Use when Steam needs a HID-shaped device instead of a generic uinput pad."));
    btrc_Vector_string* __list_156 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_156, "gamepad");
    btrc_Vector_string_push(__list_156, "keyboard");
    btrc_Vector_string_push(__list_156, "mouse");
    btrc_Vector_string* __list_157 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_157, "inputplumber");
    btrc_Vector_string_push(__list_157, "/dev/uinput");
    btrc_Vector_string_push(__list_149, emulationBackend("inputplumber", "InputPlumber", "routing_daemon", __list_156, __list_157, true, false, "Preferred route for Deck-style trackpad/radial/keyboard composition."));
    btrc_Vector_string* __list_158 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_158, "steam_virtual_gamepad");
    btrc_Vector_string_push(__list_158, "keyboard");
    btrc_Vector_string_push(__list_158, "mouse");
    btrc_Vector_string* __list_159 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_159, "steam");
    btrc_Vector_string_push(__list_159, "gamescope_or_game_mode");
    btrc_Vector_string_push(__list_149, emulationBackend("steam_input", "Steam Input / Game Mode", "steam_client", __list_158, __list_159, false, true, "Final integration layer; requires visual/live Steam verification."));
    btrc_Vector_string_push(__list_134, jsonField("emulation_backends", jsonArray(__list_149)));
    btrc_Vector_string* __list_160 = btrc_Vector_string_new();
    btrc_Vector_string* __list_161 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_161, "device_created");
    btrc_Vector_string_push(__list_161, "save_load_quit_events");
    btrc_Vector_string_push(__list_161, "mouse_motion_optional");
    btrc_Vector_string_push(__list_161, "no_gyro_events_when_disabled");
    btrc_Vector_string_push(__list_160, verificationProfile("linux_virtual_input", "Linux virtual input smoke test", "uinput", controllerIds, __list_161, true, false));
    btrc_Vector_string* __list_162 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_162, "descriptor_replay");
    btrc_Vector_string_push(__list_162, "button_event_order");
    btrc_Vector_string_push(__list_162, "axis_ranges");
    btrc_Vector_string_push(__list_160, verificationProfile("evdev_replay", "Recorded controller replay", "evemu", controllerIds, __list_162, true, false));
    btrc_Vector_string* __list_163 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_163, "steam_deck");
    btrc_Vector_string_push(__list_163, "steam_controller");
    btrc_Vector_string* __list_164 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_164, "steam_detects_device");
    btrc_Vector_string_push(__list_164, "hid_identity_stable");
    btrc_Vector_string_push(__list_160, verificationProfile("steam_hid_shape", "Steam HID shape test", "uhid", __list_163, __list_164, true, false));
    btrc_Vector_string* __list_165 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_165, "steam_deck");
    btrc_Vector_string_push(__list_165, "steam_controller");
    btrc_Vector_string* __list_166 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_166, "right_trackpad_mouse");
    btrc_Vector_string_push(__list_166, "left_radial_hotkeys");
    btrc_Vector_string_push(__list_166, "keyboard_chords");
    btrc_Vector_string_push(__list_160, verificationProfile("deck_route", "Deck-style route test", "inputplumber", __list_165, __list_166, true, false));
    btrc_Vector_string* __list_167 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_167, "steam_deck");
    btrc_Vector_string* __list_168 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_168, "template_visible");
    btrc_Vector_string_push(__list_168, "radial_menu_visual");
    btrc_Vector_string_push(__list_168, "hotkeys_work_in_emulators");
    btrc_Vector_string_push(__list_168, "quit_returns_to_es_de");
    btrc_Vector_string_push(__list_160, verificationProfile("steam_deck_game_mode", "Steam Deck Game Mode final pass", "steam_input", __list_167, __list_168, false, true));
    btrc_Vector_string_push(__list_134, jsonField("verification_profiles", jsonArray(__list_160)));
    return jsonObject(__list_134);
}

char* controllerProfiles(void) {
    btrc_Vector_string* __list_169 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_169, jsonStrField("templates_dir", "~/.steam/steam/controller_base/templates"));
    btrc_Vector_string* __list_170 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_170, steamInputTemplate("neptune_simple", "Schemulator: Steam Deck - Neptune SIMPLE", "${project}/steam-input/neptune-simple.vdf", "Generated Steam Input template; doctor validates it and steam-input install copies it."));
    btrc_Vector_string_push(__list_170, steamInputTemplate("neptune_full", "Schemulator: Steam Deck - Neptune FULL", "${project}/steam-input/neptune-full.vdf", "Optional full hotkey layout for users who want RetroDeck-style chords."));
    btrc_Vector_string_push(__list_169, jsonField("templates", jsonArray(__list_170)));
    char* steamInput = jsonObject(__list_169);
    btrc_Vector_string* __list_173 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_173, jsonStrField("controller_model", "steam_deck"));
    btrc_Vector_string* __list_174 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_174, "linux_virtual_input");
    btrc_Vector_string_push(__list_174, "deck_route");
    btrc_Vector_string_push(__list_174, "steam_deck_game_mode");
    btrc_Vector_string_push(__list_173, jsonField("verification_profiles", jsonStringArray(__list_174)));
    btrc_Vector_string_push(__list_173, jsonStrField("description", "Steam Deck built-in controls with no gyro, right trackpad as mouse, and left trackpad radial hotkeys."));
    btrc_Vector_string_push(__list_173, jsonBoolField("gyro_enabled", false));
    btrc_Vector_string_push(__list_173, jsonStrField("hotkey_button", "View; L4/R4 optional in Steam Input templates"));
    btrc_Vector_string* __list_175 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_175, jsonStrField("left", "radial_hotkeys"));
    btrc_Vector_string_push(__list_175, jsonStrField("right", "mouse"));
    btrc_Vector_string_push(__list_173, jsonField("trackpads", jsonObject(__list_175)));
    btrc_Vector_string_push(__list_173, jsonField("hotkeys", steamDeckHotkeys()));
    btrc_Vector_string_push(__list_173, jsonField("steam_input", steamInput));
    btrc_Vector_string_push(__list_173, jsonField("profiles", jsonStringArray(controllerProfileFiles())));
    char* steamDeck = jsonObject(__list_173);
    btrc_Vector_string* __list_179 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_179, jsonField("steam_deck", steamDeck));
    return jsonObject(__list_179);
}

char* syncFolderSpec(char* id, char* label, char* path, bool enabled, bool watch, int rescanSeconds) {
    btrc_Vector_string* __list_180 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_180, jsonStrField("id", id));
    btrc_Vector_string_push(__list_180, jsonStrField("label", label));
    btrc_Vector_string_push(__list_180, jsonStrField("path", path));
    btrc_Vector_string_push(__list_180, jsonBoolField("enabled", enabled));
    btrc_Vector_string_push(__list_180, jsonBoolField("watch", watch));
    btrc_Vector_string_push(__list_180, jsonField("rescan_interval_s", Strings_fromInt(rescanSeconds)));
    return jsonObject(__list_180);
}

char* syncProfile(void) {
    btrc_Vector_string* __list_181 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_181, jsonBoolField("enabled", true));
    btrc_Vector_string_push(__list_181, jsonBoolField("start_at_boot", true));
    btrc_Vector_string_push(__list_181, jsonBoolField("tray", true));
    btrc_Vector_string_push(__list_181, jsonStrField("engine", "syncthing"));
    btrc_Vector_string_push(__list_181, jsonStrField("tray_app", "syncthingtray"));
    btrc_Vector_string_push(__list_181, jsonStrField("gui_address", "127.0.0.1:8384"));
    btrc_Vector_string* __list_182 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_182, syncFolderSpec("saves", "Saves", "${paths.project_saves}", true, true, 900));
    btrc_Vector_string_push(__list_182, syncFolderSpec("states", "States", "${paths.project_states}", true, true, 900));
    btrc_Vector_string_push(__list_182, syncFolderSpec("emulator_state", "Emulator State", "${paths.project}/.schemulator/appimage-state", true, true, 900));
    btrc_Vector_string_push(__list_182, syncFolderSpec("screenshots", "Screenshots", "${paths.project_screenshots}", true, true, 1800));
    btrc_Vector_string_push(__list_182, syncFolderSpec("gamelists", "Gamelists", "${paths.project_gamelists}", true, true, 1800));
    btrc_Vector_string_push(__list_182, syncFolderSpec("roms", "ROMs", "${paths.project_roms}", false, true, 3600));
    btrc_Vector_string_push(__list_182, syncFolderSpec("bios", "BIOS", "${paths.project_bios}", false, true, 3600));
    btrc_Vector_string_push(__list_181, jsonField("folders", jsonArray(__list_182)));
    return jsonObject(__list_181);
}

char* biosEntries(void) {
    btrc_Vector_string* __list_183 = btrc_Vector_string_new();
    btrc_Vector_string* __list_184 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_184, "scph5500.bin");
    btrc_Vector_string_push(__list_184, "scph5501.bin");
    btrc_Vector_string_push(__list_184, "scph5502.bin");
    btrc_Vector_string_push(__list_183, biosSpec("psx", "psx", true, __list_184, "${paths.bios}", "", "Beetle PSX expects user-supplied PlayStation BIOS dumps."));
    btrc_Vector_string* __list_185 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_185, "ps2-0230a-20080220.bin");
    btrc_Vector_string_push(__list_185, "ps2-0230e-20080220.bin");
    btrc_Vector_string_push(__list_185, "ps2-0230j-20080220.bin");
    btrc_Vector_string_push(__list_183, biosSpec("ps2", "ps2", true, __list_185, "${paths.bios}/ps2", "any", "PCSX2 needs at least one legally dumped PS2 BIOS image."));
    btrc_Vector_string* __list_186 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_186, "prod.keys");
    btrc_Vector_string_push(__list_186, "title.keys");
    btrc_Vector_string_push(__list_183, biosSpec("switch_keys", "switch", true, __list_186, "${paths.bios}/switch", "", "Ryujinx needs dumped Switch keys and firmware from the user's console."));
    btrc_Vector_string* __list_187 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_187, "keys.txt");
    btrc_Vector_string_push(__list_183, biosSpec("wiiu_keys", "wiiu", true, __list_187, "${project}/Cemu/data", "", "Cemu needs user-supplied Wii U keys for encrypted content."));
    btrc_Vector_string* __list_188 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_188, "dc_boot.bin");
    btrc_Vector_string_push(__list_188, "dc_flash.bin");
    btrc_Vector_string_push(__list_183, biosSpec("dreamcast", "dreamcast", false, __list_188, "${paths.bios}/dc", "", "Flycast can use HLE for many games, but original BIOS files improve compatibility."));
    return jsonArray(__list_183);
}

btrc_Vector_string* linuxLauncherNames(void) {
    btrc_Vector_string* __list_189 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_189, "RETROARCH");
    btrc_Vector_string_push(__list_189, "DOLPHIN");
    btrc_Vector_string_push(__list_189, "PPSSPP");
    btrc_Vector_string_push(__list_189, "FLYCAST");
    btrc_Vector_string_push(__list_189, "AZAHAR");
    btrc_Vector_string_push(__list_189, "GOPHER64");
    btrc_Vector_string_push(__list_189, "MELONDS");
    btrc_Vector_string_push(__list_189, "PCSX2");
    btrc_Vector_string_push(__list_189, "CEMU");
    btrc_Vector_string_push(__list_189, "RYUJINX");
    return __list_189;
}

btrc_Vector_string* lowercaseValues(btrc_Vector_string* values) {
    btrc_Vector_string* out = btrc_Vector_string_new();
    int __n_191 = btrc_Vector_string_iterLen(values);
    for (int __i_190 = 0; (__i_190 < __n_191); (__i_190++)) {
        char* value = btrc_Vector_string_iterGet(values, __i_190);
        btrc_Vector_string_push(out, __btrc_str_track(__btrc_toLower(value)));
    }
    return out;
}

char* schemLauncherName(char* emulator) {
    return __btrc_str_track(__btrc_strcat("schem-", __btrc_str_track(__btrc_toLower(emulator))));
}

btrc_Vector_string* retroarchCoreSearchPaths(void) {
    btrc_Vector_string* __list_192 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_192, "/usr/lib/x86_64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_192, "/usr/lib/aarch64-linux-gnu/libretro");
    btrc_Vector_string_push(__list_192, "/usr/lib/libretro");
    return __list_192;
}

char* launcherEntries(void) {
    btrc_Vector_string* linuxEmulatorFields = btrc_Vector_string_new();
    int __n_194 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_193 = 0; (__i_193 < __n_194); (__i_193++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_193);
        btrc_Vector_string_push(linuxEmulatorFields, jsonStrField(emulator, __btrc_str_track(__btrc_strcat("${project}/linux/bin/", schemLauncherName(emulator)))));
    }
    btrc_Vector_string* __list_195 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_195, jsonField("emulators", jsonObject(linuxEmulatorFields)));
    btrc_Vector_string* __list_196 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_196, jsonField("RETROARCH", jsonStringArray(retroarchCoreSearchPaths())));
    btrc_Vector_string_push(__list_195, jsonField("cores", jsonObject(__list_196)));
    btrc_Vector_string* __list_197 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_197, jsonStrField("org.DolphinEmu.dolphin-emu", "GameCube/Wii"));
    btrc_Vector_string_push(__list_197, jsonStrField("org.ppsspp.PPSSPP", "PSP"));
    btrc_Vector_string_push(__list_197, jsonStrField("org.flycast.Flycast", "Dreamcast"));
    btrc_Vector_string_push(__list_197, jsonStrField("org.azahar_emu.Azahar", "Nintendo 3DS"));
    btrc_Vector_string_push(__list_197, jsonStrField("io.github.gopher64.gopher64", "Nintendo 64"));
    btrc_Vector_string_push(__list_197, jsonStrField("net.kuribo64.melonDS", "Nintendo DS"));
    btrc_Vector_string_push(__list_197, jsonStrField("net.pcsx2.PCSX2", "PlayStation 2"));
    btrc_Vector_string_push(__list_197, jsonStrField("info.cemu.Cemu", "Wii U"));
    btrc_Vector_string_push(__list_197, jsonStrField("org.ryujinx.Ryujinx", "Nintendo Switch"));
    btrc_Vector_string_push(__list_195, jsonField("flatpaks", jsonObject(__list_197)));
    char* linux = jsonObject(__list_195);
    btrc_Vector_string* __list_201 = btrc_Vector_string_new();
    btrc_Vector_string* __list_202 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_202, jsonStrField("RETROARCH", "${project}/result/bin/retroarch"));
    btrc_Vector_string_push(__list_202, jsonStrField("AZAHAR", "${project}/result/Applications/azahar.app/Contents/MacOS/azahar"));
    btrc_Vector_string_push(__list_202, jsonStrField("DOLPHIN", "${project}/result/Applications/Dolphin.app/Contents/MacOS/Dolphin"));
    btrc_Vector_string_push(__list_202, jsonStrField("PCSX2", "${project}/result/Applications/PCSX2.app/Contents/MacOS/PCSX2"));
    btrc_Vector_string_push(__list_202, jsonStrField("CEMU", "${project}/result/Applications/Cemu.app/Contents/MacOS/Cemu"));
    btrc_Vector_string_push(__list_202, jsonStrField("RYUJINX", "~/.local/share/ryujinx-app/Ryujinx.app/Contents/MacOS/Ryujinx"));
    btrc_Vector_string_push(__list_202, jsonStrField("ARES", "${project}/result/Applications/ares.app/Contents/MacOS/ares"));
    btrc_Vector_string_push(__list_201, jsonField("emulators", jsonObject(__list_202)));
    btrc_Vector_string* __list_203 = btrc_Vector_string_new();
    btrc_Vector_string* __list_204 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_204, "${project}/result/cores");
    btrc_Vector_string_push(__list_203, jsonField("RETROARCH", jsonStringArray(__list_204)));
    btrc_Vector_string_push(__list_201, jsonField("cores", jsonObject(__list_203)));
    char* macos = jsonObject(__list_201);
    btrc_Vector_string* __list_209 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_209, jsonField("linux", linux));
    btrc_Vector_string_push(__list_209, jsonField("macos", macos));
    return jsonObject(__list_209);
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
    int __n_211 = btrc_Vector_string_iterLen(extensions);
    for (int __i_210 = 0; (__i_210 < __n_211); (__i_210++)) {
        char* extension = btrc_Vector_string_iterGet(extensions, __i_210);
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
    int __n_213 = btrc_Vector_string_iterLen(linuxCommands);
    for (int __i_212 = 0; (__i_212 < __n_213); (__i_212++)) {
        char* command = btrc_Vector_string_iterGet(linuxCommands, __i_212);
        (result = __btrc_str_track(__btrc_strcat(result, esCommandXml(command))));
    }
    (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "    <platform>")), xmlEscape(platform))), "</platform>\n")), "    <theme>")), xmlEscape(theme))), "</theme>\n")), "  </system>\n")));
    return result;
}

void SystemCatalog_init(SystemCatalog* self) {
    self->__rc = 1;
    if (self->ids != NULL) {
        if ((--self->ids->__rc) <= 0) {
            btrc_Vector_string_free(self->ids);
        }
    }
    btrc_Vector_string* __list_215 = btrc_Vector_string_new();
    (self->ids = __list_215);
    btrc_Vector_string* __list_214 = btrc_Vector_string_new();
    (__list_214->__rc++);
    if (self->romDirs != NULL) {
        if ((--self->romDirs->__rc) <= 0) {
            btrc_Vector_string_free(self->romDirs);
        }
    }
    btrc_Vector_string* __list_217 = btrc_Vector_string_new();
    (self->romDirs = __list_217);
    btrc_Vector_string* __list_216 = btrc_Vector_string_new();
    (__list_216->__rc++);
    if (self->jsonSpecs != NULL) {
        if ((--self->jsonSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->jsonSpecs);
        }
    }
    btrc_Vector_string* __list_219 = btrc_Vector_string_new();
    (self->jsonSpecs = __list_219);
    btrc_Vector_string* __list_218 = btrc_Vector_string_new();
    (__list_218->__rc++);
    if (self->esSystemSpecs != NULL) {
        if ((--self->esSystemSpecs->__rc) <= 0) {
            btrc_Vector_string_free(self->esSystemSpecs);
        }
    }
    btrc_Vector_string* __list_221 = btrc_Vector_string_new();
    (self->esSystemSpecs = __list_221);
    btrc_Vector_string* __list_220 = btrc_Vector_string_new();
    (__list_220->__rc++);
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
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by schemulator.btrc from the system catalog -->\n")), "<systemList>\n")), btrc_Vector_string_join(self->esSystemSpecs, ""))), "</systemList>\n"));
}

SystemCatalog* systemCatalog(void) {
    SystemCatalog* catalog = SystemCatalog_new();
    btrc_Vector_string* noBios = btrc_Vector_string_new();
    btrc_Vector_string* __list_225 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_225, ".bin");
    btrc_Vector_string_push(__list_225, ".dmg");
    btrc_Vector_string_push(__list_225, ".gb");
    btrc_Vector_string_push(__list_225, ".gbs");
    btrc_Vector_string_push(__list_225, ".7z");
    btrc_Vector_string_push(__list_225, ".zip");
    btrc_Vector_string* __list_226 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_226, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%"));
    btrc_Vector_string* __list_227 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_227, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gb", "Nintendo Game Boy", "gb", "gb", "gb", __list_225, __list_226, __list_227, noBios, "");
    btrc_Vector_string* __list_231 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_231, ".bin");
    btrc_Vector_string_push(__list_231, ".cgb");
    btrc_Vector_string_push(__list_231, ".gb");
    btrc_Vector_string_push(__list_231, ".gbc");
    btrc_Vector_string_push(__list_231, ".7z");
    btrc_Vector_string_push(__list_231, ".zip");
    btrc_Vector_string* __list_232 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_232, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.so %ROM%"));
    btrc_Vector_string* __list_233 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_233, commandSpec("Gambatte", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/gambatte_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gbc", "Nintendo Game Boy Color", "gbc", "gbc", "gbc", __list_231, __list_232, __list_233, noBios, "");
    btrc_Vector_string* __list_237 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_237, ".agb");
    btrc_Vector_string_push(__list_237, ".bin");
    btrc_Vector_string_push(__list_237, ".cgb");
    btrc_Vector_string_push(__list_237, ".gb");
    btrc_Vector_string_push(__list_237, ".gba");
    btrc_Vector_string_push(__list_237, ".gbc");
    btrc_Vector_string_push(__list_237, ".7z");
    btrc_Vector_string_push(__list_237, ".zip");
    btrc_Vector_string* __list_238 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_238, commandSpec("mGBA", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mgba_libretro.so %ROM%"));
    btrc_Vector_string* __list_239 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_239, commandSpec("mGBA", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mgba_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "gba", "Nintendo Game Boy Advance", "gba", "gba", "gba", __list_237, __list_238, __list_239, noBios, "");
    btrc_Vector_string* __list_243 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_243, ".3dsen");
    btrc_Vector_string_push(__list_243, ".bin");
    btrc_Vector_string_push(__list_243, ".fds");
    btrc_Vector_string_push(__list_243, ".nes");
    btrc_Vector_string_push(__list_243, ".unf");
    btrc_Vector_string_push(__list_243, ".unif");
    btrc_Vector_string_push(__list_243, ".7z");
    btrc_Vector_string_push(__list_243, ".zip");
    btrc_Vector_string* __list_244 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_244, commandSpec("Nestopia", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/nestopia_libretro.so %ROM%"));
    btrc_Vector_string* __list_245 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_245, commandSpec("Mesen", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mesen_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "nes", "Nintendo Entertainment System", "nes", "nes", "nes", __list_243, __list_244, __list_245, noBios, "");
    btrc_Vector_string* __list_249 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_249, ".bin");
    btrc_Vector_string_push(__list_249, ".bml");
    btrc_Vector_string_push(__list_249, ".bs");
    btrc_Vector_string_push(__list_249, ".bsx");
    btrc_Vector_string_push(__list_249, ".fig");
    btrc_Vector_string_push(__list_249, ".sfc");
    btrc_Vector_string_push(__list_249, ".smc");
    btrc_Vector_string_push(__list_249, ".swc");
    btrc_Vector_string_push(__list_249, ".7z");
    btrc_Vector_string_push(__list_249, ".zip");
    btrc_Vector_string* __list_250 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_250, commandSpec("Snes9x", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/snes9x_libretro.so %ROM%"));
    btrc_Vector_string* __list_251 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_251, commandSpec("Snes9x", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/snes9x_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "snes", "Super Nintendo", "snes", "snes", "snes", __list_249, __list_250, __list_251, noBios, "");
    btrc_Vector_string* __list_255 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_255, ".32x");
    btrc_Vector_string_push(__list_255, ".68k");
    btrc_Vector_string_push(__list_255, ".bin");
    btrc_Vector_string_push(__list_255, ".chd");
    btrc_Vector_string_push(__list_255, ".gen");
    btrc_Vector_string_push(__list_255, ".iso");
    btrc_Vector_string_push(__list_255, ".md");
    btrc_Vector_string_push(__list_255, ".mdx");
    btrc_Vector_string_push(__list_255, ".sg");
    btrc_Vector_string_push(__list_255, ".smd");
    btrc_Vector_string_push(__list_255, ".7z");
    btrc_Vector_string_push(__list_255, ".zip");
    btrc_Vector_string* __list_256 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_256, commandSpec("Genesis Plus GX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/genesis_plus_gx_libretro.so %ROM%"));
    btrc_Vector_string* __list_257 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_257, commandSpec("Genesis Plus GX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/genesis_plus_gx_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "genesis", "Sega Genesis", "genesis,megadrive", "genesis", "genesis", __list_255, __list_256, __list_257, noBios, "");
    btrc_Vector_string* __list_261 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_261, ".bin");
    btrc_Vector_string_push(__list_261, ".d64");
    btrc_Vector_string_push(__list_261, ".n64");
    btrc_Vector_string_push(__list_261, ".ndd");
    btrc_Vector_string_push(__list_261, ".u1");
    btrc_Vector_string_push(__list_261, ".v64");
    btrc_Vector_string_push(__list_261, ".z64");
    btrc_Vector_string_push(__list_261, ".7z");
    btrc_Vector_string_push(__list_261, ".zip");
    btrc_Vector_string* __list_262 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_262, commandSpec("Gopher64", "%EMULATOR_GOPHER64% %ROM%"));
    btrc_Vector_string* __list_263 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_263, commandSpec("ares", "%EMULATOR_ARES% --fullscreen --system \"Nintendo 64\" %ROM%"));
    SystemCatalog_addSystem(catalog, "n64", "Nintendo 64", "n64", "n64", "n64", __list_261, __list_262, __list_263, noBios, "");
    btrc_Vector_string* __list_267 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_267, ".app");
    btrc_Vector_string_push(__list_267, ".bin");
    btrc_Vector_string_push(__list_267, ".nds");
    btrc_Vector_string_push(__list_267, ".7z");
    btrc_Vector_string_push(__list_267, ".zip");
    btrc_Vector_string* __list_268 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_268, commandSpec("melonDS", "%EMULATOR_MELONDS% %ROM%"));
    btrc_Vector_string_push(__list_268, commandSpec("DeSmuME", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/desmume_libretro.so %ROM%"));
    btrc_Vector_string* __list_269 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_269, commandSpec("DeSmuME", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/desmume_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "nds", "Nintendo DS", "nds", "nds", "nds", __list_267, __list_268, __list_269, noBios, "");
    btrc_Vector_string* __list_274 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_274, ".cdi");
    btrc_Vector_string_push(__list_274, ".chd");
    btrc_Vector_string_push(__list_274, ".cue");
    btrc_Vector_string_push(__list_274, ".elf");
    btrc_Vector_string_push(__list_274, ".gdi");
    btrc_Vector_string_push(__list_274, ".iso");
    btrc_Vector_string_push(__list_274, ".lst");
    btrc_Vector_string_push(__list_274, ".m3u");
    btrc_Vector_string_push(__list_274, ".7z");
    btrc_Vector_string_push(__list_274, ".zip");
    btrc_Vector_string* __list_275 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_275, commandSpec("Flycast", "%EMULATOR_FLYCAST% %ROM%"));
    btrc_Vector_string* __list_276 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_276, commandSpec("Flycast", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/flycast_libretro.dylib %ROM%"));
    btrc_Vector_string* __list_277 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_277, "dreamcast");
    SystemCatalog_addSystem(catalog, "dreamcast", "Sega Dreamcast", "dreamcast", "dreamcast", "dreamcast", __list_274, __list_275, __list_276, __list_277, "");
    btrc_Vector_string* __list_282 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_282, ".bin");
    btrc_Vector_string_push(__list_282, ".ccd");
    btrc_Vector_string_push(__list_282, ".chd");
    btrc_Vector_string_push(__list_282, ".cue");
    btrc_Vector_string_push(__list_282, ".ecm");
    btrc_Vector_string_push(__list_282, ".exe");
    btrc_Vector_string_push(__list_282, ".img");
    btrc_Vector_string_push(__list_282, ".iso");
    btrc_Vector_string_push(__list_282, ".m3u");
    btrc_Vector_string_push(__list_282, ".pbp");
    btrc_Vector_string_push(__list_282, ".psexe");
    btrc_Vector_string_push(__list_282, ".toc");
    btrc_Vector_string_push(__list_282, ".7z");
    btrc_Vector_string_push(__list_282, ".zip");
    btrc_Vector_string* __list_283 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_283, commandSpec("Beetle PSX", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mednafen_psx_libretro.so %ROM%"));
    btrc_Vector_string* __list_284 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_284, commandSpec("Beetle PSX HW", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/mednafen_psx_hw_libretro.dylib %ROM%"));
    btrc_Vector_string* __list_285 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_285, "psx");
    SystemCatalog_addSystem(catalog, "psx", "Sony PlayStation", "psx", "psx", "psx", __list_282, __list_283, __list_284, __list_285, "");
    btrc_Vector_string* __list_290 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_290, ".bin");
    btrc_Vector_string_push(__list_290, ".chd");
    btrc_Vector_string_push(__list_290, ".cso");
    btrc_Vector_string_push(__list_290, ".dump");
    btrc_Vector_string_push(__list_290, ".gz");
    btrc_Vector_string_push(__list_290, ".img");
    btrc_Vector_string_push(__list_290, ".iso");
    btrc_Vector_string_push(__list_290, ".mdf");
    btrc_Vector_string_push(__list_290, ".nrg");
    btrc_Vector_string_push(__list_290, ".7z");
    btrc_Vector_string_push(__list_290, ".zip");
    btrc_Vector_string* __list_291 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_291, commandSpec("PCSX2", "%EMULATOR_PCSX2% -batch -fullscreen %ROM%"));
    btrc_Vector_string* __list_292 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_292, commandSpec("PCSX2", "%EMULATOR_PCSX2% -batch -fullscreen %ROM%"));
    btrc_Vector_string* __list_293 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_293, "ps2");
    SystemCatalog_addSystem(catalog, "ps2", "Sony PlayStation 2", "ps2", "ps2", "ps2", __list_290, __list_291, __list_292, __list_293, "");
    btrc_Vector_string* __list_297 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_297, ".chd");
    btrc_Vector_string_push(__list_297, ".cso");
    btrc_Vector_string_push(__list_297, ".elf");
    btrc_Vector_string_push(__list_297, ".iso");
    btrc_Vector_string_push(__list_297, ".pbp");
    btrc_Vector_string_push(__list_297, ".prx");
    btrc_Vector_string_push(__list_297, ".7z");
    btrc_Vector_string_push(__list_297, ".zip");
    btrc_Vector_string* __list_298 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_298, commandSpec("PPSSPP", "%EMULATOR_PPSSPP% %ROM%"));
    btrc_Vector_string* __list_299 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_299, commandSpec("PPSSPP", "%EMULATOR_RETROARCH% -f -L %CORE_RETROARCH%/ppsspp_libretro.dylib %ROM%"));
    SystemCatalog_addSystem(catalog, "psp", "Sony PlayStation Portable", "psp", "psp", "psp", __list_297, __list_298, __list_299, noBios, "");
    btrc_Vector_string* __list_303 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_303, ".3ds");
    btrc_Vector_string_push(__list_303, ".3dsx");
    btrc_Vector_string_push(__list_303, ".app");
    btrc_Vector_string_push(__list_303, ".axf");
    btrc_Vector_string_push(__list_303, ".cci");
    btrc_Vector_string_push(__list_303, ".cxi");
    btrc_Vector_string_push(__list_303, ".elf");
    btrc_Vector_string_push(__list_303, ".7z");
    btrc_Vector_string_push(__list_303, ".zip");
    btrc_Vector_string* __list_304 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_304, commandSpec("Azahar", "%EMULATOR_AZAHAR% %ROM%"));
    btrc_Vector_string* __list_305 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_305, commandSpec("Azahar", "%EMULATOR_AZAHAR% %ROM%"));
    SystemCatalog_addSystem(catalog, "n3ds", "Nintendo 3DS", "n3ds", "n3ds", "n3ds", __list_303, __list_304, __list_305, noBios, "");
    btrc_Vector_string* __list_309 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_309, ".ciso");
    btrc_Vector_string_push(__list_309, ".dol");
    btrc_Vector_string_push(__list_309, ".elf");
    btrc_Vector_string_push(__list_309, ".gcm");
    btrc_Vector_string_push(__list_309, ".gcz");
    btrc_Vector_string_push(__list_309, ".iso");
    btrc_Vector_string_push(__list_309, ".nkit.iso");
    btrc_Vector_string_push(__list_309, ".rvz");
    btrc_Vector_string_push(__list_309, ".tgc");
    btrc_Vector_string_push(__list_309, ".wad");
    btrc_Vector_string_push(__list_309, ".wbfs");
    btrc_Vector_string_push(__list_309, ".wia");
    btrc_Vector_string_push(__list_309, ".7z");
    btrc_Vector_string_push(__list_309, ".zip");
    btrc_Vector_string* __list_310 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_310, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% %ROM%"));
    btrc_Vector_string* __list_311 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_311, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% --config Dolphin.Display.Fullscreen=True -b -e %ROM%"));
    SystemCatalog_addSystem(catalog, "gc", "Nintendo GameCube", "gc", "gc", "gc", __list_309, __list_310, __list_311, noBios, "steam_deck");
    btrc_Vector_string* __list_315 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_315, ".ciso");
    btrc_Vector_string_push(__list_315, ".dol");
    btrc_Vector_string_push(__list_315, ".elf");
    btrc_Vector_string_push(__list_315, ".gcm");
    btrc_Vector_string_push(__list_315, ".gcz");
    btrc_Vector_string_push(__list_315, ".iso");
    btrc_Vector_string_push(__list_315, ".nkit.iso");
    btrc_Vector_string_push(__list_315, ".rvz");
    btrc_Vector_string_push(__list_315, ".tgc");
    btrc_Vector_string_push(__list_315, ".wad");
    btrc_Vector_string_push(__list_315, ".wbfs");
    btrc_Vector_string_push(__list_315, ".wia");
    btrc_Vector_string_push(__list_315, ".7z");
    btrc_Vector_string_push(__list_315, ".zip");
    btrc_Vector_string* __list_316 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_316, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% %ROM%"));
    btrc_Vector_string* __list_317 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_317, commandSpec("Dolphin", "%EMULATOR_DOLPHIN% --config Dolphin.Display.Fullscreen=True -b -e %ROM%"));
    SystemCatalog_addSystem(catalog, "wii", "Nintendo Wii", "wii", "wii", "wii", __list_315, __list_316, __list_317, noBios, "steam_deck");
    btrc_Vector_string* __list_322 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_322, ".rpx");
    btrc_Vector_string_push(__list_322, ".wua");
    btrc_Vector_string_push(__list_322, ".wud");
    btrc_Vector_string_push(__list_322, ".wux");
    btrc_Vector_string* __list_323 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_323, commandSpec("Cemu", "%EMULATOR_CEMU% -f -g %ROM%"));
    btrc_Vector_string* __list_324 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_324, commandSpec("Cemu", "%EMULATOR_CEMU% -f -g %ROM%"));
    btrc_Vector_string* __list_325 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_325, "wiiu_keys");
    SystemCatalog_addSystem(catalog, "wiiu", "Nintendo Wii U", "wiiu", "wiiu", "wiiu", __list_322, __list_323, __list_324, __list_325, "steam_deck");
    btrc_Vector_string* __list_330 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_330, ".nca");
    btrc_Vector_string_push(__list_330, ".nro");
    btrc_Vector_string_push(__list_330, ".nso");
    btrc_Vector_string_push(__list_330, ".nsp");
    btrc_Vector_string_push(__list_330, ".xci");
    btrc_Vector_string_push(__list_330, ".7z");
    btrc_Vector_string_push(__list_330, ".zip");
    btrc_Vector_string* __list_331 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_331, commandSpec("Ryujinx", "%EMULATOR_RYUJINX% --fullscreen %ROM%"));
    btrc_Vector_string* __list_332 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_332, commandSpec("Ryujinx", "%EMULATOR_RYUJINX% --fullscreen %ROM%"));
    btrc_Vector_string* __list_333 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_333, "switch_keys");
    SystemCatalog_addSystem(catalog, "switch", "Nintendo Switch", "switch", "switch", "switch", __list_330, __list_331, __list_332, __list_333, "");
    return catalog;
    if (catalog != NULL) {
        if ((--catalog->__rc) <= 0) {
            SystemCatalog_destroy(catalog);
        }
    }
}

char* systemEntries(void) {
    SystemCatalog* catalog = systemCatalog();
    return SystemCatalog_systemsJson(catalog);
}

char* buildJson(void) {
    btrc_Vector_string* __list_334 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_334, jsonField("schema_version", "1"));
    btrc_Vector_string_push(__list_334, jsonField("paths", manifestPaths()));
    btrc_Vector_string_push(__list_334, jsonField("es_de", esDe()));
    btrc_Vector_string_push(__list_334, jsonField("input", inputStack()));
    btrc_Vector_string_push(__list_334, jsonField("controller_profiles", controllerProfiles()));
    btrc_Vector_string_push(__list_334, jsonField("keymaps", keymapsJson()));
    btrc_Vector_string_push(__list_334, jsonField("screenshot_verification", screenshotVerificationProfile()));
    btrc_Vector_string_push(__list_334, jsonField("sync", syncProfile()));
    btrc_Vector_string_push(__list_334, jsonField("bios", biosEntries()));
    btrc_Vector_string_push(__list_334, jsonField("launchers", launcherEntries()));
    btrc_Vector_string_push(__list_334, jsonField("systems", systemEntries()));
    return jsonObject(__list_334);
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

char* keymapsRoot(char* project) {
    return joinPath(project, "keymaps");
}

char* keymapSourcePath(char* project) {
    return joinPath(keymapsRoot(project), "steam_deck.skm");
}

char* homeDir(void) {
    return Environment_get("SCHEMULATOR_HOME", Environment_get("HOME", "."));
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

btrc_Vector_string* declaredSystemIds(void) {
    SystemCatalog* catalog = systemCatalog();
    return catalog->ids;
}

btrc_Vector_string* declaredRomDirs(void) {
    SystemCatalog* catalog = systemCatalog();
    return catalog->romDirs;
}

btrc_Vector_string* controllerProfileFiles(void) {
    btrc_Vector_string* __list_335 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_335, "RetroArch/retroarch.cfg.linux-backup");
    btrc_Vector_string_push(__list_335, "Dolphin/config/Profiles/GCPad/Steam Deck.ini");
    btrc_Vector_string_push(__list_335, "Dolphin/config/Profiles/Hotkeys/Steam Deck.ini");
    btrc_Vector_string_push(__list_335, "Dolphin/config/Profiles/Wiimote/Wiimote (SD).ini");
    btrc_Vector_string_push(__list_335, "Dolphin/config/Profiles/Wiimote/Wiimote + Classic Controller (SD).ini");
    btrc_Vector_string_push(__list_335, "Cemu/config/controllerProfiles/SteamInput-P1.xml");
    btrc_Vector_string_push(__list_335, "PCSX2/config/inputprofiles/Steam Deck.ini");
    btrc_Vector_string_push(__list_335, "Ryujinx/config/profiles/controller/Steam Virtual Controller.json");
    return __list_335;
}

char* esSettingsXmlWithPaths(char* roms, char* media, char* themes) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<string name=\"ROMDirectory\" value=\"")), roms)), "\" />\n")), "<string name=\"MediaDirectory\" value=\"")), media)), "\" />\n")), "<string name=\"UserThemeDirectory\" value=\"")), themes)), "\" />\n")), "<string name=\"SaveGamelistsMode\" value=\"always\" />\n")), "<string name=\"CreatePlaceholderSystemDirectories\" value=\"false\" />\n")), "<string name=\"InputControllerType\" value=\"xbox\" />\n")), "<string name=\"Theme\" value=\"slate-es-de\" />\n"));
}

char* esSettingsXmlForProject(char* project) {
    return esSettingsXmlWithPaths(configuredRomsRoot(project), joinPath(contentRoot(project), "downloaded_media"), joinPath(contentRoot(project), "themes"));
}

void ensureDir(char* path) {
    FileSystem_mkdirp(path);
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
    int __n_337 = btrc_Vector_string_iterLen(FileSystem_listDir(project));
    for (int __i_336 = 0; (__i_336 < __n_337); (__i_336++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(project), __i_336);
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
        int __n_339 = btrc_Vector_string_iterLen(FileSystem_listDir(source));
        for (int __i_338 = 0; (__i_338 < __n_339); (__i_338++)) {
            char* child = btrc_Vector_string_iterGet(FileSystem_listDir(source), __i_338);
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
    return Environment_get("SCHEMULATOR_ROMS_DIR", configuredRomsRoot(project));
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
    int __n_341 = btrc_Vector_string_iterLen(args);
    for (int __i_340 = 0; (__i_340 < __n_341); (__i_340++)) {
        char* arg = btrc_Vector_string_iterGet(args, __i_340);
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
    char* scratch = joinPath(homeDir(), __btrc_str_track(__btrc_strcat(".local/share/schemulator/scratch/", emulator)));
    FileSystem_removeRecursive(scratch);
    ensureDir(scratch);
    char* emuDir = sandboxProjectEmulatorDir(project, emulator);
    if (((int)strlen(emuDir)) == 0) {
        int __fstr_344_len = snprintf(NULL, 0, "sandbox: no emulator directory for '%s' under %s", emulator, project);
        char* __fstr_344_buf = __btrc_str_track(((char*)malloc((__fstr_344_len + 1))));
        snprintf(__fstr_344_buf, (__fstr_344_len + 1), "sandbox: no emulator directory for '%s' under %s", emulator, project);
        printf("%s\n", __fstr_344_buf);
        return 3;
    }
    if (!sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch)) {
        int __fstr_347_len = snprintf(NULL, 0, "sandbox: no BTRC symlink spec for '%s'", emulator);
        char* __fstr_347_buf = __btrc_str_track(((char*)malloc((__fstr_347_len + 1))));
        snprintf(__fstr_347_buf, (__fstr_347_len + 1), "sandbox: no BTRC symlink spec for '%s'", emulator);
        printf("%s\n", __fstr_347_buf);
        return 3;
    }
    char* home = homeDir();
    char* runDir = xdgRunDir();
    char* shareNet = Environment_get("SCHEMULATOR_SHARE_NET", "1");
    char* command = ShellWords_quote(Environment_get("SCHEMULATOR_BWRAP", "bwrap"));
    btrc_Vector_string* __list_348 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_348, "--ro-bind");
    btrc_Vector_string_push(__list_348, "/usr");
    btrc_Vector_string_push(__list_348, "/usr");
    btrc_Vector_string_push(__list_348, "--ro-bind");
    btrc_Vector_string_push(__list_348, "/etc");
    btrc_Vector_string_push(__list_348, "/etc");
    btrc_Vector_string_push(__list_348, "--ro-bind");
    btrc_Vector_string_push(__list_348, "/opt");
    btrc_Vector_string_push(__list_348, "/opt");
    btrc_Vector_string_push(__list_348, "--ro-bind");
    btrc_Vector_string_push(__list_348, "/var");
    btrc_Vector_string_push(__list_348, "/var");
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, "/lib");
    btrc_Vector_string_push(__list_348, "/lib");
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, "/lib64");
    btrc_Vector_string_push(__list_348, "/lib64");
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, "/bin");
    btrc_Vector_string_push(__list_348, "/bin");
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, "/sbin");
    btrc_Vector_string_push(__list_348, "/sbin");
    btrc_Vector_string_push(__list_348, "--proc");
    btrc_Vector_string_push(__list_348, "/proc");
    btrc_Vector_string_push(__list_348, "--dev");
    btrc_Vector_string_push(__list_348, "/dev");
    btrc_Vector_string_push(__list_348, "--dev-bind");
    btrc_Vector_string_push(__list_348, "/dev/dri");
    btrc_Vector_string_push(__list_348, "/dev/dri");
    btrc_Vector_string_push(__list_348, "--dev-bind-try");
    btrc_Vector_string_push(__list_348, "/dev/snd");
    btrc_Vector_string_push(__list_348, "/dev/snd");
    btrc_Vector_string_push(__list_348, "--dev-bind-try");
    btrc_Vector_string_push(__list_348, "/dev/input");
    btrc_Vector_string_push(__list_348, "/dev/input");
    btrc_Vector_string_push(__list_348, "--dev-bind-try");
    btrc_Vector_string_push(__list_348, "/dev/uinput");
    btrc_Vector_string_push(__list_348, "/dev/uinput");
    btrc_Vector_string_push(__list_348, "--ro-bind");
    btrc_Vector_string_push(__list_348, "/sys");
    btrc_Vector_string_push(__list_348, "/sys");
    btrc_Vector_string_push(__list_348, "--bind");
    btrc_Vector_string_push(__list_348, scratch);
    btrc_Vector_string_push(__list_348, home);
    btrc_Vector_string_push(__list_348, "--tmpfs");
    btrc_Vector_string_push(__list_348, "/tmp");
    btrc_Vector_string_push(__list_348, "--bind");
    btrc_Vector_string_push(__list_348, project);
    btrc_Vector_string_push(__list_348, project);
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, launchRomsRoot(project));
    btrc_Vector_string_push(__list_348, launchRomsRoot(project));
    btrc_Vector_string_push(__list_348, "--bind");
    btrc_Vector_string_push(__list_348, runDir);
    btrc_Vector_string_push(__list_348, runDir);
    btrc_Vector_string_push(__list_348, "--ro-bind-try");
    btrc_Vector_string_push(__list_348, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_348, "/tmp/.X11-unix");
    btrc_Vector_string_push(__list_348, "--unshare-pid");
    btrc_Vector_string_push(__list_348, "--unshare-uts");
    btrc_Vector_string_push(__list_348, "--unshare-ipc");
    btrc_Vector_string_push(__list_348, "--die-with-parent");
    btrc_Vector_string_push(__list_348, "--new-session");
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "HOME");
    btrc_Vector_string_push(__list_348, home);
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "XDG_RUNTIME_DIR");
    btrc_Vector_string_push(__list_348, runDir);
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "XDG_DATA_HOME");
    btrc_Vector_string_push(__list_348, joinPath(home, ".local/share"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "XDG_CONFIG_HOME");
    btrc_Vector_string_push(__list_348, joinPath(home, ".config"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "XDG_CACHE_HOME");
    btrc_Vector_string_push(__list_348, joinPath(home, ".cache"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "XDG_SESSION_TYPE");
    btrc_Vector_string_push(__list_348, Environment_get("XDG_SESSION_TYPE", "wayland"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "WAYLAND_DISPLAY");
    btrc_Vector_string_push(__list_348, Environment_get("WAYLAND_DISPLAY", "wayland-0"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "DBUS_SESSION_BUS_ADDRESS");
    btrc_Vector_string_push(__list_348, Environment_get("DBUS_SESSION_BUS_ADDRESS", __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("unix:path=", runDir)), "/bus"))));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "QT_QPA_PLATFORM");
    btrc_Vector_string_push(__list_348, Environment_get("QT_QPA_PLATFORM", "wayland;xcb"));
    btrc_Vector_string_push(__list_348, "--setenv");
    btrc_Vector_string_push(__list_348, "SDL_VIDEODRIVER");
    btrc_Vector_string_push(__list_348, Environment_get("SDL_VIDEODRIVER", "wayland"));
    btrc_Vector_string* bwrapArgs = __list_348;
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
    int __n_350 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_349 = 0; (__i_349 < __n_350); (__i_349++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_349);
        btrc_Vector_string_push(bwrapArgs, arg);
    }
    (command = shellAppendAll(command, bwrapArgs));
    if (strcmp(Environment_get("SCHEMULATOR_DEBUG", "0"), "1") == 0) {
        printf("%s\n", command);
    }
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_351 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_351;
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
        int __fstr_354_len = snprintf(NULL, 0, "sandbox: no emulator directory for '%s' under %s", emulator, project);
        char* __fstr_354_buf = __btrc_str_track(((char*)malloc((__fstr_354_len + 1))));
        snprintf(__fstr_354_buf, (__fstr_354_len + 1), "sandbox: no emulator directory for '%s' under %s", emulator, project);
        printf("%s\n", __fstr_354_buf);
        return 3;
    }
    ensureDir(scratch);
    bool ok = sandboxApplyKnownLinks(__btrc_str_track(__btrc_toLower(emulator)), emuDir, scratch);
    if (!ok) {
        int __fstr_357_len = snprintf(NULL, 0, "sandbox: no BTRC symlink spec for '%s'", emulator);
        char* __fstr_357_buf = __btrc_str_track(((char*)malloc((__fstr_357_len + 1))));
        snprintf(__fstr_357_buf, (__fstr_357_len + 1), "sandbox: no BTRC symlink spec for '%s'", emulator);
        printf("%s\n", __fstr_357_buf);
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
    char* forceX11 = Environment_get("SCHEMULATOR_FLATPAK_X11", Environment_get("SCHEM_FLATPAK_X11", "0"));
    return (((strcmp(key, "azahar") == 0) || (strcmp(key, "dolphin") == 0)) || (strcmp(forceX11, "1") == 0));
}

btrc_Vector_string* launcherPresetArgs(char* emulator) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if (strcmp(key, "dolphin") == 0) {
        btrc_Vector_string* __list_358 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_358, "-b");
        btrc_Vector_string_push(__list_358, "-e");
        return __list_358;
    }
    btrc_Vector_string* empty = btrc_Vector_string_new();
    return empty;
}

char* launcherFlatpakStateRoot(char* project, char* emulator) {
    return joinPath(joinPath(project, ".schemulator/flatpak-state"), __btrc_str_track(__btrc_toLower(emulator)));
}

char* launcherRoutedStateRoot(char* project, char* emulator) {
    return joinPath(joinPath(project, ".schemulator/appimage-state"), __btrc_str_track(__btrc_toLower(emulator)));
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

void launcherSeedRoutedState(char* project, char* emulator, char* configRoot, char* dataRoot) {
    char* key = __btrc_str_track(__btrc_toLower(emulator));
    if (strcmp(key, "retroarch") == 0) {
        launcherCopyDirContents(joinPath(project, "RetroArch/config"), joinPath(configRoot, "retroarch"));
        launcherCopyFile(joinPath(project, "RetroArch/retroarch.cfg"), joinPath(configRoot, "retroarch/retroarch.cfg"));
        return;
    }
    if (strcmp(key, "dolphin") == 0) {
        launcherCopyDirContents(joinPath(project, "Dolphin/config"), joinPath(configRoot, "dolphin-emu"));
        launcherCopyDirContents(joinPath(project, "Dolphin/data"), joinPath(dataRoot, "dolphin-emu"));
        return;
    }
    if (strcmp(key, "pcsx2") == 0) {
        launcherCopyDirContents(joinPath(project, "PCSX2/config"), joinPath(configRoot, "PCSX2"));
        return;
    }
    if (strcmp(key, "cemu") == 0) {
        launcherCopyDirContents(joinPath(project, "Cemu/config"), joinPath(configRoot, "Cemu"));
        launcherCopyDirContents(joinPath(project, "Cemu/data"), joinPath(dataRoot, "Cemu"));
        return;
    }
    if (strcmp(key, "azahar") == 0) {
        launcherCopyDirContents(joinPath(project, "Azahar/data"), joinPath(dataRoot, "azahar-emu"));
        return;
    }
    if (strcmp(key, "ppsspp") == 0) {
        launcherCopyDirContents(joinPath(project, "PPSSPP/config"), joinPath(configRoot, "ppsspp"));
        launcherCopyDirContents(joinPath(project, "PPSSPP/data"), joinPath(dataRoot, "ppsspp"));
        return;
    }
    if (strcmp(key, "flycast") == 0) {
        launcherCopyDirContents(joinPath(project, "Flycast/config"), joinPath(configRoot, "flycast"));
        launcherCopyDirContents(joinPath(project, "Flycast/data"), joinPath(dataRoot, "flycast"));
        return;
    }
    if (strcmp(key, "gopher64") == 0) {
        launcherCopyDirContents(joinPath(project, "Gopher64/config"), joinPath(configRoot, "gopher64"));
        return;
    }
    if (strcmp(key, "melonds") == 0) {
        launcherCopyDirContents(joinPath(project, "melonDS/config"), joinPath(configRoot, "melonDS"));
        launcherCopyDirContents(joinPath(project, "melonDS/data"), joinPath(dataRoot, "melonDS"));
        return;
    }
    if (strcmp(key, "ryujinx") == 0) {
        launcherCopyDirContents(joinPath(project, "Ryujinx/config"), joinPath(configRoot, "Ryujinx"));
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
    int __n_360 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_359 = 0; (__i_359 < __n_360); (__i_359++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_359);
        btrc_Vector_string_push(finalArgs, arg);
    }
    char* roms = launchRomsRoot(project);
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("SCHEMULATOR_PROJECT_DIR=", ShellWords_quote(project))), " SCHEMULATOR_ROMS_DIR=")), ShellWords_quote(roms))), " HOME=")), ShellWords_quote(home))), " XDG_CONFIG_HOME=")), ShellWords_quote(configRoot))), " XDG_DATA_HOME=")), ShellWords_quote(dataRoot))), " XDG_CACHE_HOME=")), ShellWords_quote(cacheRoot))), " ")), ShellWords_quote(executable)));
    (command = shellAppendAll(command, finalArgs));
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_361 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_361;
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
    btrc_Vector_string* __list_362 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat("--filesystem=", project)));
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat("--filesystem=", roms)));
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("--filesystem=", stateRoot)), ":create")));
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat("--env=XDG_CONFIG_HOME=", joinPath(stateRoot, "config"))));
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat("--env=XDG_DATA_HOME=", joinPath(stateRoot, "data"))));
    btrc_Vector_string_push(__list_362, __btrc_str_track(__btrc_strcat("--env=XDG_CACHE_HOME=", joinPath(stateRoot, "cache"))));
    btrc_Vector_string* flatpakArgs = __list_362;
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
    int __n_364 = btrc_Vector_string_iterLen(launcherPresetArgs(emulator));
    for (int __i_363 = 0; (__i_363 < __n_364); (__i_363++)) {
        char* arg = btrc_Vector_string_iterGet(launcherPresetArgs(emulator), __i_363);
        btrc_Vector_string_push(flatpakArgs, arg);
    }
    int __n_366 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_365 = 0; (__i_365 < __n_366); (__i_365++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_365);
        btrc_Vector_string_push(flatpakArgs, arg);
    }
    (command = shellAppendAll(command, flatpakArgs));
    UnixShell* shell = UnixShell_new();
    screenshotCaptureHook(project, emulator, "before_launch");
    screenshotScheduleHook(project, emulator, "after_spawn");
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    screenshotCaptureHook(project, emulator, "after_exit");
    int __btrc_ret_367 = result->code;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_367;
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

void writeGeneratedManifest(char* output) {
    FileSystem_writeText(output, __btrc_str_track(__btrc_strcat(buildJson(), "\n")));
}

char* assetRoot(char* project) {
    char* configured = Environment_get("SCHEMULATOR_ASSET_ROOT", "");
    if ((((int)strlen(configured)) > 0) && FileSystem_exists(joinPath(configured, "linux/bin/schem-retroarch"))) {
        return configured;
    }
    if (FileSystem_exists("linux/bin/schem-retroarch")) {
        return ".";
    }
    if (FileSystem_exists(joinPath(project, "linux/bin/schem-retroarch"))) {
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
    int __n_369 = btrc_Vector_string_iterLen(FileSystem_listDir(sourceDir));
    for (int __i_368 = 0; (__i_368 < __n_369); (__i_368++)) {
        char* name = btrc_Vector_string_iterGet(FileSystem_listDir(sourceDir), __i_368);
        char* source = joinPath(sourceDir, name);
        if (FileSystem_isFile(source)) {
            seedBundledFileFromRoot(root, project, joinPath(relative, name), executable);
        }
    }
}

void seedLinuxAssets(char* project) {
    seedBundledFile(project, "linux/AppRun", true);
    seedBundledFile(project, "linux/sandbox.sh", true);
    seedBundledFile(project, "linux/schemulator.desktop", false);
    seedBundledFile(project, "linux/ES-DE/es_systems_linux.xml", false);
    seedBundledFile(project, "linux/ES-DE/es_find_rules_linux.xml", false);
    seedBundledDirFiles(project, "linux/bin", true);
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
    KeymapIr* __btrc_ret_370 = compileKeymap(defaultKeymapSource(), fallbackErrors);
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
    return __btrc_ret_370;
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
    btrc_Vector_string* __list_371 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_371, "config_save_on_exit = \"true\"");
    btrc_Vector_string_push(__list_371, "video_fullscreen = \"true\"");
    btrc_Vector_string_push(__list_371, "video_vsync = \"true\"");
    btrc_Vector_string_push(__list_371, "audio_driver = \"pulse\"");
    btrc_Vector_string_push(__list_371, "input_driver = \"sdl2\"");
    btrc_Vector_string_push(__list_371, "input_autodetect_enable = \"true\"");
    btrc_Vector_string_push(__list_371, "input_remap_binds_enable = \"true\"");
    btrc_Vector_string_push(__list_371, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("savestate_directory = \"", joinPath(contentRoot(project), "states"))), "\"")));
    btrc_Vector_string_push(__list_371, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("savefile_directory = \"", joinPath(contentRoot(project), "saves"))), "\"")));
    btrc_Vector_string_push(__list_371, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("screenshot_directory = \"", joinPath(contentRoot(project), "screenshots"))), "\"")));
    return __btrc_str_track(__btrc_strcat(textLines(__list_371), renderRetroArchKeymap(ir)));
}

char* dolphinGcpadProfileText(void) {
    btrc_Vector_string* __list_372 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_372, "[Profile]");
    btrc_Vector_string_push(__list_372, "Device = SDL/0/Steam Deck Controller");
    btrc_Vector_string_push(__list_372, "Buttons/A = `Button S`");
    btrc_Vector_string_push(__list_372, "Buttons/B = `Button W`");
    btrc_Vector_string_push(__list_372, "Buttons/X = `Button E`");
    btrc_Vector_string_push(__list_372, "Buttons/Y = `Button N`");
    btrc_Vector_string_push(__list_372, "Buttons/Z = `Shoulder R`|Back");
    btrc_Vector_string_push(__list_372, "Buttons/Start = Start");
    btrc_Vector_string_push(__list_372, "Main Stick/Up = `Axis 1-`");
    btrc_Vector_string_push(__list_372, "Main Stick/Down = `Axis 1+`");
    btrc_Vector_string_push(__list_372, "Main Stick/Left = `Axis 0-`");
    btrc_Vector_string_push(__list_372, "Main Stick/Right = `Axis 0+`");
    btrc_Vector_string_push(__list_372, "C-Stick/Up = `Axis 4-`");
    btrc_Vector_string_push(__list_372, "C-Stick/Down = `Axis 4+`");
    btrc_Vector_string_push(__list_372, "C-Stick/Left = `Axis 3-`");
    btrc_Vector_string_push(__list_372, "C-Stick/Right = `Axis 3+`");
    btrc_Vector_string_push(__list_372, "Triggers/L = `Trigger L`");
    btrc_Vector_string_push(__list_372, "Triggers/R = `Trigger R`");
    btrc_Vector_string_push(__list_372, "Triggers/L-Analog = `Trigger L`");
    btrc_Vector_string_push(__list_372, "Triggers/R-Analog = `Trigger R`");
    btrc_Vector_string_push(__list_372, "D-Pad/Up = `Pad N`");
    btrc_Vector_string_push(__list_372, "D-Pad/Down = `Pad S`");
    btrc_Vector_string_push(__list_372, "D-Pad/Left = `Pad W`");
    btrc_Vector_string_push(__list_372, "D-Pad/Right = `Pad E`");
    btrc_Vector_string_push(__list_372, "Rumble/Motor = Strong");
    btrc_Vector_string_push(__list_372, "Options/Always Connected = True");
    return textLines(__list_372);
}

char* dolphinHotkeysProfileText(KeymapIr* ir) {
    btrc_Vector_string* __list_373 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_373, "[Profile]");
    btrc_Vector_string_push(__list_373, "Device = XInput2/0/Virtual core pointer");
    btrc_Vector_string* __list_374 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_374, "Emulation Speed/Disable Emulation Speed Limit = Tab");
    btrc_Vector_string_push(__list_374, "Controller Profile 1/Next Profile = @(Alt+F5)");
    btrc_Vector_string_push(__list_374, "Other State Hotkeys/Undo Load State = F12");
    btrc_Vector_string_push(__list_374, "Other State Hotkeys/Undo Save State = @(Shift+F12)");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(textLines(__list_373), renderDolphinKeymap(ir))), textLines(__list_374)));
}

char* dolphinWiimoteProfileText(bool classic) {
    btrc_Vector_string* __list_375 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_375, "[Profile]");
    btrc_Vector_string_push(__list_375, "Device = SteamDeck/0/Steam Deck");
    btrc_Vector_string_push(__list_375, "Buttons/A = `XInput2/0/Virtual core pointer:Click 1`");
    btrc_Vector_string_push(__list_375, "Buttons/B = `R4`");
    btrc_Vector_string_push(__list_375, "Buttons/1 = `L4`");
    btrc_Vector_string_push(__list_375, "Buttons/2 = `L5`");
    btrc_Vector_string_push(__list_375, "Buttons/- = View");
    btrc_Vector_string_push(__list_375, "Buttons/+ = Menu");
    btrc_Vector_string_push(__list_375, "D-Pad/Up = `D-Pad Up`");
    btrc_Vector_string_push(__list_375, "D-Pad/Down = `D-Pad Down`");
    btrc_Vector_string_push(__list_375, "D-Pad/Left = `D-Pad Left`");
    btrc_Vector_string_push(__list_375, "D-Pad/Right = `D-Pad Right`");
    btrc_Vector_string_push(__list_375, "IR/Vertical Offset = 12.");
    btrc_Vector_string_push(__list_375, "IR/Total Yaw = 19.");
    btrc_Vector_string_push(__list_375, "IR/Total Pitch = 22.");
    btrc_Vector_string_push(__list_375, "IR/Auto-Hide = True");
    btrc_Vector_string_push(__list_375, "IR/Up = `XInput2/0/Virtual core pointer:Cursor Y-`");
    btrc_Vector_string_push(__list_375, "IR/Down = `XInput2/0/Virtual core pointer:Cursor Y+`");
    btrc_Vector_string_push(__list_375, "IR/Left = `XInput2/0/Virtual core pointer:Cursor X-`");
    btrc_Vector_string_push(__list_375, "IR/Right = `XInput2/0/Virtual core pointer:Cursor X+`");
    btrc_Vector_string_push(__list_375, "IR/Hide = `Thumb L`");
    btrc_Vector_string_push(__list_375, "IMUIR/Enabled = False");
    btrc_Vector_string* lines = __list_375;
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
    btrc_Vector_string* __list_376 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_376, "[Pad]");
    btrc_Vector_string_push(__list_376, "UseProfileHotkeyBindings = true");
    btrc_Vector_string_push(__list_376, "MultitapPort1 = false");
    btrc_Vector_string_push(__list_376, "MultitapPort2 = false");
    btrc_Vector_string_push(__list_376, "");
    btrc_Vector_string_push(__list_376, "[InputSources]");
    btrc_Vector_string_push(__list_376, "Keyboard = true");
    btrc_Vector_string_push(__list_376, "Mouse = true");
    btrc_Vector_string_push(__list_376, "SDL = true");
    btrc_Vector_string_push(__list_376, "DInput = false");
    btrc_Vector_string_push(__list_376, "XInput = false");
    btrc_Vector_string_push(__list_376, "");
    btrc_Vector_string_push(__list_376, "[Pad1]");
    btrc_Vector_string_push(__list_376, "Type = DualShock2");
    btrc_Vector_string_push(__list_376, "Up = SDL-1/DPadUp");
    btrc_Vector_string_push(__list_376, "Right = SDL-1/DPadRight");
    btrc_Vector_string_push(__list_376, "Down = SDL-1/DPadDown");
    btrc_Vector_string_push(__list_376, "Left = SDL-1/DPadLeft");
    btrc_Vector_string_push(__list_376, "Triangle = SDL-1/Y");
    btrc_Vector_string_push(__list_376, "Circle = SDL-1/B");
    btrc_Vector_string_push(__list_376, "Cross = SDL-1/A");
    btrc_Vector_string_push(__list_376, "Square = SDL-1/X");
    btrc_Vector_string_push(__list_376, "Select = SDL-1/Back");
    btrc_Vector_string_push(__list_376, "Start = SDL-1/Start");
    btrc_Vector_string_push(__list_376, "L1 = SDL-1/LeftShoulder");
    btrc_Vector_string_push(__list_376, "L2 = SDL-1/+LeftTrigger");
    btrc_Vector_string_push(__list_376, "R1 = SDL-1/RightShoulder");
    btrc_Vector_string_push(__list_376, "R2 = SDL-1/+RightTrigger");
    btrc_Vector_string_push(__list_376, "L3 = SDL-1/LeftStick");
    btrc_Vector_string_push(__list_376, "R3 = SDL-1/RightStick");
    btrc_Vector_string_push(__list_376, "LUp = SDL-1/-LeftY");
    btrc_Vector_string_push(__list_376, "LRight = SDL-1/+LeftX");
    btrc_Vector_string_push(__list_376, "LDown = SDL-1/+LeftY");
    btrc_Vector_string_push(__list_376, "LLeft = SDL-1/-LeftX");
    btrc_Vector_string_push(__list_376, "RUp = SDL-1/-RightY");
    btrc_Vector_string_push(__list_376, "RRight = SDL-1/+RightX");
    btrc_Vector_string_push(__list_376, "RDown = SDL-1/+RightY");
    btrc_Vector_string_push(__list_376, "RLeft = SDL-1/-RightX");
    btrc_Vector_string_push(__list_376, "");
    btrc_Vector_string_push(__list_376, "[Pad2]");
    btrc_Vector_string_push(__list_376, "Type = None");
    btrc_Vector_string_push(__list_376, "");
    btrc_Vector_string_push(__list_376, "[Hotkeys]");
    return __btrc_str_track(__btrc_strcat(textLines(__list_376), renderPcsx2Keymap(ir)));
}

char* cemuProfileText(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", "<emulated_controller>\n")), "\t<type>Wii U GamePad</type>\n")), "\t<profile>SteamInput-P1</profile>\n")), "\t<controller>\n")), "\t\t<api>SDLController</api>\n")), "\t\t<display_name>Steam Virtual Gamepad</display_name>\n")), "\t\t<rumble>0</rumble>\n")), "\t</controller>\n")), "</emulated_controller>\n"));
}

char* ryujinxProfileText(void) {
    btrc_Vector_string* __list_377 = btrc_Vector_string_new();
    btrc_Vector_string* __list_378 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_378, jsonStrField("joystick", "Left"));
    btrc_Vector_string_push(__list_378, jsonBoolField("invert_stick_x", false));
    btrc_Vector_string_push(__list_378, jsonBoolField("invert_stick_y", false));
    btrc_Vector_string_push(__list_378, jsonStrField("stick_button", "LeftStick"));
    btrc_Vector_string_push(__list_377, jsonField("left_joycon_stick", jsonObject(__list_378)));
    btrc_Vector_string* __list_379 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_379, jsonStrField("joystick", "Right"));
    btrc_Vector_string_push(__list_379, jsonBoolField("invert_stick_x", false));
    btrc_Vector_string_push(__list_379, jsonBoolField("invert_stick_y", false));
    btrc_Vector_string_push(__list_379, jsonStrField("stick_button", "RightStick"));
    btrc_Vector_string_push(__list_377, jsonField("right_joycon_stick", jsonObject(__list_379)));
    btrc_Vector_string* __list_380 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_380, jsonField("slot", "0"));
    btrc_Vector_string_push(__list_380, jsonField("alt_slot", "0"));
    btrc_Vector_string_push(__list_380, jsonBoolField("mirror_input", false));
    btrc_Vector_string_push(__list_380, jsonStrField("motion_backend", "CemuHook"));
    btrc_Vector_string_push(__list_380, jsonField("sensitivity", "100"));
    btrc_Vector_string_push(__list_380, jsonField("gyro_deadzone", "1"));
    btrc_Vector_string_push(__list_380, jsonBoolField("enable_motion", false));
    btrc_Vector_string_push(__list_377, jsonField("motion", jsonObject(__list_380)));
    btrc_Vector_string* __list_381 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_381, jsonField("strong_rumble", "1"));
    btrc_Vector_string_push(__list_381, jsonField("weak_rumble", "1"));
    btrc_Vector_string_push(__list_381, jsonBoolField("enable_rumble", true));
    btrc_Vector_string_push(__list_377, jsonField("rumble", jsonObject(__list_381)));
    btrc_Vector_string* __list_382 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_382, jsonStrField("button_minus", "Back"));
    btrc_Vector_string_push(__list_382, jsonStrField("button_l", "LeftShoulder"));
    btrc_Vector_string_push(__list_382, jsonStrField("button_zl", "LeftTrigger"));
    btrc_Vector_string_push(__list_382, jsonStrField("dpad_up", "DpadUp"));
    btrc_Vector_string_push(__list_382, jsonStrField("dpad_down", "DpadDown"));
    btrc_Vector_string_push(__list_382, jsonStrField("dpad_left", "DpadLeft"));
    btrc_Vector_string_push(__list_382, jsonStrField("dpad_right", "DpadRight"));
    btrc_Vector_string_push(__list_377, jsonField("left_joycon", jsonObject(__list_382)));
    btrc_Vector_string* __list_383 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_383, jsonStrField("button_plus", "Start"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_r", "RightShoulder"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_zr", "RightTrigger"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_x", "Y"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_b", "A"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_y", "X"));
    btrc_Vector_string_push(__list_383, jsonStrField("button_a", "B"));
    btrc_Vector_string_push(__list_377, jsonField("right_joycon", jsonObject(__list_383)));
    btrc_Vector_string_push(__list_377, jsonField("version", "1"));
    btrc_Vector_string_push(__list_377, jsonStrField("backend", "GamepadSDL2"));
    btrc_Vector_string_push(__list_377, jsonStrField("id", "0-f7390003-28de-0000-ff11-000001000000"));
    btrc_Vector_string_push(__list_377, jsonStrField("controller_type", "ProController"));
    btrc_Vector_string_push(__list_377, jsonStrField("player_index", "Player1"));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_377), "\n"));
}

void writeProfile(char* project, char* relative, char* text) {
    char* target = joinPath(project, relative);
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
    char* roms = ((((int)strlen(romsDir)) > 0) ? romsDir : "${paths.project_roms}");
    btrc_Vector_string* __list_384 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_384, jsonBoolField("enabled", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("start_at_boot", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("tray", true));
    btrc_Vector_string_push(__list_384, jsonStrField("gui_address", "127.0.0.1:8384"));
    btrc_Vector_string_push(__list_384, jsonStrField("roms_dir", roms));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_saves", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_states", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_emulator_state", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_screenshots", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_gamelists", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_roms", false));
    btrc_Vector_string_push(__list_384, jsonBoolField("sync_bios", false));
    btrc_Vector_string_push(__list_384, jsonField("rescan_saves_s", "900"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_states_s", "900"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_emulator_state_s", "900"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_screenshots_s", "1800"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_gamelists_s", "1800"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_roms_s", "3600"));
    btrc_Vector_string_push(__list_384, jsonField("rescan_bios_s", "3600"));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_saves", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_states", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_emulator_state", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_screenshots", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_gamelists", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_roms", true));
    btrc_Vector_string_push(__list_384, jsonBoolField("watch_bios", true));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_384), "\n"));
}

void writeSyncDefaults(char* project, char* romsDir) {
    ensureDir(syncRoot(project));
    ensureDir(syncScriptsDir(project));
    ensureDir(syncLogDir(project));
    ensureDir(syncthingConfigDir(project));
    ensureDir(syncthingDataDir(project));
    char* path = syncConfigPath(project);
    if (!FileSystem_exists(path)) {
        FileSystem_writeText(path, syncDefaultConfigText(project, romsDir));
        return;
    }
    if (((int)strlen(romsDir)) > 0) {
        JsonObject* config = JsonObject_readFile(path);
        JsonObject_setString(config, "roms_dir", romsDir);
        JsonObject_writeFile(config, path);
    }
}

void ensureRomDirsAt(char* root) {
    ensureDir(root);
    int __n_386 = btrc_Vector_string_iterLen(declaredRomDirs());
    for (int __i_385 = 0; (__i_385 < __n_386); (__i_385++)) {
        char* rom = btrc_Vector_string_iterGet(declaredRomDirs(), __i_385);
        ensureDir(joinPath(root, rom));
    }
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
        int __n_388 = btrc_Vector_string_iterLen(Strings_split(modifiers, "+"));
        for (int __i_387 = 0; (__i_387 < __n_388); (__i_387++)) {
            char* modifier = btrc_Vector_string_iterGet(Strings_split(modifiers, "+"), __i_387);
            btrc_Vector_string_push(keys, steamInputKeyName(modifier));
        }
    }
    btrc_Vector_string_push(keys, steamInputKeyName(keymapCommandKeyPart(command)));
    btrc_Vector_string* bindings = btrc_Vector_string_new();
    int __n_390 = btrc_Vector_string_iterLen(keys);
    for (int __i_389 = 0; (__i_389 < __n_390); (__i_389++)) {
        char* key = btrc_Vector_string_iterGet(keys, __i_389);
        btrc_Vector_string_push(bindings, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"binding\" \"key_press ", key)), ", ")), label)), ", , \"")));
    }
    return btrc_Vector_string_join(bindings, " ");
}

char* steamInputActionBindings(KeymapIr* ir, char* actionId) {
    return steamInputKeyBindings(irActionCommand(ir, actionId), steamInputActionLabel(actionId));
}

char* steamInputTemplateVdf(char* title, bool full, KeymapIr* ir) {
    char* radialName = (full ? "Schemulator Full Radial" : "Schemulator Simple Radial");
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("\"controller_mappings\"\n", "{\n")), "\t\"version\"\t\t\"3\"\n")), "\t\"revision\"\t\t\"1\"\n")), "\t\"title\"\t\t\"")), title)), "\"\n")), "\t\"description\"\t\t\"Steam Deck controls for Schemulator: no gyro, right pad mouse, left pad hotkeys.\"\n")), "\t\"creator\"\t\t\"Schemulator\"\n")), "\t\"controller_type\"\t\t\"controller_neptune\"\n")), "\t\"actions\"\n")), "\t{\n")), "\t\t\"Default\" { \"title\" \"Gamepad\" \"legacy_set\" \"1\" }\n")), "\t\t\"Preset_1000001\" { \"title\" \"Hotkeys\" \"legacy_set\" \"1\" }\n")), "\t}\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"0\"\n")), "\t\t\"mode\" \"four_buttons\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"button_a\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button A, , \" } } } }\n")), "\t\t\t\"button_b\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button B, , \" } } } }\n")), "\t\t\t\"button_x\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button X, , \" } } } }\n")), "\t\t\t\"button_y\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button Y, , \" } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"1\"\n")), "\t\t\"mode\" \"dpad\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"dpad_north\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button DPAD_UP, , \" } } } }\n")), "\t\t\t\"dpad_south\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button DPAD_DOWN, , \" } } } }\n")), "\t\t\t\"dpad_east\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button DPAD_RIGHT, , \" } } } }\n")), "\t\t\t\"dpad_west\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button DPAD_LEFT, , \" } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\" { \"id\" \"2\" \"mode\" \"joystick_move\" }\n")), "\t\"group\" { \"id\" \"3\" \"mode\" \"joystick_move\" \"inputs\" { \"click\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button JOYSTICK_RIGHT, , \" } } } } } }\n")), "\t\"group\" { \"id\" \"4\" \"mode\" \"trigger\" \"inputs\" { \"click\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button TRIGGER_LEFT, , \" } } } } } }\n")), "\t\"group\" { \"id\" \"5\" \"mode\" \"trigger\" \"inputs\" { \"click\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button TRIGGER_RIGHT, , \" } } } } } }\n")), "\t\"group\" { \"id\" \"6\" \"mode\" \"absolute_mouse\" \"inputs\" { \"click\" { \"activators\" { \"Soft_Press\" { \"bindings\" { \"binding\" \"mouse_button LEFT, , \" } } } } } }\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"7\"\n")), "\t\t\"mode\" \"switches\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"button_escape\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button START, , \" } } } }\n")), "\t\t\t\"button_menu\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button SELECT, , \" } } \"Long_Press\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 2 0 0, Hotkey, , \" } } } }\n")), "\t\t\t\"left_bumper\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button SHOULDER_LEFT, , \" } } } }\n")), "\t\t\t\"right_bumper\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"xinput_button SHOULDER_RIGHT, , \" } } } }\n")), "\t\t\t\"button_back_left_upper\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 2 0 0, Hotkey, , \" } } } }\n")), "\t\t\t\"button_back_right_upper\" { \"activators\" { \"Full_Press\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 2 0 0, Hotkey, , \" } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"10\"\n")), "\t\t\"mode\" \"four_buttons\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"button_a\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.pause"))), " } } } }\n")), "\t\t\t\"button_b\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.screenshot"))), " } } } }\n")), "\t\t\t\"button_x\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.fullscreen"))), " } } } }\n")), "\t\t\t\"button_y\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.menu"))), " } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\" { \"id\" \"11\" \"mode\" \"trigger\" \"inputs\" { \"click\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "speed.rewind"))), " } } } } } }\n")), "\t\"group\" { \"id\" \"12\" \"mode\" \"trigger\" \"inputs\" { \"click\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "speed.fast"))), " } } } } } }\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"13\"\n")), "\t\t\"mode\" \"dpad\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"dpad_north\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.open"))), " } } } }\n")), "\t\t\t\"dpad_south\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.escape"))), " } } } }\n")), "\t\t\t\"dpad_east\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.next"))), " } } } }\n")), "\t\t\t\"dpad_west\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.prev"))), " } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"14\"\n")), "\t\t\"mode\" \"switches\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"button_escape\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "app.quit"))), " } } } }\n")), "\t\t\t\"button_menu\" { \"activators\" { \"release\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 1 0 0, , \" } } } }\n")), "\t\t\t\"left_bumper\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.load"))), " } } } }\n")), "\t\t\t\"right_bumper\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.save"))), " } } } }\n")), "\t\t\t\"button_back_left_upper\" { \"activators\" { \"release\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 1 0 0, , \" } } } }\n")), "\t\t\t\"button_back_right_upper\" { \"activators\" { \"release\" { \"bindings\" { \"binding\" \"controller_action CHANGE_PRESET 1 0 0, , \" } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"group\"\n")), "\t{\n")), "\t\t\"id\" \"20\"\n")), "\t\t\"mode\" \"radial_menu\"\n")), "\t\t\"name\" \"")), radialName)), "\"\n")), "\t\t\"inputs\"\n")), "\t\t{\n")), "\t\t\t\"touch_menu_button_0\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.save"))), " } } } }\n")), "\t\t\t\"touch_menu_button_1\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "state.load"))), " } } } }\n")), "\t\t\t\"touch_menu_button_2\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "app.quit"))), " } } } }\n")), "\t\t\t\"touch_menu_button_3\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.menu"))), " } } } }\n")), "\t\t\t\"touch_menu_button_4\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.screenshot"))), " } } } }\n")), "\t\t\t\"touch_menu_button_5\" { \"activators\" { \"Full_Press\" { \"bindings\" { ")), steamInputActionBindings(ir, "ui.escape"))), " } } } }\n")), "\t\t}\n")), "\t}\n")), "\t\"preset\" { \"id\" \"0\" \"name\" \"Default\" \"group_source_bindings\" { \"0\" \"button_diamond active\" \"1\" \"dpad active\" \"2\" \"joystick active\" \"3\" \"right_joystick active\" \"4\" \"left_trigger active\" \"5\" \"right_trigger active\" \"6\" \"right_trackpad active\" \"7\" \"switch active\" \"20\" \"left_trackpad active\" } }\n")), "\t\"preset\" { \"id\" \"1\" \"name\" \"Preset_1000001\" \"group_source_bindings\" { \"10\" \"button_diamond active\" \"13\" \"dpad active\" \"2\" \"joystick active\" \"3\" \"right_joystick active\" \"11\" \"left_trigger active\" \"12\" \"right_trigger active\" \"6\" \"right_trackpad active\" \"14\" \"switch active\" \"20\" \"left_trackpad active\" } }\n")), "\t\"settings\" { \"left_trackpad_mode\" \"0\" \"right_trackpad_mode\" \"0\" }\n")), "}\n"));
}

void writeSteamInputTemplates(char* project) {
    ensureDir(joinPath(project, "steam-input"));
    KeymapIr* ir = projectKeymapIr(project);
    FileSystem_writeText(joinPath(project, "steam-input/neptune-simple.vdf"), steamInputTemplateVdf("Schemulator: Steam Deck - Neptune SIMPLE", false, ir));
    FileSystem_writeText(joinPath(project, "steam-input/neptune-full.vdf"), steamInputTemplateVdf("Schemulator: Steam Deck - Neptune FULL", true, ir));
}

char* esFindRulesXml(char* launcherBin) {
    char* result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<?xml version=\"1.0\"?>\n", "<!-- Generated by schemulator.btrc from the launcher catalog -->\n")), "<ruleList>\n"));
    int __n_392 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_391 = 0; (__i_391 < __n_392); (__i_391++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_391);
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "  <emulator name=\"")), xmlEscape(emulator))), "\">\n")), "    <rule type=\"staticpath\">\n")), "      <entry>")), xmlEscape(joinPath(launcherBin, schemLauncherName(emulator))))), "</entry>\n")), "    </rule>\n")), "  </emulator>\n")));
    }
    (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "  <core name=\"RETROARCH\">\n")), "    <rule type=\"corepath\">\n")));
    int __n_394 = btrc_Vector_string_iterLen(retroarchCoreSearchPaths());
    for (int __i_393 = 0; (__i_393 < __n_394); (__i_393++)) {
        char* corePath = btrc_Vector_string_iterGet(retroarchCoreSearchPaths(), __i_393);
        (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "      <entry>")), xmlEscape(corePath))), "</entry>\n")));
    }
    (result = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(result, "    </rule>\n")), "  </core>\n")), "</ruleList>\n")));
    return result;
}

void writeEsDeFiles(char* project) {
    ensureDir(customSystemsRoot(project));
    SystemCatalog* catalog = systemCatalog();
    FileSystem_writeText(joinPath(customSystemsRoot(project), "es_systems.xml"), SystemCatalog_esSystemsXml(catalog));
    char* launcherBin = Environment_get("SCHEMULATOR_LAUNCHER_BIN", joinPath(project, "linux/bin"));
    FileSystem_writeText(joinPath(customSystemsRoot(project), "es_find_rules.xml"), esFindRulesXml(launcherBin));
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
    char* launcherBin = Environment_get("SCHEMULATOR_LAUNCHER_BIN", joinPath(project, "linux/bin"));
    FileSystem_writeText(joinPath(customRoot, "es_find_rules.xml"), esFindRulesXml(launcherBin));
    char* roms = Environment_get("SCHEMULATOR_ROMS_DIR", configuredRomsRoot(project));
    FileSystem_writeText(joinPath(settingsRoot, "es_settings.xml"), esSettingsXmlForRuntime(project, roms));
}

void bootstrapSteamDeck(char* project) {
    writeSyncDefaults(project, "");
    ensureDir(contentRoot(project));
    ensureRomDirsAt(configuredRomsRoot(project));
    if (!(strcmp(configuredRomsRoot(project), romsRoot(project)) == 0)) {
        ensureRomDirsAt(romsRoot(project));
    }
    ensureDir(biosRoot(project));
    ensureDir(joinPath(contentRoot(project), "saves"));
    ensureDir(joinPath(contentRoot(project), "states"));
    ensureDir(joinPath(contentRoot(project), "screenshots"));
    ensureDir(joinPath(contentRoot(project), "downloaded_media"));
    ensureDir(joinPath(contentRoot(project), "gamelists"));
    ensureDir(joinPath(contentRoot(project), "themes"));
    ensureDir(customSystemsRoot(project));
    btrc_Vector_string* ids = declaredSystemIds();
    int __n_396 = btrc_Vector_string_iterLen(ids);
    for (int __i_395 = 0; (__i_395 < __n_396); (__i_395++)) {
        char* id = btrc_Vector_string_iterGet(ids, __i_395);
        ensureDir(joinPath(joinPath(contentRoot(project), "downloaded_media"), id));
        ensureDir(joinPath(joinPath(contentRoot(project), "gamelists"), id));
    }
    ensureDir(joinPath(biosRoot(project), "ps2"));
    ensureDir(joinPath(biosRoot(project), "switch"));
    ensureDir(joinPath(biosRoot(project), "dc"));
    ensureDir(joinPath(project, "Cemu/data"));
    ensureDir(joinPath(project, "steam-input"));
    seedKeymapDefaults(project);
    seedEmulatorDefaults(project);
    writeSteamInputTemplates(project);
    writeScreenshotDefaults(project);
    writeGeneratedManifest(joinPath(project, "schemulator.json"));
    writeEsDeFiles(project);
    int __fstr_399_len = snprintf(NULL, 0, "Bootstrapped Steam Deck/Linux content at %s", contentRoot(project));
    char* __fstr_399_buf = __btrc_str_track(((char*)malloc((__fstr_399_len + 1))));
    snprintf(__fstr_399_buf, (__fstr_399_len + 1), "Bootstrapped Steam Deck/Linux content at %s", contentRoot(project));
    printf("%s\n", __fstr_399_buf);
    int __fstr_402_len = snprintf(NULL, 0, "  roms:    %s", romsRoot(project));
    char* __fstr_402_buf = __btrc_str_track(((char*)malloc((__fstr_402_len + 1))));
    snprintf(__fstr_402_buf, (__fstr_402_len + 1), "  roms:    %s", romsRoot(project));
    printf("%s\n", __fstr_402_buf);
    int __fstr_405_len = snprintf(NULL, 0, "  bios:    %s", biosRoot(project));
    char* __fstr_405_buf = __btrc_str_track(((char*)malloc((__fstr_405_len + 1))));
    snprintf(__fstr_405_buf, (__fstr_405_len + 1), "  bios:    %s", biosRoot(project));
    printf("%s\n", __fstr_405_buf);
    int __fstr_408_len = snprintf(NULL, 0, "  systems: %s", joinPath(customSystemsRoot(project), "es_systems.xml"));
    char* __fstr_408_buf = __btrc_str_track(((char*)malloc((__fstr_408_len + 1))));
    snprintf(__fstr_408_buf, (__fstr_408_len + 1), "  systems: %s", joinPath(customSystemsRoot(project), "es_systems.xml"));
    printf("%s\n", __fstr_408_buf);
}

bool allPresent(char* target, btrc_Vector_string* files) {
    int __n_410 = btrc_Vector_string_iterLen(files);
    for (int __i_409 = 0; (__i_409 < __n_410); (__i_409++)) {
        char* file = btrc_Vector_string_iterGet(files, __i_409);
        if (!FileSystem_exists(joinPath(target, file))) {
            return false;
        }
    }
    return true;
}

bool anyPresent(char* target, btrc_Vector_string* files) {
    int __n_412 = btrc_Vector_string_iterLen(files);
    for (int __i_411 = 0; (__i_411 < __n_412); (__i_411++)) {
        char* file = btrc_Vector_string_iterGet(files, __i_411);
        if (FileSystem_exists(joinPath(target, file))) {
            return true;
        }
    }
    return false;
}

void reportPath(char* label, char* path) {
    char* mark = (FileSystem_isDir(path) ? "OK" : "MISSING");
    int __fstr_415_len = snprintf(NULL, 0, "  %s %s: %s", mark, label, path);
    char* __fstr_415_buf = __btrc_str_track(((char*)malloc((__fstr_415_len + 1))));
    snprintf(__fstr_415_buf, (__fstr_415_len + 1), "  %s %s: %s", mark, label, path);
    printf("%s\n", __fstr_415_buf);
}

void reportBios(char* id, char* target, btrc_Vector_string* files, bool required, bool anyMatch) {
    bool ok = (anyMatch ? anyPresent(target, files) : allPresent(target, files));
    char* mark = (ok ? "OK" : (required ? "MISSING" : "optional"));
    char* requiredText = (required ? "required" : "optional");
    int __fstr_418_len = snprintf(NULL, 0, "  %s %s (%s) -> %s", mark, id, requiredText, target);
    char* __fstr_418_buf = __btrc_str_track(((char*)malloc((__fstr_418_len + 1))));
    snprintf(__fstr_418_buf, (__fstr_418_len + 1), "  %s %s (%s) -> %s", mark, id, requiredText, target);
    printf("%s\n", __fstr_418_buf);
    if ((!ok) && required) {
        int __fstr_421_len = snprintf(NULL, 0, "       expected: %s", btrc_Vector_string_join(files, ", "));
        char* __fstr_421_buf = __btrc_str_track(((char*)malloc((__fstr_421_len + 1))));
        snprintf(__fstr_421_buf, (__fstr_421_len + 1), "       expected: %s", btrc_Vector_string_join(files, ", "));
        printf("%s\n", __fstr_421_buf);
    }
}

void reportFile(char* label, char* path) {
    char* mark = (FileSystem_exists(path) ? "OK" : "MISSING");
    int __fstr_424_len = snprintf(NULL, 0, "  %s %s: %s", mark, label, path);
    char* __fstr_424_buf = __btrc_str_track(((char*)malloc((__fstr_424_len + 1))));
    snprintf(__fstr_424_buf, (__fstr_424_len + 1), "  %s %s: %s", mark, label, path);
    printf("%s\n", __fstr_424_buf);
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
        int __fstr_427_len = snprintf(NULL, 0, "  MISSING %s: %s", label, path);
        char* __fstr_427_buf = __btrc_str_track(((char*)malloc((__fstr_427_len + 1))));
        snprintf(__fstr_427_buf, (__fstr_427_len + 1), "  MISSING %s: %s", label, path);
        printf("%s\n", __fstr_427_buf);
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
    if (!__btrc_strContains(text, "key_press Q, Quit")) {
        btrc_Vector_string_push(missing, "quit");
    }
    if (__btrc_strContains(text, "\"mode\" \"gyro") || __btrc_strContains(text, "gyro_")) {
        btrc_Vector_string_push(missing, "no_gyro_policy");
    }
    int balance = braceBalance(text);
    if (balance != 0) {
        int __fstr_429_len = snprintf(NULL, 0, "brace_balance=%d", balance);
        char* __fstr_429_buf = __btrc_str_track(((char*)malloc((__fstr_429_len + 1))));
        snprintf(__fstr_429_buf, (__fstr_429_len + 1), "brace_balance=%d", balance);
        btrc_Vector_string_push(missing, __fstr_429_buf);
    }
    if (missing->len == 0) {
        int __fstr_432_len = snprintf(NULL, 0, "  OK %s: controller_neptune, trackpads, save/load/quit", label);
        char* __fstr_432_buf = __btrc_str_track(((char*)malloc((__fstr_432_len + 1))));
        snprintf(__fstr_432_buf, (__fstr_432_len + 1), "  OK %s: controller_neptune, trackpads, save/load/quit", label);
        printf("%s\n", __fstr_432_buf);
    } else {
        int __fstr_435_len = snprintf(NULL, 0, "  INVALID %s: %s", label, btrc_Vector_string_join(missing, ", "));
        char* __fstr_435_buf = __btrc_str_track(((char*)malloc((__fstr_435_len + 1))));
        snprintf(__fstr_435_buf, (__fstr_435_len + 1), "  INVALID %s: %s", label, btrc_Vector_string_join(missing, ", "));
        printf("%s\n", __fstr_435_buf);
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
        int __fstr_436_len = snprintf(NULL, 0, "%d NoCrypto partition(s)", result->partitions);
        char* __fstr_436_buf = __btrc_str_track(((char*)malloc((__fstr_436_len + 1))));
        snprintf(__fstr_436_buf, (__fstr_436_len + 1), "%d NoCrypto partition(s)", result->partitions);
        (result->note = __fstr_436_buf);
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

void reportN3dsRomPreflight(char* project) {
    char* n3dsDir = joinPath(romsRoot(project), "n3ds");
    if (!FileSystem_isDir(n3dsDir)) {
        int __fstr_439_len = snprintf(NULL, 0, "  MISSING n3ds rom dir: %s", n3dsDir);
        char* __fstr_439_buf = __btrc_str_track(((char*)malloc((__fstr_439_len + 1))));
        snprintf(__fstr_439_buf, (__fstr_439_len + 1), "  MISSING n3ds rom dir: %s", n3dsDir);
        printf("%s\n", __fstr_439_buf);
        return;
    }
    btrc_Vector_string* entries = FileSystem_listDir(n3dsDir);
    int checked = 0;
    int archives = 0;
    int __n_441 = btrc_Vector_string_iterLen(entries);
    for (int __i_440 = 0; (__i_440 < __n_441); (__i_440++)) {
        char* name = btrc_Vector_string_iterGet(entries, __i_440);
        char* path = joinPath(n3dsDir, name);
        if (FileSystem_isFile(path) && n3dsRomName(name)) {
            (checked++);
            N3dsRomCheck* result = checkN3dsRom(path);
            int __fstr_444_len = snprintf(NULL, 0, "  %s n3ds/%s: %s", result->status, name, result->note);
            char* __fstr_444_buf = __btrc_str_track(((char*)malloc((__fstr_444_len + 1))));
            snprintf(__fstr_444_buf, (__fstr_444_len + 1), "  %s n3ds/%s: %s", result->status, name, result->note);
            printf("%s\n", __fstr_444_buf);
        } else if (FileSystem_isFile(path) && n3dsArchiveName(name)) {
            (archives++);
        }
    }
    if (checked == 0) {
        printf("%s\n", "  OK n3ds: no top-level .3ds/.cci files to preflight");
    }
    if (archives > 0) {
        int __fstr_447_len = snprintf(NULL, 0, "  WARN n3ds archives unchecked: %d zip/7z file(s)", archives);
        char* __fstr_447_buf = __btrc_str_track(((char*)malloc((__fstr_447_len + 1))));
        snprintf(__fstr_447_buf, (__fstr_447_len + 1), "  WARN n3ds archives unchecked: %d zip/7z file(s)", archives);
        printf("%s\n", __fstr_447_buf);
    }
}

bool commandExists(char* name) {
    UnixShell* shell = UnixShell_new();
    int __fstr_448_len = snprintf(NULL, 0, "command -v %s >/dev/null 2>&1", ShellWords_quote(name));
    char* __fstr_448_buf = __btrc_str_track(((char*)malloc((__fstr_448_len + 1))));
    snprintf(__fstr_448_buf, (__fstr_448_len + 1), "command -v %s >/dev/null 2>&1", ShellWords_quote(name));
    ExecResult* result = UnixShell_runUnchecked(shell, __fstr_448_buf);
    bool __btrc_ret_450 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_450;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

char* verificationRoot(char* project) {
    return joinPath(project, "verification");
}

char* screenshotConfigPath(char* project) {
    return joinPath(verificationRoot(project), "screenshots.json");
}

char* screenshotDefaultConfigText(void) {
    btrc_Vector_string* __list_451 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_451, jsonField("schema_version", "1"));
    btrc_Vector_string_push(__list_451, jsonBoolField("enabled", false));
    btrc_Vector_string_push(__list_451, jsonStrField("tool", "auto"));
    btrc_Vector_string_push(__list_451, jsonStrField("command", ""));
    btrc_Vector_string_push(__list_451, jsonField("delay_seconds", "2"));
    btrc_Vector_string_push(__list_451, jsonBoolField("capture_before_launch", true));
    btrc_Vector_string_push(__list_451, jsonBoolField("capture_after_spawn", true));
    btrc_Vector_string_push(__list_451, jsonBoolField("capture_after_exit", true));
    btrc_Vector_string_push(__list_451, jsonStrField("output_pattern", "${paths.project_screenshots}/verification/${emulator}/${hook}.png"));
    return __btrc_str_track(__btrc_strcat(jsonObject(__list_451), "\n"));
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
    char* value = Environment_get("SCHEMULATOR_SCREENSHOT_HOOKS", "");
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
    int __fstr_452_len = snprintf(NULL, 0, "%d", delay);
    char* __fstr_452_buf = __btrc_str_track(((char*)malloc((__fstr_452_len + 1))));
    snprintf(__fstr_452_buf, (__fstr_452_len + 1), "%d", delay);
    return Environment_get("SCHEMULATOR_SCREENSHOT_DELAY_SECONDS", __fstr_452_buf);
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
    char* configured = Environment_get("SCHEMULATOR_SCREENSHOT_OUTPUT", screenshotString(project, "output_pattern", "${paths.project_screenshots}/verification/${emulator}/${hook}.png"));
    return screenshotExpandPathTemplate(project, configured, emulator, hook);
}

char* screenshotAutoTool(void) {
    btrc_Vector_string* __list_453 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_453, "grim");
    btrc_Vector_string_push(__list_453, "spectacle");
    btrc_Vector_string_push(__list_453, "gnome-screenshot");
    btrc_Vector_string_push(__list_453, "import");
    btrc_Vector_string* candidates = __list_453;
    int __n_455 = btrc_Vector_string_iterLen(candidates);
    for (int __i_454 = 0; (__i_454 < __n_455); (__i_454++)) {
        char* candidate = btrc_Vector_string_iterGet(candidates, __i_454);
        if (commandExists(candidate)) {
            return candidate;
        }
    }
    return "";
}

char* screenshotConfiguredTool(char* project) {
    char* envTool = Environment_get("SCHEMULATOR_SCREENSHOT_TOOL", "");
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
    char* template = Environment_get("SCHEMULATOR_SCREENSHOT_CMD", screenshotString(project, "command", ""));
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
        btrc_Vector_string* __list_456 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_456, "-b");
        btrc_Vector_string_push(__list_456, "-n");
        btrc_Vector_string_push(__list_456, "-o");
        btrc_Vector_string_push(__list_456, output);
        return shellAppendAll(command, __list_456);
    }
    if (strcmp(base, "gnome-screenshot") == 0) {
        btrc_Vector_string* __list_457 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_457, "-f");
        btrc_Vector_string_push(__list_457, output);
        return shellAppendAll(command, __list_457);
    }
    if (strcmp(base, "import") == 0) {
        btrc_Vector_string* __list_458 = btrc_Vector_string_new();
        btrc_Vector_string_push(__list_458, "-window");
        btrc_Vector_string_push(__list_458, "root");
        btrc_Vector_string_push(__list_458, output);
        return shellAppendAll(command, __list_458);
    }
    return shellAppend(command, output);
}

bool screenshotCaptureTo(char* project, char* emulator, char* hook, char* output) {
    ensureDir(PathTools_dirname(output));
    char* command = screenshotCaptureCommand(project, emulator, hook, output);
    if (((int)strlen(command)) == 0) {
        printf("%s\n", "MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SCHEMULATOR_SCREENSHOT_CMD");
        return false;
    }
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runRaw(shell, command, false, false, "");
    if (ExecResult_ok(result) && FileSystem_isFile(output)) {
        int __fstr_461_len = snprintf(NULL, 0, "OK screenshot %s:%s: %s", emulator, hook, output);
        char* __fstr_461_buf = __btrc_str_track(((char*)malloc((__fstr_461_len + 1))));
        snprintf(__fstr_461_buf, (__fstr_461_len + 1), "OK screenshot %s:%s: %s", emulator, hook, output);
        printf("%s\n", __fstr_461_buf);
        bool __btrc_ret_462 = true;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_462;
    }
    int __fstr_465_len = snprintf(NULL, 0, "MISSING screenshot %s:%s: %s", emulator, hook, output);
    char* __fstr_465_buf = __btrc_str_track(((char*)malloc((__fstr_465_len + 1))));
    snprintf(__fstr_465_buf, (__fstr_465_len + 1), "MISSING screenshot %s:%s: %s", emulator, hook, output);
    printf("%s\n", __fstr_465_buf);
    bool __btrc_ret_466 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_466;
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
        printf("%s\n", "MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SCHEMULATOR_SCREENSHOT_CMD");
        return;
    }
    char* delay = screenshotDelaySeconds(project);
    UnixShell* shell = UnixShell_new();
    UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("(sleep ", ShellWords_quote(delay))), "; ")), command)), ") >/dev/null 2>&1 &")), false, false, "");
    int __fstr_469_len = snprintf(NULL, 0, "OK screenshot scheduled %s:%s: %s", emulator, hook, output);
    char* __fstr_469_buf = __btrc_str_track(((char*)malloc((__fstr_469_len + 1))));
    snprintf(__fstr_469_buf, (__fstr_469_len + 1), "OK screenshot scheduled %s:%s: %s", emulator, hook, output);
    printf("%s\n", __fstr_469_buf);
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
    printf("%s\n", (screenshotHooksEnabled(project) ? "  OK launcher_hooks: enabled" : "  optional launcher_hooks: disabled (set SCHEMULATOR_SCREENSHOT_HOOKS=1)"));
    printf("%s\n", "  OK hooks: before_launch, after_spawn, after_exit, manual_visual_checkpoint");
    char* tool = screenshotConfiguredTool(project);
    if (((int)strlen(tool)) > 0) {
        int __fstr_472_len = snprintf(NULL, 0, "  OK screenshot_tool: %s", tool);
        char* __fstr_472_buf = __btrc_str_track(((char*)malloc((__fstr_472_len + 1))));
        snprintf(__fstr_472_buf, (__fstr_472_len + 1), "  OK screenshot_tool: %s", tool);
        printf("%s\n", __fstr_472_buf);
    } else {
        printf("%s\n", "  MISSING screenshot_tool: install grim/spectacle/gnome-screenshot/ImageMagick import or set SCHEMULATOR_SCREENSHOT_CMD");
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
        return joinPath(schemulatorStateRoot(project), "appimage-state");
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
    btrc_Vector_string* __list_473 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_473, "saves");
    btrc_Vector_string_push(__list_473, "states");
    btrc_Vector_string_push(__list_473, "emulator_state");
    btrc_Vector_string_push(__list_473, "screenshots");
    btrc_Vector_string_push(__list_473, "gamelists");
    btrc_Vector_string_push(__list_473, "roms");
    btrc_Vector_string_push(__list_473, "bios");
    return __list_473;
}

char* syncFolderLabel(char* id) {
    if (strcmp(id, "saves") == 0) {
        return "Schemulator Saves";
    }
    if (strcmp(id, "states") == 0) {
        return "Schemulator States";
    }
    if (strcmp(id, "emulator_state") == 0) {
        return "Schemulator Emulator State";
    }
    if (strcmp(id, "screenshots") == 0) {
        return "Schemulator Screenshots";
    }
    if (strcmp(id, "gamelists") == 0) {
        return "Schemulator Gamelists";
    }
    if (strcmp(id, "roms") == 0) {
        return "Schemulator ROMs";
    }
    if (strcmp(id, "bios") == 0) {
        return "Schemulator BIOS";
    }
    return "Schemulator";
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
    return Environment_get("SCHEMULATOR_BIN", "schemulator");
}

char* syncServiceText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Schemulator Syncthing\n")), "After=network-online.target\n\n")), "[Service]\n")), "Type=simple\n")), "ExecStart=syncthing serve -H ")), ShellWords_quote(syncthingHome(project)))), " --no-browser --no-restart\n")), "Restart=on-failure\n")), "RestartSec=5\n\n")), "[Install]\n")), "WantedBy=default.target\n"));
}

char* syncForceScriptText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env bash\n", "set -euo pipefail\n")), "exec ")), ShellWords_quote(serviceExecutable()))), " sync force all --project ")), ShellWords_quote(project))), "\n"));
}

char* syncForceServiceText(char* project) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Schemulator force Syncthing scan\n\n")), "[Service]\n")), "Type=oneshot\n")), "ExecStart=")), ShellWords_quote(joinPath(syncScriptsDir(project), "sync-force.sh")))), "\n"));
}

char* syncTimerText(void) {
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Unit]\n", "Description=Schemulator scheduled Syncthing scan\n\n")), "[Timer]\n")), "OnBootSec=5min\n")), "OnUnitActiveSec=15min\n")), "Persistent=true\n\n")), "[Install]\n")), "WantedBy=timers.target\n"));
}

void writeSyncSystemdUnits(char* project) {
    ensureDir(systemdUserDir());
    ensureDir(syncScriptsDir(project));
    char* script = joinPath(syncScriptsDir(project), "sync-force.sh");
    FileSystem_writeText(script, syncForceScriptText(project));
    FileSystem_chmod(script, 493);
    FileSystem_writeText(joinPath(systemdUserDir(), "schemulator-syncthing.service"), syncServiceText(project));
    FileSystem_writeText(joinPath(systemdUserDir(), "schemulator-sync-force.service"), syncForceServiceText(project));
    FileSystem_writeText(joinPath(systemdUserDir(), "schemulator-sync-force.timer"), syncTimerText());
}

char* deckDesktopText(char* project) {
    char* exe = serviceExecutable();
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("[Desktop Entry]\n", "Type=Application\n")), "Name=Schemulator\n")), "Comment=Steam Deck emulation frontend\n")), "Exec=")), exe)), " deck launch --project ")), ShellWords_quote(project))), "\n")), "Terminal=false\n")), "Categories=Game;Emulator;\n")), "Actions=ForceSync;SyncStatus;OpenSyncthing;OpenSyncTray;\n\n")), "[Desktop Action ForceSync]\n")), "Name=Force Sync\n")), "Exec=")), exe)), " sync force all --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action SyncStatus]\n")), "Name=Sync Status\n")), "Exec=")), exe)), " sync status --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action OpenSyncthing]\n")), "Name=Open Syncthing\n")), "Exec=")), exe)), " sync open --project ")), ShellWords_quote(project))), "\n\n")), "[Desktop Action OpenSyncTray]\n")), "Name=Open Sync Tray\n")), "Exec=")), exe)), " sync tray --project ")), ShellWords_quote(project))), "\n"));
}

void writeDeckDesktopEntry(char* project) {
    ensureDir(applicationsDir());
    FileSystem_writeText(joinPath(applicationsDir(), "schemulator.desktop"), deckDesktopText(project));
}

char* schemulatorStateRoot(char* project) {
    return joinPath(project, ".schemulator");
}

char* lifecycleStatePath(char* project) {
    return joinPath(schemulatorStateRoot(project), "lifecycle.json");
}

char* lifecycleBackupsRoot(char* project) {
    return joinPath(schemulatorStateRoot(project), "backups");
}

char* upgradeBackupPath(char* project) {
    return joinPath(lifecycleBackupsRoot(project), "pre-upgrade-schemulator.json");
}

void removePath(char* path) {
    if (FileSystem_exists(path)) {
        FileSystem_removeRecursive(path);
    }
}

void writeLifecycleState(char* project, char* action) {
    ensureDir(schemulatorStateRoot(project));
    btrc_Vector_string* __list_475 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_475, jsonStrField("schema_version", "1"));
    btrc_Vector_string_push(__list_475, jsonStrField("action", action));
    btrc_Vector_string_push(__list_475, jsonStrField("project", project));
    btrc_Vector_string_push(__list_475, jsonStrField("roms_dir", configuredRomsRoot(project)));
    btrc_Vector_string_push(__list_475, jsonStrField("source", "schemulator.btrc"));
    FileSystem_writeText(lifecycleStatePath(project), __btrc_str_track(__btrc_strcat(jsonObject(__list_475), "\n")));
}

void lifecycleReconfigure(char* project, char* romsDir) {
    writeSyncDefaults(project, romsDir);
    ensureRomDirsAt(configuredRomsRoot(project));
    if (!(strcmp(configuredRomsRoot(project), romsRoot(project)) == 0)) {
        ensureRomDirsAt(romsRoot(project));
    }
    seedKeymapDefaults(project);
    seedEmulatorDefaults(project);
    writeSteamInputTemplates(project);
    writeScreenshotDefaults(project);
    writeGeneratedManifest(joinPath(project, "schemulator.json"));
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
    removePath(joinPath(systemdUserDir(), "schemulator-syncthing.service"));
    removePath(joinPath(systemdUserDir(), "schemulator-sync-force.service"));
    removePath(joinPath(systemdUserDir(), "schemulator-sync-force.timer"));
    removePath(joinPath(applicationsDir(), "schemulator.desktop"));
    removePath(joinPath(syncScriptsDir(project), "sync-force.sh"));
    removePath(joinPath(schemulatorStateRoot(project), "appimage-state"));
    if (purgeGenerated) {
        removePath(joinPath(project, "ES-DE/custom_systems"));
        removePath(joinPath(project, "ES-DE/es_settings.xml"));
        removePath(joinPath(project, "schemulator.json"));
    }
    if (purgeState) {
        removePath(schemulatorStateRoot(project));
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
    int __n_477 = btrc_Vector_string_iterLen(lines);
    for (int __i_476 = 0; (__i_476 < __n_477); (__i_476++)) {
        char* line = btrc_Vector_string_iterGet(lines, __i_476);
        if (__btrc_startsWith(line, prefix)) {
            btrc_Vector_string_push(out, __btrc_str_track(__btrc_strcat(prefix, command)));
            (changed = true);
        } else if (((int)strlen(line)) > 0) {
            btrc_Vector_string_push(out, line);
        }
    }
    if (!changed) {
        int __fstr_480_len = snprintf(NULL, 0, "error 0:0 unknown keymap action '%s'", actionId);
        char* __fstr_480_buf = __btrc_str_track(((char*)malloc((__fstr_480_len + 1))));
        snprintf(__fstr_480_buf, (__fstr_480_len + 1), "error 0:0 unknown keymap action '%s'", actionId);
        printf("%s\n", __fstr_480_buf);
        return false;
    }
    char* next = __btrc_str_track(__btrc_strcat(btrc_Vector_string_join(out, "\n"), "\n"));
    KeymapErrors* errors = KeymapErrors_new();
    compileKeymap(next, errors);
    if (KeymapErrors_count(errors) > 0) {
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_483_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_483_buf = __btrc_str_track(((char*)malloc((__fstr_483_len + 1))));
            snprintf(__fstr_483_buf, (__fstr_483_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_483_buf);
        }
        bool __btrc_ret_484 = false;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_484;
    }
    FileSystem_writeText(path, next);
    lifecycleReconfigure(project, "");
    writeLifecycleState(project, "change");
    int __fstr_487_len = snprintf(NULL, 0, "OK lifecycle change: %s=%s", actionId, command);
    char* __fstr_487_buf = __btrc_str_track(((char*)malloc((__fstr_487_len + 1))));
    snprintf(__fstr_487_buf, (__fstr_487_len + 1), "OK lifecycle change: %s=%s", actionId, command);
    printf("%s\n", __fstr_487_buf);
    bool __btrc_ret_488 = true;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_488;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
}

void lifecycleUpgrade(char* project) {
    ensureDir(lifecycleBackupsRoot(project));
    if (FileSystem_exists(joinPath(project, "schemulator.json"))) {
        FileSystem_writeText(upgradeBackupPath(project), FileSystem_readText(joinPath(project, "schemulator.json")));
    }
    lifecycleReconfigure(project, "");
    writeLifecycleState(project, "upgrade");
    int __fstr_491_len = snprintf(NULL, 0, "OK lifecycle upgrade: backup %s", upgradeBackupPath(project));
    char* __fstr_491_buf = __btrc_str_track(((char*)malloc((__fstr_491_len + 1))));
    snprintf(__fstr_491_buf, (__fstr_491_len + 1), "OK lifecycle upgrade: backup %s", upgradeBackupPath(project));
    printf("%s\n", __fstr_491_buf);
}

void lifecycleStatus(char* project) {
    printf("%s\n", "Schemulator lifecycle");
    reportFile("state", lifecycleStatePath(project));
    reportFile("manifest", joinPath(project, "schemulator.json"));
    reportFile("sync_config", syncConfigPath(project));
    reportFile("desktop_entry", joinPath(applicationsDir(), "schemulator.desktop"));
    reportFile("systemd_service", joinPath(systemdUserDir(), "schemulator-syncthing.service"));
    int __fstr_494_len = snprintf(NULL, 0, "  roms_dir: %s", configuredRomsRoot(project));
    char* __fstr_494_buf = __btrc_str_track(((char*)malloc((__fstr_494_len + 1))));
    snprintf(__fstr_494_buf, (__fstr_494_len + 1), "  roms_dir: %s", configuredRomsRoot(project));
    printf("%s\n", __fstr_494_buf);
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
        (url = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(url, "?folder=schemulator-")), target)));
    }
    return url;
}

void syncGenerateIfNeeded(char* project) {
    if (FileSystem_exists(syncConfigXmlPath(project))) {
        return;
    }
    UnixShell* shell = UnixShell_new();
    UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("syncthing generate -H ", ShellWords_quote(syncthingHome(project)))), " --no-port-probing >/dev/null 2>&1")));
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncSystemctl(char* verb, char* unit) {
    UnixShell* shell = UnixShell_new();
    ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("systemctl --user ", verb)), " ")), unit)), " >/dev/null 2>&1")));
    bool __btrc_ret_495 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_495;
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
            bool __btrc_ret_496 = true;
            if (shell != NULL) {
                if ((--shell->__rc) <= 0) {
                    UnixShell_destroy(shell);
                }
            }
            return __btrc_ret_496;
        }
        UnixShell_runUnchecked(shell, "sleep 1");
    }
    bool __btrc_ret_497 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_497;
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
    UnixShell* shell = UnixShell_new();
    char* command = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("syncthing cli -H ", ShellWords_quote(syncthingHome(project)))), " --gui-address ")), ShellWords_quote(syncGuiAddress(project)))), " config folders add")), " --id ")), ShellWords_quote(__btrc_str_track(__btrc_strcat("schemulator-", id))))), " --label ")), ShellWords_quote(syncFolderLabel(id)))), " --path ")), ShellWords_quote(syncFolderPath(project, id)))), " --type sendreceive")), " --rescan-intervals ")), Strings_fromInt(syncFolderRescan(project, id))));
    if (syncFolderWatch(project, id)) {
        (command = __btrc_str_track(__btrc_strcat(command, " --fswatcher-enabled")));
    }
    ExecResult* result = UnixShell_runUnchecked(shell, __btrc_str_track(__btrc_strcat(command, " >/dev/null 2>&1")));
    bool __btrc_ret_498 = ExecResult_ok(result);
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_498;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool syncConfigureFolders(char* project) {
    bool ok = true;
    int __n_500 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_499 = 0; (__i_499 < __n_500); (__i_499++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_499);
        if (!syncAddFolder(project, id)) {
            (ok = false);
        }
    }
    return ok;
}

bool syncSetup(char* project) {
    writeSyncDefaults(project, "");
    writeSyncSystemdUnits(project);
    if (!commandExists("syncthing")) {
        printf("%s\n", "MISSING syncthing: install package or use bundled Schemulator");
        return false;
    }
    syncGenerateIfNeeded(project);
    bool ok = syncSystemctl("daemon-reload", "");
    if (syncBool(project, "start_at_boot", true)) {
        if (!syncSystemctl("enable", "schemulator-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("enable", "schemulator-sync-force.timer")) {
            (ok = false);
        }
    }
    if (syncBool(project, "enabled", true)) {
        if (!syncSystemctl("start", "schemulator-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("start", "schemulator-sync-force.timer")) {
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
        int __fstr_503_len = snprintf(NULL, 0, "OK sync setup: %s", syncConfigPath(project));
        char* __fstr_503_buf = __btrc_str_track(((char*)malloc((__fstr_503_len + 1))));
        snprintf(__fstr_503_buf, (__fstr_503_len + 1), "OK sync setup: %s", syncConfigPath(project));
        printf("%s\n", __fstr_503_buf);
    } else {
        int __fstr_506_len = snprintf(NULL, 0, "MISSING sync setup incomplete: %s", syncConfigPath(project));
        char* __fstr_506_buf = __btrc_str_track(((char*)malloc((__fstr_506_len + 1))));
        snprintf(__fstr_506_buf, (__fstr_506_len + 1), "MISSING sync setup incomplete: %s", syncConfigPath(project));
        printf("%s\n", __fstr_506_buf);
    }
    return ok;
}

bool syncStart(char* project) {
    bool ok = syncSetup(project);
    if (!syncSystemctl("start", "schemulator-syncthing.service")) {
        (ok = false);
    }
    printf("%s\n", (ok ? "OK sync start" : "MISSING sync start failed"));
    return ok;
}

bool syncStop(char* project) {
    bool ok = true;
    if (!syncSystemctl("stop", "schemulator-sync-force.timer")) {
        (ok = false);
    }
    if (!syncSystemctl("stop", "schemulator-syncthing.service")) {
        (ok = false);
    }
    printf("%s\n", (ok ? "OK sync stop" : "MISSING sync stop incomplete"));
    return ok;
}

bool syncAutostart(char* project, bool enabled) {
    writeSyncSystemdUnits(project);
    bool ok = syncSystemctl("daemon-reload", "");
    if (enabled) {
        if (!syncSystemctl("enable", "schemulator-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("enable", "schemulator-sync-force.timer")) {
            (ok = false);
        }
        printf("%s\n", (ok ? "OK sync autostart enabled" : "MISSING sync autostart enable incomplete"));
    } else {
        if (!syncSystemctl("disable", "schemulator-syncthing.service")) {
            (ok = false);
        }
        if (!syncSystemctl("disable", "schemulator-sync-force.timer")) {
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
        int __fstr_509_len = snprintf(NULL, 0, "OK sync force: %s", target);
        char* __fstr_509_buf = __btrc_str_track(((char*)malloc((__fstr_509_len + 1))));
        snprintf(__fstr_509_buf, (__fstr_509_len + 1), "OK sync force: %s", target);
        printf("%s\n", __fstr_509_buf);
        bool __btrc_ret_510 = true;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_510;
    } else {
        int __fstr_513_len = snprintf(NULL, 0, "MISSING sync force failed: %s", target);
        char* __fstr_513_buf = __btrc_str_track(((char*)malloc((__fstr_513_len + 1))));
        snprintf(__fstr_513_buf, (__fstr_513_len + 1), "MISSING sync force failed: %s", target);
        printf("%s\n", __fstr_513_buf);
    }
    bool __btrc_ret_514 = false;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_514;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

void syncStatus(char* project) {
    printf("%s\n", "Schemulator sync");
    reportFile("sync_config", syncConfigPath(project));
    reportFile("syncthing_config", syncConfigXmlPath(project));
    reportFile("systemd_service", joinPath(systemdUserDir(), "schemulator-syncthing.service"));
    reportFile("systemd_timer", joinPath(systemdUserDir(), "schemulator-sync-force.timer"));
    printf("%s\n", (commandExists("syncthing") ? "  OK syncthing: executable found" : "  MISSING syncthing: executable not found"));
    printf("%s\n", (commandExists("syncthingtray") ? "  OK syncthingtray: executable found" : "  optional syncthingtray: executable not found"));
    printf("%s\n", (commandExists("curl") ? "  OK curl: executable found" : "  MISSING curl: executable not found"));
    int __n_516 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_515 = 0; (__i_515 < __n_516); (__i_515++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_515);
        char* mark = (syncFolderEnabled(project, id) ? "OK" : "optional");
        int __fstr_519_len = snprintf(NULL, 0, "  %s %s: %s", mark, id, syncFolderPath(project, id));
        char* __fstr_519_buf = __btrc_str_track(((char*)malloc((__fstr_519_len + 1))));
        snprintf(__fstr_519_buf, (__fstr_519_len + 1), "  %s %s: %s", mark, id, syncFolderPath(project, id));
        printf("%s\n", __fstr_519_buf);
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
    printf("%s\n", (commandExists("syncthing") ? "  OK syncthing: executable found" : "  MISSING syncthing: executable not found"));
    printf("%s\n", (commandExists("curl") ? "  OK curl: executable found" : "  MISSING curl: executable not found"));
    printf("%s\n", (syncBool(project, "start_at_boot", true) ? "  OK start_at_boot: enabled" : "  optional start_at_boot: disabled"));
    printf("%s\n", (syncBool(project, "tray", true) ? "  OK tray: enabled" : "  optional tray: disabled"));
    int __n_521 = btrc_Vector_string_iterLen(syncFolderIds());
    for (int __i_520 = 0; (__i_520 < __n_521); (__i_520++)) {
        char* id = btrc_Vector_string_iterGet(syncFolderIds(), __i_520);
        char* mark = (syncFolderEnabled(project, id) ? "OK" : "optional");
        char* watch = (syncFolderWatch(project, id) ? "watch" : "scan");
        int interval = syncFolderRescan(project, id);
        int __fstr_524_len = snprintf(NULL, 0, "  %s %s: %s, %ds, %s", mark, id, watch, interval, syncFolderPath(project, id));
        char* __fstr_524_buf = __btrc_str_track(((char*)malloc((__fstr_524_len + 1))));
        snprintf(__fstr_524_buf, (__fstr_524_len + 1), "  %s %s: %s, %ds, %s", mark, id, watch, interval, syncFolderPath(project, id));
        printf("%s\n", __fstr_524_buf);
    }
}

void doctorSteamDeck(char* project) {
    printf("%s\n", "Schemulator doctor (BTRC)");
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
    btrc_Vector_string* __list_526 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_526, "scph5500.bin");
    btrc_Vector_string_push(__list_526, "scph5501.bin");
    btrc_Vector_string_push(__list_526, "scph5502.bin");
    reportBios("psx", biosRoot(project), __list_526, true, false);
    btrc_Vector_string* __list_528 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_528, "ps2-0230a-20080220.bin");
    btrc_Vector_string_push(__list_528, "ps2-0230e-20080220.bin");
    btrc_Vector_string_push(__list_528, "ps2-0230j-20080220.bin");
    reportBios("ps2", joinPath(biosRoot(project), "ps2"), __list_528, true, true);
    btrc_Vector_string* __list_530 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_530, "prod.keys");
    btrc_Vector_string_push(__list_530, "title.keys");
    reportBios("switch_keys", joinPath(biosRoot(project), "switch"), __list_530, true, false);
    btrc_Vector_string* __list_532 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_532, "keys.txt");
    reportBios("wiiu_keys", joinPath(project, "Cemu/data"), __list_532, true, false);
    btrc_Vector_string* __list_534 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_534, "dc_boot.bin");
    btrc_Vector_string_push(__list_534, "dc_flash.bin");
    reportBios("dreamcast", joinPath(biosRoot(project), "dc"), __list_534, false, false);
    printf("%s\n", "");
    printf("%s\n", "Controller profiles");
    bool controllersOk = true;
    int __n_536 = btrc_Vector_string_iterLen(controllerProfileFiles());
    for (int __i_535 = 0; (__i_535 < __n_536); (__i_535++)) {
        char* profile = btrc_Vector_string_iterGet(controllerProfileFiles(), __i_535);
        if (!FileSystem_exists(joinPath(project, profile))) {
            (controllersOk = false);
            int __fstr_539_len = snprintf(NULL, 0, "  MISSING %s", profile);
            char* __fstr_539_buf = __btrc_str_track(((char*)malloc((__fstr_539_len + 1))));
            snprintf(__fstr_539_buf, (__fstr_539_len + 1), "  MISSING %s", profile);
            printf("%s\n", __fstr_539_buf);
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
        int __fstr_542_len = snprintf(NULL, 0, "  OK steam_deck: %s", keymapPath);
        char* __fstr_542_buf = __btrc_str_track(((char*)malloc((__fstr_542_len + 1))));
        snprintf(__fstr_542_buf, (__fstr_542_len + 1), "  OK steam_deck: %s", keymapPath);
        printf("%s\n", __fstr_542_buf);
    } else {
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_545_len = snprintf(NULL, 0, "  %s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_545_buf = __btrc_str_track(((char*)malloc((__fstr_545_len + 1))));
            snprintf(__fstr_545_buf, (__fstr_545_len + 1), "  %s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_545_buf);
        }
    }
    printf("%s\n", "");
    printf("%s\n", "Steam Input templates");
    reportSteamInputTemplate("neptune_simple", joinPath(project, "steam-input/neptune-simple.vdf"));
    reportSteamInputTemplate("neptune_full", joinPath(project, "steam-input/neptune-full.vdf"));
    printf("%s\n", "");
    printf("%s\n", "ROM preflight");
    reportN3dsRomPreflight(project);
    doctorScreenshotHooks(project);
    doctorSync(project);
    printf("%s\n", "");
    printf("%s\n", "linux launchers");
    int __n_547 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_546 = 0; (__i_546 < __n_547); (__i_546++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_546);
        reportFile(emulator, joinPath(joinPath(project, "linux/bin"), schemLauncherName(emulator)));
    }
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
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
    char* tmp = e2eTempDir("schemulator-sandbox-smoke");
    char* project = joinPath(tmp, "project");
    char* scratchRoot = joinPath(tmp, "scratch");
    ensureDir(project);
    ensureDir(scratchRoot);
    e2eSeedFile(joinPath(project, "Azahar/data/qt-config.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "azahar")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "azahar/.local/share/azahar-emu/qt-config.ini"), joinPath(project, "Azahar/data/qt-config.ini"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "Cemu/config/settings.xml"));
    e2eSeedFile(joinPath(project, "Cemu/data/keys.txt"));
    if (!e2ePrepareSandbox(project, scratchRoot, "cemu")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "cemu/.config/Cemu/settings.xml"), joinPath(project, "Cemu/config/settings.xml"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "cemu/.local/share/Cemu/keys.txt"), joinPath(project, "Cemu/data/keys.txt"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "Dolphin/config/Dolphin.ini"));
    e2eSeedFile(joinPath(project, "Dolphin/data/Wii/title.dat"));
    if (!e2ePrepareSandbox(project, scratchRoot, "dolphin")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "dolphin/.config/dolphin-emu/Dolphin.ini"), joinPath(project, "Dolphin/config/Dolphin.ini"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "dolphin/.local/share/dolphin-emu/Wii"), joinPath(project, "Dolphin/data/Wii"))) {
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
    e2eSeedFile(joinPath(project, "Lime3DS/data/qt-config.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "lime3ds")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "lime3ds/.config/lime3ds-emu/qt-config.ini"), joinPath(project, "Lime3DS/data/qt-config.ini"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "PCSX2/config/PCSX2.ini"));
    if (!e2ePrepareSandbox(project, scratchRoot, "pcsx2")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "pcsx2/.config/PCSX2/PCSX2.ini"), joinPath(project, "PCSX2/config/PCSX2.ini"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "RetroArch/config/input.cfg"));
    e2eSeedFile(joinPath(project, "RetroArch/retroarch.cfg"));
    if (!e2ePrepareSandbox(project, scratchRoot, "retroarch")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "retroarch/.config/retroarch/input.cfg"), joinPath(project, "RetroArch/config/input.cfg"))) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "retroarch/.config/retroarch/retroarch.cfg"), joinPath(project, "RetroArch/retroarch.cfg"))) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "Ryujinx/config/Config.json"));
    if (!e2ePrepareSandbox(project, scratchRoot, "ryujinx")) {
        return 1;
    }
    if (!e2eAssertLink(joinPath(scratchRoot, "ryujinx/.config/Ryujinx/Config.json"), joinPath(project, "Ryujinx/config/Config.json"))) {
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

ExecResult* e2eRunLifecycle(char* exe, char* home, char* mode, char* project, char* romsDir) {
    Command* command = Command_check(Command_capture(Command_flag(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), mode), "--project", project), true), false);
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
    Command* command = Command_check(Command_capture(Command_flag(Command_flag(Command_flag(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), "change"), "--project", project), "--action", actionId), "--command", commandText), true), false);
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
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "PATH", "/usr/bin:/bin"), "lifecycle"), true), false);
    int __n_549 = btrc_Vector_string_iterLen(lifecycleArgs);
    for (int __i_548 = 0; (__i_548 < __n_549); (__i_548++)) {
        char* arg = btrc_Vector_string_iterGet(lifecycleArgs, __i_548);
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
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "PATH", __btrc_str_track(__btrc_strcat(binDir, ":/usr/bin:/bin"))), "SCHEM_CAPTURE", joinPath(PathTools_dirname(binDir), "capture")), "sync"), true), false);
    int __n_551 = btrc_Vector_string_iterLen(syncArgs);
    for (int __i_550 = 0; (__i_550 < __n_551); (__i_550++)) {
        char* arg = btrc_Vector_string_iterGet(syncArgs, __i_550);
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
    Command* command = Command_check(Command_capture(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "PATH", "/usr/bin:/bin"), "steam-input"), true), false);
    int __n_553 = btrc_Vector_string_iterLen(steamInputArgs);
    for (int __i_552 = 0; (__i_552 < __n_553); (__i_552++)) {
        char* arg = btrc_Vector_string_iterGet(steamInputArgs, __i_552);
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
    Command* command = Command_check(Command_capture(Command_arg(Command_arg(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_envVar(Command_new(exe), "SCHEMULATOR_HOME", home), "SCHEMULATOR_BIN", exe), "SCHEMULATOR_PROJECT_DIR", project), "SCHEMULATOR_ROMS_DIR", roms), "SCHEM_FLATPAK_CAPTURE", captureDir), "SCHEMULATOR_BWRAP", bwrapPath), "SCHEMULATOR_SCREENSHOT_HOOKS", "1"), "SCHEMULATOR_SCREENSHOT_DELAY_SECONDS", "0"), "PATH", __btrc_str_track(__btrc_strcat(binDir, ":/usr/bin:/bin"))), "WAYLAND_DISPLAY", "wayland-test"), "launcher"), emulator), true), false);
    int __n_555 = btrc_Vector_string_iterLen(emulatorArgs);
    for (int __i_554 = 0; (__i_554 < __n_555); (__i_554++)) {
        char* arg = btrc_Vector_string_iterGet(emulatorArgs, __i_554);
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
    bool __btrc_ret_556 = e2eOk(ExecResult_ok(result), __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(label, ": missing ")), path)));
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_556;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
}

bool e2eCatalogConsistency(char* project, char* launcherBin) {
    char* systems = FileSystem_readText(joinPath(customSystemsRoot(project), "es_systems.xml"));
    char* rules = FileSystem_readText(joinPath(customSystemsRoot(project), "es_find_rules.xml"));
    int __n_558 = btrc_Vector_string_iterLen(declaredSystemIds());
    for (int __i_557 = 0; (__i_557 < __n_558); (__i_557++)) {
        char* id = btrc_Vector_string_iterGet(declaredSystemIds(), __i_557);
        if (!e2eContains(systems, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("<name>", id)), "</name>")), __btrc_str_track(__btrc_strcat("ES-DE system catalog ", id)))) {
            return false;
        }
    }
    int __n_560 = btrc_Vector_string_iterLen(linuxLauncherNames());
    for (int __i_559 = 0; (__i_559 < __n_560); (__i_559++)) {
        char* emulator = btrc_Vector_string_iterGet(linuxLauncherNames(), __i_559);
        char* launcher = joinPath(launcherBin, schemLauncherName(emulator));
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
    char* tmp = e2eTempDir("schemulator-lifecycle-smoke");
    char* project = joinPath(tmp, "project");
    char* home = joinPath(tmp, "home");
    char* romsOne = joinPath(tmp, "roms-one");
    char* romsTwo = joinPath(tmp, "roms-two");
    char* exe = Environment_get("SCHEMULATOR_BIN", args->program);
    ensureDir(project);
    ensureDir(home);
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "install", project, romsOne), "install")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "install", project, romsOne), "install idempotent")) {
        return 1;
    }
    if (!e2eCatalogConsistency(project, joinPath(project, "linux/bin"))) {
        return 1;
    }
    if (!e2eOk(FileSystem_isDir(joinPath(romsOne, "gba")), "install did not create configured ROM dirs")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(syncConfigPath(project)), romsOne, "sync config after install")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(screenshotConfigPath(project)), "output_pattern", "screenshot config after install")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(home, ".local/share/applications/schemulator.desktop")), "desktop entry missing after install")) {
        return 1;
    }
    if (!e2eOk(FileSystem_exists(joinPath(home, ".config/systemd/user/schemulator-syncthing.service")), "systemd service missing after install")) {
        return 1;
    }
    char* steamTemplates = joinPath(home, "steam-templates");
    btrc_Vector_string* __list_561 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_561, "install");
    btrc_Vector_string_push(__list_561, "--dest");
    btrc_Vector_string_push(__list_561, steamTemplates);
    if (!e2eRunOk(e2eRunSteamInput(exe, home, project, __list_561), "steam-input install")) {
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
    if (!e2eOk(FileSystem_exists(joinPath(romsOne, "gba")), "old ROM root should not be deleted")) {
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
    if (!e2eContains(FileSystem_readText(joinPath(project, "RetroArch/retroarch.cfg")), "input_save_state = \"v\"", "RetroArch keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "Dolphin/config/Profiles/Hotkeys/Steam Deck.ini")), "Save State/Save State Slot 1 = @(Ctrl+V)", "Dolphin keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "PCSX2/config/inputprofiles/Steam Deck.ini")), "SaveStateToSlot = Keyboard/Control & Keyboard/V", "PCSX2 keymap after change")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "steam-input/neptune-full.vdf")), "key_press V, Save State", "Steam Input keymap after change")) {
        return 1;
    }
    char* keymapBeforeFailure = FileSystem_readText(keymapSourcePath(project));
    char* retroarchBeforeFailure = FileSystem_readText(joinPath(project, "RetroArch/retroarch.cfg"));
    btrc_Vector_string* __list_562 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_562, "change");
    btrc_Vector_string_push(__list_562, "--action");
    btrc_Vector_string_push(__list_562, "state.save");
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleArgs(exe, home, project, __list_562))), "lifecycle change without command unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleChange(exe, home, project, "missing.action", "Ctrl+B"))), "unknown keymap action unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleChange(exe, home, project, "state.save", "Ctrl"))), "invalid keymap command unexpectedly succeeded")) {
        return 1;
    }
    btrc_Vector_string* __list_563 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_563, "definitely-not-a-mode");
    if (!e2eOk((!ExecResult_ok(e2eRunLifecycleArgs(exe, home, project, __list_563))), "unknown lifecycle mode unexpectedly succeeded")) {
        return 1;
    }
    if (!e2eOk((strcmp(FileSystem_readText(keymapSourcePath(project)), keymapBeforeFailure) == 0), "failed keymap change mutated source")) {
        return 1;
    }
    if (!e2eOk((strcmp(FileSystem_readText(joinPath(project, "RetroArch/retroarch.cfg")), retroarchBeforeFailure) == 0), "failed keymap change mutated RetroArch config")) {
        return 1;
    }
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "uninstall", project, ""), "uninstall")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(home, ".local/share/applications/schemulator.desktop"))), "desktop entry should be removed by uninstall")) {
        return 1;
    }
    if (!e2eOk((!FileSystem_exists(joinPath(home, ".config/systemd/user/schemulator-syncthing.service"))), "systemd service should be removed by uninstall")) {
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
    if (!e2eOk(FileSystem_exists(joinPath(home, ".local/share/applications/schemulator.desktop")), "desktop entry missing after reinstall")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(syncConfigPath(project)), romsTwo, "sync config after reinstall")) {
        return 1;
    }
    FileSystem_writeText(joinPath(project, "schemulator.json"), "{\"legacy\": true}\n");
    if (!e2eRunOk(e2eRunLifecycle(exe, home, "upgrade", project, ""), "upgrade")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(upgradeBackupPath(project)), "\"legacy\": true", "upgrade backup")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(project, "schemulator.json")), "\"schema_version\": 1", "regenerated manifest")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(keymapSourcePath(project)), "action state.save = Ctrl+V", "keymap should survive upgrade")) {
        return 1;
    }
    btrc_Vector_string* __list_564 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_564, "uninstall");
    btrc_Vector_string_push(__list_564, "--purge-generated");
    btrc_Vector_string_push(__list_564, "--purge-state");
    if (!e2eRunOk(e2eRunLifecycleArgs(exe, home, project, __list_564), "purge uninstall")) {
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

int e2eLauncherSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("schemulator-launcher-smoke");
    char* project = joinPath(tmp, "project");
    char* roms = joinPath(tmp, "roms");
    char* home = joinPath(tmp, "home");
    char* capture = joinPath(tmp, "capture");
    char* binDir = joinPath(tmp, "bin");
    char* exe = Environment_get("SCHEMULATOR_BIN", args->program);
    ensureDir(project);
    ensureDir(roms);
    ensureDir(home);
    ensureDir(capture);
    ensureDir(binDir);
    char* fakeFlatpak = joinPath(binDir, "flatpak");
    FileSystem_writeText(fakeFlatpak, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "capture=\"${SCHEM_FLATPAK_CAPTURE:?}\"\n")), "mkdir -p \"$capture\"\n")), "idx=\"$(find \"$capture\" -type f -name 'flatpak-*.args' | wc -l | tr -d ' ')\"\n")), "idx=\"$((idx + 1))\"\n")), "printf '%s\\n' \"$@\" > \"$capture/flatpak-$idx.args\"\n")));
    FileSystem_chmod(fakeFlatpak, 493);
    char* fakeBwrap = joinPath(binDir, "bwrap");
    FileSystem_writeText(fakeBwrap, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "printf '%s\\n' \"$@\" > \"${SCHEM_FLATPAK_CAPTURE:?}/retroarch.args\"\n")));
    FileSystem_chmod(fakeBwrap, 493);
    char* fakeGrim = joinPath(binDir, "grim");
    FileSystem_writeText(fakeGrim, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "out=\"${1:?}\"\n")), "mkdir -p \"$(dirname \"$out\")\"\n")), "printf 'fake screenshot %s\\n' \"$out\" > \"$out\"\n")), "printf '%s\\n' \"$out\" >> \"${SCHEM_FLATPAK_CAPTURE:?}/grim.log\"\n")));
    FileSystem_chmod(fakeGrim, 493);
    btrc_Vector_string* __list_565 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_565, "game.wua");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "cemu", __list_565), "launcher cemu")) {
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
    btrc_Vector_string* __list_566 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_566, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "dolphin", __list_566), "launcher dolphin")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "--socket=x11", "dolphin x11")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-2.args")), "org.DolphinEmu.dolphin-emu", "dolphin flatpak id")) {
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
    btrc_Vector_string* __list_567 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_567, "game.3ds");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "azahar", __list_567), "launcher azahar")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-3.args")), "--socket=x11", "azahar x11")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-3.args")), "org.azahar_emu.Azahar", "azahar flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_568 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_568, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "pcsx2", __list_568), "launcher pcsx2")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "--socket=wayland", "pcsx2 wayland")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-4.args")), "net.pcsx2.PCSX2", "pcsx2 flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_569 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_569, "game.nsp");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "ryujinx", __list_569), "launcher ryujinx")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-5.args")), "org.ryujinx.Ryujinx", "ryujinx flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_570 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_570, "game.iso");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "ppsspp", __list_570), "launcher ppsspp")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-6.args")), "org.ppsspp.PPSSPP", "ppsspp flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_571 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_571, "game.chd");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "flycast", __list_571), "launcher flycast")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-7.args")), "org.flycast.Flycast", "flycast flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_572 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_572, "game.z64");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "gopher64", __list_572), "launcher gopher64")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-8.args")), "io.github.gopher64.gopher64", "gopher64 flatpak id")) {
        return 1;
    }
    btrc_Vector_string* __list_573 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_573, "game.nds");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "melonds", __list_573), "launcher melonds")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "flatpak-9.args")), "net.kuribo64.melonDS", "melonds flatpak id")) {
        return 1;
    }
    e2eSeedFile(joinPath(project, "RetroArch/config/input.cfg"));
    e2eSeedFile(joinPath(project, "RetroArch/retroarch.cfg"));
    btrc_Vector_string* __list_574 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_574, "-L");
    btrc_Vector_string_push(__list_574, "core.so");
    btrc_Vector_string_push(__list_574, "game.gba");
    if (!e2eRunOk(e2eRunLauncher(exe, home, project, roms, binDir, capture, fakeBwrap, "retroarch", __list_574), "launcher retroarch")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "retroarch.args")), "/usr/bin/retroarch", "retroarch executable")) {
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
    printf("%s\n", "OK BTRC launcher smoke");
    return 0;
}

void e2eWriteSyncFakes(char* binDir) {
    char* fakeSystemctl = joinPath(binDir, "systemctl");
    FileSystem_writeText(fakeSystemctl, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SCHEM_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SCHEM_CAPTURE/systemctl.log\"\n")));
    FileSystem_chmod(fakeSystemctl, 493);
    char* fakeCurl = joinPath(binDir, "curl");
    FileSystem_writeText(fakeCurl, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SCHEM_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SCHEM_CAPTURE/curl.log\"\n")));
    FileSystem_chmod(fakeCurl, 493);
    char* fakeSyncthing = joinPath(binDir, "syncthing");
    FileSystem_writeText(fakeSyncthing, __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("#!/usr/bin/env sh\n", "set -eu\n")), "mkdir -p \"${SCHEM_CAPTURE:?}\"\n")), "printf '%s\\n' \"$*\" >> \"$SCHEM_CAPTURE/syncthing.log\"\n")), "if [ \"${1:-}\" = generate ]; then\n")), "  home=\"\"\n")), "  while [ $# -gt 0 ]; do\n")), "    if [ \"$1\" = -H ]; then home=\"$2\"; shift 2; else shift; fi\n")), "  done\n")), "  mkdir -p \"$home\"\n")), "  printf '<configuration><gui><apikey>test-key</apikey></gui></configuration>\\n' > \"$home/config.xml\"\n")), "  exit 0\n")), "fi\n")), "exit 0\n")));
    FileSystem_chmod(fakeSyncthing, 493);
}

int e2eSyncSmoke(CliArgs* args) {
    char* tmp = e2eTempDir("schemulator-sync-smoke");
    char* project = joinPath(tmp, "project");
    char* missingProject = joinPath(tmp, "missing-project");
    char* home = joinPath(tmp, "home");
    char* binDir = joinPath(tmp, "bin");
    char* capture = joinPath(tmp, "capture");
    char* exe = Environment_get("SCHEMULATOR_BIN", args->program);
    ensureDir(project);
    ensureDir(missingProject);
    ensureDir(home);
    ensureDir(binDir);
    ensureDir(capture);
    e2eWriteSyncFakes(binDir);
    btrc_Vector_string* __list_575 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_575, "setup");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_575), "sync setup")) {
        return 1;
    }
    char* systemctlLog = FileSystem_readText(joinPath(capture, "systemctl.log"));
    if (!e2eContains(systemctlLog, "--user daemon-reload", "sync daemon-reload")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable schemulator-syncthing.service", "sync enable service")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable schemulator-sync-force.timer", "sync enable timer")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user start schemulator-syncthing.service", "sync start service")) {
        return 1;
    }
    if (!e2eContains(FileSystem_readText(joinPath(capture, "syncthing.log")), "schemulator-emulator_state", "sync emulator state folder")) {
        return 1;
    }
    btrc_Vector_string* __list_576 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_576, "force");
    btrc_Vector_string_push(__list_576, "all");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_576), "sync force all")) {
        return 1;
    }
    btrc_Vector_string* __list_577 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_577, "force");
    btrc_Vector_string_push(__list_577, "saves");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_577), "sync force saves")) {
        return 1;
    }
    char* curlLog = FileSystem_readText(joinPath(capture, "curl.log"));
    if (!e2eContains(curlLog, "/rest/db/scan", "sync force all url")) {
        return 1;
    }
    if (!e2eContains(curlLog, "?folder=schemulator-saves", "sync force saves url")) {
        return 1;
    }
    if (!e2eContains(curlLog, "X-API-Key: test-key", "sync force api key")) {
        return 1;
    }
    btrc_Vector_string* __list_578 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_578, "autostart");
    btrc_Vector_string_push(__list_578, "disable");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_578), "sync autostart disable")) {
        return 1;
    }
    btrc_Vector_string* __list_579 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_579, "autostart");
    btrc_Vector_string_push(__list_579, "enable");
    if (!e2eRunOk(e2eRunSync(exe, home, binDir, project, __list_579), "sync autostart enable")) {
        return 1;
    }
    (systemctlLog = FileSystem_readText(joinPath(capture, "systemctl.log")));
    if (!e2eContains(systemctlLog, "--user disable schemulator-syncthing.service", "sync disable service")) {
        return 1;
    }
    if (!e2eContains(systemctlLog, "--user enable schemulator-syncthing.service", "sync re-enable service")) {
        return 1;
    }
    btrc_Vector_string* __list_580 = btrc_Vector_string_new();
    btrc_Vector_string_push(__list_580, "force");
    btrc_Vector_string_push(__list_580, "all");
    if (!e2eOk((!ExecResult_ok(e2eRunSync(exe, home, binDir, missingProject, __list_580))), "sync force without API key unexpectedly succeeded")) {
        return 1;
    }
    printf("%s\n", "OK BTRC sync smoke");
    return 0;
}

int e2eCommand(CliArgs* args) {
    char* mode = "all";
    if ((CliArgs_count(args) > 1) && (!__btrc_startsWith(CliArgs_get(args, 1), "--"))) {
        (mode = CliArgs_get(args, 1));
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
        return e2eLifecycleSmoke(args);
    }
    printUsage();
    return 1;
}

void printUsage(void) {
    printf("%s\n", "schemulator [manifest|bootstrap|doctor|deck|lifecycle|sync|config|apprun|steam-input|keymap|screenshot|sandbox|launcher|e2e] [validate|render|install|setup|reconfigure|change|uninstall|reinstall|upgrade|status|force|capture|prepare|launch] [--project PATH] [--roms PATH] [--source PATH] [--output PATH] [--dest PATH] [--target manifest|retroarch|dolphin|pcsx2|steam-input] [--emulator NAME] [--hook HOOK] [--scratch PATH] [--action ID] [--command KEYS]");
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
    return __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("input_enable_hotkey = \"ctrl\"\n", "input_exit_emulator = ")), jsonQ(keymapCommandKey(ir, "app.quit")))), "\n")), "input_load_state = ")), jsonQ(keymapCommandKey(ir, "state.load")))), "\n")), "input_menu_toggle = ")), jsonQ(keymapCommandKey(ir, "ui.menu")))), "\n")), "input_save_state = ")), jsonQ(keymapCommandKey(ir, "state.save")))), "\n")), "input_screenshot = ")), jsonQ(keymapCommandKey(ir, "ui.screenshot")))), "\n")), "input_state_slot_decrease = ")), jsonQ(keymapCommandKey(ir, "state.prev")))), "\n")), "input_state_slot_increase = ")), jsonQ(keymapCommandKey(ir, "state.next")))), "\n")), "input_toggle_fast_forward = ")), jsonQ(keymapCommandKey(ir, "speed.fast")))), "\n")), "input_toggle_fullscreen = ")), jsonQ(keymapCommandKey(ir, "ui.fullscreen")))), "\n"));
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
        int __n_582 = btrc_Vector_string_iterLen(modifierParts);
        for (int __i_581 = 0; (__i_581 < __n_582); (__i_581++)) {
            char* modifier = btrc_Vector_string_iterGet(modifierParts, __i_581);
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
        return steamInputTemplateVdf("Schemulator: Steam Deck - Neptune FULL", true, ir);
    }
    int __fstr_583_len = snprintf(NULL, 0, "unknown keymap target: %s\n", target);
    char* __fstr_583_buf = __btrc_str_track(((char*)malloc((__fstr_583_len + 1))));
    snprintf(__fstr_583_buf, (__fstr_583_len + 1), "unknown keymap target: %s\n", target);
    return __fstr_583_buf;
}

bool isKeymapTarget(char* target) {
    return (((((strcmp(target, "manifest") == 0) || (strcmp(target, "retroarch") == 0)) || (strcmp(target, "dolphin") == 0)) || (strcmp(target, "pcsx2") == 0)) || (strcmp(target, "steam-input") == 0));
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
            int __fstr_586_len = snprintf(NULL, 0, "error 0:0 keymap source not found: %s", sourcePath);
            char* __fstr_586_buf = __btrc_str_track(((char*)malloc((__fstr_586_len + 1))));
            snprintf(__fstr_586_buf, (__fstr_586_len + 1), "error 0:0 keymap source not found: %s", sourcePath);
            printf("%s\n", __fstr_586_buf);
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
            int __btrc_ret_587 = 0;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_587;
        }
        for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
            int __fstr_590_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            char* __fstr_590_buf = __btrc_str_track(((char*)malloc((__fstr_590_len + 1))));
            snprintf(__fstr_590_buf, (__fstr_590_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
            printf("%s\n", __fstr_590_buf);
        }
        int __btrc_ret_591 = 1;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_591;
    }
    if (strcmp(mode, "render") == 0) {
        if (KeymapErrors_count(errors) > 0) {
            for (int i = 0; (i < KeymapErrors_count(errors)); (i++)) {
                int __fstr_594_len = snprintf(NULL, 0, "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
                char* __fstr_594_buf = __btrc_str_track(((char*)malloc((__fstr_594_len + 1))));
                snprintf(__fstr_594_buf, (__fstr_594_len + 1), "%s %d:%d %s", btrc_Vector_string_get(errors->levels, i), btrc_Vector_int_get(errors->lines, i), btrc_Vector_int_get(errors->columns, i), btrc_Vector_string_get(errors->messages, i));
                printf("%s\n", __fstr_594_buf);
            }
            int __btrc_ret_595 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_595;
        }
        char* target = CliArgs_valueAfter(args, "--target", "manifest");
        if (!isKeymapTarget(target)) {
            int __fstr_598_len = snprintf(NULL, 0, "error 0:0 unknown keymap target '%s'", target);
            char* __fstr_598_buf = __btrc_str_track(((char*)malloc((__fstr_598_len + 1))));
            snprintf(__fstr_598_buf, (__fstr_598_len + 1), "error 0:0 unknown keymap target '%s'", target);
            printf("%s\n", __fstr_598_buf);
            int __btrc_ret_599 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_599;
        }
        char* rendered = renderKeymap(ir, target);
        char* output = CliArgs_valueAfter(args, "--output", "");
        if (((int)strlen(output)) > 0) {
            FileSystem_writeText(output, rendered);
        } else {
            printf("%s\n", rendered);
        }
        int __btrc_ret_600 = 0;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_600;
    }
    printUsage();
    int __btrc_ret_601 = 1;
    if (errors != NULL) {
        if ((--errors->__rc) <= 0) {
            KeymapErrors_destroy(errors);
        }
    }
    return __btrc_ret_601;
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
        int __fstr_604_len = snprintf(NULL, 0, "OK screenshot defaults: %s", screenshotConfigPath(project));
        char* __fstr_604_buf = __btrc_str_track(((char*)malloc((__fstr_604_len + 1))));
        snprintf(__fstr_604_buf, (__fstr_604_len + 1), "OK screenshot defaults: %s", screenshotConfigPath(project));
        printf("%s\n", __fstr_604_buf);
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
    return CliArgs_valueAfter(args, "--dest", Environment_get("SCHEMULATOR_STEAM_INPUT_DIR", joinPath(homeDir(), ".steam/steam/controller_base/templates")));
}

void copySteamInputTemplate(char* project, char* destination, char* name) {
    char* source = joinPath(project, __btrc_str_track(__btrc_strcat("steam-input/", name)));
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
        int __fstr_607_len = snprintf(NULL, 0, "OK steam-input templates: %s", destination);
        char* __fstr_607_buf = __btrc_str_track(((char*)malloc((__fstr_607_len + 1))));
        snprintf(__fstr_607_buf, (__fstr_607_len + 1), "OK steam-input templates: %s", destination);
        printf("%s\n", __fstr_607_buf);
        return 0;
    }
    if ((strcmp(mode, "status") == 0) || (strcmp(mode, "validate") == 0)) {
        writeSteamInputTemplates(project);
        reportSteamInputTemplate("neptune_simple", joinPath(project, "steam-input/neptune-simple.vdf"));
        reportSteamInputTemplate("neptune_full", joinPath(project, "steam-input/neptune-full.vdf"));
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
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SCHEMULATOR_PROJECT_DIR=", ShellWords_quote(project))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SCHEMULATOR_ROMS_DIR=", ShellWords_quote(configuredRomsRoot(project)))));
        printf("%s\n", __btrc_str_track(__btrc_strcat("export SCHEMULATOR_BIN=", ShellWords_quote(serviceExecutable()))));
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
        ensureRomDirsAt(roms);
        writeEsDeFiles(project);
        int __fstr_610_len = snprintf(NULL, 0, "OK roms_dir: %s", roms);
        char* __fstr_610_buf = __btrc_str_track(((char*)malloc((__fstr_610_len + 1))));
        snprintf(__fstr_610_buf, (__fstr_610_len + 1), "OK roms_dir: %s", roms);
        printf("%s\n", __fstr_610_buf);
        return 0;
    }
    if (strcmp(mode, "show") == 0) {
        reportFile("sync_config", syncConfigPath(project));
        int __fstr_613_len = snprintf(NULL, 0, "  roms_dir: %s", configuredRomsRoot(project));
        char* __fstr_613_buf = __btrc_str_track(((char*)malloc((__fstr_613_len + 1))));
        snprintf(__fstr_613_buf, (__fstr_613_len + 1), "  roms_dir: %s", configuredRomsRoot(project));
        printf("%s\n", __fstr_613_buf);
        return 0;
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
    char* env = __btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat(__btrc_str_track(__btrc_strcat("SCHEMULATOR_PROJECT_DIR=", ShellWords_quote(project))), " SCHEMULATOR_ROMS_DIR=")), ShellWords_quote(configuredRomsRoot(project))));
    if (commandExists("es-de")) {
        UnixShell_runRaw(shell, __btrc_str_track(__btrc_strcat(env, " es-de")), false, false, "");
        int __btrc_ret_614 = 0;
        if (shell != NULL) {
            if ((--shell->__rc) <= 0) {
                UnixShell_destroy(shell);
            }
        }
        return __btrc_ret_614;
    } else {
        printf("%s\n", "MISSING es-de: use the bundled AppImage or install ES-DE");
    }
    int __btrc_ret_615 = 127;
    if (shell != NULL) {
        if ((--shell->__rc) <= 0) {
            UnixShell_destroy(shell);
        }
    }
    return __btrc_ret_615;
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
    if (((((((strcmp(mode, "setup") == 0) || (strcmp(mode, "reconfigure") == 0)) || (strcmp(mode, "change") == 0)) || (strcmp(mode, "uninstall") == 0)) || (strcmp(mode, "reinstall") == 0)) || (strcmp(mode, "upgrade") == 0)) || (strcmp(mode, "status") == 0)) {
        return lifecycleCommand(args, project);
    }
    if (strcmp(mode, "verify") == 0) {
        doctorSteamDeck(project);
        KeymapErrors* errors = KeymapErrors_new();
        compileKeymap((FileSystem_exists(keymapSourcePath(project)) ? FileSystem_readText(keymapSourcePath(project)) : defaultKeymapSource()), errors);
        if (KeymapErrors_count(errors) > 0) {
            int __btrc_ret_616 = 1;
            if (errors != NULL) {
                if ((--errors->__rc) <= 0) {
                    KeymapErrors_destroy(errors);
                }
            }
            return __btrc_ret_616;
        }
        int __btrc_ret_617 = 0;
        if (errors != NULL) {
            if ((--errors->__rc) <= 0) {
                KeymapErrors_destroy(errors);
            }
        }
        return __btrc_ret_617;
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

char* launcherNameFromProgram(char* program) {
    char* base = PathTools_basename(program);
    if (!__btrc_startsWith(base, "schem-")) {
        return "";
    }
    if (((strcmp(base, "schem-btrc") == 0) || (strcmp(base, "schem-flatpak") == 0)) || (strcmp(base, "schemulator") == 0)) {
        return "";
    }
    return Strings_removePrefix(base, "schem-");
}

int main(int argc, char** argv) {
    CliArgs* args = CliArgs_new(argc, argv);
    char* command = CliArgs_command(args);
    char* project = CliArgs_valueAfter(args, "--project", Environment_get("SCHEMULATOR_PROJECT_DIR", "."));
    char* programLauncher = launcherNameFromProgram(args->program);
    if (((int)strlen(programLauncher)) > 0) {
        int __btrc_ret_618 = launcherRunEmulator(project, programLauncher, launcherPassthroughArgs(args));
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_618;
    }
    if ((strcmp(command, "") == 0) || (strcmp(command, "manifest") == 0)) {
        char* output = CliArgs_valueAfter(args, "--output", "schemulator.json");
        writeGeneratedManifest(output);
        int __btrc_ret_619 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_619;
    }
    if (strcmp(command, "bootstrap") == 0) {
        bootstrapSteamDeck(project);
        int __btrc_ret_620 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_620;
    }
    if (strcmp(command, "doctor") == 0) {
        doctorSteamDeck(project);
        int __btrc_ret_621 = 0;
        if (args != NULL) {
            if ((--args->__rc) <= 0) {
                CliArgs_destroy(args);
            }
        }
        return __btrc_ret_621;
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
    int __btrc_ret_622 = 1;
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
    return __btrc_ret_622;
    if (args != NULL) {
        if ((--args->__rc) <= 0) {
            CliArgs_destroy(args);
        }
    }
}
