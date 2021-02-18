
#include "ogl_imgui.h"
#include "strings.h"
#include "icon.h"

#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <vector>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#ifndef NDEBUG
#define RESSOURCES_PATH "../data/"
#else
#define RESSOURCES_PATH "data/"
#endif

#ifdef WIN32
#include <process.h>
#define FFMPEG_EXEC "ffmpeg.exe"
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <spawn.h>
#define FFMPEG_EXEC "ffmpeg"
#endif


typedef struct DialogEntry {
    unsigned int VoiceActor;
    unsigned int VoiceLineID;
} DialogEntry;

#define FFMPEG_PATH RESSOURCES_PATH FFMPEG_EXEC
#define LANGS_FILE RESSOURCES_PATH "langs.txt"
#define ACTORS_FILE RESSOURCES_PATH "actors.txt"
#define VOICELINES_FILE RESSOURCES_PATH "voicelines.txt"

static int entriesAmount = 0;
static int currentLanguage = LANG_EN;
std::vector<DialogEntry> dialogEntries;

const DialogEntry defaultEntry = { VA_AL, 0 };

//Wrapper for cross-platform... if only spawn was universal...
int launchAndWaitChildProcess(char* file_or_path, char** argv, bool isAbsolutePath) {
    argv[0] = file_or_path;
#ifdef WIN32
    if (isAbsolutePath)
        return spawnv(P_WAIT, file_or_path, argv);
    else
        return spawnvp(P_WAIT, file_or_path, argv);
#else
    int ret = 0;
    pid_t pid;
    if (isAbsolutePath)
        ret = posix_spawn(&pid, file_or_path, NULL, NULL, argv, NULL);
    else
        ret = posix_spawnp(&pid, file_or_path, NULL, NULL, argv, NULL);
    if (ret == 0)
        waitpid(pid, NULL, 0);
    return ret;
#endif
}

void getLineFromFH(FILE* fh, char* outStr, size_t maxSiz) {
    char ch, buf[2048] = { u8"Paradox ERR" };
    int ret, bufctr = 0;
    if (maxSiz < sizeof(buf)) {
        while (bufctr < sizeof(buf) && bufctr < maxSiz) {
            ret = fread(&ch, 1, 1, fh);
            if (ret == 0 || ch == '\n') {
                if (bufctr != 0) buf[bufctr] = '\0'; //If nothing was read, we need to copy out Paradox ERR; so don't NUL the first char.
                break;
            }
            else buf[bufctr++] = ch;
        }
    }
    else maxSiz = sizeof(buf) - 1;
    strncpy(outStr, buf, maxSiz);
}

//Returns NULL on error, a valid address otherwise.
//NOTE : you need to free() the returned address after usage.
//allocsize is an OPTIONAL parameter, the allocation size will be copied there if specified.
char* copyStringToMallocedSpace(char* string, size_t* allocsize = NULL) {
    size_t alloclen = strlen(string) + 1;
    char* ret = (char*)malloc(alloclen);
    if (ret != NULL)
        strncpy(ret, string, alloclen);
    if (allocsize != NULL)
        *allocsize = alloclen;
    return ret;
}

//Returns 1 on success, 0 otherwise.
bool loadStrings(void) {
    FILE *langFH = fopen(LANGS_FILE, "r"), *actorFH = fopen(ACTORS_FILE, "r"), *linesFH = fopen(VOICELINES_FILE, "r");
    if (langFH == NULL) {
        fprintf(stderr, "Failed to open '" LANGS_FILE "'.\n");
        return 0;
    }
    if (actorFH == NULL) {
        fprintf(stderr, "Failed to open '" ACTORS_FILE "'.\n");
        return 0;
    }
    for (int langID = 0; langID < LANGUAGES_COUNT; langID++) {
        getLineFromFH(langFH, languages[langID], sizeof(languages[langID]));
        for (int actorID = 0; actorID < VA_COUNT; actorID++) {
            char lineBuf[512];
            getLineFromFH(actorFH, lineBuf, sizeof(lineBuf));
            char* strp = copyStringToMallocedSpace(lineBuf);
            if (strp == NULL) {
                fprintf(stderr, "Failed to allocate memory for voice actor name string ('%s').\n", lineBuf);
                return 0;
            }
            VoiceActorsNames[langID][actorID] = strp;
            for (int stringID = 0; stringID < VoiceLinesCounts[actorID]; stringID++) {
                getLineFromFH(linesFH, lineBuf, sizeof(lineBuf));
                strp = copyStringToMallocedSpace(lineBuf);
                if (strp == NULL) {
                    fprintf(stderr, "Failed to allocate memory for voiceline name strings.\n");
                    return 0;
                }
                VoiceLinesNames[langID][actorID][stringID] = strp;
            }
        }
    }
    fclose(langFH); fclose(actorFH); fclose(linesFH);
    return 1;
}

void unloadStrings(void) {
    for (int langID = 0; langID < LANGUAGES_COUNT; langID++)
        for (int actorID = 0; actorID < VA_COUNT; actorID++) {
            if (VoiceActorsNames[langID][actorID] != NULL) 
                free(VoiceActorsNames[langID][actorID]);
            for (int stringID = 0; stringID < VoiceLinesCounts[actorID]; stringID++)
                if (VoiceLinesNames[langID][actorID][stringID] != NULL)
                    free(VoiceLinesNames[langID][actorID][stringID]);
        }
}

void createAudioFile() {
    if (entriesAmount < 2) return;
    FILE* fh = fopen(RESSOURCES_PATH"filelist.txt", "w");
    if (fh == NULL) {
        fprintf(stderr, "Failed opening '" RESSOURCES_PATH "filelist.txt' file for writing.\n");
        return;
    }
    for (int i = 0; i < entriesAmount; i++) { //Write the command list
        fprintf(fh, "file '" RESSOURCES_PATH "%s/%s/%d.flac'\n", languageCodes[currentLanguage], actorCodes[dialogEntries[i].VoiceActor], dialogEntries[i].VoiceLineID);
    }
    fclose(fh);

    int rgn = rand();
    char outFileName[64] = { 0 };
    sprintf(outFileName, "%d.flac", rgn);
    char* argv[] = { NULL, "-f", "concat", "-safe", "0", "-i", RESSOURCES_PATH"filelist.txt", outFileName, NULL };
    int ret = launchAndWaitChildProcess(FFMPEG_EXEC, argv, false); //First, we try to run it from path.
    if (ret != 0) {
        fprintf(stderr, "Error in spawn/exec of ffmpeg process (first try) : 0x%X, 0x%X\n", ret, errno);
        ret = launchAndWaitChildProcess(FFMPEG_PATH, argv, true); //Then from local directory
        if (ret != 0) {
            fprintf(stderr, "Error in spawn/exec of ffmpeg process (second try) : 0x%X, 0x%X\n", ret, errno);
            fprintf(stderr, "File export to %s must have failed ! Sorry :'(\n", outFileName);
            return;
        }
    }
    printf("File should have been successfully exported as '%s'.\n", outFileName);
}

int main(int, char**){
    bool ret = loadStrings();
    if (!ret) {
        fprintf(stderr, "Failed loading strings files (%d).\nAre you sure they are present ?\n", errno);
        (void)getchar();
        return 1;
    }

    GLFWwindow* window = NULL;
    ret = init_OGL_ImGui(&window);
    IM_ASSERT(ret);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL; //Disable saving, as there are no window positions to save anyways.

    //Load icon
    GLFWimage winIcon = { 0 };
    winIcon.pixels = stbi_load_from_memory(icon, iconLen, &winIcon.width, &winIcon.height, NULL, 4 /* RGBA */);
    glfwSetWindowIcon(window, 1, &winIcon);
    stbi_image_free(winIcon.pixels);

    // Our state
    bool showDemoWindow = false;
    bool vsyncBool = true;

    dialogEntries.resize(32); //Pre-allocate some space. Without this, vector.erase will raise an exception for some reason.

    srand(time(NULL)); //Setup "random" seed, used for output file name

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            ImGui::BeginMainMenuBar();
            if (ImGui::MenuItem("Add new")) {
                dialogEntries.push_back(defaultEntry);
                entriesAmount++;
                printf("New entries amount : %d\n", entriesAmount);
            }
            if (ImGui::MenuItem("Create dialog !")) {
                for (int i = 0; i < entriesAmount; i++)
                    printf("%s->%s\n(%d->%d)\n", currentVANSet[dialogEntries[i].VoiceActor], 
                           currentVLNSet[dialogEntries[i].VoiceActor][dialogEntries[i].VoiceLineID], 
                            dialogEntries[i].VoiceActor, dialogEntries[i].VoiceLineID);
                createAudioFile();
            }
            char framerateText[64] = { 0 };
            sprintf(framerateText, "%.2f FPS (avg)", io.Framerate);
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(framerateText).x);
            ImGui::Text(framerateText);
            float menubarheight = ImGui::GetWindowSize().y;
            ImGui::EndMainMenuBar();
            ImGui::SetNextWindowPos(ImVec2(0, menubarheight));
            ImGui::SetNextWindowSize(ImVec2(width, height - menubarheight));

            ImGui::Begin("MainWindow", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
#ifndef NDEBUG
            ImGui::Checkbox("Show Demo Window", &showDemoWindow);
            ImGui::SameLine();
            ImGui::Checkbox("V-Sync", &vsyncBool);
            ImGui::SameLine();
#endif
            if (ImGui::BeginCombo("Language", languages[currentLanguage])) {
                for (int i = 0; i < LANGUAGES_COUNT; i++) {
                    const bool isSelected = (currentLanguage == i);
                    if (ImGui::Selectable(languages[i], isSelected))
                        currentLanguage = i;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }


            for (int i = 0; i < entriesAmount; i++) {
                ImGui::PushID(i);
                ImGui::BeginChild("child", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.245f, 81), true, ImGuiWindowFlags_NoResize);
                int actorID = dialogEntries[i].VoiceActor;
                if (ImGui::BeginCombo("Voice Actor", currentVANSet[actorID])) {
                    for (int j = 0; j < VA_COUNT; j++) {
                        const bool isSelected = (actorID == j);
                        if (ImGui::Selectable(currentVANSet[j], isSelected)) {
                            dialogEntries[i].VoiceActor = j;
                            dialogEntries[i].VoiceLineID = 0;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::BeginCombo("Voice Line", currentVLNSet[actorID][dialogEntries[i].VoiceLineID])) {
                    int maxIdx = VoiceLinesCounts[actorID];
                    for (int j = 0; j < maxIdx; j++) {
                        const bool isSelected = (dialogEntries[i].VoiceLineID == j);
                        if (ImGui::Selectable(currentVLNSet[actorID][j], isSelected))
                            dialogEntries[i].VoiceLineID = j;

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Delete")) {
                    dialogEntries.erase(dialogEntries.begin() + i);
                    entriesAmount--;
                    printf("New entries amount : %d\n", entriesAmount);
                }
                ImGui::SameLine();
                ImGui::Text("#%d\n", i);
                ImGui::EndChild();
                ImGui::PopID();
                if ( (i + 1) % 4 != 0) ImGui::SameLine();
            }
            

            ImGui::End();
        }

        if (showDemoWindow)
            ImGui::ShowDemoWindow();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#ifndef NDEBUG
        glfwSwapInterval(vsyncBool);
#endif
        glfwSwapBuffers(window);
    }

    // Cleanup
    unloadStrings();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}